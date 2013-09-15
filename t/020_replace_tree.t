#!/usr/bin/env perl

use strict;
use warnings;

use Test::More tests => 5;

use B::Replace qw(replace_tree replace_sequence);
use B::Generate;
use B;

sub dummy {
    return $_[0] + $_[1];
}

SCOPE: {
    my $dummy = B::svref_2object(\&dummy);
    my $const = B::SVOP->new('const', 0, 3);
    my $add = $dummy->START;

    $add = $add->next until $add->name eq 'add';
    $const->next($add->next);

    replace_tree($dummy->ROOT, $add, $const);

    is(dummy(1, 2), 3);
    is(dummy(3, 4), 3);
}

sub dummy2 {
    my $x = $_[0];
    $x = 11; # to be patched out
    $x = 12; # also to be patched out
    return $x;
}

SCOPE: {
    is(dummy2(42), 12);

    my $dummy = B::svref_2object(\&dummy2);
    my $const = B::SVOP->new('const', 0, 3);
    my $nextstate = $dummy->START;

    $nextstate = $nextstate->next until $nextstate->name eq 'nextstate'
                                        && $nextstate->next->name eq 'const';
    my $sassign = $nextstate->sibling->sibling->sibling;
    $const->next($sassign->next);

    replace_sequence($dummy->ROOT, $nextstate, $sassign, $const);

    is(dummy2(12), 12);
    is(dummy2(42), 42);
}
