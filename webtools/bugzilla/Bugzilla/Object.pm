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
# Contributor(s): Max Kanat-Alexander <mkanat@bugzilla.org>

use strict;

package Bugzilla::Object;

use Bugzilla::Util;
use Bugzilla::Error;

use constant NAME_FIELD => 'name';
use constant ID_FIELD   => 'id';
use constant LIST_ORDER => NAME_FIELD;

###############################
####    Initialization     ####
###############################

sub new {
    my $invocant = shift;
    my $class    = ref($invocant) || $invocant;
    my $object   = $class->_init(@_);
    bless($object, $class) if $object;
    return $object;
}


# Note: Because this uses sql_istrcmp, if you make a new object use
# Bugzilla::Object, make sure that you modify bz_setup_database
# in Bugzilla::DB::Pg appropriately, to add the right LOWER
# index. You can see examples already there.
sub _init {
    my $class = shift;
    my ($param) = @_;
    my $dbh = Bugzilla->dbh;
    my $columns = join(',', $class->DB_COLUMNS);
    my $table   = $class->DB_TABLE;
    my $name_field = $class->NAME_FIELD;
    my $id_field   = $class->ID_FIELD;

    my $id = $param unless (ref $param eq 'HASH');
    my $object;

    if (defined $id) {
        detaint_natural($id)
          || ThrowCodeError('param_must_be_numeric',
                            {function => $class . '::_init'});

        $object = $dbh->selectrow_hashref(qq{
            SELECT $columns FROM $table
             WHERE $id_field = ?}, undef, $id);
    } elsif (defined $param->{'name'}) {
        trick_taint($param->{'name'});
        $object = $dbh->selectrow_hashref(qq{
            SELECT $columns FROM $table
             WHERE } . $dbh->sql_istrcmp($name_field, '?'), 
            undef, $param->{'name'});
    } else {
        ThrowCodeError('bad_arg',
            {argument => 'param',
             function => $class . '::_init'});
    }

    return $object;
}

sub new_from_list {
    my $class = shift;
    my ($id_list) = @_;
    my $dbh = Bugzilla->dbh;
    my $columns = join(',', $class->DB_COLUMNS);
    my $table   = $class->DB_TABLE;
    my $order   = $class->LIST_ORDER;
    my $id_field = $class->ID_FIELD;

    my $objects;
    if (@$id_list) {
        my @detainted_ids;
        foreach my $id (@$id_list) {
            detaint_natural($id) ||
                ThrowCodeError('param_must_be_numeric',
                              {function => $class . '::new_from_list'});
            push(@detainted_ids, $id);
        }
        $objects = $dbh->selectall_arrayref(
            "SELECT $columns FROM $table WHERE $id_field IN (" 
            . join(',', @detainted_ids) . ") ORDER BY $order", {Slice=>{}});
    } else {
        return [];
    }

    foreach my $object (@$objects) {
        bless($object, $class);
    }
    return $objects;
}

###############################
####      Accessors      ######
###############################

sub id                { return $_[0]->{'id'};          }
sub name              { return $_[0]->{'name'};        }

###############################
####      Subroutines    ######
###############################

sub create {
    my ($class, $params) = @_;
    my $dbh = Bugzilla->dbh;

    my $required   = $class->REQUIRED_CREATE_FIELDS;
    my $validators = $class->VALIDATORS;
    my $table      = $class->DB_TABLE;

    foreach my $field ($class->REQUIRED_CREATE_FIELDS) {
        ThrowCodeError('param_required', 
            { function => "${class}->create", param => $field })
            if !exists $params->{$field};
    }

    my (@field_names, @values);
    # We do the sort just to make sure that validation always
    # happens in a consistent order.
    foreach my $field (sort keys %$params) {
        my $value;
        if (exists $validators->{$field}) {
            $value = &{$validators->{$field}}($params->{$field});
        }
        else {
            $value = $params->{$field};
        }
        trick_taint($value);
        push(@field_names, $field);
        push(@values, $value);
    }

    my $qmarks = '?,' x @values;
    chop($qmarks);
    $dbh->do("INSERT INTO $table (" . join(', ', @field_names) 
             . ") VALUES ($qmarks)", undef, @values);
    my $id = $dbh->bz_last_key($table, 'id');

    return $class->new($id);
}

