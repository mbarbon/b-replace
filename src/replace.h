#ifndef _B_REPLACE_H
#define _B_REPLACE_H

#include "EXTERN.h"
#include "perl.h"

void replace_op(CV *cv, OP *original, OP *replacement, bool keep = false);
void replace_tree(CV *cv, OP *original, OP *replacement, bool keep = false);
void replace_sequence(CV *cv, OP *orig_seq_start, OP *orig_seq_end, OP *replacement, bool keep = false);
void detach_tree(CV *cv, OP *original, bool keep = false);

#endif // _B_REPLACE_H
