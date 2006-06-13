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
# The Original Code is the Bugzilla Test Runner System.
#
# The Initial Developer of the Original Code is Maciej Maczynski.
# Portions created by Maciej Maczynski are Copyright (C) 2001
# Maciej Maczynski. All Rights Reserved.
#
# Contributor(s): Greg Hendricks <ghendricks@novell.com>

package Bugzilla::Testopia::Product;

# Extends Bugzilla::Product;
use base "Bugzilla::Product";

use Bugzilla;
use Bugzilla::Testopia::Environment;

sub environments {
    my $self = shift;
    my $dbh = Bugzilla->dbh;
    
    my $ref = $dbh->selectcol_arrayref("SELECT environment_id 
                                        FROM test_environments
                                        WHERE product_id = ?",
                                        undef, $self->{'id'});
    my @objs;
    foreach my $id (@{$ref}){
        push @objs, Bugzilla::Testopia::Environment->new($id);
    }
    $self->{'environments'} = \@objs;
    return $self->{'environmets'};
}

sub builds {
    my $self = shift;
    my $dbh = Bugzilla->dbh;
    
    my $ref = $dbh->selectcol_arrayref(
                   "SELECT build_id 
                    FROM test_builds tb 
                    JOIN test_plans tp ON tb.plan_id = tp.plan_id
                    WHERE tp.product_id = ?",
                    undef, $self->{'id'});
    my @objs;
    foreach my $id (@{$ref}){
        push @objs, Bugzilla::Testopia::Build->new($id);
    }
    $self->{'build'} = \@objs;
    return $self->{'build'};
}

1;
