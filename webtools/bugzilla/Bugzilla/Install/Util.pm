# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is the Bugzilla Bug Tracking System.
#
# The Initial Developer of the Original Code is Everything Solved.
# Portions created by Everything Solved are Copyright (C) 2006
# Everything Solved. All Rights Reserved.
#
# Contributor(s): Max Kanat-Alexander <mkanat@bugzilla.org>

package Bugzilla::Install::Util;

# The difference between this module and Bugzilla::Util is that this
# module may require *only* Bugzilla::Constants and built-in
# perl modules.

use strict;

use Bugzilla::Constants;

use POSIX ();

use base qw(Exporter);
our @EXPORT_OK = qw(
    display_version_and_os
    indicate_progress
    vers_cmp
);

sub display_version_and_os {
    # Display version information
    printf "\n* This is Bugzilla " . BUGZILLA_VERSION . " on perl %vd\n", $^V;
    my @os_details = POSIX::uname;
    # 0 is the name of the OS, 2 is the major version,
    my $os_name = $os_details[0] . ' ' . $os_details[2];
    if (ON_WINDOWS) {
        require Win32;
        $os_name = Win32::GetOSName();
    }
    # 3 is the minor version.
    print "* Running on $os_name $os_details[3]\n"
}

sub indicate_progress {
    my ($params) = @_;
    my $current = $params->{current};
    my $total   = $params->{total};
    my $every   = $params->{every} || 1;

    print "." if !($current % $every);
    if ($current % ($every * 60) == 0) {
        print "$current/$total (" . int($current * 100 / $total) . "%)\n";
    }
}

# This is taken straight from Sort::Versions 1.5, which is not included
# with perl by default.
sub vers_cmp {
    my ($a, $b) = @_;

    # Remove leading zeroes - Bug 344661
    $a =~ s/^0*(\d.+)/$1/;
    $b =~ s/^0*(\d.+)/$1/;

    my @A = ($a =~ /([-.]|\d+|[^-.\d]+)/g);
    my @B = ($b =~ /([-.]|\d+|[^-.\d]+)/g);

    my ($A, $B);
    while (@A and @B) {
        $A = shift @A;
        $B = shift @B;
        if ($A eq '-' and $B eq '-') {
            next;
        } elsif ( $A eq '-' ) {
            return -1;
        } elsif ( $B eq '-') {
            return 1;
        } elsif ($A eq '.' and $B eq '.') {
            next;
        } elsif ( $A eq '.' ) {
            return -1;
        } elsif ( $B eq '.' ) {
            return 1;
        } elsif ($A =~ /^\d+$/ and $B =~ /^\d+$/) {
            if ($A =~ /^0/ || $B =~ /^0/) {
                return $A cmp $B if $A cmp $B;
            } else {
                return $A <=> $B if $A <=> $B;
            }
        } else {
            $A = uc $A;
            $B = uc $B;
            return $A cmp $B if $A cmp $B;
        }
    }
    @A <=> @B;
}

__END__

=head1 NAME

Bugzilla::Install::Util - Utility functions that are useful both during
installation and afterwards.

=head1 DESCRIPTION

This module contains various subroutines that are used primarily
during installation. However, these subroutines can also be useful to
non-installation code, so they have been split out into this module.

The difference between this module and L<Bugzilla::Util> is that this
module is safe to C<use> anywhere in Bugzilla, even during installation,
because it depends only on L<Bugzilla::Constants> and built-in perl modules.

None of the subroutines are exported by default--you must explicitly
export them.

=head1 SUBROUTINES

=over

=item C<display_version_and_os>

Prints out some text lines, saying what version of Bugzilla we're running,
what perl version we're using, and what OS we're running on.

=item C<indicate_progress>

=over

=item B<Description>

This prints out lines of dots as a long update is going on, to let the user
know where we are and that we're not frozen. A new line of dots will start
every 60 dots.

Sample usage: C<indicate_progress({ total =E<gt> $total, current =E<gt>
$count, every =E<gt> 1 })>

=item B<Sample Output>

Here's some sample output with C<total = 1000> and C<every = 10>:

 ............................................................600/1000 (60%)
 ........................................

=item B<Params>

=over

=item C<total> - The total number of items we're processing.

=item C<current> - The number of the current item we're processing.

=item C<every> - How often the function should print out a dot.
For example, if this is 10, the function will print out a dot every
ten items. Defaults to 1 if not specified.

=back

=item B<Returns>: nothing

=back

=item C<vers_cmp>

=over

=item B<Description>

This is a comparison function, like you would use in C<sort>, except that
it compares two version numbers. So, for example, 2.10 would be greater
than 2.2.

It's based on versioncmp from L<Sort::Versions>, with some Bugzilla-specific
fixes.

=item B<Params>: C<$a> and C<$b> - The versions you want to compare.

=item B<Returns>

C<-1> if C<$a> is less than C<$b>, C<0> if they are equal, or C<1> if C<$a>
is greater than C<$b>.

=back

=back
