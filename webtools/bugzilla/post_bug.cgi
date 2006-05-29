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
# Contributor(s): Terry Weissman <terry@mozilla.org>
#                 Dan Mosedale <dmose@mozilla.org>
#                 Joe Robins <jmrobins@tgix.com>
#                 Gervase Markham <gerv@gerv.net>

use strict;
use lib qw(.);

require "globals.pl";
use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Util;
use Bugzilla::Bug;
use Bugzilla::User;
use Bugzilla::Field;
use Bugzilla::Product;
use Bugzilla::Keyword;

# Shut up misguided -w warnings about "used only once". For some reason,
# "use vars" chokes on me when I try it here.
sub sillyness {
    my $zz;
    $zz = @::legal_opsys;
    $zz = @::legal_platform;
    $zz = @::legal_priority;
    $zz = @::legal_severity;
}

my $user = Bugzilla->login(LOGIN_REQUIRED);

my $cgi = Bugzilla->cgi;
my $dbh = Bugzilla->dbh;
my $template = Bugzilla->template;
my $vars = {};

######################################################################
# Subroutines
######################################################################

# Determines whether or not a group is active by checking
# the "isactive" column for the group in the "groups" table.
# Note: This function selects groups by id rather than by name.
sub GroupIsActive {
    my ($group_id) = @_;
    $group_id ||= 0;
    detaint_natural($group_id);
    my ($is_active) = Bugzilla->dbh->selectrow_array(
        "SELECT isactive FROM groups WHERE id = ?", undef, $group_id);
    return $is_active;
}

######################################################################
# Main Script
######################################################################

# do a match on the fields if applicable

&Bugzilla::User::match_field ($cgi, {
    'cc'            => { 'type' => 'multi'  },
    'assigned_to'   => { 'type' => 'single' },
    'qa_contact'    => { 'type' => 'single' },
});

# The format of the initial comment can be structured by adding fields to the
# enter_bug template and then referencing them in the comment template.
my $comment;

my $format = $template->get_format("bug/create/comment",
                                   scalar($cgi->param('format')), "txt");

$template->process($format->{'template'}, $vars, \$comment)
  || ThrowTemplateError($template->error());

ValidateComment($comment);

# Check that the product exists and that the user
# is allowed to enter bugs into this product.
$user->can_enter_product(scalar $cgi->param('product'), 1);

my $product = Bugzilla::Product::check_product(scalar $cgi->param('product'));

# Set cookies
if (defined $cgi->param('product')) {
    if (defined $cgi->param('version')) {
        $cgi->send_cookie(-name => "VERSION-" . $product->name,
                          -value => $cgi->param('version'),
                          -expires => "Fri, 01-Jan-2038 00:00:00 GMT");
    }
}

