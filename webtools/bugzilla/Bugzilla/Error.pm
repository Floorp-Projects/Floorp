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

use strict;

package Bugzilla::Error;

use base qw(Exporter);

@Bugzilla::Error::EXPORT = qw(ThrowUserError);

sub ThrowUserError {
    my ($error, $vars, $unlock_tables) = @_;

    $vars ||= {};

    $vars->{error} = $error;

    Bugzilla->dbh->do("UNLOCK TABLES") if $unlock_tables;

    print Bugzilla->cgi->header();

    my $template = Bugzilla->template;
    $template->process("global/user-error.html.tmpl", $vars)
      || &::ThrowTemplateError($template->error());

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

An optional third argument may be supplied. If present (and defined), then the
error handling code will unlock the database tables. In the long term, this
argument will go away, to be replaced by transactional C<rollback> calls. There
is no timeframe for doing so, however.

=back

=head1 SEE ALSO

L<Bugzilla|Bugzilla>
