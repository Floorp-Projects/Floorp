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

use strict;

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
    return $self->{'environments'};
}

sub builds {
    my $self = shift;
    my($active) = @_;
    my $dbh = Bugzilla->dbh;
    
    my $query = "SELECT build_id FROM test_builds WHERE product_id = ?";
    $query .= " AND isactive = 1" if $active; 
    $query .= " ORDER BY name";
    
    my $ref = $dbh->selectcol_arrayref($query, undef, $self->{'id'});
    my @objs;
    foreach my $id (@{$ref}){
        push @objs, Bugzilla::Testopia::Build->new($id);
    }
    $self->{'builds'} = \@objs;
    return $self->{'builds'};
}

sub categories {
    my $self = shift;
    my $dbh = Bugzilla->dbh;
    
    my $ref = $dbh->selectcol_arrayref(
                   "SELECT category_id 
                    FROM test_case_categories 
                    WHERE product_id = ?
                 ORDER BY name",
                    undef, $self->{'id'});
    my @objs;
    foreach my $id (@{$ref}){
        push @objs, Bugzilla::Testopia::Category->new($id);
    }
    $self->{'categories'} = \@objs;
    return $self->{'categories'};
}

sub plans {
    my $self = shift;
    my $dbh = Bugzilla->dbh;
    
    my $ref = $dbh->selectcol_arrayref(
                   "SELECT plan_id 
                    FROM test_plans 
                    WHERE product_id = ?
                 ORDER BY name",
                    undef, $self->{'id'});
    my @objs;
    foreach my $id (@{$ref}){
        push @objs, Bugzilla::Testopia::TestPlan->new($id);
    }
    $self->{'plans'} = \@objs;
    return $self->{'plans'};
}

sub environment_categories {
    my $self = shift;
    my $dbh = Bugzilla->dbh;
    
    my $ref = $dbh->selectcol_arrayref(
                   "SELECT env_category_id 
                    FROM test_environment_category 
                    WHERE product_id = ?",
                    undef, $self->id);
    my @objs;
    foreach my $id (@{$ref}){
        push @objs, Bugzilla::Testopia::Environment::Category->new($id);
    }
    $self->{'environment_categories'} = \@objs;
    return $self->{'environment_categories'};
}

sub check_product_by_name {
    my $self = shift;
    my ($name) = @_;
    my $dbh = Bugzilla->dbh;
    my ($used) = $dbh->selectrow_array(qq{
    	SELECT id 
		  FROM products
		  WHERE name = ?},undef,$name);
    return $used;  
}

sub versions {
    my $self = shift;
    my $dbh = Bugzilla->dbh;

    my $values = $dbh->selectall_arrayref(
        "SELECT value AS id, value AS name
           FROM versions
          WHERE product_id = ?
          ORDER BY value", {'Slice' =>{}}, $self->id);

    $self->{'versions'} = $values;
    return $self->{'versions'};
}

sub milestones {
    my $self = shift;
    my $dbh = Bugzilla->dbh;

    my $values = $dbh->selectall_arrayref(
        "SELECT value AS id, value AS name
           FROM milestones
          WHERE product_id = ?
          ORDER BY value", {'Slice' =>{}}, $self->id);

    $self->{'milestones'} = $values;
    return $self->{'milestones'};
}

sub tags {
    my $self = shift;
    my $dbh = Bugzilla->dbh;
    my $ref = $dbh->selectcol_arrayref(
    "(SELECT test_tags.tag_id, test_tags.tag_name AS name 
         FROM test_tags
        INNER JOIN test_case_tags ON test_tags.tag_id = test_case_tags.tag_id
        INNER JOIN test_cases on test_cases.case_id = test_case_tags.case_id
        INNER JOIN test_case_plans on test_case_plans.case_id = test_cases.case_id
        INNER JOIN test_plans ON test_plans.plan_id = test_case_plans.plan_id
        WHERE test_plans.product_id = ?)  
     UNION 
      (SELECT test_tags.tag_id, test_tags.tag_name AS name
         FROM test_tags
        INNER JOIN test_plan_tags ON test_plan_tags.tag_id = test_tags.tag_id
        INNER JOIN test_plans ON test_plan_tags.plan_id = test_plans.plan_id
        WHERE test_plans.product_id = ?)
     UNION 
      (SELECT test_tags.tag_id, test_tags.tag_name AS name
         FROM test_tags
        INNER JOIN test_run_tags ON test_run_tags.tag_id = test_tags.tag_id
        INNER JOIN test_runs ON test_runs.run_id = test_run_tags.run_id
        INNER JOIN test_plans ON test_plans.plan_id = test_runs.plan_id
        WHERE test_plans.product_id = ?)
     ORDER BY name", undef, ($self->id,$self->id,$self->id));
    
    my @product_tags;
    foreach my $id (@$ref){
        push @product_tags, Bugzilla::Testopia::TestTag->new($id);
    }

    $self->{'tags'} = \@product_tags;
    return $self->{'tags'};
}

=head2 type

Returns 'product'

=cut

sub type {
    my $self = shift;
    $self->{'type'} = 'product';
    return $self->{'type'};
}

1;
