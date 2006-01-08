#!/usr/bin/perl -w
# -*- mode: cperl; c-basic-offset: 8; indent-tabs-mode: nil; -*-

# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
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

use strict;
$|++;

#use Time::HiRes qw( gettimeofday tv_interval );
#my $t0 = [gettimeofday];

use Litmus;
use Litmus::Auth;
use Litmus::Cache;
use Litmus::Error;
use Litmus::FormWidget;

use CGI;
use Time::Piece::MySQL;

use diagnostics;

Litmus::Auth::requireAdmin("edit_categories.cgi");

my $c = Litmus->cgi();
print $c->header();

if ($c->param) {
  my $product;
  my $rebuild_cache;
  if ($c->param('add_product')) {
    my $name = $c->param('new_product_name');
    my $icon_path = $c->param('new_icon_path') || "";
    my $enabled = $c->param('new_enabled') ? 1 : 0;
    eval {
      $product = Litmus::DB::Product->create({
                                              name => $name,
                                              iconpath => $icon_path,
                                              enabled => $enabled,
                                             });
      $rebuild_cache = 1;
    };
  }
  if ($c->param('delete_product')) {
    $product = Litmus::DB::Product->retrieve($c->param('modify_product_id'));
    $product->delete;
    $rebuild_cache = 1;
  }
  if ($c->param('update_product')) {
    $product = Litmus::DB::Product->retrieve($c->param('modify_product_id'));
    $product->name($c->param('modify_product_name'));
    $product->iconpath($c->param('modify_icon_path'));
    if ($c->param('modify_enabled')) {
      $product->enabled(1);
    } else {
      $product->enabled(0);
    }
    eval {
      $product->update;
      $rebuild_cache = 1;
    };
  }

  if ($rebuild_cache) {
    use Litmus::Cache;
    rebuildCache();
  }
}

my $products = Litmus::FormWidget->getProducts();
my $platforms = Litmus::FormWidget->getPlatforms();
my $branches = Litmus::FormWidget->getBranches();
my $opsyses = Litmus::FormWidget->getOpsyses();
my $log_types = Litmus::FormWidget->getLogTypes();

my $vars = {
            title => 'Edit Categories',
            products => $products,
            platforms => $platforms,
            branches => $branches,
            opsyses => $opsyses,
            log_types => $log_types,
           };


my $cookie =  Litmus::Auth::getCookie();
$vars->{"defaultemail"} = $cookie;
$vars->{"show_admin"} = Litmus::Auth::istrusted($cookie);

Litmus->template()->process("admin/edit_categories.tmpl", $vars) ||
  internalError(Litmus->template()->error());

#my $elapsed = tv_interval ( $t0 );
#printf  "<div id='pageload'>Page took %f seconds to load.</div>", $elapsed;

exit 0;






