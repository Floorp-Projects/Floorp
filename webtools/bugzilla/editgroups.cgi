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
# The Original Code is the Bugzilla Bug Tracking System.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Dave Miller <justdave@syndicomm.com>
#                 Joel Peshkin <bugreport@peshkin.net>
#                 Jacob Steenhagen <jake@bugzilla.org>
#                 Vlad Dascalu <jocuri@softhome.net>

# Code derived from editowners.cgi and editusers.cgi

use strict;
use lib ".";

use Bugzilla;
use Bugzilla::Constants;
require "CGI.pl";

my $cgi = Bugzilla->cgi;

use vars qw($template $vars);

Bugzilla->login(LOGIN_REQUIRED);

print Bugzilla->cgi->header();

ThrowUserError("auth_cant_edit_groups") unless UserInGroup("creategroups");

my $action = trim($cgi->param('action') || '');

# RederiveRegexp: update user_group_map with regexp-based grants
sub RederiveRegexp ($$)
{
    my $regexp = shift;
    my $gid = shift;
    my $dbh = Bugzilla->dbh;
    my $sth = $dbh->prepare("SELECT userid, login_name FROM profiles");
    my $sthadd = $dbh->prepare("INSERT IGNORE INTO user_group_map
                               (user_id, group_id, grant_type, isbless)
                               VALUES (?, ?, ?, 0)");
    my $sthdel = $dbh->prepare("DELETE FROM user_group_map
                                WHERE user_id = ? AND group_id = ?
                                AND grant_type = ? and isbless = 0");
    $sth->execute();
    while (my ($uid, $login) = $sth->fetchrow_array()) {
        if (($regexp =~ /\S+/) && ($login =~ m/$regexp/i))
        {
            $sthadd->execute($uid, $gid, GRANT_REGEXP);
        } else {
            $sthdel->execute($uid, $gid, GRANT_REGEXP);
        }
    }
}

# TestGroup: check if the group name exists
sub TestGroup ($)
{
    my $group = shift;

    # does the group exist?
    SendSQL("SELECT name
             FROM groups
             WHERE name=" . SqlQuote($group));
    return FetchOneColumn();
}

#
# action='' -> No action specified, get a list.
#

unless ($action) {
    my @groups;

    SendSQL("SELECT id,name,description,userregexp,isactive,isbuggroup " .
            "FROM groups " .
            "ORDER BY isbuggroup, name");

    while (MoreSQLData()) {
        my ($id, $name, $description, $regexp, $isactive, $isbuggroup)
            = FetchSQLData();
        my $group = {};
        $group->{'id'}          = $id;
        $group->{'name'}        = $name;
        $group->{'description'} = $description;
        $group->{'regexp'}      = $regexp;
        $group->{'isactive'}    = $isactive;
        $group->{'isbuggroup'}  = $isbuggroup;

        push(@groups, $group);
    }

    $vars->{'groups'} = \@groups;
    
    print Bugzilla->cgi->header();
    $template->process("admin/groups/list.html.tmpl", $vars)
      || ThrowTemplateError($template->error());

    exit;
}

#
# action='changeform' -> present form for altering an existing group
#
# (next action will be 'postchanges')
#

if ($action eq 'changeform') {
    my $gid = trim($cgi->param('group') || '');
    ThrowUserError("group_not_specified") unless ($gid);
    detaint_natural($gid);

    SendSQL("SELECT id, name, description, userregexp, isactive, isbuggroup
             FROM groups WHERE id = $gid");
    my ($group_id, $name, $description, $rexp, $isactive, $isbuggroup) 
        = FetchSQLData();

    # For each group, we use left joins to establish the existence of
    # a record making that group a member of this group
    # and the existence of a record permitting that group to bless
    # this one

    my @groups;
    SendSQL("SELECT groups.id, groups.name, groups.description," .
             " group_group_map.member_id IS NOT NULL," .
             " B.member_id IS NOT NULL" .
             " FROM groups" .
             " LEFT JOIN group_group_map" .
             " ON group_group_map.member_id = groups.id" .
             " AND group_group_map.grantor_id = $group_id" .
             " AND group_group_map.isbless = 0" .
             " LEFT JOIN group_group_map as B" .
             " ON B.member_id = groups.id" .
             " AND B.grantor_id = $group_id" .
             " AND B.isbless = 1" .
             " WHERE groups.id != $group_id ORDER by name");

    while (MoreSQLData()) {
        my ($grpid, $grpnam, $grpdesc, $grpmember, $blessmember) = FetchSQLData();

        my $group = {};
        $group->{'grpid'}       = $grpid;
        $group->{'grpnam'}      = $grpnam;
        $group->{'grpdesc'}     = $grpdesc;
        $group->{'grpmember'}   = $grpmember;
        $group->{'blessmember'} = $blessmember;
        push(@groups, $group);
    }

    $vars->{'group_id'}    = $group_id;
    $vars->{'name'}        = $name;
    $vars->{'description'} = $description;
    $vars->{'rexp'}        = $rexp;
    $vars->{'isactive'}    = $isactive;
    $vars->{'isbuggroup'}  = $isbuggroup;
    $vars->{'groups'}      = \@groups;

    print Bugzilla->cgi->header();
    $template->process("admin/groups/edit.html.tmpl", $vars)
      || ThrowTemplateError($template->error());

    exit;
}

#
# action='add' -> present form for parameters for new group
#
# (next action will be 'new')
#

if ($action eq 'add') {
    print Bugzilla->cgi->header();
    $template->process("admin/groups/create.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    
    exit;
}



#
# action='new' -> add group entered in the 'action=add' screen
#

if ($action eq 'new') {
    # Cleanups and valididy checks
    my $name = trim($cgi->param('name') || '');
    my $desc = trim($cgi->param('desc') || '');
    my $regexp = trim($cgi->param('regexp') || '');
    # convert an undefined value in the inactive field to zero
    # (this occurs when the inactive checkbox is not checked
    # and the browser does not send the field to the server)
    my $isactive = $cgi->param('isactive') ? 1 : 0;

    # At this point $isactive is either 0 or 1 so we can mark it safe
    trick_taint($isactive);

    ThrowUserError("empty_group_name") unless $name;
    ThrowUserError("empty_group_description") unless $desc;

    if (TestGroup($name)) {
        ThrowUserError("group_exists", { name => $name });
    }

    ThrowUserError("invalid_regexp") unless (eval {qr/$regexp/});

    # We use SqlQuote and FILTER html on name, description and regexp.
    # So they are safe to be detaint
    trick_taint($name);
    trick_taint($desc);
    trick_taint($regexp);

    # Add the new group
    SendSQL("INSERT INTO groups ( " .
            "name, description, isbuggroup, userregexp, isactive, last_changed " .
            " ) VALUES ( " .
            SqlQuote($name) . ", " .
            SqlQuote($desc) . ", " .
            "1," .
            SqlQuote($regexp) . ", " . 
            $isactive . ", NOW())" );
    SendSQL("SELECT last_insert_id()");
    my $gid = FetchOneColumn();
    my $admin = GroupNameToId('admin');
    SendSQL("INSERT INTO group_group_map (member_id, grantor_id, isbless)
             VALUES ($admin, $gid, 0)");
    SendSQL("INSERT INTO group_group_map (member_id, grantor_id, isbless)
             VALUES ($admin, $gid, 1)");
    # Permit all existing products to use the new group if makeproductgroups.
    if ($cgi->param('insertnew')) {
        SendSQL("INSERT INTO group_control_map " .
                "(group_id, product_id, entry, membercontrol, " .
                "othercontrol, canedit) " .
                "SELECT $gid, products.id, 0, " .
                CONTROLMAPSHOWN . ", " .
                CONTROLMAPNA . ", 0 " .
                "FROM products");
    }
    RederiveRegexp($regexp, $gid);

    print Bugzilla->cgi->header();
    $template->process("admin/groups/created.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

#
# action='del' -> ask if user really wants to delete
#
# (next action would be 'delete')
#

if ($action eq 'del') {
    my $gid = trim($cgi->param('group') || '');
    ThrowUserError("group_not_specified") unless ($gid);
    detaint_natural($gid);

    SendSQL("SELECT id FROM groups WHERE id=$gid");
    ThrowUserError("invalid_group_ID") unless FetchOneColumn();

    SendSQL("SELECT name,description " .
            "FROM groups " .
            "WHERE id = $gid");

    my ($name, $desc) = FetchSQLData();

    my $hasusers = 0;
    SendSQL("SELECT user_id FROM user_group_map 
             WHERE group_id = $gid AND isbless = 0");
    if (FetchOneColumn()) {
        $hasusers = 1;
    }

    my $hasbugs = 0;
    my $buglist = "";
    SendSQL("SELECT bug_id FROM bug_group_map WHERE group_id = $gid");

    if (MoreSQLData()) {
        $hasbugs = 1;
        my $buglist = "0";

        while (MoreSQLData()) {
            my ($bug) = FetchSQLData();
            $buglist .= "," . $bug;
        }
    }

    my $hasproduct = 0;
    SendSQL("SELECT name FROM products WHERE name=" . SqlQuote($name));
    if (MoreSQLData()) {
        $hasproduct = 1;
    }

    $vars->{'gid'}         = $gid;
    $vars->{'name'}        = $name;
    $vars->{'description'} = $desc;
    $vars->{'hasusers'}    = $hasusers;
    $vars->{'hasbugs'}     = $hasbugs;
    $vars->{'hasproduct'}  = $hasproduct;
    $vars->{'buglist'}     = $buglist;

    print Bugzilla->cgi->header();
    $template->process("admin/groups/delete.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    
    exit;
}

#
# action='delete' -> really delete the group
#

if ($action eq 'delete') {
    my $gid = trim($cgi->param('group') || '');
    ThrowUserError("group_not_specified") unless ($gid);
    detaint_natural($gid);

    SendSQL("SELECT name FROM groups WHERE id = $gid");
    my ($name) = FetchSQLData();

    my $cantdelete = 0;

    SendSQL("SELECT user_id FROM user_group_map 
             WHERE group_id = $gid AND isbless = 0");
    if (FetchOneColumn()) {
        if (!defined $cgi->param('removeusers')) {
            $cantdelete = 1;
        }
    }
    SendSQL("SELECT bug_id FROM bug_group_map WHERE group_id = $gid");
    if (FetchOneColumn()) {
        if (!defined $cgi->param('removebugs')) {
            $cantdelete = 1;
        }
    }
    SendSQL("SELECT name FROM products WHERE name=" . SqlQuote($name));
    if (FetchOneColumn()) {
        if (!defined $cgi->param('unbind')) {
            $cantdelete = 1;
        }
    }

    if (!$cantdelete) {
        SendSQL("DELETE FROM user_group_map WHERE group_id = $gid");
        SendSQL("DELETE FROM group_group_map WHERE grantor_id = $gid");
        SendSQL("DELETE FROM bug_group_map WHERE group_id = $gid");
        SendSQL("DELETE FROM group_control_map WHERE group_id = $gid");
        SendSQL("DELETE FROM groups WHERE id = $gid");
    }

    $vars->{'gid'}        = $gid;
    $vars->{'name'}       = $name;
    $vars->{'cantdelete'} = $cantdelete;

    print Bugzilla->cgi->header();
    $template->process("admin/groups/deleted.html.tmpl", $vars)
      || ThrowTemplateError($template->error());

    exit;
}

#
# action='postchanges' -> update the groups
#

if ($action eq 'postchanges') {
    # ZLL: Bug 181589: we need to have something to remove explictly listed users from
    # groups in order for the conversion to 2.18 groups to work
    my $action;

    if ($cgi->param('remove_explicit_members')) {
        $action = 1;
    } elsif ($cgi->param('remove_explicit_members_regexp')) {
        $action = 2;
    } else {
        $action = 3;
    }
    
    my ($gid, $chgs) = doGroupChanges();
    
    $vars->{'action'}  = $action;
    $vars->{'changes'} = $chgs;
    $vars->{'gid'}     = $gid;
    $vars->{'name'}    = $cgi->param('name');
    if ($action == 2) {
        $vars->{'regexp'} = $cgi->param("rexp");
    }
    
    print Bugzilla->cgi->header();
    $template->process("admin/groups/change.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

if (($action eq 'remove_all_regexp') || ($action eq 'remove_all')) {
    # remove all explicit users from the group with gid $cgi->param('group')
    # that match the regexp stored in the DB for that group
    # or all of them period

    my $gid = $cgi->param('group');
    ThrowUserError("group_not_specified") unless ($gid);
    detaint_natural($gid);

    my $dbh = Bugzilla->dbh;
    my $sth = $dbh->prepare("SELECT name, userregexp FROM groups
                             WHERE id = ?");
    $sth->execute($gid);
    my ($name, $regexp) = $sth->fetchrow_array();
    $dbh->do("LOCK TABLES
                  groups WRITE,
                  profiles READ,
                  user_group_map WRITE");
    $sth = $dbh->prepare("SELECT user_group_map.user_id, profiles.login_name
                             FROM user_group_map, profiles
                             WHERE user_group_map.user_id = profiles.userid
                             AND user_group_map.group_id = ?
                             AND grant_type = ?
                             AND isbless = 0");
    $sth->execute($gid, GRANT_DIRECT);

    my @users;
    my $sth2 = $dbh->prepare("DELETE FROM user_group_map
                              WHERE user_id = ?
                              AND isbless = 0
                              AND group_id = ?");
    while ( my ($userid, $userlogin) = $sth->fetchrow_array() ) {
        if ((($regexp =~ /\S/) && ($userlogin =~ m/$regexp/i))
            || ($action eq 'remove_all'))
        {
            $sth2->execute($userid,$gid);

            my $user = {};
            $user->{'login'} = $userlogin;
            push(@users, $user);
        }
    }

    $sth = $dbh->prepare("UPDATE groups
             SET last_changed = NOW()
             WHERE id = ?");
    $sth->execute($gid);
    $dbh->do("UNLOCK TABLES");

    $vars->{'users'}      = \@users;
    $vars->{'name'}       = $name;
    $vars->{'regexp'}     = $regexp;
    $vars->{'remove_all'} = ($action eq 'remove_all');
    $vars->{'gid'}        = $gid;
    
    print Bugzilla->cgi->header();
    $template->process("admin/groups/remove.html.tmpl", $vars)
      || ThrowTemplateError($template->error());

    exit;
}


#
# No valid action found
#

ThrowCodeError("action_unrecognized", $vars);


# Helper sub to handle the making of changes to a group
sub doGroupChanges {
    my $cgi = Bugzilla->cgi;
    
    my $gid = trim($cgi->param('group') || '');
    ThrowUserError("group_not_specified") unless ($gid);
    detaint_natural($gid);
    
    SendSQL("SELECT isbuggroup FROM groups WHERE id = $gid");
    my ($isbuggroup) = FetchSQLData();
    my $chgs = 0;

    if (($isbuggroup == 1) && ($cgi->param('oldname') ne $cgi->param("name"))) {
        $chgs = 1;
        SendSQL("UPDATE groups SET name = " . 
            SqlQuote($cgi->param("name")) . " WHERE id = $gid");
    }
    if (($isbuggroup == 1) && ($cgi->param('olddesc') ne $cgi->param("desc"))) {
        $chgs = 1;
        SendSQL("UPDATE groups SET description = " . 
            SqlQuote($cgi->param("desc")) . " WHERE id = $gid");
    }
    if ($cgi->param("oldrexp") ne $cgi->param("rexp")) {
        $chgs = 1;

        my $rexp = $cgi->param('rexp');
        ThrowUserError("invalid_regexp") unless (eval {qr/$rexp/});

        SendSQL("UPDATE groups SET userregexp = " . 
            SqlQuote($rexp) . " WHERE id = $gid");
        RederiveRegexp($::FORM{"rexp"}, $gid);
    }
    if (($isbuggroup == 1) && ($cgi->param("oldisactive") ne $cgi->param("isactive"))) {
        $chgs = 1;
        SendSQL("UPDATE groups SET isactive = " . 
            SqlQuote($cgi->param("isactive")) . " WHERE id = $gid");
    }

    foreach my $b (grep {/^oldgrp-\d*$/} $cgi->param()) {
        if (defined($cgi->param($b))) {
            $b =~ /^oldgrp-(\d+)$/;
            my $v = $1;
            my $grp = $cgi->param("grp-$v") || 0;
            if ($cgi->param("oldgrp-$v") != $grp) {
                $chgs = 1;
                if ($grp != 0) {
                    SendSQL("INSERT INTO group_group_map 
                             (member_id, grantor_id, isbless)
                             VALUES ($v, $gid, 0)");
                } else {
                    SendSQL("DELETE FROM group_group_map
                             WHERE member_id = $v AND grantor_id = $gid
                             AND isbless = 0");
                }
            }

            my $bless = $cgi->param("bless-$v") || 0;
            if ($cgi->param("oldbless-$v") != $bless) {
                $chgs = 1;
                if ($bless != 0) {
                    SendSQL("INSERT INTO group_group_map 
                             (member_id, grantor_id, isbless)
                             VALUES ($v, $gid, 1)");
                } else {
                    SendSQL("DELETE FROM group_group_map
                             WHERE member_id = $v AND grantor_id = $gid
                             AND isbless = 1");
                }
            }

        }
    }
    
    if ($chgs) {
        # mark the changes
        SendSQL("UPDATE groups SET last_changed = NOW() WHERE id = $gid");
    }
    return $gid, $chgs;
}
