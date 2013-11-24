#ifndef _B_REPLACE_H
#define _B_REPLACE_H

#include "EXTERN.h"
#include "perl.h"

#include <tr1/unordered_map>
#include <tr1/unordered_set>

typedef std::tr1::unordered_set<OP *> OpSet;

struct OpInfo
{
    OpSet pred;
    OP *older_sibling;
    OP *parent;

    OpInfo() :
        older_sibling(0), parent(0)
    { }
};

typedef std::tr1::unordered_map<OP *, OpInfo> LinkInfo;

struct CvInfo
{
    CV *cv;
    LinkInfo links;

    CvInfo(CV *cv);
    ~CvInfo();

    void replace_op(OP *original, OP *replacement, bool keep = false);
    void replace_tree(OP *original, OP *replacement, bool keep = false);
    void replace_sequence(OP *orig_seq_start, OP *orig_seq_end, OP *replacement, bool keep = false);
    void detach_tree(OP *original, bool keep = false);
};

void replace_op(CV *cv, OP *original, OP *replacement, bool keep = false);
void replace_tree(CV *cv, OP *original, OP *replacement, bool keep = false);
void replace_sequence(CV *cv, OP *orig_seq_start, OP *orig_seq_end, OP *replacement, bool keep = false);
void detach_tree(CV *cv, OP *original, bool keep = false);

#endif // _B_REPLACE_H
