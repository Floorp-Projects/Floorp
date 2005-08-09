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
# Contributor(s): Dan Mosedale <dmose@mozilla.org>
#                 Frédéric Buclin <LpSolit@gmail.com>

package Bugzilla::Field;

use strict;

use base qw(Exporter);
@Bugzilla::Field::EXPORT = qw(check_form_field check_form_field_defined);

use Bugzilla;
use Bugzilla::Util;
use Bugzilla::Error;


sub check_form_field {
    my ($cgi, $fieldname, $legalsRef) = @_;
    my $dbh = Bugzilla->dbh;

    if (!defined $cgi->param($fieldname)
        || trim($cgi->param($fieldname)) eq ""
        || (defined($legalsRef)
            && lsearch($legalsRef, $cgi->param($fieldname)) < 0))
    {
        trick_taint($fieldname);
        my ($result) = $dbh->selectrow_array("SELECT description FROM fielddefs
                                              WHERE name = ?", undef, $fieldname);
        
        my $field = $result || $fieldname;
        ThrowCodeError("illegal_field", { field => $field });
    }
}

sub check_form_field_defined {
    my ($cgi, $fieldname) = @_;

    if (!defined $cgi->param($fieldname)) {
        ThrowCodeError("undefined_field", { field => $fieldname });
    }
}

=head1 NAME

Bugzilla::Field - Useful routines for fields manipulation

=head1 SYNOPSIS

  use Bugzilla::Field;

  # Validation Routines
  check_form_field($cgi, $fieldname, \@legal_values);
  check_form_field_defined($cgi, $fieldname);


=head1 DESCRIPTION

This package provides functions for dealing with CGI form fields.

=head1 FUNCTIONS

This package provides several types of routines:

=head2 Validation

=over

=item C<check_form_field($cgi, $fieldname, \@legal_values)>

Description: Makes sure the field $fieldname is defined and its value
             is non empty. If @legal_values is defined, this routine
             also checks whether its value is one of the legal values
             associated with this field. If the test fails, an error
             is thrown.

Params:      $cgi          - a CGI object
             $fieldname    - the field name to check
             @legal_values - (optional) ref to a list of legal values

Returns:     nothing

=item C<check_form_field_defined($cgi, $fieldname)>

Description: Makes sure the field $fieldname is defined and its value
             is non empty. Else an error is thrown.

Params:      $cgi       - a CGI object
             $fieldname - the field name to check

Returns:     nothing

=back
