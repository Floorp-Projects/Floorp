#!/usr/bin/perl -wT
# -*- Mode: perl; indent-tabs-mode: nil -*-

#
# This is a script to edit the target milestones. It is largely a copy of
# the editversions.cgi script, since the two fields were set up in a
# very similar fashion.
#
# (basically replace each occurance of 'milestone' with 'version', and
# you'll have the original script)
#
# Matt Masson <matthew@zeroknowledge.com>
#
# Contributors : Gavin Shelley <bugzilla@chimpychompy.org>
#


use strict;
use lib ".";

require "CGI.pl";
require "globals.pl";

use Bugzilla::Constants;
use Bugzilla::Config qw(:DEFAULT $datadir);

use vars qw($template $vars);

my $cgi = Bugzilla->cgi;

# TestProduct:  just returns if the specified product does exists
# CheckProduct: same check, optionally  emit an error text
# TestMilestone:  just returns if the specified product/version combination exists
# CheckMilestone: same check, optionally emit an error text

sub TestProduct ($)
{
    my $product = shift;

    trick_taint($product);

    # does the product exist?
    my $dbh = Bugzilla->dbh;
    my $sth = $dbh->prepare_cached("SELECT name
                                    FROM products
                                    WHERE name = ?");
    $sth->execute($product);

    my ($row) = $sth->fetchrow_array;

    $sth->finish;

    return $row;
}

sub CheckProduct ($)
{
    my $product = shift;

    # do we have a product?
    unless ($product) {
        &::ThrowUserError('product_not_specified');    
        exit;
    }

    # Does it exist in the DB?
    unless (TestProduct $product) {
        &::ThrowUserError('product_doesnt_exist',
                          {'product' => $product});
        exit;
    }
}

sub TestMilestone ($$)
{
    my ($product, $milestone) = @_;

    my $dbh = Bugzilla->dbh;

    # does the product exist?
    my $sth = $dbh->prepare_cached("
             SELECT products.name, value
             FROM milestones, products
             WHERE milestones.product_id = products.id
               AND products.name = ?
               AND value = ?");

    trick_taint($product);
    trick_taint($milestone);

    $sth->execute($product, $milestone);

    my ($db_milestone) = $sth->fetchrow_array();

    $sth->finish();

    return $db_milestone;
}

sub CheckMilestone ($$)
{
    my ($product, $milestone) = @_;

    # do we have the milestone and product combination?
    unless ($milestone) {
        ThrowUserError('milestone_not_specified');
        exit;
    }

    CheckProduct($product);

    unless (TestMilestone $product, $milestone) {
        ThrowUserError('milestone_not_valid',
                       {'product' => $product,
                        'milestone' => $milestone});
        exit;
    }
}

#
# Preliminary checks:
#

Bugzilla->login(LOGIN_REQUIRED);

print Bugzilla->cgi->header();

unless (UserInGroup("editcomponents")) {
    ThrowUserError('auth_cant_edit_milestones');    
    exit;
}


#
# often used variables
#
my $product = trim($cgi->param('product')     || '');
my $milestone = trim($cgi->param('milestone') || '');
my $sortkey = trim($cgi->param('sortkey')     || '0');
my $action  = trim($cgi->param('action')      || '');

#
# product = '' -> Show nice list of milestones
#

unless ($product) {

    my @products = ();

    my $dbh = Bugzilla->dbh;

    my $sth = $dbh->prepare_cached('SELECT products.name, products.description
                                    FROM products 
                                    ORDER BY products.name');

    my $data = $dbh->selectall_arrayref($sth);

    foreach my $aref (@$data) {

        my $prod = {};

        my ($name, $description) = @$aref;

        $prod->{'name'} = $name;
        $prod->{'description'} = $description;

        push(@products, $prod);
    }

    $vars->{'products'} = \@products;
    $template->process("admin/milestones/select-product.html.tmpl",
                       $vars)
      || ThrowTemplateError($template->error());

    exit;
}



#
# action='' -> Show nice list of milestones
#

unless ($action) {

    CheckProduct($product);
    my $product_id = get_product_id($product);
    my @milestones = ();

    my $dbh = Bugzilla->dbh;

    my $sth = $dbh->prepare_cached('SELECT value, sortkey
                                    FROM milestones
                                    WHERE product_id = ?
                                    ORDER BY sortkey, value');

    my $data = $dbh->selectall_arrayref($sth,
                                        undef,
                                        $product_id);

    foreach my $aref (@$data) {

        my $milestone = {};
        my ($name, $sortkey) = @$aref;

        $milestone->{'name'} = $name;
        $milestone->{'sortkey'} = $sortkey;

        push(@milestones, $milestone);
    }

    $vars->{'product'} = $product;
    $vars->{'milestones'} = \@milestones;
    $template->process("admin/milestones/list.html.tmpl",
                       $vars)
      || ThrowTemplateError($template->error());

    exit;
}




#
# action='add' -> present form for parameters for new milestone
#
# (next action will be 'new')
#

if ($action eq 'add') {

    CheckProduct($product);
    my $product_id = get_product_id($product);

    $vars->{'product'} = $product;
    $template->process("admin/milestones/create.html.tmpl",
                       $vars)
      || ThrowTemplateError($template->error());

    exit;
}



#
# action='new' -> add milestone entered in the 'action=add' screen
#

if ($action eq 'new') {

    CheckProduct($product);
    my $product_id = get_product_id($product);

    # Cleanups and valididy checks
    unless ($milestone) {
        ThrowUserError('milestone_blank_name',
                       {'name' => $milestone});
        exit;
    }

    if (length($milestone) > 20) {
        ThrowUserError('milestone_name_too_long',
                       {'name' => $milestone});
        exit;
    }

    # Need to store in case detaint_natural() clears the sortkey
    my $stored_sortkey = $sortkey;
    if (!detaint_natural($sortkey)) {
        ThrowUserError('milestone_sortkey_invalid',
                       {'name' => $milestone,
                        'sortkey' => $stored_sortkey});
        exit;
    }
    if (TestMilestone($product, $milestone)) {
        ThrowUserError('milestone_already_exists',
                       {'name' => $milestone,
                        'product' => $product});
        exit;
    }

    # Add the new milestone
    my $dbh = Bugzilla->dbh;
    trick_taint($milestone);
    $dbh->do('INSERT INTO milestones ( value, product_id, sortkey )
              VALUES ( ?, ?, ? )',
             undef,
             $milestone,
             $product_id,
             $sortkey);

    # Make versioncache flush
    unlink "$datadir/versioncache";

    $vars->{'name'} = $milestone;
    $vars->{'product'} = $product;
    $template->process("admin/milestones/created.html.tmpl",
                       $vars)
      || ThrowTemplateError($template->error());

    exit;
}




#
# action='del' -> ask if user really wants to delete
#
# (next action would be 'delete')
#

if ($action eq 'del') {

    CheckMilestone($product, $milestone);
    my $product_id = get_product_id($product);

    my $dbh = Bugzilla->dbh;

    my $sth = $dbh->prepare('SELECT count(bug_id), product_id, target_milestone
                             FROM bugs
                             GROUP BY product_id, target_milestone
                             HAVING product_id = ?
                                AND target_milestone = ?');

    trick_taint($milestone);
    $vars->{'bug_count'} = $dbh->selectrow_array($sth,
                                                 undef,
                                                 $product_id,
                                                 $milestone) || 0;

    $sth = $dbh->prepare('SELECT defaultmilestone
                          FROM products
                          WHERE id = ?');

    $vars->{'default_milestone'} = $dbh->selectrow_array($sth,
                                                         undef,
                                                         $product_id) || '';

    $vars->{'name'} = $milestone;
    $vars->{'product'} = $product;
    $template->process("admin/milestones/confirm-delete.html.tmpl",
                       $vars)
      || ThrowTemplateError($template->error());

    exit;
}



#
# action='delete' -> really delete the milestone
#

if ($action eq 'delete') {

    CheckMilestone($product,$milestone);
    my $product_id = get_product_id($product);

    my $dbh = Bugzilla->dbh;

    # lock the tables before we start to change everything:

    $dbh->do('LOCK TABLES attachments WRITE,
                          bugs WRITE,
                          bugs_activity WRITE,
                          milestones WRITE,
                          dependencies WRITE');

    # According to MySQL doc I cannot do a DELETE x.* FROM x JOIN Y,
    # so I have to iterate over bugs and delete all the indivial entries
    # in bugs_activies and attachments.

    # Detaint this here, as we may need it if deleting bugs, but will
    # definitely need it detainted whhen we actually delete the
    # milestone itself
    trick_taint($milestone);

    if (Param("allowbugdeletion")) {

        my $deleted_bug_count = 0;

        my $sth = $dbh->prepare_cached('SELECT bug_id
                                        FROM bugs
                                        WHERE product_id = ?
                                        AND target_milestone = ?');

        my $data = $dbh->selectall_arrayref($sth,
                                            undef,
                                            $product_id,
                                            $milestone);

        foreach my $aref (@$data) {

            my ($bugid) = @$aref;

            $dbh->do('DELETE FROM attachments WHERE bug_id = ?',
                     undef,
                     $bugid);
            $dbh->do('DELETE FROM bugs_activity WHERE bug_id = ?',
                     undef,
                     $bugid);
            $dbh->do('DELETE FROM dependencies WHERE blocked = ?',
                     undef,
                     $bugid);

            $deleted_bug_count++;
        }

        $vars->{'deleted_bug_count'} = $deleted_bug_count;

        # Deleting the rest is easier:

        $dbh->do('DELETE FROM bugs
                  WHERE product_id = ?
                  AND target_milestone = ?',
                 undef,
                 $product_id,
                 $milestone);
    }

    $dbh->do('DELETE FROM milestones
              WHERE product_id = ?
              AND value = ?',
             undef,
             $product_id,
             $milestone);

    $dbh->do('UNLOCK TABLES');

    unlink "$datadir/versioncache";


    $vars->{'name'} = $milestone;
    $vars->{'product'} = $product;
    $template->process("admin/milestones/deleted.html.tmpl",
                       $vars)
      || ThrowTemplateError($template->error());
    exit;
}



#
# action='edit' -> present the edit milestone form
#
# (next action would be 'update')
#

if ($action eq 'edit') {

    CheckMilestone($product, $milestone);
    my $product_id = get_product_id($product);

    my $dbh = Bugzilla->dbh;

    my $sth = $dbh->prepare_cached('SELECT sortkey
                                    FROM milestones
                                    WHERE product_id = ?
                                    AND value = ?');

    trick_taint($milestone);

    $vars->{'sortkey'} = $dbh->selectrow_array($sth,
                                               undef,
                                               $product_id,
                                               $milestone) || 0;

    $vars->{'name'} = $milestone;
    $vars->{'product'} = $product;

    $template->process("admin/milestones/edit.html.tmpl",
                       $vars)
      || ThrowTemplateError($template->error());

    exit;
}



#
# action='update' -> update the milestone
#

if ($action eq 'update') {

    my $milestoneold = trim($cgi->param('milestoneold') || '');
    my $sortkeyold = trim($cgi->param('sortkeyold')     || '0');

    CheckMilestone($product, $milestoneold);
    my $product_id = get_product_id($product);

    if (length($milestone) > 20) {
        ThrowUserError('milestone_name_too_long',
                       {'name' => $milestone});
        exit;
    }

    my $dbh = Bugzilla->dbh;

    $dbh->do("LOCK TABLES bugs WRITE,
                          milestones WRITE,
                          products WRITE");

    # Need to store because detaint_natural() will delete this if
    # invalid
    my $stored_sortkey = $sortkey;
    if ($sortkey != $sortkeyold) {
        if (!detaint_natural($sortkey)) {

            $dbh->do('UNLOCK TABLES'); 
            ThrowUserError('milestone_sortkey_invalid',
                           {'name' => $milestone,
                            'sortkey' => $stored_sortkey});

            exit;
        }

        trick_taint($milestoneold);

        $dbh->do('UPDATE milestones SET sortkey = ?
                  WHERE product_id = ?
                  AND value = ?',
                 undef,
                 $sortkey,
                 $product_id,
                 $milestoneold);

        unlink "$datadir/versioncache";
        $vars->{'updated_sortkey'} = 1;
        $vars->{'sortkey'} = $sortkey;
    }

    if ($milestone ne $milestoneold) {
        unless ($milestone) {
            $dbh->do('UNLOCK TABLES'); 
            ThrowUserError('milestone_blank_name');
            exit;
        }
        if (TestMilestone($product, $milestone)) {
            $dbh->do('UNLOCK TABLES'); 
            ThrowUserError('milestone_already_exists',
                           {'name' => $milestone,
                            'product' => $product});
            exit;
        }

        trick_taint($milestone);
        trick_taint($milestoneold);

        $dbh->do('UPDATE bugs
                  SET target_milestone = ?,
                  delta_ts = delta_ts
                  WHERE target_milestone = ?
                  AND product_id = ?',
                 undef,
                 $milestone,
                 $milestoneold,
                 $product_id);

        $dbh->do("UPDATE milestones
                  SET value = ?
                  WHERE product_id = ?
                  AND value = ?",
                 undef,
                 $milestone,
                 $product_id,
                 $milestoneold);

        $dbh->do("UPDATE products
                  SET defaultmilestone = ?
                  WHERE id = ?
                  AND defaultmilestone = ?",
                 undef,
                 $milestone,
                 $product_id,
                 $milestoneold);

        unlink "$datadir/versioncache";

        $vars->{'updated_name'} = 1;
    }

    $dbh->do('UNLOCK TABLES'); 

    $vars->{'name'} = $milestone;
    $vars->{'product'} = $product;
    $template->process("admin/milestones/updated.html.tmpl",
                       $vars)
      || ThrowTemplateError($template->error());

    exit;
}


#
# No valid action found
#
ThrowUserError('milestone_no_action');
