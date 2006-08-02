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
#                 Myk Melez <myk@mozilla.org>

=head1 NAME

Bugzilla::Field - a particular piece of information about bugs
                  and useful routines for form field manipulation

=head1 SYNOPSIS

  use Bugzilla;
  use Data::Dumper;

  # Display information about all fields.
  print Dumper(Bugzilla->get_fields());

  # Display information about non-obsolete custom fields.
  print Dumper(Bugzilla->get_fields({ obsolete => 1, custom => 1 }));

  # Display a list of the names of non-obsolete custom fields.
  print Bugzilla->custom_field_names;

  use Bugzilla::Field;

  # Display information about non-obsolete custom fields.
  # Bugzilla->get_fields() is a wrapper around Bugzilla::Field->match(),
  # so both methods take the same arguments.
  print Dumper(Bugzilla::Field->match({ obsolete => 1, custom => 1 }));

  # Create or update a custom field or field definition.
  my $field = Bugzilla::Field::create_or_update(
    {name => 'hilarity', desc => 'Hilarity', custom => 1});

  # Instantiate a Field object for an existing field.
  my $field = new Bugzilla::Field({name => 'qacontact_accessible'});
  if ($field->obsolete) {
      print $field->description . " is obsolete\n";
  }

  # Validation Routines
  check_field($name, $value, \@legal_values, $no_warn);
  $fieldid = get_field_id($fieldname);

=head1 DESCRIPTION

Field.pm defines field objects, which represent the particular pieces
of information that Bugzilla stores about bugs.

This package also provides functions for dealing with CGI form fields.

C<Bugzilla::Field> is an implementation of L<Bugzilla::Object>, and
so provides all of the methods available in L<Bugzilla::Object>,
in addition to what is documented here.

=cut

package Bugzilla::Field;

use strict;

use base qw(Exporter Bugzilla::Object);
@Bugzilla::Field::EXPORT = qw(check_field get_field_id get_legal_field_values);

use Bugzilla::Util;
use Bugzilla::Constants;
use Bugzilla::Error;

use constant DB_TABLE   => 'fielddefs';
use constant LIST_ORDER => 'sortkey, name';

use constant DB_COLUMNS => (
    'id',
    'name',
    'description',
    'type',
    'custom',
    'obsolete',
    'enter_bug',
);

# How various field types translate into SQL data definitions.
use constant SQL_DEFINITIONS => {
    # Using commas because these are constants and they shouldn't
    # be auto-quoted by the "=>" operator.
    FIELD_TYPE_FREETEXT, { TYPE => 'varchar(255)' },
};

=pod

=head2 Instance Properties

=over

=item C<name>

the name of the field in the database; begins with "cf_" if field
is a custom field, but test the value of the boolean "custom" property
to determine if a given field is a custom field;

=item C<description>

a short string describing the field; displayed to Bugzilla users
in several places within Bugzilla's UI, f.e. as the form field label
on the "show bug" page;

=back

=cut

sub description { return $_[0]->{description} }

=over

=item C<type>

an integer specifying the kind of field this is; values correspond to
the FIELD_TYPE_* constants in Constants.pm

=back

=cut

sub type { return $_[0]->{type} }

=over

=item C<custom>

a boolean specifying whether or not the field is a custom field;
if true, field name should start "cf_", but use this property to determine
which fields are custom fields;

=back

=cut

sub custom { return $_[0]->{custom} }

=over

=item C<obsolete>

a boolean specifying whether or not the field is obsolete;

=back

=cut

sub obsolete { return $_[0]->{obsolete} }

=over

=item C<enter_bug>

A boolean specifying whether or not this field should appear on 
enter_bug.cgi

=back

=cut

sub enter_bug { return $_[0]->{enter_bug} }


=pod

=head2 Class Methods

=over

=item C<create_or_update({name => $name, desc => $desc, in_new_bugmail => 0, custom => 0})>

Description: Creates a new field, or updates one that
             already exists with the same name.

Params:      This function takes named parameters in a hashref: 
             C<name> - string - The name of the field.
             C<desc> - string - The field label to display in the UI.
             C<in_new_bugmail> - boolean - Whether this field appears at the
                 top of the bugmail for a newly-filed bug.
             C<custom> - boolean - True if this is a Custom Field. The field
                 will be added to the C<bugs> table if it does not exist.

Returns:     a C<Bugzilla::Field> object.

=back

=cut

