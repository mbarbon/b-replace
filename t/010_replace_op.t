#!/usr/bin/env perl

use strict;
use warnings;

use Test::More tests => 3;

use B::Replace qw(replace_op);
use B::Generate;
use B;

sub dummy {
    my ($a, $b) = @_;

    return $a + $b;
}

my $dummy = B::svref_2object(\&dummy);
my $const = B::SVOP->new('const', 0, 3);
my $add = $dummy->START;

$add = $add->next until $add->name eq 'add';

replace_op(\&dummy, $add->first, $const);

is(dummy(1, 2), 5);
is(dummy(1, $dummy), 3 + $dummy);
is(dummy('1', '2'), 5);
