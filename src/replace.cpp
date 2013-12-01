#include "replace.h"

#include <vector>

typedef std::vector<OP *> OpVector;

typedef std::tr1::unordered_map<OP *, OP *> OpHash;

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

void assign_next(OP *op, OP *next)
{
    switch (op->op_type)
    {
        // documented as needing manual attention, avoid breaking them
    case OP_COND_EXPR:
    case OP_AND:
    case OP_OR:
    case OP_DOR:
    case OP_ANDASSIGN:
    case OP_ORASSIGN:
    case OP_DORASSIGN:
        break;
    default:
        op->op_next = next;
        break;
    }
}

void fill_pred(OP *op, OP *next, LinkInfo &links)
{
    if (!next || op->op_type == 0)
        return;

    links[next].pred.insert(op);
}

void fill_parent(OP *op, OP *kid, LinkInfo &links)
{
    if (!kid)
        return;

    links[kid].parent = op;
}

void fill_linkinfo(pTHX_ OP *op, LinkInfo &links);
void clear_linkinfo(pTHX_ OP *op, LinkInfo &links);

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
        fill_pred(op, loop->op_redoop, links);
        fill_pred(op, loop->op_lastop, links);
        fill_pred(op, loop->op_nextop, links);
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

void clear_linkinfo(pTHX_ OP *op, LinkInfo &links)
{
    links.erase(op);

    links[op->op_next].pred.erase(op);
    if (cc_opclass(aTHX_ op) == OPc_LOOP) {
        links[cLOOPx(op)->op_redoop].pred.erase(op);
        links[cLOOPx(op)->op_nextop].pred.erase(op);
        links[cLOOPx(op)->op_lastop].pred.erase(op);
    }
    if (cc_opclass(aTHX_ op) == OPc_LOGOP)
        links[cLOGOPx(op)->op_other].pred.erase(op);

    if (op->op_flags & OPf_KIDS)
    {
        for (OP *curr = cUNOPx(op)->op_first; curr; curr = curr->op_sibling)
            clear_linkinfo(aTHX_ curr, links);
    }
}

#define REPLACE_IF(op, member) \
    if ((op)->member == original)               \
        (op)->member = replacement

#define REPLACE_FIRST_IF(op) \
    if ((op)->op_first == original)             \
        replace_first(aTHX_ cLISTOPx(op), original, replacement);

#define REPLACE_LAST_IF(op) \
    if ((op)->op_last == original)              \
        replace_last(aTHX_ cLISTOPx(op), original, replacement);

void replace_first(pTHX_ LISTOP *op, OP *original, OP *replacement)
{
    if (!replacement)
        replacement = op->op_first->op_sibling;

    op->op_first = replacement;
}

void replace_last(pTHX_ LISTOP *op, OP *original, OP *replacement)
{
    if (!replacement)
    {
        for (replacement = op->op_first;
             replacement && replacement->op_sibling;
             replacement = replacement->op_sibling)
            if (replacement != original)
                op->op_last = replacement;
    }
    else
        op->op_last = replacement;
}