sub get_all {
    my $class = shift;
    my $dbh = Bugzilla->dbh;
    my $table = $class->DB_TABLE;
    my $order = $class->LIST_ORDER;
    my $id_field = $class->ID_FIELD;

    my $ids = $dbh->selectcol_arrayref(qq{
        SELECT $id_field FROM $table ORDER BY $order});

    my $objects = $class->new_from_list($ids);
    return @$objects;
}

1;

__END__

=head1 NAME

Bugzilla::Object - A base class for objects in Bugzilla.

=head1 SYNOPSIS

 my $object = new Bugzilla::Object(1);
 my $object = new Bugzilla::Object({name => 'TestProduct'});

 my $id          = $object->id;
 my $name        = $object->name;

=head1 DESCRIPTION

Bugzilla::Object is a base class for Bugzilla objects. You never actually
create a Bugzilla::Object directly, you only make subclasses of it.

Basically, Bugzilla::Object exists to allow developers to create objects
more easily. All you have to do is define C<DB_TABLE>, C<DB_COLUMNS>,
and sometimes C<LIST_ORDER> and you have a whole new object.

You should also define accessors for any columns other than C<name>
or C<id>.

=head1 CONSTANTS

Frequently, these will be the only things you have to define in your
subclass in order to have a fully-functioning object. C<DB_TABLE>
and C<DB_COLUMNS> are required.

=over

=item C<DB_TABLE>

The name of the table that these objects are stored in. For example,
for C<Bugzilla::Keyword> this would be C<keyworddefs>.

=item C<DB_COLUMNS>

The names of the columns that you want to read out of the database
and into this object. This should be an array.

=item C<NAME_FIELD>

The name of the column that should be considered to be the unique
"name" of this object. The 'name' is a B<string> that uniquely identifies
this Object in the database. Defaults to 'name'. When you specify 
C<{name => $name}> to C<new()>, this is the column that will be 
matched against in the DB.

=item C<ID_FIELD>

The name of the column that represents the unique B<integer> ID
of this object in the database. Defaults to 'id'.

=item C<LIST_ORDER>

The order that C<new_from_list> and C<get_all> should return objects
in. This should be the name of a database column. Defaults to
L</NAME_FIELD>.

=item C<REQUIRED_CREATE_FIELDS>

The list of fields that B<must> be specified when the user calls
C<create()>. This should be an array.

=item C<VALIDATORS>

A hashref that points to a function that will validate each param to
C<create()>. Each function in this hashref will be passed a single
argument, the value passed to C<create()> for that field. These
functions should call L<Bugzilla::Error/ThrowUserError> if they fail.
They must return the validated value.

=back

=head1 METHODS

=over

=item C<new($param)>

 Description: The constructor is used to load an existing object
              from the database, by id or by name.

 Params:      $param - If you pass an integer, the integer is the
                       id of the object, from the database, that we 
                       want to read in. If you pass in a hash with 
                       C<name> key, then the value of the name key 
                       is the case-insensitive name of the object from 
                       the DB.

 Returns:     A fully-initialized object.

=item C<new_from_list(\@id_list)>

 Description: Creates an array of objects, given an array of ids.

 Params:      \@id_list - A reference to an array of numbers, database ids.
                          If any of these are not numeric, the function
                          will throw an error. If any of these are not
                          valid ids in the database, they will simply 
                          be skipped.

 Returns:     A reference to an array of objects.

=back

=head1 SUBROUTINES

=over

=item C<create($params)>

Description: Creates a new item in the database.
             Throws a User Error if any of the passed-in params
             are invalid.

Params:      C<$params> - hashref - A value to put in each database
               field for this object. Certain values must be set (the 
               ones specified in L</REQUIRED_CREATE_FIELDS>), and
               the function will throw a Code Error if you don't set
               them.

Returns:     The Object just created in the database.

Notes:       In order for this function to work in your subclass,
             your subclass's C<id> field must be of C<SERIAL>
             type in the database. Your subclass also must
             define L</REQUIRED_CREATE_FIELDS> and L</VALIDATORS>.

=item C<get_all>

 Description: Returns all objects in this table from the database.

 Params:      none.

 Returns:     A list of objects, or an empty list if there are none.

 Notes:       Note that you must call this as C<$class->get_all>. For 
              example, C<Bugzilla::Keyword->get_all>. 
              C<Bugzilla::Keyword::get_all> will not work.

=back

=cut
