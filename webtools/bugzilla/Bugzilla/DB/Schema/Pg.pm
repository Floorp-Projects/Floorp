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

package Bugzilla::DB::Schema::Pg;

###############################################################################
#
# DB::Schema implementation for PostgreSQL
#
###############################################################################

use strict;

use base qw(Bugzilla::DB::Schema);

#------------------------------------------------------------------------------
sub _initialize {

    my $self = shift;

    $self = $self->SUPER::_initialize(@_);

    # Remove FULLTEXT index types from the schemas.
    foreach my $table (keys %{ $self->{schema} }) {
        if ($self->{schema}{$table}{INDEXES}) {
            foreach my $index (@{ $self->{schema}{$table}{INDEXES} }) {
                if (ref($index) eq 'HASH') {
                    delete($index->{TYPE}) if (exists $index->{TYPE} 
                        && $index->{TYPE} eq 'FULLTEXT');
                }
            }
            foreach my $index (@{ $self->{abstract_schema}{$table}{INDEXES} }) {
                if (ref($index) eq 'HASH') {
                    delete($index->{TYPE}) if (exists $index->{TYPE} 
                        && $index->{TYPE} eq 'FULLTEXT');
                }
            }
        }
    }

    $self->{db_specific} = {

        BOOLEAN =>      'smallint',
        FALSE =>        '0', 
        TRUE =>         '1',

        INT1 =>         'integer',
        INT2 =>         'integer',
        INT3 =>         'integer',
        INT4 =>         'integer',

        SMALLSERIAL =>  'serial unique',
        MEDIUMSERIAL => 'serial unique',
        INTSERIAL =>    'serial unique',

        TINYTEXT =>     'varchar(255)',
        MEDIUMTEXT =>   'text',
        TEXT =>         'text',

        LONGBLOB =>     'bytea',

        DATETIME =>     'timestamp(0) without time zone',

    };

    $self->_adjust_schema;

    return $self;

} #eosub--_initialize
#--------------------------------------------------------------------

# Overridden because Pg has such weird ALTER TABLE problems.
sub get_add_column_ddl {
    my ($self, $table, $column, $definition) = @_;

    my @statements;
    my $specific = $self->{db_specific};

    my $type = $definition->{TYPE};
    $type = $specific->{$type} if exists $specific->{$type};
    push(@statements, "ALTER TABLE $table ADD COLUMN $column $type");

    my $default = $definition->{DEFAULT};
    # Replace any abstract default value (such as 'TRUE' or 'FALSE')
    # with its database-specific implementation.
    if (defined $default) {
        $default = $specific->{$default} if exists $specific->{$default};
        push(@statements, "ALTER TABLE $table ALTER COLUMN $column "
                         . " SET DEFAULT $default");
    }

    if ($definition->{NOTNULL}) {
        push(@statements, "ALTER TABLE $table ALTER COLUMN $column "
                         . " SET NOT NULL");
    }

    if ($definition->{PRIMARYKEY}) {
         push(@statements, "ALTER TABLE $table ALTER COLUMN "
                         . " ADD PRIMARY KEY ($column)");
    }

    return @statements;
}

1;
