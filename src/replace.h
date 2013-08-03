#ifndef _B_REPLACE_H
#define _B_REPLACE_H

#include "EXTERN.h"
#include "perl.h"

struct TRACEOP
{
    BASEOP;
    int count;
};

TRACEOP *trace_op(pTHX_ OP *next);
void register_op(pTHX);

void replace_op(pTHX_ OP *root, OP *original, OP *replacement, bool keep = false);
void replace_tree(pTHX_ OP *root, OP *original, OP *replacement, bool keep = false);

#endif // _B_REPLACE_H
