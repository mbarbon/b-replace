#!/usr/bin/env perl

use strict;
use warnings;

use Test::More tests => 7;

use B::Replace qw(replace_op trace_op);
use B;

sub dummy {
    return $_[0] + $_[1];
}

my $dummy = B::svref_2object(\&dummy);
my $trace = trace_op($dummy->START->next);

replace_op($dummy->ROOT, $dummy->START->next, $trace, 1);

is($trace->count, 0);

is(dummy(1, 2), 3);
is($trace->count, 1);

is(dummy($dummy, $trace), $dummy + $trace);
is($trace->count, 2);

is(dummy('1', '2'), 3);
is($trace->count, 3);