sub create_or_update {
    my ($params) = @_;
    
    my $custom         = $params->{custom} ? 1 : 0;
    my $name           = $params->{name};
    my $in_new_bugmail = $params->{in_new_bugmail} ? 1 : 0;

    # Some day we'll allow invocants to specify the field type.
    my $type = $custom ? FIELD_TYPE_FREETEXT : FIELD_TYPE_UNKNOWN;

    my $field = new Bugzilla::Field({name => $name});

    my $dbh = Bugzilla->dbh;
    if ($field) {
        # Update the already-existing definition.
        $dbh->do("UPDATE fielddefs SET description = ?, mailhead = ?
                   WHERE id = ?",
                 undef, $params->{desc}, $in_new_bugmail, $field->id);
    }
    else {
        # Some day we'll allow invocants to specify the sort key.
        my ($sortkey) = $dbh->selectrow_array(
            "SELECT MAX(sortkey) + 100 FROM fielddefs") || 100;

        # Add the field to the list of fields at this Bugzilla installation.
        $dbh->do("INSERT INTO fielddefs (name, description, sortkey, type,
                                         custom, mailhead)
                       VALUES (?, ?, ?, ?, ?, ?)", undef,
                 $name, $params->{desc}, $sortkey, $type, $custom, 
                 $in_new_bugmail);
    }

    if (!$dbh->bz_column_info('bugs', $name) && $custom) {
        # Create the database column that stores the data for this field.
        $dbh->bz_add_column('bugs', $name, SQL_DEFINITIONS->{$type});
    }

    return new Bugzilla::Field({name => $name});
}


=pod

=over

=item C<match($criteria)>

Description: returns a list of fields that match the specified criteria.

Params:    C<$criteria> - hash reference - the criteria to match against.
           Hash keys represent field properties; hash values represent
           their values.  All criteria are optional.  Valid criteria are
           "custom" and "obsolete", and both take boolean values.

           Note: Bugzilla->get_fields() and Bugzilla->custom_field_names
           wrap this method for most callers.

Returns:   A reference to an array of C<Bugzilla::Field> objects.

=back

=cut

sub match {
    my ($class, $criteria) = @_;
  
    my @terms;
    if (defined $criteria->{name}) {
        push(@terms, "name=" . Bugzilla->dbh->quote($criteria->{name}));
    }
    if (defined $criteria->{custom}) {
        push(@terms, "custom=" . ($criteria->{custom} ? "1" : "0"));
    }
    if (defined $criteria->{obsolete}) {
        push(@terms, "obsolete=" . ($criteria->{obsolete} ? "1" : "0"));
    }
    if (defined $criteria->{enter_bug}) {
        push(@terms, "enter_bug=" . ($criteria->{enter_bug} ? '1' : '0'));
    }
    my $where = (scalar(@terms) > 0) ? "WHERE " . join(" AND ", @terms) : "";
  
    my $ids = Bugzilla->dbh->selectcol_arrayref(
        "SELECT id FROM fielddefs $where", {Slice => {}});

    return $class->new_from_list($ids);
}

=pod

=over

=item C<get_legal_field_values($field)>

Description: returns all the legal values for a field that has a
             list of legal values, like rep_platform or resolution.
             The table where these values are stored must at least have
             the following columns: value, isactive, sortkey.

Params:    C<$field> - Name of the table where valid values are.

Returns:   a reference to a list of valid values.

=back

=cut

sub get_legal_field_values {
    my ($field) = @_;
    my $dbh = Bugzilla->dbh;
    my $result_ref = $dbh->selectcol_arrayref(
         "SELECT value FROM $field
           WHERE isactive = ?
        ORDER BY sortkey, value", undef, (1));
    return $result_ref;
}


=pod

=head2 Data Validation

=over

=item C<check_field($name, $value, \@legal_values, $no_warn)>

Description: Makes sure the field $name is defined and its $value
             is non empty. If @legal_values is defined, this routine
             checks whether its value is one of the legal values
             associated with this field, else it checks against
             the default valid values for this field obtained by
             C<get_legal_field_values($name)>. If the test is successful,
             the function returns 1. If the test fails, an error
             is thrown (by default), unless $no_warn is true, in which
             case the function returns 0.

Params:      $name         - the field name
             $value        - the field value
             @legal_values - (optional) list of legal values
             $no_warn      - (optional) do not throw an error if true

Returns:     1 on success; 0 on failure if $no_warn is true (else an
             error is thrown).

=back

=cut

sub check_field {
    my ($name, $value, $legalsRef, $no_warn) = @_;
    my $dbh = Bugzilla->dbh;

    # If $legalsRef is undefined, we use the default valid values.
    unless (defined $legalsRef) {
        $legalsRef = get_legal_field_values($name);
    }

    if (!defined($value)
        || trim($value) eq ""
        || lsearch($legalsRef, $value) < 0)
    {
        return 0 if $no_warn; # We don't want an error to be thrown; return.
        trick_taint($name);

        my $field = new Bugzilla::Field({ name => $name });
        my $field_desc = $field ? $field->description : $name;
        ThrowCodeError('illegal_field', { field => $field_desc });
    }
    return 1;
}

=pod

=over

=item C<get_field_id($fieldname)>

Description: Returns the ID of the specified field name and throws
             an error if this field does not exist.

Params:      $name - a field name

Returns:     the corresponding field ID or an error if the field name
             does not exist.

=back

=cut

sub get_field_id {
    my ($name) = @_;
    my $dbh = Bugzilla->dbh;

    trick_taint($name);
    my $id = $dbh->selectrow_array('SELECT id FROM fielddefs
                                    WHERE name = ?', undef, $name);

    ThrowCodeError('invalid_field_name', {field => $name}) unless $id;
    return $id
}

1;

__END__
