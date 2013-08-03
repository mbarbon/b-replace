#!/usr/bin/env perl

use strict;
use warnings;

use Test::More tests => 2;

use B::Replace qw(replace_tree trace_op);
use B::Generate;
use B;

sub dummy {
    return $_[0] + $_[1];
}

my $dummy = B::svref_2object(\&dummy);
my $const = B::SVOP->new('const', 0, 3);
my $add = $dummy->START;

$add = $add->next until $add->name eq 'add';
$const->next($add->next);

replace_tree($dummy->ROOT, $add, $const);

is(dummy(1, 2), 3);
is(dummy(3, 4), 3);
