# -*- mode: cperl; c-basic-offset: 8; indent-tabs-mode: nil; -*-

=head1 COPYRIGHT

 # ***** BEGIN LICENSE BLOCK *****
 # Version: MPL 1.1
 #
 # The contents of this file are subject to the Mozilla Public License
 # Version 1.1 (the "License"); you may not use this file except in
 # compliance with the License. You may obtain a copy of the License
 # at http://www.mozilla.org/MPL/
 #
 # Software distributed under the License is distributed on an "AS IS"
 # basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 # the License for the specific language governing rights and
 # limitations under the License.
 #
 # The Original Code is Litmus.
 #
 # The Initial Developer of the Original Code is
 # the Mozilla Corporation.
 # Portions created by the Initial Developer are Copyright (C) 2005
 # the Initial Developer. All Rights Reserved.
 #
 # Contributor(s):
 #   Chris Cooper <ccooper@deadsquid.com>
 #   Zach Lipton <zach@zachlipton.com>
 #
 # ***** END LICENSE BLOCK *****

=cut

package Litmus::DBI;

use strict;
use warnings;
use Litmus::Config;
use Litmus::Error;
use Memoize;

use base 'Class::DBI::mysql';

my $dsn = "dbi:mysql:$Litmus::Config::db_name:$Litmus::Config::db_host";

our %column_aliases;

Litmus::DBI->set_db('Main',
                         $dsn,
                         $Litmus::Config::db_user,
                         $Litmus::Config::db_pass);
                         
# In some cases, we have column names that make sense from a database perspective
# (i.e. subgroup_id), but that don't make sense from a class/object perspective 
# (where subgroup would be more appropriate). To handle this, we allow for 
# Litmus::DBI's subclasses to set column aliases with the column_alias() sub. 
# Takes the database column name and the alias name. 
sub column_alias {
    my ($self, $db_name, $alias_name) = @_;
    
    $column_aliases{$alias_name} = $db_name;
}

# here's where the actual work happens. We consult our alias list 
# (as created by calls to column_alias()) and substitute the 
# database column if we find a match
memoize('find_column');
sub find_column {
    my $self = shift;
    my $wanted = shift;
    
    if (ref $self) {
        $wanted =~ s/^.*::(\w+)$/$1/;
    }
    if ($column_aliases{$wanted}) {
        return $column_aliases{$wanted};
    } else {
        # not an alias, so we use the normal 
        # find_column() from Class::DBI
        $self->SUPER::find_column($wanted);
    }
}

sub AUTOLOAD {
    my $self = shift;
    my @args = @_;
    my $name = our $AUTOLOAD;
    
    my $col = $self->find_column($name);
    if (!$col) {
        internalEror("tried to call Litmus::DBI method $name which does not exist");
    }
    
    return $self->$col(@args);
}
                                         
1;

