#include "replace.h"

#include <vector>

#include <tr1/unordered_map>
#include <tr1/unordered_set>

struct OpInfo
{
    // TODO some OPs have multiple predecessors (end of conditional branches)
    OP *pred;
    OP *older_sibling;
    OP *parent;

    OpInfo() :
        pred(0), older_sibling(0), parent(0)
    { }
};

typedef std::tr1::unordered_map<OP *, OpInfo> LinkInfo;
typedef std::vector<OP *> OpVector;
typedef std::tr1::unordered_set<OP *> OpSet;
typedef std::tr1::unordered_map<OP *, OP *> OpHash;

#if PERL_API_VERSION >= 14
static XOP trace_xop;
#endif

static OP *trace_pp(pTHX)
{
    TRACEOP *op = (TRACEOP *)PL_op;

    ++op->count;

    return op->op_next;
}

void register_op(pTHX)
{
#if PERL_API_VERSION >= 14
    XopENTRY_set(&trace_xop, xop_name, "myxop");
    XopENTRY_set(&trace_xop, xop_desc, "Useless custom op");
    XopENTRY_set(&trace_xop, xop_class, OA_BASEOP);

    Perl_custom_op_register(aTHX_ trace_pp, &trace_xop);
#endif
}

TRACEOP *trace_op(OP *next)
{
    dTHX;
    TRACEOP *op;

    NewOp(1101, op, 1, TRACEOP);
    op->op_type = OP_CUSTOM;
    op->op_ppaddr = trace_pp;
    op->op_next = next;
    op->count = 0;

    return op;
}

typedef enum {
    OPc_NULL,	/* 0 */
    OPc_BASEOP,	/* 1 */
    OPc_UNOP,	/* 2 */
    OPc_BINOP,	/* 3 */
    OPc_LOGOP,	/* 4 */
    OPc_LISTOP,	/* 5 */
    OPc_PMOP,	/* 6 */
    OPc_SVOP,	/* 7 */
    OPc_PADOP,	/* 8 */
    OPc_PVOP,	/* 9 */
    OPc_LOOP,	/* 10 */
    OPc_COP	/* 11 */
} opclass;

static opclass
cc_opclass(pTHX_ const OP *o)
{
    if (!o)
	return OPc_NULL;

    if (o->op_type == 0)
	return (o->op_flags & OPf_KIDS) ? OPc_UNOP : OPc_BASEOP;

    if (o->op_type == OP_SASSIGN)
	return ((o->op_private & OPpASSIGN_BACKWARDS) ? OPc_UNOP : OPc_BINOP);

    if (o->op_type == OP_AELEMFAST) {
	if (o->op_flags & OPf_SPECIAL)
	    return OPc_BASEOP;
	else
#ifdef USE_ITHREADS
	    return OPc_PADOP;
#else
	    return OPc_SVOP;
#endif
    }

#ifdef USE_ITHREADS
    if (o->op_type == OP_GV || o->op_type == OP_GVSV ||
	o->op_type == OP_RCATLINE)
	return OPc_PADOP;
#endif

    switch (PL_opargs[o->op_type] & OA_CLASS_MASK) {
    case OA_BASEOP:
	return OPc_BASEOP;

    case OA_UNOP:
	return OPc_UNOP;

    case OA_BINOP:
	return OPc_BINOP;

    case OA_LOGOP:
	return OPc_LOGOP;

    case OA_LISTOP:
	return OPc_LISTOP;

    case OA_PMOP:
	return OPc_PMOP;

    case OA_SVOP:
	return OPc_SVOP;

    case OA_PADOP:
	return OPc_PADOP;

    case OA_PVOP_OR_SVOP:
        /*
         * Character translations (tr///) are usually a PVOP, keeping a 
         * pointer to a table of shorts used to look up translations.
         * Under utf8, however, a simple table isn't practical; instead,
         * the OP is an SVOP, and the SV is a reference to a swash
         * (i.e., an RV pointing to an HV).
         */
	return (o->op_private & (OPpTRANS_TO_UTF|OPpTRANS_FROM_UTF))
		? OPc_SVOP : OPc_PVOP;

    case OA_LOOP:
	return OPc_LOOP;

    case OA_COP:
	return OPc_COP;

    case OA_BASEOP_OR_UNOP:
	/*
	 * UNI(OP_foo) in toke.c returns token UNI or FUNC1 depending on
	 * whether parens were seen. perly.y uses OPf_SPECIAL to
	 * signal whether a BASEOP had empty parens or none.
	 * Some other UNOPs are created later, though, so the best
	 * test is OPf_KIDS, which is set in newUNOP.
	 */
	return (o->op_flags & OPf_KIDS) ? OPc_UNOP : OPc_BASEOP;

    case OA_FILESTATOP:
	/*
	 * The file stat OPs are created via UNI(OP_foo) in toke.c but use
	 * the OPf_REF flag to distinguish between OP types instead of the
	 * usual OPf_SPECIAL flag. As usual, if OPf_KIDS is set, then we
	 * return OPc_UNOP so that walkoptree can find our children. If
	 * OPf_KIDS is not set then we check OPf_REF. Without OPf_REF set
	 * (no argument to the operator) it's an OP; with OPf_REF set it's
	 * an SVOP (and op_sv is the GV for the filehandle argument).
	 */
	return ((o->op_flags & OPf_KIDS) ? OPc_UNOP :
#ifdef USE_ITHREADS
		(o->op_flags & OPf_REF) ? OPc_PADOP : OPc_BASEOP);
#else
		(o->op_flags & OPf_REF) ? OPc_SVOP : OPc_BASEOP);
#endif
    case OA_LOOPEXOP:
	/*
	 * next, last, redo, dump and goto use OPf_SPECIAL to indicate that a
	 * label was omitted (in which case it's a BASEOP) or else a term was
	 * seen. In this last case, all except goto are definitely PVOP but
	 * goto is either a PVOP (with an ordinary constant label), an UNOP
	 * with OPf_STACKED (with a non-constant non-sub) or an UNOP for
	 * OP_REFGEN (with goto &sub) in which case OPf_STACKED also seems to
	 * get set.
	 */
	if (o->op_flags & OPf_STACKED)
	    return OPc_UNOP;
	else if (o->op_flags & OPf_SPECIAL)
	    return OPc_BASEOP;
	else
	    return OPc_PVOP;
    }
    warn("can't determine class of operator %s, assuming BASEOP\n",
	 PL_op_name[o->op_type]);
    return OPc_BASEOP;
}

