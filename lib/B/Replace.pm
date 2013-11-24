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
);

use XSLoader;

XSLoader::load(__PACKAGE__);

sub study_sub { return B::Replace::CvInfo->new($_[0]) }

1;
