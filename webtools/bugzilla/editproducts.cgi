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

# Shut up misguided -w warnings about "used only once".  "use vars" just
# doesn't work for me.
use vars qw(@legal_bug_status @legal_resolution);

# TestProduct:  just returns if the specified product does exists
# CheckProduct: same check, optionally  emit an error text

sub TestProduct
{
    my $prod = shift;

    # does the product exist?
    SendSQL("SELECT name
             FROM products
             WHERE name=" . SqlQuote($prod));
    return FetchOneColumn();
}

sub CheckProduct
{
    my $prod = shift;

    # do we have a product?
    unless ($prod) {
        ThrowUserError('product_not_specified');
    }

    unless (TestProduct($prod)) {
        ThrowUserError('product_doesnt_exist',
                       {'product' => $prod});
    }
}


# TestClassification:  just returns if the specified classification does exists
# CheckClassification: same check, optionally  emit an error text

sub TestClassification
{
    my $cl = shift;

    # does the classification exist?
    SendSQL("SELECT name
             FROM classifications
             WHERE name=" . SqlQuote($cl));
    return FetchOneColumn();
}

sub CheckClassification
{
    my $cl = shift;

    # do we have a classification?
    unless ($cl) {
        ThrowUserError('classification_not_specified');    
    }

    unless (TestClassification($cl)) {
        ThrowUserError('classification_doesnt_exist',
                       {'name' => $cl});
    }
}

sub CheckClassificationProduct
{
    my ($cl, $prod) = @_;
    my $dbh = Bugzilla->dbh;
    
    CheckClassification($cl);

    trick_taint($prod);
    trick_taint($cl);

    my ($res) = $dbh->selectrow_array(q{
        SELECT products.name
        FROM products
        INNER JOIN classifications
          ON products.classification_id = classifications.id
        WHERE products.name = ? AND classifications.name = ?},
        undef, ($prod, $cl));

    unless ($res) {
        ThrowUserError('classification_doesnt_exist_for_product',
                       { product => $prod, classification => $cl });
    }
}

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
my $classification = trim($cgi->param('classification') || '');
my $product = trim($cgi->param('product') || '');
my $action  = trim($cgi->param('action')  || '');
my $dbh = Bugzilla->dbh;

#
# product = '' -> Show nice list of classifications (if
# classifications enabled)
#

if (Param('useclassification') 
    && !$classification
    && !$product)
{
    my $query = 
        "SELECT classifications.name, classifications.description,
                COUNT(classification_id) AS product_count
         FROM classifications
         LEFT JOIN products
              ON classifications.id = products.classification_id " .
              $dbh->sql_group_by('classifications.id',
                                 'classifications.name,
                                  classifications.description') . "
         ORDER BY name";

    $vars->{'classifications'} = $dbh->selectall_arrayref($query,
                                                          {'Slice' => {}});

    $template->process("admin/products/list-classifications.html.tmpl",
                       $vars)
        || ThrowTemplateError($template->error());

    exit;
}


#
# action = '' -> Show a nice list of products, unless a product
#                is already specified (then edit it)
#

if (!$action && !$product) {

    if (Param('useclassification')) {
        CheckClassification($classification);
    }

    my @execute_params = ();
    my @products = ();

    my $query = "SELECT products.name,
                        COALESCE(products.description,'') AS description, 
                        disallownew = 0 AS status,
                        votesperuser,  maxvotesperbug, votestoconfirm,
                        COUNT(bug_id) AS bug_count
                 FROM products";

    if (Param('useclassification')) {
        $query .= " INNER JOIN classifications " .
                  "ON classifications.id = products.classification_id";
    }

    $query .= " LEFT JOIN bugs ON products.id = bugs.product_id";

    if (Param('useclassification')) {
        $query .= " WHERE classifications.name = ? ";

        # trick_taint is OK because we use this in a placeholder in a SELECT
        trick_taint($classification);

        push(@execute_params,
             $classification);
    }

    $query .= " " . $dbh->sql_group_by('products.name',
                                       'products.description, disallownew,
                                        votesperuser, maxvotesperbug,
                                        votestoconfirm');
    $query .= " ORDER BY products.name";

    $vars->{'products'} = $dbh->selectall_arrayref($query,
                                                   {'Slice' => {}},
                                                   @execute_params);

    $vars->{'classification'} = $classification;
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
        CheckClassification($classification);
    }
    $vars->{'classification'} = $classification;
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
        CheckClassification($classification);
        $classification_id = get_classification_id($classification);
        $vars->{'classification'} = $classification;
    }

    unless ($product) {
        ThrowUserError("product_blank_name");  
    }

    my $existing_product = TestProduct($product);

    if ($existing_product) {

        # Check for exact case sensitive match:
        if ($existing_product eq $product) {
            ThrowUserError("prod_name_already_in_use",
                           {'product' => $product});
        }

        # Next check for a case-insensitive match:
        if (lc($existing_product) eq lc($product)) {
            ThrowUserError("prod_name_diff_in_case",
                           {'product' => $product,
                            'existing_product' => $existing_product}); 
        }
    }

    my $version = trim($cgi->param('version') || '');

    if ($version eq '') {
        ThrowUserError("product_must_have_version",
                       {'product' => $product});
    }

    my $description  = trim($cgi->param('description')  || '');
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
            SqlQuote($product) . "," .
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
    my $product_id = $dbh->bz_last_key('products', 'id');

    SendSQL("INSERT INTO versions ( " .
          "value, product_id" .
          " ) VALUES ( " .
          SqlQuote($version) . "," .
          $product_id . ")" );

    SendSQL("INSERT INTO milestones (product_id, value) VALUES (" .
            $product_id . ", " . SqlQuote($defaultmilestone) . ")");

    # If we're using bug groups, then we need to create a group for this
    # product as well.  -JMR, 2/16/00
    if (Param("makeproductgroups")) {
        # Next we insert into the groups table
        my $productgroup = $product;
        while (GroupExists($productgroup)) {
            $productgroup .= '_';
        }
        SendSQL("INSERT INTO groups " .
                "(name, description, isbuggroup, last_changed) " .
                "VALUES (" .
                SqlQuote($productgroup) . ", " .
                SqlQuote("Access to bugs in the $product product") . ", 1, NOW())");
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
                "($gid, $product_id, " . Param("useentrygroupdefault") .
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
        trick_taint($product);
    
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
            my $series = new Bugzilla::Series(undef, $product, 
                            scalar $cgi->param('subcategory'),
                            $sdata->[0], $::userid, 1,
                            $sdata->[1] . "&product=" . url_quote($product), 1);
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
    
    if (!$product) {
        ThrowUserError('product_not_specified');
    }

    my $product_id = get_product_id($product);
    $product_id || ThrowUserError('product_doesnt_exist',
                                  {product => $product});

    my $classification_id = 1;

    if (Param('useclassification')) {
        CheckClassificationProduct($classification, $product);
        $classification_id = get_classification_id($classification);
        $vars->{'classification'} = $classification;
    }

    # Extract some data about the product
    my $query = q{SELECT classifications.description,
                         products.description,
                         products.milestoneurl,
                         products.disallownew
                  FROM products
                  INNER JOIN classifications
                    ON products.classification_id = classifications.id
                  WHERE products.id = ?};

    my ($class_description,
        $prod_description,
        $milestoneurl,
        $disallownew) = $dbh->selectrow_array($query, undef,
                                              $product_id);

    $vars->{'class_description'} = $class_description;
    $vars->{'product_id'}        = $product_id;
    $vars->{'prod_description'}  = $prod_description;
    $vars->{'milestoneurl'}      = $milestoneurl;
    $vars->{'disallownew'}       = $disallownew;
    $vars->{'product_name'}      = $product;

    $vars->{'components'} = $dbh->selectall_arrayref(q{
        SELECT name, description FROM components
        WHERE product_id = ? ORDER BY name}, {'Slice' => {}},
        $product_id);

    $vars->{'versions'} = $dbh->selectcol_arrayref(q{
            SELECT value FROM versions
            WHERE product_id = ? ORDER BY value}, undef,
            $product_id);

    # Adding listing for associated target milestones - 
    # matthew@zeroknowledge.com
    if (Param('usetargetmilestone')) {
        $vars->{'milestones'} = $dbh->selectcol_arrayref(q{
            SELECT value FROM milestones
            WHERE product_id = ?
            ORDER BY sortkey, value}, undef, $product_id);
    }

    ($vars->{'bug_count'}) = $dbh->selectrow_array(q{
        SELECT COUNT(*) FROM bugs WHERE product_id = ?},
        undef, $product_id) || 0;
 
    $template->process("admin/products/confirm-delete.html.tmpl", $vars)
        || ThrowTemplateError($template->error());
    exit;
}

#
# action='delete' -> really delete the product
#

if ($action eq 'delete') {
    
    if (!$product) {
        ThrowUserError('product_not_specified');
    }

    my $product_id = get_product_id($product);
    $product_id || ThrowUserError('product_doesnt_exist',
                                  {product => $product});

    $vars->{'product'} = $product;
    $vars->{'classification'} = $classification;

    my $bug_ids = $dbh->selectcol_arrayref(q{
        SELECT bug_id FROM bugs
        WHERE product_id = ?}, undef, $product_id);

    my $nb_bugs = scalar(@$bug_ids);
    if ($nb_bugs) {
        if (Param("allowbugdeletion")) {
            foreach my $bug_id (@$bug_ids) {
                my $bug = new Bugzilla::Bug($bug_id, $whoid);
                $bug->remove_from_db();
            }
        }
        else {
            ThrowUserError("product_has_bugs", { nb => $nb_bugs });
        }
        $vars->{'nb_bugs'} = $nb_bugs;
    }

    $dbh->bz_lock_tables('products WRITE', 'components WRITE',
                         'versions WRITE', 'milestones WRITE',
                         'group_control_map WRITE',
                         'flaginclusions WRITE', 'flagexclusions WRITE');

    $dbh->do("DELETE FROM components WHERE product_id = ?",
             undef, $product_id);

    $dbh->do("DELETE FROM versions WHERE product_id = ?",
             undef, $product_id);

    $dbh->do("DELETE FROM milestones WHERE product_id = ?",
             undef, $product_id);

    $dbh->do("DELETE FROM group_control_map WHERE product_id = ?",
             undef, $product_id);

    $dbh->do("DELETE FROM flaginclusions WHERE product_id = ?",
             undef, $product_id);
             
    $dbh->do("DELETE FROM flagexclusions WHERE product_id = ?",
             undef, $product_id);
             
    $dbh->do("DELETE FROM products WHERE id = ?",
             undef, $product_id);

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

if ($action eq 'edit' || (!$action && $product)) {
    CheckProduct($product);
    trick_taint($product);
    my $product_id = get_product_id($product); 
    my $classification_id=1;
    if (Param('useclassification')) {
        # If a product has been given with no classification associated
        # with it, take this information from the DB
        if ($classification) {
            CheckClassificationProduct($classification, $product);
        } else {
            $classification =
                $dbh->selectrow_array("SELECT classifications.name
                                       FROM products, classifications
                                       WHERE products.name = ?
                                       AND classifications.id = products.classification_id",
                                       undef, $product);
        }
        $classification_id = get_classification_id($classification);
    }
    
    $vars->{'classification'} = $classification;

    # get data of product
    $vars->{'product'} = $dbh->selectrow_hashref(qq{
            SELECT id, name, classification_id, description,
                   milestoneurl, disallownew, votesperuser,
                   maxvotesperbug, votestoconfirm, defaultmilestone
              FROM products
             WHERE id = ?}, undef, $product_id);
    
   
    $vars->{'components'} = $dbh->selectall_arrayref(qq{
            SELECT name, description
              FROM components
             WHERE product_id = ?
          ORDER BY name}, {'Slice' => {}},$product_id); 
    
    
    $vars->{'versions'} = $dbh->selectcol_arrayref(q{
        SELECT value FROM versions
         WHERE product_id = ?
      ORDER BY value}, undef, $product_id);

    if (Param('usetargetmilestone')) {
        $vars->{'milestones'} = $dbh->selectcol_arrayref(q{
                                    SELECT value 
                                      FROM milestones
                                     WHERE product_id = ?
                                  ORDER BY sortkey, value}, 
                                     undef, $product_id);
    }
        
    my $query = qq{SELECT
                       groups.id, groups.name, groups.isactive,
                       group_control_map.entry,
                       group_control_map.membercontrol,
                       group_control_map.othercontrol,
                       group_control_map.canedit
                  FROM groups
                 INNER JOIN group_control_map
                        ON groups.id = group_control_map.group_id
                  WHERE group_control_map.product_id = ?
                  AND   groups.isbuggroup != 0
                  ORDER BY groups.name};
    my $groups = $dbh->selectall_arrayref($query, {'Slice' => {}},
                                          $product_id);
    
    # Convert Group Controls(membercontrol and othercontrol) from 
    # integer to string to display Membercontrol/Othercontrol names
    # at the template. <gabriel@async.com.br>
    my $constants = {
        (CONTROLMAPNA) => 'NA',
        (CONTROLMAPSHOWN) => 'Shown',
        (CONTROLMAPDEFAULT) => 'Default',
        (CONTROLMAPMANDATORY) => 'Mandatory'};

    foreach my $group (@$groups) {
        $group->{'membercontrol'} =
            $constants->{$group->{'membercontrol'}};
        $group->{'othercontrol'} =
            $constants->{$group->{'othercontrol'}};
    }
   
    $vars->{'groups'} = $groups;
    
    $vars->{'bug_count'} = $dbh->selectrow_array(qq{
            SELECT COUNT(*) FROM bugs
            WHERE product_id = ?}, undef, $product_id);
    
    $template->process("admin/products/edit.html.tmpl", $vars)
        || ThrowTemplateError($template->error());

    exit;
}

#
# action='updategroupcontrols' -> update the product
#

if ($action eq 'updategroupcontrols') {
    my $product_id = get_product_id($product);
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
                     AND bugs.product_id = $product_id " .
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
                        AND bugs.product_id = $product_id
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
            "ON group_control_map.group_id = id AND product_id = $product_id " .
            "WHERE isbuggroup != 0 AND isactive != 0");
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
                    "($groupid, $product_id, $newentry, " .
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
                    "AND product_id = $product_id");
            PopGlobalSQLState();
        }

        if (($newentry == 0) && ($newmembercontrol == 0)
          && ($newothercontrol == 0) && ($newcanedit == 0)) {
            PushGlobalSQLState();
            SendSQL("DELETE FROM group_control_map " .
                    "WHERE group_id = $groupid " .
                    "AND product_id = $product_id");
            PopGlobalSQLState();
        }
    }

    my @removed_na;
    foreach my $groupid (@now_na) {
        my $count = 0;
        SendSQL("SELECT bugs.bug_id, 
                 (lastdiffed >= delta_ts)
                 FROM bugs, bug_group_map
                 WHERE group_id = $groupid
                 AND bug_group_map.bug_id = bugs.bug_id
                 AND bugs.product_id = $product_id
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
                 (lastdiffed >= delta_ts)
                 FROM bugs
                 LEFT JOIN bug_group_map
                 ON bug_group_map.bug_id = bugs.bug_id
                 AND group_id = $groupid
                 WHERE bugs.product_id = $product_id
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

    $vars->{'classification'} = $classification;

    $vars->{'product'} = $product;

    $template->process("admin/products/groupcontrol/updated.html.tmpl", $vars)
        || ThrowTemplateError($template->error());
    exit;
}

#
# action='update' -> update the product
#
if ($action eq 'update') {

    $vars->{'classification'} = $classification;

    my $productold          = trim($cgi->param('productold')          || '');
    my $description         = trim($cgi->param('description')         || '');
    my $descriptionold      = trim($cgi->param('descriptionold')      || '');
    my $disallownew         = trim($cgi->param('disallownew')         || '');
    my $disallownewold      = trim($cgi->param('disallownewold')      || '');
    my $milestoneurl        = trim($cgi->param('milestoneurl')        || '');
    my $milestoneurlold     = trim($cgi->param('milestoneurlold')     || '');
    my $votesperuser        = trim($cgi->param('votesperuser')        || 0);
    my $votesperuserold     = trim($cgi->param('votesperuserold')     || 0);
    my $maxvotesperbug      = trim($cgi->param('maxvotesperbug')      || 0);
    my $maxvotesperbugold   = trim($cgi->param('maxvotesperbugold')   || 0);
    my $votestoconfirm      = trim($cgi->param('votestoconfirm')      || 0);
    my $votestoconfirmold   = trim($cgi->param('votestoconfirmold')   || 0);
    my $defaultmilestone    = trim($cgi->param('defaultmilestone')    || '---');
    my $defaultmilestoneold = trim($cgi->param('defaultmilestoneold') || '---');

    my $checkvotes = 0;

    CheckProduct($productold);
    my $product_id = get_product_id($productold);

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

    # Note that we got the $product_id using $productold above so it will
    # remain static even after we rename the product in the database.

    $dbh->bz_lock_tables('products WRITE',
                         'versions READ',
                         'groups WRITE',
                         'group_control_map WRITE',
                         'profiles WRITE',
                         'milestones READ');

    if ($disallownew ne $disallownewold) {
        $disallownew = $disallownew ? 1 : 0;
        SendSQL("UPDATE products
                 SET disallownew=$disallownew
                 WHERE id=$product_id");
        $vars->{'updated_bugsubmitstatus'} = 1;
        $vars->{'new_bugsubmitstatus'} = $disallownew;        
    }

    if ($description ne $descriptionold) {
        unless ($description) {
            ThrowUserError('prod_cant_delete_description',
                           {product => $productold});
        }
        SendSQL("UPDATE products
                 SET description=" . SqlQuote($description) . "
                 WHERE id=$product_id");
        $vars->{'updated_description'} = 1;
        $vars->{'old_description'} = $descriptionold;        
        $vars->{'new_description'} = $description;        
    }

    if (Param('usetargetmilestone')
        && ($milestoneurl ne $milestoneurlold)) {
        SendSQL("UPDATE products
                 SET milestoneurl=" . SqlQuote($milestoneurl) . "
                 WHERE id=$product_id");
        $vars->{'updated_milestoneurl'} = 1;
        $vars->{'old_milestoneurl'} = $milestoneurlold;        
        $vars->{'new_milestoneurl'} = $milestoneurl;        
    }


    if ($votesperuser ne $votesperuserold) {
        SendSQL("UPDATE products
                 SET votesperuser=$votesperuser
                 WHERE id=$product_id");
        $vars->{'updated_votesperuser'} = 1;
        $vars->{'old_votesperuser'} = $votesperuserold;        
        $vars->{'new_votesperuser'} = $votesperuser;        

        $checkvotes = 1;
    }


    if ($maxvotesperbug ne $maxvotesperbugold) {
        SendSQL("UPDATE products
                 SET maxvotesperbug=$maxvotesperbug
                 WHERE id=$product_id");
        $vars->{'updated_maxvotesperbug'} = 1;
        $vars->{'old_maxvotesperbug'} = $maxvotesperbugold;        
        $vars->{'new_maxvotesperbug'} = $maxvotesperbug;        

        $checkvotes = 1;
    }


    if ($votestoconfirm ne $votestoconfirmold) {
        SendSQL("UPDATE products
                 SET votestoconfirm=$votestoconfirm
                 WHERE id=$product_id");

        $vars->{'updated_votestoconfirm'} = 1;
        $vars->{'old_votestoconfirm'} = $votestoconfirmold;
        $vars->{'new_votestoconfirm'} = $votestoconfirm;

        $checkvotes = 1;
    }


    if ($defaultmilestone ne $defaultmilestoneold) {
        SendSQL("SELECT value FROM milestones " .
                "WHERE value = " . SqlQuote($defaultmilestone) .
                "  AND product_id = $product_id");
        if (!FetchOneColumn()) {
            ThrowUserError('prod_must_define_defaultmilestone',
                           {product          => $productold,
                            defaultmilestone => $defaultmilestone,
                            classification   => $classification});
        }
        SendSQL("UPDATE products " .
                "SET defaultmilestone = " . SqlQuote($defaultmilestone) .
                "WHERE id=$product_id");

        $vars->{'updated_defaultmilestone'} = 1;
        $vars->{'old_defaultmilestone'} = $defaultmilestoneold;
        $vars->{'new_defaultmilestone'} = $defaultmilestone;
    }

    my $qp = SqlQuote($product);
    my $qpold = SqlQuote($productold);

    if ($product ne $productold) {
        unless ($product) {
            ThrowUserError('prod_cant_delete_name',
                           {product => $productold});
        }

        if (lc($product) ne lc($productold) &&
            TestProduct($product)) {
            ThrowUserError('prod_name_already_in_use',
                           {product => $product});
        }

        SendSQL("UPDATE products SET name=$qp WHERE id=$product_id");

        $vars->{'updated_product'} = 1;
        $vars->{'old_product'} = $productold;
        $vars->{'new_product'} = $product;

    }
    $dbh->bz_unlock_tables();
    unlink "$datadir/versioncache";

    if ($checkvotes) {
        $vars->{'checkvotes'} = 1;

        # 1. too many votes for a single user on a single bug.
        my @toomanyvotes_list = ();
        if ($maxvotesperbug < $votesperuser) {

            SendSQL("SELECT votes.who, votes.bug_id " .
                    "FROM votes, bugs " .
                    "WHERE bugs.bug_id = votes.bug_id " .
                    " AND bugs.product_id = $product_id " .
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
                " AND bugs.product_id = $product_id");
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
                        " AND bugs.product_id = $product_id " .
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
                                                 undef, ($product_id, $votestoconfirm));

        my @updated_bugs = ();
        foreach my $bug_id (@$bug_list) {
            my $confirmed = CheckIfVotedConfirmed($bug_id, $whoid);
            push (@updated_bugs, $bug_id) if $confirmed;
        }

        $vars->{'confirmedbugs'} = \@updated_bugs;
        $vars->{'changer'} = $whoid;

    }

    $vars->{'name'} = $product;
    $template->process("admin/products/updated.html.tmpl", $vars)
        || ThrowTemplateError($template->error());

    exit;
}

#
# action='editgroupcontrols' -> update product group controls
#

if ($action eq 'editgroupcontrols') {
    my $product_id = get_product_id($product);
    $product_id
      || ThrowUserError("invalid_product_name", { product => $product });
    # Display a group if it is either enabled or has bugs for this product.
    SendSQL("SELECT id, name, entry, membercontrol, othercontrol, canedit, " .
            "isactive, COUNT(bugs.bug_id) " .
            "FROM groups " .
            "LEFT JOIN group_control_map " .
            "ON group_control_map.group_id = id " .
            "AND group_control_map.product_id = $product_id " .
            "LEFT JOIN bug_group_map " .
            "ON bug_group_map.group_id = groups.id " .
            "LEFT JOIN bugs " .
            "ON bugs.bug_id = bug_group_map.bug_id " .
            "AND bugs.product_id = $product_id " .
            "WHERE isbuggroup != 0 " .
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
    $vars->{'classification'} = $classification;
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
