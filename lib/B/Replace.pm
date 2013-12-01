package B::Replace;
# ABSTRACT: Open-heart surgery on Perl optrees

use strict;
use warnings;

# VERSION

use Exporter 'import';

our @EXPORT_OK = qw(
    replace_op
    replace_tree
    replace_sequence
    detach_tree
    study_sub

    KEEP_OPS
    KEEP_TARGETS
);

use constant {
   KEEP_OPS     => 1,
   KEEP_TARGETS => 2,
};

use XSLoader;

XSLoader::load(__PACKAGE__);

sub study_sub { return B::Replace::CvInfo->new($_[0]) }

1;

__END__

=head1 SYNOPSIS

    use B::Replace qw(study_sub);
    use B::Generate;
    use B::Utils qw(walkoptree_simple);

    sub arthur {
        my $answer = $_[0] + $_[1];

        return $answer;
    }

    my $rep = study_sub(\&arthur);
    my @adds;

    # find the wrong ops
    walkoptree_simple(B::svref_2object(\&arthur)->ROOT, sub {
        push @adds, $_[0] if ${$_[0]} && $_[0]->name eq 'add'
    });

    # replace with the right answer
    for my $op (@adds) {
        my $const = B::SVOP->new('const', 0, 42);
        $rep->replace_tree($op, $const, 0);
    }

    say arthur("the answer to life the universe and everything");

=head1 DESCRIPTION

See the L<CAVEATS> section before using this module!

Allows modifying the Perl optree in place, can be useful while
experimenting with custom ops (es. implementing the equivalent of
C<aelemfast> for hashes) or when testing a Perl JIT.

Every method has a corresponding function taking a code reference as
the first parameter.

The C<$flags> parameter can be:

=over 4

=item 0 (default)

Destroy the ops in the original tree

=item KEEP_OPS

Keep the ops in the original tree

=item KEEP_TARGETS

Destroy the ops in the original tree but keep the targets

=back

=method replace_tree

    $obj->replace_tree($original, $replacement);
    $obj->replace_tree($original, $replacement, $flags);

Replaces the C<$original> optree with C<$replacement>.

=method replace_sequence

    $obj->replace_sequence($seq_start, $seq_end, $replacement);
    $obj->replace_sequence($seq_start, $seq_end, $replacement, $flags);

Replaces a sequence of optrees with C<$replacement>.  C<$seq_start>
and C<$seq_end> must be direct childs of the same parent op.

=method detach_tree

    $obj->detach_tree($op);
    $obj->detach_tree($op, $flags);

Removes C<$op> from both execution order and the tree structure.

=func study_sub

   $obj = study_sub(\&code);

Collects information about the optree structure, to make multiple
operations faster (the function interface walks the optree every
time).

=head1 CAVEATS

=over 4

=item Does not handle logical/conditional ops

Does not fixup the C<op_next> pointer of the controlled expressions.

=item Replacing an optree with another optree

Works under the assumption that control flow enters through the op
found by recursively following the C<op_first> pointer, and exits
through the root node in the tree.

=item Replacing PMOP subtrees

Not implemented.

=item Replacing trees with multiple entry points

Does not die(), and it should.

=item Object interface might be buggy

If in doubt, try with the functional interface (but please report the bug).

=item Don't modify a subroutine while it's being executed/traversed

Includes executed in different threads or walked using one of the
C<walkoptree> variants.

=back
