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
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Terry Weissman <terry@mozilla.org>
#                 Dan Mosedale <dmose@mozilla.org>
#                 Jacob Steenhagen <jake@bugzilla.org>
#                 Bradley Baetz <bbaetz@student.usyd.edu.au>
#                 Christopher Aillon <christopher@aillon.com>

package Bugzilla::Util;

use strict;

use base qw(Exporter);
@Bugzilla::Util::EXPORT = qw(is_tainted trick_taint detaint_natural
                             html_quote url_quote value_quote xml_quote
                             css_class_quote
                             lsearch max min
                             trim format_time);

use Bugzilla::Config;

# This is from the perlsec page, slightly modifed to remove a warning
# From that page:
#      This function makes use of the fact that the presence of
#      tainted data anywhere within an expression renders the
#      entire expression tainted.
# Don't ask me how it works...
sub is_tainted {
    return not eval { my $foo = join('',@_), kill 0; 1; };
}

sub trick_taint {
    require Carp;
    Carp::confess("Undef to trick_taint") unless defined $_[0];
    $_[0] =~ /^(.*)$/s;
    $_[0] = $1;
    return (defined($_[0]));
}

sub detaint_natural {
    $_[0] =~ /^(\d+)$/;
    $_[0] = $1;
    return (defined($_[0]));
}

sub html_quote {
    my ($var) = (@_);
    $var =~ s/\&/\&amp;/g;
    $var =~ s/</\&lt;/g;
    $var =~ s/>/\&gt;/g;
    $var =~ s/\"/\&quot;/g;
    return $var;
}

# This orignally came from CGI.pm, by Lincoln D. Stein
sub url_quote {
    my ($toencode) = (@_);
    $toencode =~ s/([^a-zA-Z0-9_\-.])/uc sprintf("%%%02x",ord($1))/eg;
    return $toencode;
}

sub css_class_quote {
    my ($toencode) = (@_);
    $toencode =~ s/ /_/g;
    $toencode =~ s/([^a-zA-Z0-9_\-.])/uc sprintf("&#x%x;",ord($1))/eg;
    return $toencode;
}

sub value_quote {
    my ($var) = (@_);
    $var =~ s/\&/\&amp;/g;
    $var =~ s/</\&lt;/g;
    $var =~ s/>/\&gt;/g;
    $var =~ s/\"/\&quot;/g;
    # See bug http://bugzilla.mozilla.org/show_bug.cgi?id=4928 for 
    # explanaion of why bugzilla does this linebreak substitution. 
    # This caused form submission problems in mozilla (bug 22983, 32000).
    $var =~ s/\r\n/\&#013;/g;
    $var =~ s/\n\r/\&#013;/g;
    $var =~ s/\r/\&#013;/g;
    $var =~ s/\n/\&#013;/g;
    return $var;
}

sub xml_quote {
    my ($var) = (@_);
    $var =~ s/\&/\&amp;/g;
    $var =~ s/</\&lt;/g;
    $var =~ s/>/\&gt;/g;
    $var =~ s/\"/\&quot;/g;
    $var =~ s/\'/\&apos;/g;
    return $var;
}

sub lsearch {
    my ($list,$item) = (@_);
    my $count = 0;
    foreach my $i (@$list) {
        if ($i eq $item) {
            return $count;
        }
        $count++;
    }
    return -1;
}

sub max {
    my $max = shift(@_);
    foreach my $val (@_) {
        $max = $val if $val > $max;
    }
    return $max;
}

sub min {
    my $min = shift(@_);
    foreach my $val (@_) {
        $min = $val if $val < $min;
    }
    return $min;
}

sub trim {
    my ($str) = @_;
    if ($str) {
      $str =~ s/^\s+//g;
      $str =~ s/\s+$//g;
    }
    return $str;
}

