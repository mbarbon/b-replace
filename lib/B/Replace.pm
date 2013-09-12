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
    trace_op
);

use XSLoader;

XSLoader::load(__PACKAGE__);

1;
