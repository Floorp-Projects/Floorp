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
# Contributor(s): Andrew Dunstan <andrew@dunslane.net>,
#                 Edward J. Sabol <edwardjsabol@iname.com>

package Bugzilla::DB::Schema::Mysql;

###############################################################################
#
# DB::Schema implementation for MySQL
#
###############################################################################

use strict;
use Bugzilla::Error;

use base qw(Bugzilla::DB::Schema);

#------------------------------------------------------------------------------
sub _initialize {

    my $self = shift;

    $self = $self->SUPER::_initialize(@_);

    $self->{db_specific} = {

        BOOLEAN =>      'tinyint',
        FALSE =>        '0', 
        TRUE =>         '1',

        INT1 =>         'tinyint',
        INT2 =>         'smallint',
        INT3 =>         'mediumint',
        INT4 =>         'integer',

        SMALLSERIAL =>  'smallint auto_increment',
        MEDIUMSERIAL => 'mediumint auto_increment',
        INTSERIAL =>    'integer auto_increment',

        TINYTEXT =>     'tinytext',
        MEDIUMTEXT =>   'mediumtext',
        TEXT =>         'text',

        LONGBLOB =>     'longblob',

        DATETIME =>     'datetime',

    };

    $self->_adjust_schema;

    return $self;

} #eosub--_initialize
#------------------------------------------------------------------------------
sub _get_create_table_ddl {
    # Extend superclass method to specify the MYISAM storage engine.
    # Returns a "create table" SQL statement.

    my($self, $table) = @_;

    return($self->SUPER::_get_create_table_ddl($table) . ' TYPE = MYISAM');

} #eosub--_get_create_table_ddl
#------------------------------------------------------------------------------
sub _get_create_index_ddl {
    # Extend superclass method to create FULLTEXT indexes on text fields.
    # Returns a "create index" SQL statement.

    my($self, $table_name, $index_name, $index_fields, $index_type) = @_;

    my $sql = "CREATE ";
    $sql .= "$index_type " if ($index_type eq 'UNIQUE'
                               || $index_type eq 'FULLTEXT');
    $sql .= "INDEX $index_name ON $table_name \(" .
      join(", ", @$index_fields) . "\)";

    return($sql);

} #eosub--_get_create_index_ddl
#--------------------------------------------------------------------

# MySQL has a simpler ALTER TABLE syntax than ANSI.
sub get_alter_column_ddl {

    my ($self, $table, $column, $new_def) = @_;

    my $new_ddl = $self->get_type_ddl($new_def);

    return (("ALTER TABLE $table CHANGE COLUMN $column $column $new_ddl"));
}

1;
