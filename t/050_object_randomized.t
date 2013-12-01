#!/usr/bin/env perl

use strict;
use warnings;

use Test::More tests => 4;

use B::Replace qw(study_sub);
use B::Generate;
use B::Concise;
use B;

my $seed = time;

if ($ENV{RANDOM_SEED}) {
    srand($seed = int($ENV{RANDOM_SEED}));
}

diag("RANDOM_SEED=$seed");

sub dummy {
    my ($n) = @_;
    my ($r1, $r2) = (1, 1);

    while ($n > 1) {
        my $next = $r1 + $r2;
        $r1 = $r2;
        $r2 = $next;
        --$n;
    }

    return $r1;
}

sub get_random_op {
    my ($dummy) = @_;
    my @ops;

    no warnings 'redefine', 'once';
    local *B::OP::addit = sub {
        push @ops, $_[0]
            unless $_[0]->name eq 'stub' ||
                   $_[0]->name eq 'leavesub' ||
                   $_[0]->name eq 'lineseq';
    };

    B::walkoptree($dummy->ROOT, 'addit');

    return unless @ops;
    return $ops[rand @ops];
}

sub concise_dump_note {
    B::Concise::walk_output(\my $buf);
    B::Concise::reset_sequence();
    B::Concise::compile('', '', $_[0])->();

    note($_) for split /\n/, $buf;
}

SCOPE: {
    my $rep = study_sub(\&dummy);
    my $dummy = B::svref_2object(\&dummy);
    my @history;

    is(dummy(7), 13);
    is(dummy(15), 610);

    while (my $op = get_random_op($dummy)) {
        my $null = B::OP->new('stub', 0);
        push @history, [$op, $null];
        note "#", scalar @history, " Replacing ", $op->name;
        concise_dump_note(\&dummy);
        $rep->replace_tree($op, $null, 1);
    }

    while (@history) {
        my ($orig, $repl) = @{pop @history};
        note "#", scalar @history + 1, " Restoring ", $orig->name;
        $rep->replace_tree($repl, $orig, 0);
        concise_dump_note(\&dummy);
    }

   is(dummy(7), 13);
   is(dummy(15), 610);
}
