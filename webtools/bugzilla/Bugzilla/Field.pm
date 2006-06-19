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
  # Bugzilla->get_fields() is a wrapper around Bugzilla::Field::match(),
  # so both methods take the same arguments.
  print Dumper(Bugzilla::Field::match({ obsolete => 1, custom => 1 }));

  # Create a custom field.
  my $field = Bugzilla::Field::create("hilarity", "Hilarity");
  print "$field->{description} is a custom field\n";

  # Instantiate a Field object for an existing field.
  my $field = new Bugzilla::Field('qacontact_accessible');
  if ($field->{obsolete}) {
      print "$field->{description} is obsolete\n";
  }

  # Validation Routines
  check_field($name, $value, \@legal_values, $no_warn);
  $fieldid = get_field_id($fieldname);

=head1 DESCRIPTION

Field.pm defines field objects, which represent the particular pieces
of information that Bugzilla stores about bugs.

This package also provides functions for dealing with CGI form fields.

=cut

package Bugzilla::Field;

use strict;

use base qw(Exporter);
@Bugzilla::Field::EXPORT = qw(check_field get_field_id get_legal_field_values);

use Bugzilla::Util;
use Bugzilla::Constants;
use Bugzilla::Error;

use constant DB_COLUMNS => (
    'fieldid AS id',
    'name',
    'description',
    'type',
    'custom',
    'obsolete'
);

our $columns = join(", ", DB_COLUMNS);

sub new {
    my $invocant = shift;
    my $name = shift;
    my $self = shift || Bugzilla->dbh->selectrow_hashref(
                            "SELECT $columns FROM fielddefs WHERE name = ?",
                            undef,
                            $name
                        );
    bless($self, $invocant);
    return $self;
}

=pod

=head2 Instance Properties

=over

=item C<id>

the unique identifier for the field;

=back

=cut

sub id { return $_[0]->{id} }

=over

=item C<name>

the name of the field in the database; begins with "cf_" if field
is a custom field, but test the value of the boolean "custom" property
to determine if a given field is a custom field;

=back

=cut

sub name { return $_[0]->{name} }

=over

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


=pod

=head2 Class Methods

=over

=item C<create($name, $desc)>

Description: creates a new custom field.

Params:      C<$name> - string - the name of the field;
             C<$desc> - string - the field label to display in the UI.

Returns:     a field object.

=back

=cut

sub create {
    my ($name, $desc, $custom) = @_;
    
    # Convert the $custom argument into a DB-compatible value.
    $custom = $custom ? 1 : 0;

    my $dbh = Bugzilla->dbh;

    # Some day we'll allow invocants to specify the sort key.
    my ($sortkey) =
      $dbh->selectrow_array("SELECT MAX(sortkey) + 1 FROM fielddefs");

    # Some day we'll require invocants to specify the field type.
    my $type = FIELD_TYPE_FREETEXT;

    # Create the database column that stores the data for this field.
    $dbh->bz_add_column("bugs", $name, { TYPE => 'varchar(255)' });

    # Add the field to the list of fields at this Bugzilla installation.
    my $sth = $dbh->prepare(
                  "INSERT INTO fielddefs (name, description, sortkey, type,
                                          custom, mailhead)
                   VALUES (?, ?, ?, ?, ?, 1)"
              );
    $sth->execute($name, $desc, $sortkey, $type, $custom);

    return new Bugzilla::Field($name);
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

Returns:   a list of field objects.

=back

=cut

sub match {
    my ($criteria) = @_;
  
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
    my $where = (scalar(@terms) > 0) ? "WHERE " . join(" AND ", @terms) : "";
  
    my $records = Bugzilla->dbh->selectall_arrayref(
                      "SELECT $columns FROM fielddefs $where ORDER BY sortkey",
                      { Slice => {}}
                  );
    # Generate a array of field objects from the array of field records.
    my @fields = map( new Bugzilla::Field(undef, $_), @$records );
    return @fields;
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

        my $field = new Bugzilla::Field($name);
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
    my $id = $dbh->selectrow_array('SELECT fieldid FROM fielddefs
                                    WHERE name = ?', undef, $name);

    ThrowCodeError('invalid_field_name', {field => $name}) unless $id;
    return $id
}

1;

__END__
