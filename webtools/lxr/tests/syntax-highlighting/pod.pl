#!/bin/perl

print "this is not a comment";

# this is a comment

print "this is not a comment";

 =pod
print "this block is a compiler error";
 =cut
=pod
This is POD
=cut

print "this is not a comment";

# this is a comment

print "this is not a comment";

=pod
This is POD
#comment
=cut

print "this is not a comment";

=pod
This is silly
^=cut

print "this is pod!";
