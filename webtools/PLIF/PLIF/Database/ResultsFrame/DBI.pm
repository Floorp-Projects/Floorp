# -*- Mode: perl; tab-width: 4; indent-tabs-mode: nil; -*-
#
# This file is MPL/GPL dual-licensed under the following terms:
#
# The contents of this file are subject to the Mozilla Public License
# Version 1.1 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
# the License for the specific language governing rights and
# limitations under the License.
#
# The Original Code is PLIF 1.0.
# The Initial Developer of the Original Code is Ian Hickson.
#
# Alternatively, the contents of this file may be used under the terms
# of the GNU General Public License Version 2 or later (the "GPL"), in
# which case the provisions of the GPL are applicable instead of those
# above. If you wish to allow use of your version of this file only
# under the terms of the GPL and not to allow others to use your
# version of this file under the MPL, indicate your decision by
# deleting the provisions above and replace them with the notice and
# other provisions required by the GPL. If you do not delete the
# provisions above, a recipient may use your version of this file
# under either the MPL or the GPL.

package PLIF::Database::ResultsFrame::DBI;
use strict;
use vars qw(@ISA);
use PLIF;
use PLIF::Database;
use PLIF::Exception;
@ISA = qw(PLIF);
1;

# DEPENDENCY: This assumes the DBI module is already loaded

sub init {
    my $self = shift;
    $self->SUPER::init(@_);
    my($handle, $database, $executed) = @_;
    $self->handle($handle);
    $self->database($database);
    $self->executed($executed);
}

sub lastError {
    my $self = shift;
    return $self->handle->err;
}

sub rowsAffected {
    my $self = shift;
    return $self->handle->rows;
}

sub row {
    my $self = shift;
    $self->assert($self->executed, 1, 'Tried to fetch data from an unexecuted statement');
    my $wantarray = wantarray; # to propagate it into the try block below
    my @result = try {
        if ($wantarray) {
            return $self->handle->fetchrow_array();
        } else {
            my $array = $self->handle->fetchrow_arrayref();
            if ((not defined($array)) or @$array == 0) {
                # no data
                return undef;
            } elsif (@$array == 1) {
                # only one data point
                return $array->[0];
            } else {
                # more than one data point
                return $array;
            }
        }
    } except {
        my($exception) = @_;
        if (my $error = $self->lastError) {
            $self->raiseError();
        } else {
            # some random other exception
            raise $exception;
        }
    };
    return $wantarray ? @result : $result[0];
}

sub rows {
    my $self = shift;
    $self->assert($self->executed, 1, 'Tried to fetch data from an unexecuted statement');
    my $result = try {
        $self->handle->fetchall_arrayref();
    } except {
        my($exception) = @_;
        if (my $error = $self->lastError) {
            $self->raiseError();
        } else {
            # some random other exception
            raise $exception;
        }
    };
    return $result;
}

sub reexecute {
    my $self = shift;
    my(@values) = @_;
    if ($self->handle->execute(@values)) {
        $self->executed(1);
        return $self;
    } else {
        return undef;
    }
}

sub raiseError {
    my $self = shift;
    if ($self->lastError eq '1062') {
        # XXX MySQL specific
        # This should only be used by MySQL-specific DBI data sources
        raise PLIF::Exception::Database::Duplicate (
            'code' => $self->lastError,
            'message' => $self->handle->errstr,
        );
    } else {
        raise PLIF::Exception::Database (
            'code' => $self->lastError,
            'message' => $self->handle->errstr,
        );
    }
}

# This should only be used by MySQL-specific DBI data sources
sub MySQLID {
    my $self = shift;
    return $self->handle->{'mysql_insertid'};
}

# other possible APIs:
# $ary_ref  = $sth->fetchrow_arrayref;
# $hash_ref = $sth->fetchrow_hashref;