sub format_time {
    my ($time) = @_;

    my ($year, $month, $day, $hour, $min);
    if ($time =~ m/^\d{14}$/) {
        # We appear to have a timestamp direct from MySQL
        $year  = substr($time,0,4);
        $month = substr($time,4,2);
        $day   = substr($time,6,2);
        $hour  = substr($time,8,2);
        $min   = substr($time,10,2);
    }
    elsif ($time =~ m/^(\d{4})[-\.](\d{2})[-\.](\d{2}) (\d{2}):(\d{2})(:\d{2})?$/) {
        $year  = $1;
        $month = $2;
        $day   = $3;
        $hour  = $4;
        $min   = $5;
    }
    else {
        warn "Date/Time format ($time) unrecogonzied";
    }

    if (defined $year) {
        $time = "$year-$month-$day $hour:$min";
        $time .= " " . &::Param('timezone') if &::Param('timezone');
    }
    return $time;
}

1;

__END__

=head1 NAME

Bugzilla::Util - Generic utility functions for bugzilla

=head1 SYNOPSIS

  use Bugzilla::Util;

  # Functions for dealing with variable tainting
  $rv = is_tainted($var);
  trick_taint($var);
  detaint_natural($var);

  # Functions for quoting
  html_quote($var);
  url_quote($var);
  value_quote($var);
  xml_quote($var);

  # Functions for searching
  $loc = lsearch(\@arr, $val);
  $val = max($a, $b, $c);
  $val = min($a, $b, $c);

  # Functions for trimming variables
  $val = trim(" abc ");

  # Functions for formatting time
  format_time($time);

=head1 DESCRIPTION

This package contains various utility functions which do not belong anywhere
else.

B<It is not intended as a general dumping group for something which
people feel might be useful somewhere, someday>. Do not add methods to this
package unless it is intended to be used for a significant number of files,
and it does not belong anywhere else.

=head1 FUNCTIONS

This package provides several types of routines:

=head2 Tainting

Several functions are available to deal with tainted variables. B<Use these
with care> to avoid security holes.

=over 4

=item C<is_tainted>

Determines whether a particular variable is tainted

=item C<trick_taint($val)>

Tricks perl into untainting a particular variable.

Use trick_taint() when you know that there is no way that the data
in a scalar can be tainted, but taint mode still bails on it.

B<WARNING!! Using this routine on data that really could be tainted defeats
the purpose of taint mode.  It should only be used on variables that have been
sanity checked in some way and have been determined to be OK.>

=item C<detaint_natural($num)>

This routine detaints a natural number. It returns a true value if the
value passed in was a valid natural number, else it returns false. You
B<MUST> check the result of this routine to avoid security holes.

=back

=head2 Quoting

Some values may need to be quoted from perl. However, this should in general
be done in the template where possible.

=over 4

=item C<html_quote($val)>

Returns a value quoted for use in HTML, with &, E<lt>, E<gt>, and E<34> being
replaced with their appropriate HTML entities.

=item C<url_quote($val)>

Quotes characters so that they may be included as part of a url.

=item C<css_class_quote($val)>

Quotes characters so that they may be used as CSS class names. Spaces
are replaced by underscores.

=item C<value_quote($val)>

As well as escaping html like C<html_quote>, this routine converts newlines
into &#013;, suitable for use in html attributes.

=item C<xml_quote($val)>

This is similar to C<html_quote>, except that ' is escaped to &apos;. This
is kept separate from html_quote partly for compatibility with previous code
(for &apos;) and partly for future handling of non-ASCII characters.

=back

=head2 Searching

Functions for searching within a set of values.

=over 4

=item C<lsearch($list, $item)>

Returns the position of C<$item> in C<$list>. C<$list> must be a list
reference.

If the item is not in the list, returns -1.

=item C<max($a, $b, ...)>

Returns the maximum from a set of values.

=item C<min($a, $b, ...)>

Returns the minimum from a set of values.

=back

=head2 Trimming

=over 4

=item C<trim($str)>

Removes any leading or trailing whitespace from a string. This routine does not
modify the existing string.

=back

=head2 Formatting Time

=over 4

=item C<format_time($time)>

Takes a time and appends the timezone as defined in editparams.cgi.  This routine
will be expanded in the future to adjust for user preferences regarding what
timezone to display times in.  In the future, it may also allow for the time to be
shown in different formats.

=back

