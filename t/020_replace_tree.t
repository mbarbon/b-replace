#!/usr/bin/env perl

use strict;
use warnings;

use Test::More tests => 11;

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

    replace_tree(\&dummy, $add, $const);

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

    replace_sequence(\&dummy2, $nextstate, $sassign, $const);

    is(dummy2(12), 12);
    is(dummy2(42), 42);
}

sub dummy3 {
    my ($a, $b) = @_;
    my $c = ((($a += 1) && ($b += 2)), 7);
    return $a + $b + $c;
}

SCOPE: {
    is(dummy3(1, 2), 13);
    is(dummy3(-1, 2), 9);
    is(dummy3(0, -2), 8);

    my $dummy = B::svref_2object(\&dummy3);
    my $const = B::SVOP->new('const', 0, 6);
    my $seven = $dummy->START;

    $seven = $seven->next until $seven->name eq 'and';
    $seven = $seven->next;

    $const->next($seven->next);

    replace_tree(\&dummy3, $seven, $const);

    is(dummy3(1, 2), 12);
    is(dummy3(-1, 2), 8);
    is(dummy3(0, -2), 7);
}