void fill_pred(OP *op, OP *next, LinkInfo &links)
{
    if (!next || op->op_type == 0)
        return;

    links[next].pred = op;
}

void fill_parent(OP *op, OP *kid, LinkInfo &links)
{
    if (!kid)
        return;

    links[kid].parent = op;
}

void fill_linkinfo(pTHX_ OP *op, LinkInfo &links);

void fill_older_sibling(pTHX_ OP *firstborn, LinkInfo &links)
{
    OP *pred = 0, *curr = firstborn;

    while (curr)
    {
        if (pred)
            links[curr].older_sibling = pred;
        fill_linkinfo(aTHX_ curr, links);

        pred = curr;
        curr = curr->op_sibling;
    }
}

void fill_linkinfo(pTHX_ OP *op, LinkInfo &links)
{
    fill_pred(op, op->op_next, links);

    if (op->op_flags & OPf_KIDS)
        fill_older_sibling(aTHX_ cUNOPx(op)->op_first, links);

    switch (cc_opclass(aTHX_ op))
    {
    case OPc_UNOP:
    {
        UNOP *unop = (UNOP *)op;

        fill_parent(op, unop->op_first, links);
    }
        break;
    case OPc_BINOP:
    {
        BINOP *binop = (BINOP *)op;

        fill_parent(op, binop->op_first, links);
        fill_parent(op, binop->op_last, links);
    }
        break;
    case OPc_LOGOP:
    {
        LOGOP *logop = (LOGOP *)op;

        fill_pred(op, logop->op_other, links);
        fill_parent(op, logop->op_first, links);
        fill_parent(op, logop->op_other, links);
    }
        break;
    case OPc_LISTOP:
    {
        LISTOP *listop = (LISTOP *)op;

        fill_parent(op, listop->op_first, links);
        fill_parent(op, listop->op_last, links);
    }
        break;
    case OPc_PMOP:
    {
        PMOP *pmop = (PMOP *)op;

        fill_parent(op, pmop->op_first, links);
        fill_parent(op, pmop->op_last, links);

        // ignore pmrepl stuff
    }
        break;
    case OPc_LOOP:
    {
        LOOP *loop = (LOOP *)op;

        fill_parent(op, loop->op_first, links);
        fill_parent(op, loop->op_last, links);
    }
        break;
    case OPc_NULL:
    case OPc_BASEOP:
    case OPc_SVOP:
    case OPc_PADOP:
    case OPc_PVOP:
    case OPc_COP:
        break;
    }
}

#define REPLACE_IF(op, member) \
    if ((op)->member == original)               \
        (op)->member = replacement

void replace_next(pTHX_ OP *op, OP *original, OP *replacement)
{
    REPLACE_IF(op, op_next);

    if (cc_opclass(aTHX_ op) == OPc_LOGOP)
        REPLACE_IF(cLOGOPx(op), op_other);
}

