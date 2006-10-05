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

package Bugzilla::Testopia::Classification;

use strict;

use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Config;

# Extends Bugzilla::Classification;
use base "Bugzilla::Classification";

use Bugzilla;

sub user_visible_products {
    my $self = shift;
    my $dbh = Bugzilla->dbh;

    if (!$self->{'products'}) {
    my $query = "SELECT id FROM products " .
                "LEFT JOIN group_control_map " .
                "ON group_control_map.product_id = products.id ";
    if (Param('useentrygroupdefault')) {
       $query .= "AND group_control_map.entry != 0 ";
    } else {
       $query .= "AND group_control_map.membercontrol = " .
              CONTROLMAPMANDATORY . " ";
    }
    if (%{Bugzilla->user->groups}) {
       $query .= "AND group_id NOT IN(" . 
              join(',', values(%{Bugzilla->user->groups})) . ") ";
    }
    $query .= "WHERE group_id IS NULL AND products.classification_id= ? ORDER BY products.name";
            
        my $product_ids = $dbh->selectcol_arrayref($query, undef, $self->id);
 
        my @products;
        foreach my $product_id (@$product_ids) {
            push (@products, new Bugzilla::Testopia::Product($product_id));
        }
        $self->{'user_visible_products'} = \@products;
    }
    return $self->{'user_visible_products'};
}

sub products_to_json {
    my $self = shift;
    my ($disable_move) = @_;
    
    $disable_move ||= '';
    $disable_move = ',"addChild","move"' if $disable_move;
    my $products = $self->user_visible_products;
    my $json = "[";
    foreach my $obj (@{$products}){
        $json .= '{title:"' . $obj->name . '",';
        $json .=  'isFolder:' . (scalar @{$obj->environment_categories} > 0 ? "true" : "false") . ',';
        $json .=  'objectId:"' . $obj->id . '",';
        $json .=  'widgetId:"product' . $obj->id . '",';
        $json .=  'actionsDisabled:["addElement","addProperty","addValue"'. $disable_move .'],';
        $json .=  'childIconSrc:"testopia/img/folder_red.gif"},';
    }
    chop $json;
    $json .= "]";
    return $json;   
}
1;