=documentation


 *********************
 **** WARNING!!!! ****
 *********************

 The following section is under the same license as Perl's man pages
 and is not licensed under the MPL/GPL.

 Lines marked "<<<<<<<<<<<<<<<<<<<<<" are important.

 *********************

       fetchrow_arrayref

             $ary_ref = $sth->fetchrow_arrayref;
             $ary_ref = $sth->fetch;    # alias

           Fetches the next row of data and returns a reference
           to an array holding the field values.  Null field
           values are returned as undef.  This is the fastest way        <<<<<<<<<<<<<<<<<<<<<
           to fetch data, particularly if used with                      <<<<<<<<<<<<<<<<<<<<<
           $sth->bind_columns.

           If there are no more rows or an error occurs then             <<<<<<<<<<<<<<<<<<<<<
           fetchrow_arrayref returns an undef. You should check          <<<<<<<<<<<<<<<<<<<<<
           $sth->err afterwards (or use the RaiseError attribute)        <<<<<<<<<<<<<<<<<<<<<
           to discover if the undef returned was due to an error.        <<<<<<<<<<<<<<<<<<<<<

           Note that currently the same array ref will be
           returned for each fetch so don't store the ref and
           then use it after a later fetch.

       fetchrow_array

            @ary = $sth->fetchrow_array;

           An alternative to fetchrow_arrayref. Fetches the next
           row of data and returns it as an array holding the
           field values.  Null values are returned as undef.

           If there are no more rows or an error occurs then             <<<<<<<<<<<<<<<<<<<<<
           fetchrow_array returns an empty list. You should check        <<<<<<<<<<<<<<<<<<<<<
           $sth->err afterwards (or use the RaiseError attribute)        <<<<<<<<<<<<<<<<<<<<<
           to discover if the empty list returned was due to an          <<<<<<<<<<<<<<<<<<<<<
           error.

       fetchrow_hashref

            $hash_ref = $sth->fetchrow_hashref;
            $hash_ref = $sth->fetchrow_hashref($name);

           An alternative to fetchrow_arrayref. Fetches the next
           row of data and returns it as a reference to a hash
           containing field name and field value pairs.  Null
           values are returned as undef.

           If there are no more rows or an error occurs then             <<<<<<<<<<<<<<<<<<<<<
           fetchrow_hashref returns an undef. You should check           <<<<<<<<<<<<<<<<<<<<<
           $sth->err afterwards (or use the RaiseError attribute)        <<<<<<<<<<<<<<<<<<<<<
           to discover if the undef returned was due to an error.        <<<<<<<<<<<<<<<<<<<<<

           The optional $name parameter specifies the name of the
           statement handle attribute to use to get the field
           names. It defaults to 'the NAME entry elsewhere in
           this document'.

           The keys of the hash are the same names returned by
           $sth->{$name}. If more than one field has the same
           name there will only be one entry in the returned hash
           for those fields.

           Note that using fetchrow_hashref is currently not
           portable between databases because different databases
           return fields names with different letter cases (some
           all uppercase, some all lower, and some return the
           letter case used to create the table). This will be
           addressed in a future version of the DBI.

           Because of the extra work fetchrow_hashref and perl           <<<<<<<<<<<<<<<<<<<<<
           have to perform it is not as efficient as                     <<<<<<<<<<<<<<<<<<<<<
           fetchrow_arrayref or fetchrow_array and is not                <<<<<<<<<<<<<<<<<<<<<
           recommended where performance is very important.              <<<<<<<<<<<<<<<<<<<<<

           Currently a new hash reference is returned for each
           row.  This is likely to change in the future so don't
           rely on it.

       fetchall_arrayref

             $tbl_ary_ref = $sth->fetchall_arrayref;
             $tbl_ary_ref = $sth->fetchall_arrayref( $slice_array_ref );
             $tbl_ary_ref = $sth->fetchall_arrayref( $slice_hash_ref  );

           The fetchall_arrayref method can be used to fetch all
           the data to be returned from a prepared and executed
           statement handle. It returns a reference to an array
           which contains one reference per row.

           If there are no rows to return, fetchall_arrayref             <<<<<<<<<<<<<<<<<<<<<
           returns a reference to an empty array. If an error            <<<<<<<<<<<<<<<<<<<<<
           occurs fetchall_arrayref returns the data fetched thus        <<<<<<<<<<<<<<<<<<<<<
           far, which may be none.  You should check $sth->err           <<<<<<<<<<<<<<<<<<<<<
           afterwards (or use the RaiseError attribute) to               <<<<<<<<<<<<<<<<<<<<<
           discover if the data is complete or was truncated due         <<<<<<<<<<<<<<<<<<<<<
           to an error.

           When passed an array reference, fetchall_arrayref uses
           the fetchrow_arrayref entry elsewhere in this
           documentto fetch each row as an array ref. If the
           parameter array is not empty then it is used as a
           slice to select individual columns by index number.

           With no parameters, fetchall_arrayref acts as if
           passed an empty array ref.

           When passed a hash reference, fetchall_arrayref uses
           the fetchrow_hashref entry elsewhere in this
           documentto fetch each row as a hash ref. If the
           parameter hash is not empty then it is used as a slice
           to select individual columns by name. The names should
           be lower case regardless of the letter case in
           $sth->{NAME}.  The values of the hash should be set to
           1.

           For example, to fetch just the first column of every
           row you can use:

             $tbl_ary_ref = $sth->fetchall_arrayref([0]);

           To fetch the second to last and last column of every
           row you can use:

             $tbl_ary_ref = $sth->fetchall_arrayref([-2,-1]);

           To fetch only the fields called foo and bar of every
           row you can use:

             $tbl_ary_ref = $sth->fetchall_arrayref({ foo=>1, bar=>1 });

           The first two examples return a ref to an array of
           array refs. The last returns a ref to an array of hash
           refs.

=cut