void replace_child(pTHX_ OP *op, OP *original, OP *replacement)
{
    switch (cc_opclass(aTHX_ op))
    {
    case OPc_UNOP:
    {
        UNOP *unop = (UNOP *)op;

        REPLACE_IF(unop, op_first);
    }
        break;
    case OPc_BINOP:
    {
        BINOP *binop = (BINOP *)op;

        REPLACE_IF(binop, op_first);
        REPLACE_IF(binop, op_last);
    }
        break;
    case OPc_LOGOP:
    {
        LOGOP *logop = (LOGOP *)op;

        REPLACE_IF(logop, op_first);
        REPLACE_IF(logop, op_other);
    }
        break;
    case OPc_LISTOP:
    {
        LISTOP *listop = (LISTOP *)op;

        REPLACE_IF(listop, op_first);
        REPLACE_IF(listop, op_last);
    }
        break;
    case OPc_PMOP:
    {
        PMOP *pmop = (PMOP *)op;

        REPLACE_IF(pmop, op_first);
        REPLACE_IF(pmop, op_last);

        // ignore pmrepl stuff
    }
        break;
    case OPc_LOOP:
    {
        LOOP *loop = (LOOP *)op;

        REPLACE_IF(loop, op_first);
        REPLACE_IF(loop, op_last);
    }
        break;
    case OPc_NULL:
    case OPc_BASEOP:
    case OPc_SVOP:
    case OPc_PADOP:
    case OPc_PVOP:
    case OPc_COP:
        assert(0);
        break;
    }
}

#undef REPLACE_IF

void replace_op(OP *root, OP *original, OP *replacement, bool keep)
{
    dTHX;
    LinkInfo links;

    fill_linkinfo(aTHX_ root, links);

    LinkInfo::iterator it = links.find(original);
    if (it == links.end())
        croak("Did not find original op in the tree");
    OpInfo opinfo = it->second;

    if (opinfo.pred)
        replace_next(aTHX_ opinfo.pred, original, replacement);
    if (opinfo.parent)
        replace_child(aTHX_ opinfo.parent, original, replacement);
    if (opinfo.older_sibling)
        opinfo.older_sibling->op_sibling = replacement;

    if (!keep)
        op_free(original);
}

void tree_nodes(pTHX_ OP *op, OpVector &accumulator)
{
    if (op->op_type != OP_NULL)
        accumulator.push_back(op);

    if (op->op_flags & OPf_KIDS)
    {
        for (OP *curr = cUNOPx(op)->op_first; curr; curr = curr->op_sibling)
            tree_nodes(aTHX_ curr, accumulator);
    }
}

void replace_tree(OP *root, OP *original, OP *replacement, bool keep)
{
    dTHX;
    LinkInfo links;
    OpVector nodes;
    OpHash tree_pred;

    fill_linkinfo(aTHX_ root, links);

    LinkInfo::iterator it = links.find(original);
    if (it == links.end())
        croak("Did not find original op in the tree");
    OpInfo opinfo = it->second;

    tree_nodes(aTHX_ original, nodes);

    for (OpVector::iterator it = nodes.begin(), end = nodes.end();
         it != end; ++it)
        if (links[*it].pred)
            tree_pred.insert(std::make_pair(links[*it].pred, *it));

    for (OpVector::iterator it = nodes.begin(), end = nodes.end();
         it != end; ++it)
        tree_pred.erase(*it);

    if (tree_pred.size() > 1)
        croak("Found %d predecessor for the tree, 1 expected", tree_pred.size());

    if (tree_pred.size())
        replace_next(aTHX_ tree_pred.begin()->first, tree_pred.begin()->second, replacement);
    if (opinfo.parent)
        replace_child(aTHX_ opinfo.parent, original, replacement);
    if (opinfo.older_sibling)
        opinfo.older_sibling->op_sibling = replacement;

    replacement->op_next = original->op_next;
    replacement->op_sibling = original->op_sibling;

    if (!keep)
    {
        // TODO this code is becoming so tied to Perl::JIT that
        //      it makes a lot of sense to just move it
        for (OpVector::iterator it = nodes.begin(), end = nodes.end();
             it != end; ++it)
            (*it)->op_targ = 0;

        op_free(original);
    }
}

void detach_tree(OP *root, OP *original, bool keep)
{
    dTHX;
    OP *o;

    NewOp(42, o, 1, OP);
    o->op_flags = OPf_WANT_VOID;
    o->op_type = OP_STUB;
    o->op_ppaddr = PL_ppaddr[OP_STUB];
    replace_tree(root, original, o, keep);
}