if (defined $cgi->param('maketemplate')) {
    $vars->{'url'} = $cgi->query_string();
    $vars->{'short_desc'} = $cgi->param('short_desc');
    
    print $cgi->header();
    $template->process("bug/create/make-template.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

umask 0;

# Some sanity checking
$cgi->param('component') || ThrowUserError("require_component");
my $component = Bugzilla::Component::check_component($product, scalar $cgi->param('component'));

# Set the parameter to itself, but cleaned up
$cgi->param('short_desc', clean_text($cgi->param('short_desc')));

if (!defined $cgi->param('short_desc')
    || $cgi->param('short_desc') eq "") {
    ThrowUserError("require_summary");
}

# Check that if required a description has been provided
# This has to go somewhere after 'maketemplate' 
#  or it breaks bookmarks with no comments.
if (Param("commentoncreate") && !trim($cgi->param('comment'))) {
    ThrowUserError("description_required");
}

# If bug_file_loc is "http://", the default, use an empty value instead.
$cgi->param('bug_file_loc', '') if $cgi->param('bug_file_loc') eq 'http://';


# Default assignee is the component owner.
if (!UserInGroup("editbugs") || $cgi->param('assigned_to') eq "") {
    my $initialowner = $dbh->selectrow_array(q{SELECT initialowner
                                                 FROM components
                                                WHERE id = ?},
                                               undef, $component->id);
    $cgi->param(-name => 'assigned_to', -value => $initialowner);
} else {
    $cgi->param(-name => 'assigned_to',
                -value => login_to_id(trim($cgi->param('assigned_to')), THROW_ERROR));
}

my @bug_fields = ("version", "rep_platform",
                  "bug_severity", "priority", "op_sys", "assigned_to",
                  "bug_status", "everconfirmed", "bug_file_loc", "short_desc",
                  "target_milestone", "status_whiteboard");

if (Param("usebugaliases")) {
   my $alias = trim($cgi->param('alias') || "");
   if ($alias ne "") {
       ValidateBugAlias($alias);
       $cgi->param('alias', $alias);
       push (@bug_fields,"alias");
   }
}

# Retrieve the default QA contact if the field is empty
if (Param("useqacontact")) {
    my $qa_contact;
    if (!UserInGroup("editbugs") || !defined $cgi->param('qa_contact')
        || trim($cgi->param('qa_contact')) eq "") {
        ($qa_contact) = $dbh->selectrow_array(q{SELECT initialqacontact 
                                                  FROM components 
                                                 WHERE id = ?},
                                                undef, $component->id);
    } else {
        $qa_contact = login_to_id(trim($cgi->param('qa_contact')), THROW_ERROR);
    }

    if ($qa_contact) {
        $cgi->param(-name => 'qa_contact', -value => $qa_contact);
        push(@bug_fields, "qa_contact");
    }
}

if (UserInGroup("editbugs") || UserInGroup("canconfirm")) {
    # Default to NEW if the user hasn't selected another status
    if (!defined $cgi->param('bug_status')) {
        $cgi->param(-name => 'bug_status', -value => "NEW");
    }
} else {
    # Default to UNCONFIRMED if we are using it, NEW otherwise
    $cgi->param(-name => 'bug_status', -value => 'UNCONFIRMED');
    my $votestoconfirm = $dbh->selectrow_array(q{SELECT votestoconfirm 
                                                   FROM products 
                                                  WHERE id = ?},
                                                 undef, $product->id);

    if (!$votestoconfirm) {
        $cgi->param(-name => 'bug_status', -value => "NEW");
    }
}

if (!defined $cgi->param('target_milestone')) {
    my $defaultmilestone = $dbh->selectrow_array(q{SELECT defaultmilestone
                                                     FROM products
                                                    WHERE name = ?},
                                                    undef, $product->name);
    $cgi->param(-name => 'target_milestone', -value => $defaultmilestone);
}

if (!Param('letsubmitterchoosepriority')) {
    $cgi->param(-name => 'priority', -value => Param('defaultpriority'));
}

GetVersionTable();

# Some more sanity checking
check_field('rep_platform', scalar $cgi->param('rep_platform'), \@::legal_platform);
check_field('bug_severity', scalar $cgi->param('bug_severity'), \@::legal_severity);
check_field('priority',     scalar $cgi->param('priority'),     \@::legal_priority);
check_field('op_sys',       scalar $cgi->param('op_sys'),       \@::legal_opsys);
check_field('bug_status',   scalar $cgi->param('bug_status'),   ['UNCONFIRMED', 'NEW']);
check_field('version',      scalar $cgi->param('version'),
            [map($_->name, @{$product->versions})]);
check_field('target_milestone', scalar $cgi->param('target_milestone'),
            [map($_->name, @{$product->milestones})]);

foreach my $field_name ('assigned_to', 'bug_file_loc', 'comment') {
    defined($cgi->param($field_name))
      || ThrowCodeError('undefined_field', { field => $field_name });
}

my $everconfirmed = ($cgi->param('bug_status') eq 'UNCONFIRMED') ? 0 : 1;
$cgi->param(-name => 'everconfirmed', -value => $everconfirmed);

my @used_fields;
foreach my $field (@bug_fields) {
    if (defined $cgi->param($field)) {
        push (@used_fields, $field);
    }
}

$cgi->param(-name => 'product_id', -value => $product->id);
push(@used_fields, "product_id");
$cgi->param(-name => 'component_id', -value => $component->id);
push(@used_fields, "component_id");

my %ccids;

# Create the ccid hash for inserting into the db
# use a hash rather than a list to avoid adding users twice
if (defined $cgi->param('cc')) {
    foreach my $person ($cgi->param('cc')) {
        next unless $person;
        my $ccid = login_to_id($person, THROW_ERROR);
        if ($ccid && !$ccids{$ccid}) {
           $ccids{$ccid} = 1;
        }
    }
}
# Check for valid keywords and create list of keywords to be added to db
# (validity routine copied from process_bug.cgi)
my @keywordlist;
my %keywordseen;

if ($cgi->param('keywords') && UserInGroup("editbugs")) {
    foreach my $keyword (split(/[\s,]+/, $cgi->param('keywords'))) {
        if ($keyword eq '') {
           next;
        }
        my $keyword_obj = new Bugzilla::Keyword({name => $keyword});
        if (!$keyword_obj) {
            ThrowUserError("unknown_keyword",
                           { keyword => $keyword });
        }
        if (!$keywordseen{$keyword_obj->id}) {
            push(@keywordlist, $keyword_obj->id);
            $keywordseen{$keyword_obj->id} = 1;
        }
    }
}

if (Param("strict_isolation")) {
    my @blocked_users = ();
    my %related_users = %ccids;
    $related_users{$cgi->param('assigned_to')} = 1;
    if (Param('useqacontact') && $cgi->param('qa_contact')) {
        $related_users{$cgi->param('qa_contact')} = 1;
    }
    foreach my $pid (keys %related_users) {
        my $related_user = Bugzilla::User->new($pid);
        if (!$related_user->can_edit_product($product->id)) {
            push (@blocked_users, $related_user->login);
        }
    }
    if (scalar(@blocked_users)) {
        ThrowUserError("invalid_user_group", 
            {'users' => \@blocked_users,
             'new' => 1,
             'product' => $product->name
            });
    }
}

# Check for valid dependency info. 
foreach my $field ("dependson", "blocked") {
    if (UserInGroup("editbugs") && $cgi->param($field)) {
        my @validvalues;
        foreach my $id (split(/[\s,]+/, $cgi->param($field))) {
            next unless $id;
            # $field is not passed to ValidateBugID to prevent adding new 
            # dependencies on inacessible bugs.
            ValidateBugID($id);
            push(@validvalues, $id);
        }
        $cgi->param(-name => $field, -value => join(",", @validvalues));
    }
}
# Gather the dependency list, and make sure there are no circular refs
my %deps;
if (UserInGroup("editbugs")) {
    %deps = Bugzilla::Bug::ValidateDependencies(scalar($cgi->param('dependson')),
                                                scalar($cgi->param('blocked')));
}

# get current time
my $timestamp = $dbh->selectrow_array(q{SELECT NOW()});

# Build up SQL string to add bug.
# creation_ts will only be set when all other fields are defined.

my @fields_values;

foreach my $field (@used_fields) {
    my $value = $cgi->param($field);
    trick_taint($value);
    push (@fields_values, $value);
}

my $sql_used_fields = join(", ", @used_fields);
my $sql_placeholders = "?, " x scalar(@used_fields);

my $query = qq{INSERT INTO bugs ($sql_used_fields, reporter, delta_ts,
                                 estimated_time, remaining_time, deadline)
               VALUES ($sql_placeholders ?, ?, ?, ?, ?)};

$comment =~ s/\r\n?/\n/g;     # Get rid of \r.
$comment = trim($comment);
# If comment is all whitespace, it'll be null at this point. That's
# OK except for the fact that it causes e-mail to be suppressed.
$comment = $comment ? $comment : " ";

push (@fields_values, $user->id);
push (@fields_values, $timestamp);

my $est_time = 0;
my $deadline;

# Time Tracking
if (UserInGroup(Param("timetrackinggroup")) &&
    defined $cgi->param('estimated_time')) {

    $est_time = $cgi->param('estimated_time');
    Bugzilla::Bug::ValidateTime($est_time, 'estimated_time');
    trick_taint($est_time);

}

push (@fields_values, $est_time, $est_time);

if ((UserInGroup(Param("timetrackinggroup"))) && ($cgi->param('deadline'))) {
    validate_date($cgi->param('deadline'))
      || ThrowUserError('illegal_date', {date => $cgi->param('deadline'),
                                         format => 'YYYY-MM-DD'});
    $deadline = $cgi->param('deadline');
    trick_taint($deadline);
}

push (@fields_values, $deadline);

# Groups
my @groupstoadd = ();
my $sth_othercontrol = $dbh->prepare(q{SELECT othercontrol
                                         FROM group_control_map
                                        WHERE group_id = ?
                                          AND product_id = ?});

foreach my $b (grep(/^bit-\d*$/, $cgi->param())) {
    if ($cgi->param($b)) {
        my $v = substr($b, 4);
        detaint_natural($v)
          || ThrowUserError("invalid_group_ID");
        if (!GroupIsActive($v)) {
            # Prevent the user from adding the bug to an inactive group.
            # Should only happen if there is a bug in Bugzilla or the user
            # hacked the "enter bug" form since otherwise the UI 
            # for adding the bug to the group won't appear on that form.
            $vars->{'bit'} = $v;
            ThrowCodeError("inactive_group");
        }
        my ($permit) = $user->in_group_id($v);
        if (!$permit) {
            my $othercontrol = $dbh->selectrow_array($sth_othercontrol, 
                                                     undef, ($v, $product->id));
            $permit = (($othercontrol == CONTROLMAPSHOWN)
                       || ($othercontrol == CONTROLMAPDEFAULT));
        }
        if ($permit) {
            push(@groupstoadd, $v)
        }
    }
}

my $groups = $dbh->selectall_arrayref(q{
                 SELECT DISTINCT groups.id, groups.name, membercontrol,
                                 othercontrol, description
                            FROM groups
                       LEFT JOIN group_control_map 
                              ON group_id = id
                             AND product_id = ?
                           WHERE isbuggroup != 0
                             AND isactive != 0
                        ORDER BY description}, undef, $product->id);

foreach my $group (@$groups) {
    my ($id, $groupname, $membercontrol, $othercontrol) = @$group;
    $membercontrol ||= 0;
    $othercontrol ||= 0;
    # Add groups required
    if (($membercontrol == CONTROLMAPMANDATORY)
       || (($othercontrol == CONTROLMAPMANDATORY) 
            && (!UserInGroup($groupname)))) {
        # User had no option, bug needs to be in this group.
        push(@groupstoadd, $id)
    }
}

# Add the bug report to the DB.
$dbh->bz_lock_tables('bugs WRITE', 'bug_group_map WRITE', 'longdescs WRITE',
                     'cc WRITE', 'keywords WRITE', 'dependencies WRITE',
                     'bugs_activity WRITE', 'groups READ',
                     'user_group_map READ', 'group_group_map READ',
                     'keyworddefs READ', 'fielddefs READ');

$dbh->do($query, undef, @fields_values);

# Get the bug ID back.
my $id = $dbh->bz_last_key('bugs', 'bug_id');

# Add the group restrictions
my $sth_addgroup = $dbh->prepare(q{
            INSERT INTO bug_group_map (bug_id, group_id) VALUES (?, ?)});
foreach my $grouptoadd (@groupstoadd) {
    $sth_addgroup->execute($id, $grouptoadd);
}

# Add the initial comment, allowing for the fact that it may be private
my $privacy = 0;
if (Param("insidergroup") && UserInGroup(Param("insidergroup"))) {
    $privacy = $cgi->param('commentprivacy') ? 1 : 0;
}

trick_taint($comment);
$dbh->do(q{INSERT INTO longdescs (bug_id, who, bug_when, thetext,isprivate)
           VALUES (?, ?, ?, ?, ?)}, undef, ($id, $user->id, $timestamp,
                                            $comment, $privacy));

# Insert the cclist into the database
my $sth_cclist = $dbh->prepare(q{INSERT INTO cc (bug_id, who) VALUES (?,?)});
foreach my $ccid (keys(%ccids)) {
    $sth_cclist->execute($id, $ccid);
}

my @all_deps;
my $sth_addkeyword = $dbh->prepare(q{
            INSERT INTO keywords (bug_id, keywordid) VALUES (?, ?)});
if (UserInGroup("editbugs")) {
    foreach my $keyword (@keywordlist) {
        $sth_addkeyword->execute($id, $keyword);
    }
    if (@keywordlist) {
        # Make sure that we have the correct case for the kw
        my $kw_ids = join(', ', @keywordlist);
        my $list = $dbh->selectcol_arrayref(qq{
                                    SELECT name 
                                      FROM keyworddefs 
                                     WHERE id IN ($kw_ids)});
        my $kw_list = join(', ', @$list);
        $dbh->do(q{UPDATE bugs 
                      SET delta_ts = ?, keywords = ? 
                    WHERE bug_id = ?}, undef, ($timestamp, $kw_list, $id));
    }
    if ($cgi->param('dependson') || $cgi->param('blocked')) {
        foreach my $pair (["blocked", "dependson"], ["dependson", "blocked"]) {
            my ($me, $target) = @{$pair};
            my $sth_dep = $dbh->prepare(qq{
                        INSERT INTO dependencies ($me, $target) VALUES (?, ?)});
            foreach my $i (@{$deps{$target}}) {
                $sth_dep->execute($id, $i);
                push(@all_deps, $i); # list for mailing dependent bugs
                # Log the activity for the other bug:
                LogActivityEntry($i, $me, "", $id, $user->id, $timestamp);
            }
        }
    }
}

# All fields related to the newly created bug are set.
# The bug can now be made accessible.
$dbh->do("UPDATE bugs SET creation_ts = ? WHERE bug_id = ?",
          undef, ($timestamp, $id));

$dbh->bz_unlock_tables();

# Email everyone the details of the new bug 
$vars->{'mailrecipients'} = {'changer' => $user->login};

$vars->{'id'} = $id;
my $bug = new Bugzilla::Bug($id, $user->id);
$vars->{'bug'} = $bug;

ThrowCodeError("bug_error", { bug => $bug }) if $bug->error;

$vars->{'sentmail'} = [];

push (@{$vars->{'sentmail'}}, { type => 'created',
                                id => $id,
                              });

foreach my $i (@all_deps) {
    push (@{$vars->{'sentmail'}}, { type => 'dep', id => $i, });
}

my @bug_list;
if ($cgi->cookie("BUGLIST")) {
    @bug_list = split(/:/, $cgi->cookie("BUGLIST"));
}
$vars->{'bug_list'} = \@bug_list;
$vars->{'use_keywords'} = 1 if Bugzilla::Keyword::keyword_count();

print $cgi->header();
$template->process("bug/create/created.html.tmpl", $vars)
  || ThrowTemplateError($template->error());


