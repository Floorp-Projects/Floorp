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
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Holger
# Schurig. Portions created by Holger Schurig are
# Copyright (C) 1999 Holger Schurig. All
# Rights Reserved.
#
# Contributor(s): Holger Schurig <holgerschurig@nikocity.de>
#               Terry Weissman <terry@mozilla.org>
#               Dawn Endico <endico@mozilla.org>
#               Joe Robins <jmrobins@tgix.com>
#               Gavin Shelley <bugzilla@chimpychompy.org>
#               Fr��ic Buclin <LpSolit@gmail.com>
#               Greg Hendricks <ghendricks@novell.com>
#               Lance Larsh <lance.larsh@oracle.com>
#
# Direct any questions on this source code to
#
# Holger Schurig <holgerschurig@nikocity.de>

use strict;
use lib ".";
use vars qw ($template $vars);
use Bugzilla::Constants;
require "globals.pl";
use Bugzilla::Bug;
use Bugzilla::Series;
use Bugzilla::Config qw(:DEFAULT $datadir);
use Bugzilla::Product;
use Bugzilla::Classification;
use Bugzilla::Milestone;

# Shut up misguided -w warnings about "used only once".  "use vars" just
# doesn't work for me.
use vars qw(@legal_bug_status @legal_resolution);

#
# Preliminary checks:
#

my $user = Bugzilla->login(LOGIN_REQUIRED);
my $whoid = $user->id;

my $cgi = Bugzilla->cgi;
print $cgi->header();

$user->in_group('editcomponents')
  || ThrowUserError("auth_failure", {group  => "editcomponents",
                                     action => "edit",
                                     object => "products"});

#
# often used variables
#
my $classification_name = trim($cgi->param('classification') || '');
my $product_name = trim($cgi->param('product') || '');
my $action  = trim($cgi->param('action')  || '');
my $dbh = Bugzilla->dbh;

#
# product = '' -> Show nice list of classifications (if
# classifications enabled)
#

if (Param('useclassification') 
    && !$classification_name
    && !$product_name)
{
    my @classifications =
        Bugzilla::Classification::get_all_classifications();
    
    $vars->{'classifications'} = \@classifications;
    
    $template->process("admin/products/list-classifications.html.tmpl",
                       $vars)
        || ThrowTemplateError($template->error());

    exit;
}


#
# action = '' -> Show a nice list of products, unless a product
#                is already specified (then edit it)
#

if (!$action && !$product_name) {
    my @products;

    if (Param('useclassification')) {
        my $classification = 
            Bugzilla::Classification::check_classification($classification_name);

        @products = @{$classification->products};
        $vars->{'classification'} = $classification;
    } else {
        @products = Bugzilla::Product::get_all_products;
    }

    $vars->{'products'} = \@products; 

    $template->process("admin/products/list.html.tmpl",
                       $vars)
      || ThrowTemplateError($template->error());

    exit;
}




#
# action='add' -> present form for parameters for new product
#
# (next action will be 'new')
#

if ($action eq 'add') {

    if (Param('useclassification')) {
        my $classification = 
            Bugzilla::Classification::check_classification($classification_name);
        $vars->{'classification'} = $classification;
    }
    $template->process("admin/products/create.html.tmpl", $vars)
      || ThrowTemplateError($template->error());

    exit;
}


#
# action='new' -> add product entered in the 'action=add' screen
#

