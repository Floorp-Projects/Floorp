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
# Contributor(s): Bradley Baetz <bbaetz@student.usyd.edu.au>

use strict;

package Bugzilla::CGI;

use CGI qw(-no_xhtml -oldstyle_urls :private_tempfiles :unique_headers SERVER_PUSH);
use CGI::Util qw(rearrange);

use base qw(CGI);

use Bugzilla::Util;
use Bugzilla::Config;

# We need to disable output buffering - see bug 179174
$| = 1;

# CGI.pm uses AUTOLOAD, but explicitly defines a DESTROY sub.
# We need to do so, too, otherwise perl dies when the object is destroyed
# and we don't have a DESTROY method (because CGI.pm's AUTOLOAD will |die|
# on getting an unknown sub to try to call)
sub DESTROY {};

sub new {
    my ($invocant, @args) = @_;
    my $class = ref($invocant) || $invocant;

    my $self = $class->SUPER::new(@args);

    # Make sure our outgoing cookie list is empty on each invocation
    $self->{Bugzilla_cookie_list} = [];

    # Make sure that we don't send any charset headers
    $self->charset('');

    # Check for errors
    # All of the Bugzilla code wants to do this, so do it here instead of
    # in each script

    my $err = $self->cgi_error;

    if ($err) {
        # XXX - under mod_perl we can use the request object to
        # enable the apache ErrorDocument stuff, which is localisable
        # (and localised by default under apache2).
        # This doesn't appear to be possible under mod_cgi.
        # Under mod_perl v2, though, this happens automatically, and the
        # message body is ignored.

        # Note that this error block is only triggered by CGI.pm for malformed
        # multipart requests, and so should never happen unless there is a
        # browser bug.

        print $self->header(-status => $err);

        # ThrowCodeError wants to print the header, so it grabs Bugzilla->cgi
        # which creates a new Bugzilla::CGI object, which fails again, which
        # ends up here, and calls ThrowCodeError, and then recurses forever.
        # So don't use it.
        # In fact, we can't use templates at all, because we need a CGI object
        # to determine the template lang as well as the current url (from the
        # template)
        # Since this is an internal error which indicates a severe browser bug,
        # just die.
        die "CGI parsing error: $err";
    }

    return $self;
}

# We want this sorted plus the ability to exclude certain params
sub canonicalise_query {
    my ($self, @exclude) = @_;

    # Reconstruct the URL by concatenating the sorted param=value pairs
    my @parameters;
    foreach my $key (sort($self->param())) {
        # Leave this key out if it's in the exclude list
        next if lsearch(\@exclude, $key) != -1;

        my $esc_key = url_quote($key);

        foreach my $value ($self->param($key)) {
            if ($value) {
                my $esc_value = url_quote($value);

                push(@parameters, "$esc_key=$esc_value");
            }
        }
    }

    return join("&", @parameters);
}

# Overwrite to handle nph parameter. This should stay here until perl 5.8.1 CGI
# has been fixed to support -nph as a parameter
#
sub multipart_init {
    my($self,@p) = @_;
    my($boundary,$nph,@other) = rearrange(['BOUNDARY','NPH'],@p);
    $boundary = $boundary || '------- =_aaaaaaaaaa0';
    $self->{'separator'} = "\r\n--$boundary\r\n";
    $self->{'final_separator'} = "\r\n--$boundary--\r\n";
    my $type = SERVER_PUSH($boundary);
    return $self->header(
        -nph => 0,
        -type => $type,
        (map { split "=", $_, 2 } @other),
    ) . "WARNING: YOUR BROWSER DOESN'T SUPPORT THIS SERVER-PUSH TECHNOLOGY." . $self->multipart_end;
}

# Override header so we can add the cookies in
sub header {
    my $self = shift;

    # Add the cookies in if we have any
    if (scalar(@{$self->{Bugzilla_cookie_list}})) {
        if (scalar(@_) == 1) {
            # if there's only one parameter, then it's a Content-Type.
            # Since we're adding parameters we have to name it.
            unshift(@_, '-type' => shift(@_));
        }
        unshift(@_, '-cookie' => $self->{Bugzilla_cookie_list});
    }

    return $self->SUPER::header(@_) || "";
}

# We override the entirety of multipart_start instead of falling through to
# SUPER because the built-in one can't deal with cookies in any kind of sane
# way.  This sub is gratuitously swiped from the real CGI.pm, but fixed so
# it actually works (but only as much as we need it to).
sub multipart_start {
    my(@header);
    my($self,@p) = @_;
    my($type,@other) = rearrange([['TYPE','CONTENT_TYPE','CONTENT-TYPE']],@p);
    my $charset = $self->charset;
    $type = $type || 'text/html';
    $type .= "; charset=$charset" if $type ne '' and $type =~ m!^text/! and $type !~ /\bcharset\b/ and $charset ne '';

    push(@header,"Content-Type: $type");

    # Add the cookies in if we have any
    if (scalar(@{$self->{Bugzilla_cookie_list}})) {
        foreach my $cookie (@{$self->{Bugzilla_cookie_list}}) {
            push @header, "Set-Cookie: $cookie";
        }
    }

    my $header = join($CGI::CRLF,@header)."${CGI::CRLF}${CGI::CRLF}";
    return $header;
}

# The various parts of Bugzilla which create cookies don't want to have to
# pass them arround to all of the callers. Instead, store them locally here,
# and then output as required from |header|.
sub send_cookie {
    my $self = shift;

    # Add the default path in
    unshift(@_, '-path' => Param('cookiepath'));

    # Use CGI::Cookie directly, because CGI.pm's |cookie| method gives the
    # current value if there isn't a -value attribute, which happens when
    # we're expiring an entry.
    require CGI::Cookie;
    my $cookie = CGI::Cookie->new(@_);
    push @{$self->{Bugzilla_cookie_list}}, $cookie;

    return;
}


1;

__END__

=head1 NAME

Bugzilla::CGI - CGI handling for Bugzilla

=head1 SYNOPSIS

  use Bugzilla::CGI;

  my $cgi = new Bugzilla::CGI();

=head1 DESCRIPTION

This package inherits from the standard CGI module, to provide additional
Bugzilla-specific functionality. In general, see L<the CGI.pm docs|CGI> for
documention.

=head1 CHANGES FROM L<CGI.PM|CGI>

Bugzilla::CGI has some differences from L<CGI.pm|CGI>.

=over 4

=item C<cgi_error> is automatically checked

After creating the CGI object, C<Bugzilla::CGI> automatically checks
I<cgi_error>, and throws a CodeError if a problem is detected.

=back

=head1 ADDITIONAL FUNCTIONS

I<Bugzilla::CGI> also includes additional functions.

=over 4

=item C<canonicalise_query(@exclude)>

This returns a sorted string of the parameters, suitable for use in a url.
Values in C<@exclude> are not included in the result.

=item C<send_cookie>

This routine is identical to CGI.pm's C<cookie> routine, except that the cookie
is sent to the browser, rather than returned. This should be used by all
Bugzilla code (instead of C<cookie> or the C<-cookie> argument to C<header>),
so that under mod_perl the headers can be sent correctly, using C<print> or
the mod_perl APIs as appropriate.

=back

=head1 SEE ALSO

L<CGI|CGI>, L<CGI::Cookie|CGI::Cookie>
