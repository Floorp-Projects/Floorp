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
# Contributor(s): Bradley Baetz <bbaetz@acm.org>

package Bugzilla::Error;

use strict;
use base qw(Exporter);

@Bugzilla::Error::EXPORT = qw(ThrowCodeError ThrowTemplateError ThrowUserError);

use Bugzilla::Config;
use Bugzilla::Util;
use Date::Format;

sub _throw_error {
    my ($name, $error, $vars, $unlock_tables) = @_;

    $vars ||= {};

    $vars->{error} = $error;

    Bugzilla->dbh->do("UNLOCK TABLES") if $unlock_tables;

    # If a writable data/errorlog exists, log error details there.
    if (-w "data/errorlog") {
        require Data::Dumper;
        my $mesg = "";
        for (1..75) { $mesg .= "-"; };
        $mesg .= "\n[$$] " . time2str("%D %H:%M:%S ", time());
        $mesg .= "$name $error ";
        $mesg .= "$ENV{REMOTE_ADDR} " if $ENV{REMOTE_ADDR};
        $mesg .= Bugzilla->user->login;
        $mesg .= "\n";
        my %params = Bugzilla->cgi->Vars;
        $Data::Dumper::Useqq = 1;
        for my $param (sort keys %params) {
            my $val = $params{$param};
            # obscure passwords
            $val = "*****" if $param =~ /password/i;
            # limit line length
            $val =~ s/^(.{512}).*$/$1\[CHOP\]/;
            $mesg .= "[$$] " . Data::Dumper->Dump([$val],["param($param)"]);
        }
        for my $var (sort keys %ENV) {
            my $val = $ENV{$var};
            $val = "*****" if $val =~ /password|http_pass/i;
            $mesg .= "[$$] " . Data::Dumper->Dump([$val],["env($var)"]);
        }
        open(ERRORLOGFID, ">>data/errorlog");
        print ERRORLOGFID "$mesg\n";
        close ERRORLOGFID;
    }

    print Bugzilla->cgi->header();

    my $template = Bugzilla->template;
    $template->process($name, $vars)
      || ThrowTemplateError($template->error());

    exit;
}

sub ThrowUserError {
    _throw_error("global/user-error.html.tmpl", @_);
}

sub ThrowCodeError {
    _throw_error("global/code-error.html.tmpl", @_);
}

sub ThrowTemplateError {
    my ($template_err) = @_;

    my $vars = {};

    $vars->{'template_error_msg'} = $template_err;
    $vars->{'error'} = "template_error";

    my $template = Bugzilla->template;

    # Try a template first; but if this one fails too, fall back
    # on plain old print statements.
    if (!$template->process("global/code-error.html.tmpl", $vars)) {
        my $maintainer = Param('maintainer');
        my $error = html_quote($vars->{'template_error_msg'});
        my $error2 = html_quote($template->error());
        print <<END;
        <tt>
          <p>
            Bugzilla has suffered an internal error. Please save this page and 
            send it to $maintainer with details of what you were doing at the 
            time this message appeared.
          </p>
          <script type="text/javascript"> <!--
            document.write("<p>URL: " + document.location + "</p>");
          // -->
          </script>
          <p>Template->process() failed twice.<br>
          First error: $error<br>
          Second error: $error2</p>
        </tt>
END
    }
    exit;
}

1;

__END__

=head1 NAME

Bugzilla::Error - Error handling utilities for Bugzilla

=head1 SYNOPSIS

  use Bugzilla::Error;

  ThrowUserError("error_tag",
                 { foo => 'bar' });
 
  # supplying "abort" to ensure tables are unlocked
  ThrowUserError("another_error_tag",
                 { foo => 'bar' }, 'abort');

=head1 DESCRIPTION

Various places throughout the Bugzilla codebase need to report errors to the
user. The C<Throw*Error> family of functions allow this to be done in a
generic and localisable manner.

=head1 FUNCTIONS

=over 4

=item C<ThrowUserError>

This function takes an error tag as the first argument, and an optional hashref
of variables as a second argument. These are used by the
I<global/user-error.html.tmpl> template to format the error, using the passed
in variables as required.

An optional third argument may be supplied. If present, the error
handling code will unlock the database tables: it is a Bugzilla standard
to provide the string "abort" as the argument value. In the long term,
this argument will go away, to be replaced by transactional C<rollback>
calls. There is no timeframe for doing so, however.

=item C<ThrowCodeError>

This function is used when an internal check detects an error of some sort.
This usually indicates a bug in Bugzilla, although it can occur if the user
manually constructs urls without correct parameters.

This function's behaviour is similar to C<ThrowUserError>, except that the
template used to display errors is I<global/code-error.html.tmpl>. In addition
if the hashref used as the optional second argument contains a key I<variables>
then the contents of the hashref (which is expected to be another hashref) will
be displayed after the error message, as a debugging aid.

=item C<ThrowTemplateError>

This function should only be called if a C<template-<gt>process()> fails.
It tries another template first, because often one template being
broken or missing doesn't mean that they all are. But it falls back to
a print statement as a last-ditch error.

=back

=head1 SEE ALSO

L<Bugzilla|Bugzilla>
