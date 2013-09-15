#ifndef _B_REPLACE_H
#define _B_REPLACE_H

#include "EXTERN.h"
#include "perl.h"

void replace_op(OP *root, OP *original, OP *replacement, bool keep = false);
void replace_tree(OP *root, OP *original, OP *replacement, bool keep = false);
void replace_sequence(OP *root, OP *orig_seq_start, OP *orig_seq_end, OP *replacement, bool keep = false);
void detach_tree(OP *root, OP *original, bool keep = false);

#endif // _B_REPLACE_H