if ($action eq 'new') {

    # Cleanups and validity checks

    my $classification_id = 1;
    if (Param('useclassification')) {
        my $classification = 
            Bugzilla::Classification::check_classification($classification_name);
        $classification_id = $classification->id;
        $vars->{'classification'} = $classification;
    }

    unless ($product_name) {
        ThrowUserError("product_blank_name");  
    }

    my $product = new Bugzilla::Product({name => $product_name});

    if ($product) {

        # Check for exact case sensitive match:
        if ($product->name eq $product_name) {
            ThrowUserError("prod_name_already_in_use",
                           {'product' => $product->name});
        }

        # Next check for a case-insensitive match:
        if (lc($product->name) eq lc($product_name)) {
            ThrowUserError("prod_name_diff_in_case",
                           {'product' => $product_name,
                            'existing_product' => $product->name}); 
        }
    }

    my $version = trim($cgi->param('version') || '');

    if ($version eq '') {
        ThrowUserError("product_must_have_version",
                       {'product' => $product_name});
    }

    my $description  = trim($cgi->param('description')  || '');

    if ($description eq '') {
        ThrowUserError('product_must_have_description',
                       {'product' => $product_name});
    }

    my $milestoneurl = trim($cgi->param('milestoneurl') || '');
    my $disallownew = 0;
    $disallownew = 1 if $cgi->param('disallownew');
    my $votesperuser = $cgi->param('votesperuser');
    $votesperuser ||= 0;
    my $maxvotesperbug = $cgi->param('maxvotesperbug');
    $maxvotesperbug = 10000 if !defined $maxvotesperbug;
    my $votestoconfirm = $cgi->param('votestoconfirm');
    $votestoconfirm ||= 0;
    my $defaultmilestone = $cgi->param('defaultmilestone') || "---";

    # Add the new product.
    SendSQL("INSERT INTO products ( " .
            "name, description, milestoneurl, disallownew, votesperuser, " .
            "maxvotesperbug, votestoconfirm, defaultmilestone, classification_id" .
            " ) VALUES ( " .
            SqlQuote($product_name) . "," .
            SqlQuote($description) . "," .
            SqlQuote($milestoneurl) . "," .
            # had tainting issues under cygwin, IIS 5.0, perl -T %s %s
            # see bug 208647. http://bugzilla.mozilla.org/show_bug.cgi?id=208647
            # had to de-taint $disallownew, $votesperuser, $maxvotesperbug,
            #  and $votestoconfirm w/ SqlQuote()
            # - jpyeron@pyerotechnics.com
            SqlQuote($disallownew) . "," .
            SqlQuote($votesperuser) . "," .
            SqlQuote($maxvotesperbug) . "," .
            SqlQuote($votestoconfirm) . "," .
            SqlQuote($defaultmilestone) . "," .
            SqlQuote($classification_id) . ")");

    $product = new Bugzilla::Product({name => $product_name});
    
    SendSQL("INSERT INTO versions ( " .
          "value, product_id" .
          " ) VALUES ( " .
          SqlQuote($version) . "," .
          $product->id . ")" );

    SendSQL("INSERT INTO milestones (product_id, value) VALUES (" .
            $product->id . ", " . SqlQuote($defaultmilestone) . ")");

    # If we're using bug groups, then we need to create a group for this
    # product as well.  -JMR, 2/16/00
    if (Param("makeproductgroups")) {
        # Next we insert into the groups table
        my $productgroup = $product->name;
        while (GroupExists($productgroup)) {
            $productgroup .= '_';
        }
        SendSQL("INSERT INTO groups " .
                "(name, description, isbuggroup, last_changed) " .
                "VALUES (" .
                SqlQuote($productgroup) . ", " .
                SqlQuote("Access to bugs in the " . $product->name . 
                         " product") . ", 1, NOW())");
        my $gid = $dbh->bz_last_key('groups', 'id');
        my $admin = GroupNameToId('admin');
        # If we created a new group, give the "admin" group priviledges
        # initially.
        SendSQL("INSERT INTO group_group_map (member_id, grantor_id, grant_type)
                 VALUES ($admin, $gid," . GROUP_MEMBERSHIP .")");
        SendSQL("INSERT INTO group_group_map (member_id, grantor_id, grant_type)
                 VALUES ($admin, $gid," . GROUP_BLESS .")");
        SendSQL("INSERT INTO group_group_map (member_id, grantor_id, grant_type)
                 VALUES ($admin, $gid," . GROUP_VISIBLE .")");

        # Associate the new group and new product.
        SendSQL("INSERT INTO group_control_map " .
                "(group_id, product_id, entry, " .
                "membercontrol, othercontrol, canedit) VALUES " .
                "($gid, " . $product->id . ", " . 
                Param("useentrygroupdefault") .
                ", " . CONTROLMAPDEFAULT . ", " .
                CONTROLMAPNA . ", 0)");
    }

    if ($cgi->param('createseries')) {
        # Insert default charting queries for this product.
        # If they aren't using charting, this won't do any harm.
        GetVersionTable();

        # $open_name and $product are sqlquoted by the series code 
        # and never used again here, so we can trick_taint them.
        my $open_name = $cgi->param('open_name');
        trick_taint($open_name);
    
        my @series;
    
        # We do every status, every resolution, and an "opened" one as well.
        foreach my $bug_status (@::legal_bug_status) {
            push(@series, [$bug_status, 
                           "bug_status=" . url_quote($bug_status)]);
        }

        foreach my $resolution (@::legal_resolution) {
            next if !$resolution;
            push(@series, [$resolution, "resolution=" .url_quote($resolution)]);
        }

        # For localisation reasons, we get the name of the "global" subcategory
        # and the title of the "open" query from the submitted form.
        my @openedstatuses = OpenStates();
        my $query = 
               join("&", map { "bug_status=" . url_quote($_) } @openedstatuses);
        push(@series, [$open_name, $query]);
    
        foreach my $sdata (@series) {
            my $series = new Bugzilla::Series(undef, $product->name, 
                            scalar $cgi->param('subcategory'),
                            $sdata->[0], $::userid, 1,
                            $sdata->[1] . "&product=" .
                            url_quote($product->name), 1);
            $series->writeToDatabase();
        }
    }
    # Make versioncache flush
    unlink "$datadir/versioncache";

    $vars->{'product'} = $product;
    
    $template->process("admin/products/created.html.tmpl", $vars)
        || ThrowTemplateError($template->error());
    exit;
}

#
# action='del' -> ask if user really wants to delete
#
# (next action would be 'delete')
#

if ($action eq 'del') {
    
    my $product = Bugzilla::Product::check_product($product_name);

    if (Param('useclassification')) {
        my $classification = 
            Bugzilla::Classification::check_classification($classification_name);
        if ($classification->id != $product->classification_id) {
            ThrowUserError('classification_doesnt_exist_for_product',
                           { product => $product->name,
                             classification => $classification->name });
        }
        $vars->{'classification'} = $classification;
    }

    $vars->{'product'} = $product;

    $template->process("admin/products/confirm-delete.html.tmpl", $vars)
        || ThrowTemplateError($template->error());
    exit;
}

#
# action='delete' -> really delete the product
#

if ($action eq 'delete') {
    
    my $product = Bugzilla::Product::check_product($product_name);
    
    $vars->{'product'} = $product;

    if (Param('useclassification')) {
        my $classification = 
            Bugzilla::Classification::check_classification($classification_name);
        if ($classification->id != $product->classification_id) {
            ThrowUserError('classification_doesnt_exist_for_product',
                           { product => $product->name,
                             classification => $classification->name });
        }
        $vars->{'classification'} = $classification;
    }

    if ($product->bug_count) {
        if (Param("allowbugdeletion")) {
            foreach my $bug_id (@{$product->bug_ids}) {
                my $bug = new Bugzilla::Bug($bug_id, $whoid);
                $bug->remove_from_db();
            }
        }
        else {
            ThrowUserError("product_has_bugs", 
                           { nb => $product->bug_count });
        }
    }

    $dbh->bz_lock_tables('products WRITE', 'components WRITE',
                         'versions WRITE', 'milestones WRITE',
                         'group_control_map WRITE',
                         'flaginclusions WRITE', 'flagexclusions WRITE');

    $dbh->do("DELETE FROM components WHERE product_id = ?",
             undef, $product->id);

    $dbh->do("DELETE FROM versions WHERE product_id = ?",
             undef, $product->id);

    $dbh->do("DELETE FROM milestones WHERE product_id = ?",
             undef, $product->id);

    $dbh->do("DELETE FROM group_control_map WHERE product_id = ?",
             undef, $product->id);

    $dbh->do("DELETE FROM flaginclusions WHERE product_id = ?",
             undef, $product->id);
             
    $dbh->do("DELETE FROM flagexclusions WHERE product_id = ?",
             undef, $product->id);
             
    $dbh->do("DELETE FROM products WHERE id = ?",
             undef, $product->id);

    $dbh->bz_unlock_tables();

    unlink "$datadir/versioncache";

    $template->process("admin/products/deleted.html.tmpl", $vars)
        || ThrowTemplateError($template->error());
    exit;
}

#
# action='edit' -> present the 'edit product' form
# If a product is given with no action associated with it, then edit it.
#
# (next action would be 'update')
#

if ($action eq 'edit' || (!$action && $product_name)) {

    my $product = Bugzilla::Product::check_product($product_name);

    if (Param('useclassification')) {
        my $classification; 
        if (!$classification_name) {
            $classification = 
                new Bugzilla::Classification($product->classification_id);
        } else {
            $classification = 
                Bugzilla::Classification::check_classification($classification_name);
            if ($classification->id != $product->classification_id) {
                ThrowUserError('classification_doesnt_exist_for_product',
                               { product => $product->name,
                                 classification => $classification->name });
            }
        }
        $vars->{'classification'} = $classification;
    }
    my $group_controls = $product->group_controls;
        
    # Convert Group Controls(membercontrol and othercontrol) from 
    # integer to string to display Membercontrol/Othercontrol names
    # at the template. <gabriel@async.com.br>
    my $constants = {
        (CONTROLMAPNA) => 'NA',
        (CONTROLMAPSHOWN) => 'Shown',
        (CONTROLMAPDEFAULT) => 'Default',
        (CONTROLMAPMANDATORY) => 'Mandatory'};

    foreach my $group (keys(%$group_controls)) {
        foreach my $control ('membercontrol', 'othercontrol') {
            $group_controls->{$group}->{$control} = 
                $constants->{$group_controls->{$group}->{$control}};
        }
    }
    $vars->{'group_controls'} = $group_controls;

    $vars->{'product'} = $product;
        
    $template->process("admin/products/edit.html.tmpl", $vars)
        || ThrowTemplateError($template->error());

    exit;
}

#
# action='updategroupcontrols' -> update the product
#

if ($action eq 'updategroupcontrols') {

    my $product = Bugzilla::Product::check_product($product_name);
    my @now_na = ();
    my @now_mandatory = ();
    foreach my $f ($cgi->param()) {
        if ($f =~ /^membercontrol_(\d+)$/) {
            my $id = $1;
            if ($cgi->param($f) == CONTROLMAPNA) {
                push @now_na,$id;
            } elsif ($cgi->param($f) == CONTROLMAPMANDATORY) {
                push @now_mandatory,$id;
            }
        }
    }
    if (!defined $cgi->param('confirmed')) {
        my @na_groups = ();
        if (@now_na) {
            SendSQL("SELECT groups.name, COUNT(bugs.bug_id) 
                     FROM bugs, bug_group_map, groups
                     WHERE groups.id IN(" . join(', ', @now_na) . ")
                     AND bug_group_map.group_id = groups.id
                     AND bug_group_map.bug_id = bugs.bug_id
                     AND bugs.product_id = " . $product->id . " " .
                    $dbh->sql_group_by('groups.name'));
            while (MoreSQLData()) {
                my ($groupname, $bugcount) = FetchSQLData();
                my %g = ();
                $g{'name'} = $groupname;
                $g{'count'} = $bugcount;
                push @na_groups,\%g;
            }
        }

        my @mandatory_groups = ();
        if (@now_mandatory) {
            SendSQL("SELECT groups.name, COUNT(bugs.bug_id) 
                       FROM bugs
                  LEFT JOIN bug_group_map
                         ON bug_group_map.bug_id = bugs.bug_id
                 INNER JOIN groups
                         ON bug_group_map.group_id = groups.id
                      WHERE groups.id IN(" . join(', ', @now_mandatory) . ")
                        AND bugs.product_id = " . $product->id . " 
                        AND bug_group_map.bug_id IS NULL " .
                        $dbh->sql_group_by('groups.name'));
            while (MoreSQLData()) {
                my ($groupname, $bugcount) = FetchSQLData();
                my %g = ();
                $g{'name'} = $groupname;
                $g{'count'} = $bugcount;
                push @mandatory_groups,\%g;
            }
        }
        if ((@na_groups) || (@mandatory_groups)) {
            $vars->{'product'} = $product;
            $vars->{'na_groups'} = \@na_groups;
            $vars->{'mandatory_groups'} = \@mandatory_groups;
            $template->process("admin/products/groupcontrol/confirm-edit.html.tmpl", $vars)
                || ThrowTemplateError($template->error());
            exit;                
        }
    }
    SendSQL("SELECT id, name FROM groups " .
            "WHERE isbuggroup != 0 AND isactive != 0");
    while (MoreSQLData()){
        my ($groupid, $groupname) = FetchSQLData();
        my $newmembercontrol = $cgi->param("membercontrol_$groupid") || 0;
        my $newothercontrol = $cgi->param("othercontrol_$groupid") || 0;
        #  Legality of control combination is a function of
        #  membercontrol\othercontrol
        #                 NA SH DE MA
        #              NA  +  -  -  -
        #              SH  +  +  +  +
        #              DE  +  -  +  +
        #              MA  -  -  -  +
        unless (($newmembercontrol == $newothercontrol)
              || ($newmembercontrol == CONTROLMAPSHOWN)
              || (($newmembercontrol == CONTROLMAPDEFAULT)
               && ($newothercontrol != CONTROLMAPSHOWN))) {
            ThrowUserError('illegal_group_control_combination',
                            {groupname => $groupname});
        }
    }
    $dbh->bz_lock_tables('groups READ',
                         'group_control_map WRITE',
                         'bugs WRITE',
                         'bugs_activity WRITE',
                         'bug_group_map WRITE',
                         'fielddefs READ');
    SendSQL("SELECT id, name, entry, membercontrol, othercontrol, canedit " .
            "FROM groups " .
            "LEFT JOIN group_control_map " .
            "ON group_control_map.group_id = id AND product_id = " .
            $product->id . " WHERE isbuggroup != 0 AND isactive != 0");
    while (MoreSQLData()) {
        my ($groupid, $groupname, $entry, $membercontrol, 
            $othercontrol, $canedit) = FetchSQLData();
        my $newentry = $cgi->param("entry_$groupid") || 0;
        my $newmembercontrol = $cgi->param("membercontrol_$groupid") || 0;
        my $newothercontrol = $cgi->param("othercontrol_$groupid") || 0;
        my $newcanedit = $cgi->param("canedit_$groupid") || 0;
        my $oldentry = $entry;
        $entry = $entry || 0;
        $membercontrol = $membercontrol || 0;
        $othercontrol = $othercontrol || 0;
        $canedit = $canedit || 0;
        detaint_natural($newentry);
        detaint_natural($newothercontrol);
        detaint_natural($newmembercontrol);
        detaint_natural($newcanedit);
        if ((!defined($oldentry)) && 
             (($newentry) || ($newmembercontrol) || ($newcanedit))) {
            PushGlobalSQLState();
            SendSQL("INSERT INTO group_control_map " .
                    "(group_id, product_id, entry, " .
                    "membercontrol, othercontrol, canedit) " .
                    "VALUES " .
                    "($groupid, " . $product->id . ", $newentry, " .
                    "$newmembercontrol, $newothercontrol, $newcanedit)");
            PopGlobalSQLState();
        } elsif (($newentry != $entry) 
                  || ($newmembercontrol != $membercontrol) 
                  || ($newothercontrol != $othercontrol) 
                  || ($newcanedit != $canedit)) {
            PushGlobalSQLState();
            SendSQL("UPDATE group_control_map " .
                    "SET entry = $newentry, " .
                    "membercontrol = $newmembercontrol, " .
                    "othercontrol = $newothercontrol, " .
                    "canedit = $newcanedit " .
                    "WHERE group_id = $groupid " .
                    "AND product_id = " . $product->id);
            PopGlobalSQLState();
        }

        if (($newentry == 0) && ($newmembercontrol == 0)
          && ($newothercontrol == 0) && ($newcanedit == 0)) {
            PushGlobalSQLState();
            SendSQL("DELETE FROM group_control_map " .
                    "WHERE group_id = $groupid " .
                    "AND product_id = " . $product->id);
            PopGlobalSQLState();
        }
    }

    my @removed_na;
    foreach my $groupid (@now_na) {
        my $count = 0;
        SendSQL("SELECT bugs.bug_id, 
                 CASE WHEN (lastdiffed >= delta_ts) THEN 1 ELSE 0 END
                 FROM bugs, bug_group_map
                 WHERE group_id = $groupid
                 AND bug_group_map.bug_id = bugs.bug_id
                 AND bugs.product_id = " . $product->id . "
                 ORDER BY bugs.bug_id");
        while (MoreSQLData()) {
            my ($bugid, $mailiscurrent) = FetchSQLData();
            PushGlobalSQLState();
            SendSQL("DELETE FROM bug_group_map WHERE
                     bug_id = $bugid AND group_id = $groupid");
            SendSQL("SELECT name, NOW() FROM groups WHERE id = $groupid");
            my ($removed, $timestamp) = FetchSQLData();
            LogActivityEntry($bugid, "bug_group", $removed, "",
                             $::userid, $timestamp);
            my $diffed = "";
            if ($mailiscurrent) {
                $diffed = ", lastdiffed = " . SqlQuote($timestamp);
            }
            SendSQL("UPDATE bugs SET delta_ts = " . SqlQuote($timestamp) .
                    $diffed . " WHERE bug_id = $bugid");
            PopGlobalSQLState();
            $count++;
        }
        my %group = (name => GroupIdToName($groupid),
                     bug_count => $count);

        push(@removed_na, \%group);
    }

    my @added_mandatory;
    foreach my $groupid (@now_mandatory) {
        my $count = 0;
        SendSQL("SELECT bugs.bug_id,
                 CASE WHEN (lastdiffed >= delta_ts) THEN 1 ELSE 0 END
                 FROM bugs
                 LEFT JOIN bug_group_map
                 ON bug_group_map.bug_id = bugs.bug_id
                 AND group_id = $groupid
                 WHERE bugs.product_id = " . $product->id . "
                 AND bug_group_map.bug_id IS NULL
                 ORDER BY bugs.bug_id");
        while (MoreSQLData()) {
            my ($bugid, $mailiscurrent) = FetchSQLData();
            PushGlobalSQLState();
            SendSQL("INSERT INTO bug_group_map (bug_id, group_id)
                     VALUES ($bugid, $groupid)");
            SendSQL("SELECT name, NOW() FROM groups WHERE id = $groupid");
            my ($added, $timestamp) = FetchSQLData();
            LogActivityEntry($bugid, "bug_group", "", $added,
                             $::userid, $timestamp);
            my $diffed = "";
            if ($mailiscurrent) {
                $diffed = ", lastdiffed = " . SqlQuote($timestamp);
            }
            SendSQL("UPDATE bugs SET delta_ts = " . SqlQuote($timestamp) .
                    $diffed . " WHERE bug_id = $bugid");
            PopGlobalSQLState();
            $count++;
        }
        my %group = (name => GroupIdToName($groupid),
                     bug_count => $count);

        push(@added_mandatory, \%group);
    }
    $dbh->bz_unlock_tables();

    $vars->{'removed_na'} = \@removed_na;

    $vars->{'added_mandatory'} = \@added_mandatory;

    $vars->{'product'} = $product;

    $template->process("admin/products/groupcontrol/updated.html.tmpl", $vars)
        || ThrowTemplateError($template->error());
    exit;
}

#
# action='update' -> update the product
#
if ($action eq 'update') {

    my $product_old_name    = trim($cgi->param('product_old_name')    || '');
    my $description         = trim($cgi->param('description')         || '');
    my $disallownew         = trim($cgi->param('disallownew')         || '');
    my $milestoneurl        = trim($cgi->param('milestoneurl')        || '');
    my $votesperuser        = trim($cgi->param('votesperuser')        || 0);
    my $maxvotesperbug      = trim($cgi->param('maxvotesperbug')      || 0);
    my $votestoconfirm      = trim($cgi->param('votestoconfirm')      || 0);
    my $defaultmilestone    = trim($cgi->param('defaultmilestone')    || '---');

    my $checkvotes = 0;

    my $product_old = Bugzilla::Product::check_product($product_old_name);

    if (Param('useclassification')) {
        my $classification; 
        if (!$classification_name) {
            $classification = 
                new Bugzilla::Classification($product_old->classification_id);
        } else {
            $classification = 
                Bugzilla::Classification::check_classification($classification_name);
            if ($classification->id != $product_old->classification_id) {
                ThrowUserError('classification_doesnt_exist_for_product',
                               { product => $product_old->name,
                                 classification => $classification->name });
            }
        }
        $vars->{'classification'} = $classification;
    }

    unless ($product_name) {
        ThrowUserError('prod_cant_delete_name',
                       {product => $product_old->name});
    }

    unless ($description) {
        ThrowUserError('prod_cant_delete_description',
                       {product => $product_old->name});
    }

    my $stored_maxvotesperbug = $maxvotesperbug;
    if (!detaint_natural($maxvotesperbug)) {
        ThrowUserError('prod_votes_per_bug_must_be_nonnegative',
                       {maxvotesperbug => $stored_maxvotesperbug});
    }

    my $stored_votesperuser = $votesperuser;
    if (!detaint_natural($votesperuser)) {
        ThrowUserError('prod_votes_per_user_must_be_nonnegative',
                       {votesperuser => $stored_votesperuser});
    }

    my $stored_votestoconfirm = $votestoconfirm;
    if (!detaint_natural($votestoconfirm)) {
        ThrowUserError('prod_votes_to_confirm_must_be_nonnegative',
                       {votestoconfirm => $stored_votestoconfirm});
    }

    $dbh->bz_lock_tables('products WRITE',
                         'versions READ',
                         'groups WRITE',
                         'group_control_map WRITE',
                         'profiles WRITE',
                         'milestones READ');

    my $testproduct = 
        new Bugzilla::Product({name => $product_name});
    if (lc($product_name) ne lc($product_old->name) &&
        $testproduct) {
        ThrowUserError('prod_name_already_in_use',
                       {product => $product_name});
    }

    my $milestone = new Bugzilla::Milestone($product_old->id,
                                            $defaultmilestone);
    if (!$milestone) {
        ThrowUserError('prod_must_define_defaultmilestone',
                       {product          => $product_old->name,
                        defaultmilestone => $defaultmilestone,
                        classification   => $classification_name});
    }

    $disallownew = $disallownew ? 1 : 0;
    if ($disallownew ne $product_old->disallow_new) {
        SendSQL("UPDATE products
                 SET disallownew=$disallownew
                 WHERE id = " . $product_old->id);
    }

    if ($description ne $product_old->description) {
        SendSQL("UPDATE products
                 SET description=" . SqlQuote($description) . "
                 WHERE id = " . $product_old->id);
    }

    if (Param('usetargetmilestone')
        && ($milestoneurl ne $product_old->milestone_url)) {
        SendSQL("UPDATE products
                 SET milestoneurl=" . SqlQuote($milestoneurl) . "
                 WHERE id = " . $product_old->id);
    }


    if ($votesperuser ne $product_old->votes_per_user) {
        SendSQL("UPDATE products
                 SET votesperuser=$votesperuser
                 WHERE id = " . $product_old->id);
        $checkvotes = 1;
    }


    if ($maxvotesperbug ne $product_old->max_votes_per_bug) {
        SendSQL("UPDATE products
                 SET maxvotesperbug=$maxvotesperbug
                 WHERE id = " . $product_old->id);
        $checkvotes = 1;
    }


    if ($votestoconfirm ne $product_old->votes_to_confirm) {
        SendSQL("UPDATE products
                 SET votestoconfirm=$votestoconfirm
                 WHERE id = " . $product_old->id);

        $checkvotes = 1;
    }


    if ($defaultmilestone ne $product_old->default_milestone) {
        SendSQL("UPDATE products " .
                "SET defaultmilestone = " . SqlQuote($defaultmilestone) .
                "WHERE id = " . $product_old->id);

    }

    my $qp = SqlQuote($product_name);

    if ($product_name ne $product_old->name) {
        SendSQL("UPDATE products SET name=$qp WHERE id= ".$product_old->id);

    }
    $dbh->bz_unlock_tables();
    unlink "$datadir/versioncache";

    my $product = new Bugzilla::Product({name => $product_name});

    if ($checkvotes) {
        $vars->{'checkvotes'} = 1;

        # 1. too many votes for a single user on a single bug.
        my @toomanyvotes_list = ();
        if ($maxvotesperbug < $votesperuser) {

            SendSQL("SELECT votes.who, votes.bug_id " .
                    "FROM votes, bugs " .
                    "WHERE bugs.bug_id = votes.bug_id " .
                    " AND bugs.product_id = " . $product->id .
                    " AND votes.vote_count > $maxvotesperbug");
            my @list;
            while (MoreSQLData()) {
                my ($who, $id) = (FetchSQLData());
                push(@list, [$who, $id]);
            }


            foreach my $ref (@list) {
                my ($who, $id) = (@$ref);
                RemoveVotes($id, $who, "The rules for voting on this product has changed;\nyou had too many votes for a single bug.");
                my $name = DBID_to_name($who);

                push(@toomanyvotes_list,
                     {id => $id, name => $name});
            }

        }

        $vars->{'toomanyvotes'} = \@toomanyvotes_list;


        # 2. too many total votes for a single user.
        # This part doesn't work in the general case because RemoveVotes
        # doesn't enforce votesperuser (except per-bug when it's less
        # than maxvotesperbug).  See RemoveVotes in globals.pl.

        SendSQL("SELECT votes.who, votes.vote_count FROM votes, bugs " .
                "WHERE bugs.bug_id = votes.bug_id " .
                " AND bugs.product_id = " . $product->id);
        my %counts;
        while (MoreSQLData()) {
            my ($who, $count) = (FetchSQLData());
            if (!defined $counts{$who}) {
                $counts{$who} = $count;
            } else {
                $counts{$who} += $count;
            }
        }
        my @toomanytotalvotes_list = ();
        foreach my $who (keys(%counts)) {
            if ($counts{$who} > $votesperuser) {
                SendSQL("SELECT votes.bug_id FROM votes, bugs " .
                        "WHERE bugs.bug_id = votes.bug_id " .
                        " AND bugs.product_id = " . $product->id .
                        " AND votes.who = $who");
                while (MoreSQLData()) {
                    my ($id) = FetchSQLData();
                    RemoveVotes($id, $who,
                                "The rules for voting on this product has changed; you had too many\ntotal votes, so all votes have been removed.");
                    my $name = DBID_to_name($who);

                    push(@toomanytotalvotes_list,
                         {id => $id, name => $name});
                }
            }
        }
        $vars->{'toomanytotalvotes'} = \@toomanytotalvotes_list;

        # 3. enough votes to confirm
        my $bug_list = $dbh->selectcol_arrayref("SELECT bug_id FROM bugs
                                                 WHERE product_id = ?
                                                 AND bug_status = 'UNCONFIRMED'
                                                 AND votes >= ?",
                                                 undef, ($product->id, $votestoconfirm));

        my @updated_bugs = ();
        foreach my $bug_id (@$bug_list) {
            my $confirmed = CheckIfVotedConfirmed($bug_id, $whoid);
            push (@updated_bugs, $bug_id) if $confirmed;
        }

        $vars->{'confirmedbugs'} = \@updated_bugs;
        $vars->{'changer'} = $whoid;

    }

    $vars->{'old_product'} = $product_old;
    $vars->{'product'} = $product;

    $template->process("admin/products/updated.html.tmpl", $vars)
        || ThrowTemplateError($template->error());

    exit;
}

#
# action='editgroupcontrols' -> update product group controls
#

if ($action eq 'editgroupcontrols') {
    my $product = Bugzilla::Product::check_product($product_name);
    # Display a group if it is either enabled or has bugs for this product.
    SendSQL("SELECT id, name, entry, membercontrol, othercontrol, canedit, " .
            "isactive, COUNT(bugs.bug_id) " .
            "FROM groups " .
            "LEFT JOIN group_control_map " .
            "ON group_control_map.group_id = id " .
            "AND group_control_map.product_id = " . $product->id .
            " LEFT JOIN bug_group_map " .
            "ON bug_group_map.group_id = groups.id " .
            "LEFT JOIN bugs " .
            "ON bugs.bug_id = bug_group_map.bug_id " .
            "AND bugs.product_id = " . $product->id .
            " WHERE isbuggroup != 0 " .
            "AND (isactive != 0 OR entry IS NOT NULL " .
            "OR bugs.bug_id IS NOT NULL) " .
            $dbh->sql_group_by('name', 'id, entry, membercontrol,
                                othercontrol, canedit, isactive'));
    my @groups = ();
    while (MoreSQLData()) {
        my %group = ();
        my ($groupid, $groupname, $entry, $membercontrol, $othercontrol, 
            $canedit, $isactive, $bugcount) = FetchSQLData();
        $group{'id'} = $groupid;
        $group{'name'} = $groupname;
        $group{'entry'} = $entry;
        $group{'membercontrol'} = $membercontrol;
        $group{'othercontrol'} = $othercontrol;
        $group{'canedit'} = $canedit;
        $group{'isactive'} = $isactive;
        $group{'bugcount'} = $bugcount;
        push @groups,\%group;
    }
    $vars->{'product'} = $product;

    $vars->{'groups'} = \@groups;
    
    $vars->{'const'} = {
        'CONTROLMAPNA' => CONTROLMAPNA,
        'CONTROLMAPSHOWN' => CONTROLMAPSHOWN,
        'CONTROLMAPDEFAULT' => CONTROLMAPDEFAULT,
        'CONTROLMAPMANDATORY' => CONTROLMAPMANDATORY,
    };

    $template->process("admin/products/groupcontrol/edit.html.tmpl", $vars)
        || ThrowTemplateError($template->error());
    exit;                
}


#
# No valid action found
#

ThrowUserError('no_valid_action', {field => "product"});
