#!/usr/bin/perl -wT
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
# The Original Code is the Bugzilla Testopia System.
#
# The Initial Developer of the Original Code is Greg Hendricks.
# Portions created by Greg Hendricks are Copyright (C) 2006
# Novell. All Rights Reserved.
#
# Contributor(s): Greg Hendricks <ghendricks@novell.com>
#                 Michael Hight <mjhight@gmail.com>
#                 Garrett Braden <gbraden@novell.com>

use strict;
use lib ".";

use Bugzilla;
use Bugzilla::Util;
use Bugzilla::Config;
use Bugzilla::Constants;
use Bugzilla::Testopia::Util;
use Bugzilla::Testopia::Environment;
use Bugzilla::Testopia::Environment::Element;
use Bugzilla::Testopia::Environment::Category;
use Bugzilla::Testopia::Environment::Property;

Bugzilla->login(LOGIN_REQUIRED);

my $cgi = Bugzilla->cgi;

use vars qw($vars);
my $template = Bugzilla->template;

print $cgi->header;

my $action = $cgi->param('action') || '';

if ($action eq 'Add'){
    my $name = $cgi->param('name');
    my $product = $cgi->param('product');
    
    trick_taint($name);
    detaint_natural($product);
    
    my $env = Bugzilla::Testopia::Environment->new({'environment_id' => 0});
    
    my $success = $env->store_environment_name($name, $product);
    unless ($success){
        $vars->{'tr_error'} = "The environment name '$name' is already taken.";
        $vars->{'environment'} = $env;
        $template->process("testopia/environment/add.html.tmpl", $vars)
            || print $template->error();
        exit;
    }
    $vars->{'tr_message'} = "The environment '$name' was successfully added.";
    
    $env = Bugzilla::Testopia::Environment->new($success);
    my $category = Bugzilla::Testopia::Environment::Category->new({'id' => 0});
    if (Param('useclassification')){
        $vars->{'allhaschild'} = $category->get_all_child_count;
        $vars->{'toplevel'} = Bugzilla->user->get_selectable_classifications;
        $vars->{'type'} = 'classification';
    }
    else {
        $vars->{'toplevel'} = $category->get_env_product_list;
        $vars->{'type'} = 'product';
    }
    $vars->{'user'} = Bugzilla->user;
    $vars->{'action'} = 'do_edit';
    $vars->{'environment'} = $env;
    $template->process("testopia/environment/show.html.tmpl", $vars)
        || print $template->error();
    
}

else {
    $vars->{'environment'} = Bugzilla::Testopia::Environment->new({'environment_id' => 0});
    $vars->{'backlink'} = $vars->{'environment'};
    $vars->{'products'} = Bugzilla->user->get_selectable_products;
    
    $template->process("testopia/environment/add.html.tmpl", $vars)
        || print $template->error();
}