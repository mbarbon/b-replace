#!/usr/bin/env perl

use strict;
use warnings;

use Test::More tests => 4;

use B::Replace qw(study_sub);
use B::Generate;
use B;

sub dummy {
    my ($a, $b) = @_;
    my $c = $a + $b;
    my $d = $c * 2;

    return $d;
}

SCOPE: {
    my $rep = study_sub(\&dummy);
    my $dummy = B::svref_2object(\&dummy);
    my $const = B::SVOP->new('const', 0, 3);
    my $null = B::OP->new('stub', 0);
    my $add = $dummy->START;
    my $padsv;

    $add = $add->next until $add->name eq 'add';
    $padsv = $add->sibling;

    $rep->replace_tree($padsv, $null, 1);
    $rep->replace_tree($add, $const, 1);
    $rep->replace_tree($null, $padsv, 1);

    is(dummy(1, 2), 6);
    is(dummy(3, 4), 6);

    $rep->replace_tree($const, $add, 1);

    is(dummy(1, 2), 6);
    is(dummy(3, 4), 14);
}
