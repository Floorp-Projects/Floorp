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
#                 Max Kanat-Alexander <mkanat@kerio.com>

package Bugzilla::Util;

use strict;

use base qw(Exporter);
@Bugzilla::Util::EXPORT = qw(is_tainted trick_taint detaint_natural
                             html_quote url_quote value_quote xml_quote
                             css_class_quote
                             lsearch max min
                             trim diff_strings wrap_comment
                             format_time format_time_decimal
                             file_mod_time);

use Bugzilla::Config;
use Bugzilla::Error;
use Bugzilla::Constants;
use Date::Parse;
use Date::Format;
use Text::Autoformat qw(autoformat break_wrap);

our $autoformat_options = {
    # Reformat all paragraphs, not just the first one.
    all        => 1,
    # Break only on spaces, and let long lines overflow.
    break      => break_wrap,
    # Columns are COMMENT_COLS wide.
    right      => COMMENT_COLS,
    # Don't reformat into perfect paragraphs, just wrap.
    fill       => 0,
    # Don't compress whitespace.
    squeeze    => 0,
    # Lines starting with ">" are not wrapped.
    ignore     => qr/^>/,
    # Don't re-arrange numbered lists.
    renumber   => 0,
    # Keep short lines at the end of paragraphs as-is.
    widow      => 0,
    # Even if a paragraph looks centered, don't "auto-center" it.
    autocentre => 0,
};

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

sub diff_strings {
    my ($oldstr, $newstr) = @_;

    # Split the old and new strings into arrays containing their values.
    $oldstr =~ s/[\s,]+/ /g;
    $newstr =~ s/[\s,]+/ /g;
    my @old = split(" ", $oldstr);
    my @new = split(" ", $newstr);

    # For each pair of (old, new) entries:
    # If they're equal, set them to empty. When done, @old contains entries
    # that were removed; @new contains ones that got added.

    foreach my $oldv (@old) {
        foreach my $newv (@new) {
            next if ($newv eq '');
            if ($oldv eq $newv) {
                $newv = $oldv = '';
            }
        }
    }
    my $removed = join (", ", grep { $_ ne '' } @old);
    my $added = join (", ", grep { $_ ne '' } @new);

    return ($removed, $added);
}

sub wrap_comment ($) {
    my ($comment) = @_;
    return autoformat($comment, $autoformat_options);
}

sub format_time {
    my ($time) = @_;

    my ($year, $month, $day, $hour, $min, $sec);
    if ($time =~ m/^\d{14}$/) {
        # We appear to have a timestamp direct from MySQL
        $year  = substr($time,0,4);
        $month = substr($time,4,2);
        $day   = substr($time,6,2);
        $hour  = substr($time,8,2);
        $min   = substr($time,10,2);
    }
    elsif ($time =~ m/^(\d{4})[-\.](\d{2})[-\.](\d{2}) (\d{2}):(\d{2})(:(\d{2}))?$/) {
        $year  = $1;
        $month = $2;
        $day   = $3;
        $hour  = $4;
        $min   = $5;
        $sec   = $7;
    }
    else {
        warn "Date/Time format ($time) unrecogonzied";
    }

    if (defined $year) {
        $time = "$year-$month-$day $hour:$min";
        if (defined $sec) {
            $time .= ":$sec";
        }
        $time .= " " . &::Param('timezone') if &::Param('timezone');
    }
    return $time;
}

sub format_time_decimal {
    my ($time) = (@_);

    my $newtime = sprintf("%.2f", $time);

    if ($newtime =~ /0\Z/) {
        $newtime = sprintf("%.1f", $time);
    }

    return $newtime;
}

sub file_mod_time ($) {
    my ($filename) = (@_);
    my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
        $atime,$mtime,$ctime,$blksize,$blocks)
        = stat($filename);
    return $mtime;
}

sub ValidateDate {
    my ($date, $format) = @_;

    my $ts  = str2time($date);
    my $date2 = time2str("%Y-%m-%d", $ts);

    $date =~ s/(\d+)-0*(\d+?)-0*(\d+?)/$1-$2-$3/; 
    $date2 =~ s/(\d+)-0*(\d+?)-0*(\d+?)/$1-$2-$3/;

    if ($date ne $date2) {
        ThrowUserError('illegal_date', {date => $date, format => $format});
    } 
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

  # Functions for manipulating strings
  $val = trim(" abc ");
  ($removed, $added) = diff_strings($old, $new);
  $wrapped = wrap_comment($comment);

  # Functions for formatting time
  format_time($time);

  # Functions for dealing with files
  $time = file_mod_time($filename);

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

=head2 String Manipulation

=over 4

=item C<trim($str)>

Removes any leading or trailing whitespace from a string. This routine does not
modify the existing string.

=item C<diff_strings($oldstr, $newstr)>

Takes two strings containing a list of comma- or space-separated items
and returns what items were removed from or added to the new one, 
compared to the old one. Returns a list, where the first entry is a scalar
containing removed items, and the second entry is a scalar containing added
items.

=item C<wrap_comment($comment)>

Takes a bug comment, and wraps it to the appropriate length. The length is
currently specified in C<Bugzilla::Constants::COMMENT_COLS>. Lines beginning
with ">" are assumed to be quotes, and they will not be wrapped.

The intended use of this function is to wrap comments that are about to be
displayed or emailed. Generally, wrapped text should not be stored in the
database.

=back

=head2 Formatting Time

=over 4

=item C<format_time($time)>

Takes a time and appends the timezone as defined in editparams.cgi.  This routine
will be expanded in the future to adjust for user preferences regarding what
timezone to display times in.  In the future, it may also allow for the time to be
shown in different formats.

=item C<format_time_decimal($time)>

Returns a number with 2 digit precision, unless the last digit is a 0. Then it 
returns only 1 digit precision.

=head2 Files

=over 4

=item C<file_mod_time($filename)>

Takes a filename and returns the modification time. It returns it in the format
of the "mtime" parameter of the perl "stat" function.

=back

