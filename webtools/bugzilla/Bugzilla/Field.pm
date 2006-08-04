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

###############################
####    Initialization     ####
###############################

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

# Field definitions for the fields that ship with Bugzilla.
# These are used by populate_field_definitions to populate
# the fielddefs table.
use constant DEFAULT_FIELDS => (
    {name => 'bug_id',       desc => 'Bug #',      in_new_bugmail => 1},
    {name => 'short_desc',   desc => 'Summary',    in_new_bugmail => 1},
    {name => 'classification', desc => 'Classification', in_new_bugmail => 1},
    {name => 'product',      desc => 'Product',    in_new_bugmail => 1},
    {name => 'version',      desc => 'Version',    in_new_bugmail => 1},
    {name => 'rep_platform', desc => 'Platform',   in_new_bugmail => 1},
    {name => 'bug_file_loc', desc => 'URL',        in_new_bugmail => 1},
    {name => 'op_sys',       desc => 'OS/Version', in_new_bugmail => 1},
    {name => 'bug_status',   desc => 'Status',     in_new_bugmail => 1},
    {name => 'status_whiteboard', desc => 'Status Whiteboard',
     in_new_bugmail => 1},
    {name => 'keywords',     desc => 'Keywords',   in_new_bugmail => 1},
    {name => 'resolution',   desc => 'Resolution'},
    {name => 'bug_severity', desc => 'Severity',   in_new_bugmail => 1},
    {name => 'priority',     desc => 'Priority',   in_new_bugmail => 1},
    {name => 'component',    desc => 'Component',  in_new_bugmail => 1},
    {name => 'assigned_to',  desc => 'AssignedTo', in_new_bugmail => 1},
    {name => 'reporter',     desc => 'ReportedBy', in_new_bugmail => 1},
    {name => 'votes',        desc => 'Votes'},
    {name => 'qa_contact',   desc => 'QAContact',  in_new_bugmail => 1},
    {name => 'cc',           desc => 'CC',         in_new_bugmail => 1},
    {name => 'dependson',    desc => 'BugsThisDependsOn', in_new_bugmail => 1},
    {name => 'blocked',      desc => 'OtherBugsDependingOnThis',
     in_new_bugmail => 1},

    {name => 'attachments.description', desc => 'Attachment description'},
    {name => 'attachments.filename',    desc => 'Attachment filename'},
    {name => 'attachments.mimetype',    desc => 'Attachment mime type'},
    {name => 'attachments.ispatch',     desc => 'Attachment is patch'},
    {name => 'attachments.isobsolete',  desc => 'Attachment is obsolete'},
    {name => 'attachments.isprivate',   desc => 'Attachment is private'},

    {name => 'target_milestone',      desc => 'Target Milestone'},
    {name => 'creation_ts', desc => 'Creation date', in_new_bugmail => 1},
    {name => 'delta_ts', desc => 'Last changed date', in_new_bugmail => 1},
    {name => 'longdesc',              desc => 'Comment'},
    {name => 'alias',                 desc => 'Alias'},
    {name => 'everconfirmed',         desc => 'Ever Confirmed'},
    {name => 'reporter_accessible',   desc => 'Reporter Accessible'},
    {name => 'cclist_accessible',     desc => 'CC Accessible'},
    {name => 'bug_group',             desc => 'Group'},
    {name => 'estimated_time', desc => 'Estimated Hours', in_new_bugmail => 1},
    {name => 'remaining_time',        desc => 'Remaining Hours'},
    {name => 'deadline',              desc => 'Deadline', in_new_bugmail => 1},
    {name => 'commenter',             desc => 'Commenter'},
    {name => 'flagtypes.name',        desc => 'Flag'},
    {name => 'requestees.login_name', desc => 'Flag Requestee'},
    {name => 'setters.login_name',    desc => 'Flag Setter'},
    {name => 'work_time',             desc => 'Hours Worked'},
    {name => 'percentage_complete',   desc => 'Percentage Complete'},
    {name => 'content',               desc => 'Content'},
    {name => 'attach_data.thedata',   desc => 'Attachment data'},
    {name => 'attachments.isurl',     desc => 'Attachment is a URL'},
    {name => "owner_idle_time",       desc => "Time Since Assignee Touched"},
);

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