void replace_next(pTHX_ OP *op, OP *original, OP *replacement)
{
    REPLACE_IF(op, op_next);
    if (cc_opclass(aTHX_ op) == OPc_LOOP) {
        REPLACE_IF((LOOP*)op, op_redoop);
        REPLACE_IF((LOOP*)op, op_nextop);
        REPLACE_IF((LOOP*)op, op_lastop);
    }

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

        // in this case we add a stub OP to preserve the property that
        // op->op_first->op_sibling == op->op_last
        if (binop->op_first == original && !replacement)
        {
            NewOp(42, replacement, 1, OP);
            replacement->op_flags = OPf_WANT_VOID;
            replacement->op_type = OP_STUB;
            replacement->op_ppaddr = PL_ppaddr[OP_STUB];
            replacement->op_sibling = binop->op_last;
        }

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

        REPLACE_FIRST_IF(listop);
        REPLACE_LAST_IF(listop);
    }
        break;
    case OPc_PMOP:
    {
        PMOP *pmop = (PMOP *)op;

        REPLACE_FIRST_IF(pmop);
        REPLACE_LAST_IF(pmop);

        // ignore pmrepl stuff
    }
        break;
    case OPc_LOOP:
    {
        LOOP *loop = (LOOP *)op;

        REPLACE_FIRST_IF(loop);
        REPLACE_LAST_IF(loop);
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

void replace_sibling(pTHX_ OP *older_sibling, OP *original, OP *replacement)
{
    older_sibling->op_sibling = replacement;
}

#undef REPLACE_FIRST_IF
#undef REPLACE_LAST_IF
#undef REPLACE_IF

void replace_op(pTHX_ CV *cv, LinkInfo &links, OP *original, OP *replacement, int flags)
{
    bool keep = flags & KEEP_OPS;

    LinkInfo::iterator it = links.find(original);
    if (it == links.end())
        croak("Did not find original op in the tree");
    OpInfo opinfo = it->second;

    if (opinfo.pred.size())
        for (OpSet::iterator it = opinfo.pred.begin(), end = opinfo.pred.end();
             it != end; ++it)
            replace_next(aTHX_ *it, original, replacement);
    if (CvSTART(cv) == original)
        CvSTART(cv) = replacement;
    if (opinfo.parent)
        replace_child(aTHX_ opinfo.parent, original, replacement);
    if (opinfo.older_sibling)
        replace_sibling(aTHX_ opinfo.older_sibling, original, replacement);

    assign_next(replacement, original->op_next);
    replacement->op_sibling = original->op_sibling;

    links.erase(original);
    links[replacement] = opinfo;
    fill_linkinfo(aTHX_ replacement, links);
    if (original->op_sibling)
        links[original->op_sibling].older_sibling = replacement;

    if (!keep)
        op_free(original);
}

void replace_op(CV *cv, OP *original, OP *replacement, int flags)
{
    dTHX;

    LinkInfo links;

    fill_linkinfo(aTHX_ CvROOT(cv), links);
    replace_op(aTHX_ cv, links, original, replacement, flags);
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

void replace_sequence(pTHX_ CV *cv, LinkInfo &links, OP *orig_seq_start, OP *orig_seq_end, OP *replacement, int flags)
{
    OpVector nodes;
    OpHash tree_pred;
    OP *start = replacement;
    bool keep = flags & KEEP_OPS;

    // handle the case when the replacement is a full tree rather than
    // a single op
    while (start && start->op_flags & OPf_KIDS)
        start = cUNOPx(start)->op_first;
    if (!start)
        start = orig_seq_end->op_next;

    LinkInfo::iterator start_seq_it = links.find(orig_seq_start);
    if (start_seq_it == links.end())
        croak("Did not find original start op in the tree");
    OpInfo start_opinfo = start_seq_it->second;

    LinkInfo::iterator end_seq_it = links.find(orig_seq_end);
    if (end_seq_it == links.end())
        croak("Did not find original end op in the tree");

    OP *o = orig_seq_start;
    do {
	tree_nodes(aTHX_ o, nodes);
    } while (o != orig_seq_end && (o = o->op_sibling));

    for (OpVector::iterator it = nodes.begin(), end = nodes.end();
         it != end; ++it)
    {
        for (OpSet::iterator pred = links[*it].pred.begin(), end = links[*it].pred.end(); pred != end; ++pred)
            tree_pred.insert(std::make_pair(*pred, *it));
        if (*it == CvSTART(cv))
            CvSTART(cv) = start;
    }

    for (OpVector::iterator it = nodes.begin(), end = nodes.end();
         it != end; ++it)
        tree_pred.erase(*it);

    for (OpHash::iterator it = tree_pred.begin(), end = tree_pred.end();
         it != end; ++it)
        replace_next(aTHX_ it->first, it->second, start);
    if (start_opinfo.parent)
        replace_child(aTHX_ start_opinfo.parent, orig_seq_start, replacement);
    if (start_opinfo.parent)
        replace_child(aTHX_ start_opinfo.parent, orig_seq_end, replacement);
    if (start_opinfo.older_sibling)
        replace_sibling(aTHX_ start_opinfo.older_sibling, orig_seq_start, replacement ? replacement : orig_seq_end->op_sibling);

    if (replacement)
    {
        assign_next(replacement, orig_seq_end->op_next);
        replacement->op_sibling = orig_seq_end->op_sibling;

        for (OpHash::iterator it = tree_pred.begin(), end = tree_pred.end();
             it != end; ++it)
            links[start].pred.insert(it->first);
        links[replacement].parent = start_opinfo.parent;
        links[replacement].older_sibling = start_opinfo.older_sibling;
        fill_linkinfo(aTHX_ replacement, links);
        if (orig_seq_end->op_sibling)
            links[orig_seq_end->op_sibling].older_sibling = replacement;
    }

    if (!keep)
    {
        if (flags & KEEP_TARGETS)
        {
            for (OpVector::iterator it = nodes.begin(), end = nodes.end();
                 it != end; ++it)
                (*it)->op_targ = 0;
        }

	OP *o = orig_seq_start;
	OP *next;
	do {
	    next = o->op_sibling;
	    OP *tmp = o;
            clear_linkinfo(aTHX_ tmp, links);
	    op_free(tmp); // just in case it's a macro
	} while (o != orig_seq_end && (o = next));
    } else {
	OP *o = orig_seq_start;
	OP *next;
	do {
	    next = o->op_sibling;
            clear_linkinfo(aTHX_ o, links);
	} while (o != orig_seq_end && (o = next));
    }
}

void replace_sequence(CV *cv, OP *orig_seq_start, OP *orig_seq_end, OP *replacement, int flags)
{
    dTHX;

    LinkInfo links;

    fill_linkinfo(aTHX_ CvROOT(cv), links);
    replace_sequence(aTHX_ cv, links, orig_seq_start, orig_seq_end, replacement, flags);
}

void replace_tree(CV *cv, OP *original, OP *replacement, int flags)
{
    dTHX;

    LinkInfo links;

    fill_linkinfo(aTHX_ CvROOT(cv), links);
    replace_sequence(aTHX_ cv, links, original, original, replacement, flags);
}

void detach_tree(CV *cv, OP *original, int flags)
{
    dTHX;

    LinkInfo links;

    fill_linkinfo(aTHX_ CvROOT(cv), links);
    replace_sequence(aTHX_ cv, links, original, original, 0, flags);
}

CvInfo::CvInfo(CV *_cv) : cv(_cv)
{
    dTHX;

    SvREFCNT_inc(cv);
    fill_linkinfo(aTHX_ CvROOT(cv), links);
}

CvInfo::~CvInfo()
{
    dTHX;

    SvREFCNT_dec(cv);
}

void CvInfo::replace_op(OP *original, OP *replacement, int flags)
{
    dTHX;

    ::replace_op(aTHX_ cv, links, original, replacement, flags);
}

void CvInfo::replace_tree(OP *original, OP *replacement, int flags)
{
    dTHX;

    ::replace_sequence(aTHX_ cv, links, original, original, replacement, flags);
}

void CvInfo::replace_sequence(OP *orig_seq_start, OP *orig_seq_end, OP *replacement, int flags)
{
    dTHX;

    ::replace_sequence(aTHX_ cv, links, orig_seq_start, orig_seq_end, replacement, flags);
}

void CvInfo::detach_tree(OP *original, int flags)
{
    dTHX;

    ::replace_sequence(aTHX_ cv, links, original, original, 0, flags);
}
