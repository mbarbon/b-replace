package B::Replace;
# ABSTRACT: fill me in...

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