=over

=item C<populate_field_definitions()>

Description: Populates the fielddefs table during an installation
             or upgrade.

Params:      none

Returns:     nothing

=back

=cut

sub populate_field_definitions {
    my $dbh = Bugzilla->dbh;

    # ADD and UPDATE field definitions
    foreach my $definition (DEFAULT_FIELDS) {
        create_or_update($definition);
    }

    # DELETE fields which were added only accidentally, or which
    # were never tracked in bugs_activity. Note that you can never
    # delete fields which are used by bugs_activity.

    # Oops. Bug 163299
    $dbh->do("DELETE FROM fielddefs WHERE name='cc_accessible'");
    # Oops. Bug 215319
    $dbh->do("DELETE FROM fielddefs WHERE name='requesters.login_name'");
    # This field was never tracked in bugs_activity, so it's safe to delete.
    $dbh->do("DELETE FROM fielddefs WHERE name='attachments.thedata'");

    # MODIFY old field definitions

    # 2005-11-13 LpSolit@gmail.com - Bug 302599
    # One of the field names was a fragment of SQL code, which is DB dependent.
    # We have to rename it to a real name, which is DB independent.
    my $new_field_name = 'days_elapsed';
    my $field_description = 'Days since bug changed';

    my ($old_field_id, $old_field_name) =
        $dbh->selectrow_array('SELECT id, name FROM fielddefs
                                WHERE description = ?',
                              undef, $field_description);

    if ($old_field_id && ($old_field_name ne $new_field_name)) {
        print "SQL fragment found in the 'fielddefs' table...\n";
        print "Old field name: " . $old_field_name . "\n";
        # We have to fix saved searches first. Queries have been escaped
        # before being saved. We have to do the same here to find them.
        $old_field_name = url_quote($old_field_name);
        my $broken_named_queries =
            $dbh->selectall_arrayref('SELECT userid, name, query
                                        FROM namedqueries WHERE ' .
                                      $dbh->sql_istrcmp('query', '?', 'LIKE'),
                                      undef, "%=$old_field_name%");

        my $sth_UpdateQueries = $dbh->prepare('UPDATE namedqueries SET query = ?
                                                WHERE userid = ? AND name = ?');

        print "Fixing saved searches...\n" if scalar(@$broken_named_queries);
        foreach my $named_query (@$broken_named_queries) {
            my ($userid, $name, $query) = @$named_query;
            $query =~ s/=\Q$old_field_name\E(&|$)/=$new_field_name$1/gi;
            $sth_UpdateQueries->execute($query, $userid, $name);
        }

        # We now do the same with saved chart series.
        my $broken_series =
            $dbh->selectall_arrayref('SELECT series_id, query
                                        FROM series WHERE ' .
                                      $dbh->sql_istrcmp('query', '?', 'LIKE'),
                                      undef, "%=$old_field_name%");

        my $sth_UpdateSeries = $dbh->prepare('UPDATE series SET query = ?
                                               WHERE series_id = ?');

        print "Fixing saved chart series...\n" if scalar(@$broken_series);
        foreach my $series (@$broken_series) {
            my ($series_id, $query) = @$series;
            $query =~ s/=\Q$old_field_name\E(&|$)/=$new_field_name$1/gi;
            $sth_UpdateSeries->execute($query, $series_id);
        }
        # Now that saved searches have been fixed, we can fix the field name.
        print "Fixing the 'fielddefs' table...\n";
        print "New field name: " . $new_field_name . "\n";
        $dbh->do('UPDATE fielddefs SET name = ? WHERE id = ?',
                  undef, ($new_field_name, $old_field_id));
}

    # This field has to be created separately, or the above upgrade code
    # might not run properly.
    Bugzilla::Field::create_or_update(
        {name => $new_field_name, desc => $field_description});

}



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
