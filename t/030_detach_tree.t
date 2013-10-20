#!/usr/bin/env perl

use strict;
use warnings;

use Test::More tests => 14;

use B::Replace qw(detach_tree);
use B;

sub dummy1 {
    return (1, 2, 3);
}

SCOPE: {
    my $dummy = B::svref_2object(\&dummy1);
    my $pushmark = $dummy->START;

    $pushmark = $pushmark->next until $pushmark->name eq 'pushmark';

    detach_tree(\&dummy1, $pushmark->sibling->sibling);

    is_deeply([dummy1()], [1, 3]);
    is($pushmark->sibling->sibling->name, 'const');
}

sub dummy2 {
    return ($a, 2, 3);
}

SCOPE: {
    my $dummy = B::svref_2object(\&dummy2);
    my $return = $dummy->START;

    $return = $return->next until $return->name eq 'return';

    detach_tree(\&dummy2, $return->first);

    is($return->first->first->name, 'gvsv', 'first op patched out correctly');
    is($return->last->name, 'const');
    isa_ok($return->first->sibling->sibling->sibling, 'B::NULL');
}

sub dummy3 {
    return (1, $a, 3);
}

SCOPE: {
    my $dummy = B::svref_2object(\&dummy3);
    my $return = $dummy->START;

    $return = $return->next until $return->name eq 'return';

    detach_tree(\&dummy3, $return->last);

    is($return->first->sibling->name, 'const');
    is($return->last->first->name, 'gvsv', 'last op patched out correctly');
    isa_ok($return->first->sibling->sibling->sibling, 'B::NULL');
}

sub dummy4 {
    return $a + 1;
}

SCOPE: {
    my $dummy = B::svref_2object(\&dummy4);
    my $add = $dummy->START;

    $add = $add->next until $add->name eq 'add';

    detach_tree(\&dummy4, $add->first);

    is($add->first->name, 'stub');
    is($add->last->name, 'const');
    is($add->first->sibling->name, 'const');
    isa_ok($add->first->sibling->sibling, 'B::NULL');
}

sub dummy5 {
    for (1..2) {
        $a += 1;
    }
}

SCOPE: {
    my $dummy = B::svref_2object(\&dummy5);
    my $and = $dummy->START;

    $and = $and->next until $and->name eq 'and';

    die unless $and->other->name eq 'nextstate';
    die unless $and->first->sibling->first->name eq 'nextstate';
    die unless $and->first->sibling->name eq 'lineseq';
    detach_tree(\&dummy5, $and->other);

    is($and->other->name, 'gvsv');
    is($and->first->sibling->first->name, 'add');
}
