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
#                 Marc Schumann <wurblzap@gmail.com>

use strict;
use lib qw(.);

use Bugzilla;
use Bugzilla::Attachment;
use Bugzilla::Constants;
use Bugzilla::Util;
use Bugzilla::Error;
use Bugzilla::Bug;
use Bugzilla::User;
use Bugzilla::Field;
use Bugzilla::Product;
use Bugzilla::Component;
use Bugzilla::Keyword;
use Bugzilla::Token;
use Bugzilla::Flag;

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

# Detect if the user already used the same form to submit a bug
my $token = trim($cgi->param('token'));
if ($token) {
    my ($creator_id, $date, $old_bug_id) = Bugzilla::Token::GetTokenData($token);
    unless ($creator_id
              && ($creator_id == $user->id)
              && ($old_bug_id =~ "^createbug:"))
    {
        # The token is invalid.
        ThrowUserError('token_inexistent');
    }

    $old_bug_id =~ s/^createbug://;

    if ($old_bug_id && (!$cgi->param('ignore_token')
                        || ($cgi->param('ignore_token') != $old_bug_id)))
    {
        $vars->{'bugid'} = $old_bug_id;
        $vars->{'allow_override'} = defined $cgi->param('ignore_token') ? 0 : 1;

        print $cgi->header();
        $template->process("bug/create/confirm-create-dupe.html.tmpl", $vars)
           || ThrowTemplateError($template->error());
        exit;
    }
}    

# do a match on the fields if applicable

&Bugzilla::User::match_field ($cgi, {
    'cc'            => { 'type' => 'multi'  },
    'assigned_to'   => { 'type' => 'single' },
    'qa_contact'    => { 'type' => 'single' },
    '^requestee_type-(\d+)$' => { 'type' => 'multi' },
});

# The format of the initial comment can be structured by adding fields to the
# enter_bug template and then referencing them in the comment template.
my $comment;

my $format = $template->get_format("bug/create/comment",
                                   scalar($cgi->param('format')), "txt");

$template->process($format->{'template'}, $vars, \$comment)
  || ThrowTemplateError($template->error());

# Check that the product exists and that the user
# is allowed to enter bugs into this product.
my $product = Bugzilla::Bug->_check_product($cgi->param('product'));

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

# This has to go somewhere after 'maketemplate' 
# or it breaks bookmarks with no comments. 
$comment = Bugzilla::Bug->_check_comment($cgi->param('comment'));
# If comment is all whitespace, it'll be null at this point. That's
# OK except for the fact that it causes e-mail to be suppressed.
$comment = $comment ? $comment : " ";

my $cc_ids = Bugzilla::Bug->_check_cc([$cgi->param('cc')]);
my @keyword_ids = @{Bugzilla::Bug->_check_keywords($cgi->param('keywords'))};

# XXX These checks are only here until strict_isolation can move fully
# into Bugzilla::Bug.
my $component = Bugzilla::Bug->_check_component($product, 
    $cgi->param('component'));
my $assigned_to_id = Bugzilla::Bug->_check_assigned_to($component,
    $cgi->param('assigned_to'));
my $qa_contact_id = Bugzilla::Bug->_check_qa_contact($component,
    $cgi->param('qa_contact'));
Bugzilla::Bug->_check_strict_isolation($product, $cc_ids, $assigned_to_id, 
    $qa_contact_id);

my ($depends_on_ids, $blocks_ids) = Bugzilla::Bug->_check_dependencies(
    scalar $cgi->param('dependson'), scalar $cgi->param('blocked'));

# get current time
my $timestamp = $dbh->selectrow_array(q{SELECT NOW()});

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
            && (!Bugzilla->user->in_group($groupname)))) {
        # User had no option, bug needs to be in this group.
        push(@groupstoadd, $id)
    }
}

my @bug_fields = map {$_->name} Bugzilla->get_fields(
    { custom => 1, obsolete => 0, enter_bug => 1});
push(@bug_fields, qw(
    product
    component

    assigned_to
    qa_contact

    alias
    bug_file_loc
    bug_severity
    bug_status
    short_desc
    op_sys
    priority
    rep_platform
    version
    target_milestone
    status_whiteboard

    estimated_time
    deadline
));
my %bug_params;
foreach my $field (@bug_fields) {
    $bug_params{$field} = $cgi->param($field);
}
$bug_params{'creation_ts'} = $timestamp;

# Add the bug report to the DB.
$dbh->bz_lock_tables('bugs WRITE', 'bug_group_map WRITE', 'longdescs WRITE',
                     'cc WRITE', 'keywords WRITE', 'dependencies WRITE',
                     'bugs_activity WRITE', 'groups READ',
                     'user_group_map READ', 'group_group_map READ',
                     'keyworddefs READ', 'fielddefs READ',
                     'products READ', 'versions READ', 'milestones READ',
                     'components READ', 'profiles READ', 'bug_severity READ',
                     'op_sys READ', 'priority READ', 'rep_platform READ');

my $bug = Bugzilla::Bug->create(\%bug_params);

# Get the bug ID back.
my $id = $bug->bug_id;

# Add the group restrictions
my $sth_addgroup = $dbh->prepare(q{
            INSERT INTO bug_group_map (bug_id, group_id) VALUES (?, ?)});
foreach my $grouptoadd (@groupstoadd) {
    $sth_addgroup->execute($id, $grouptoadd);
}

# Add the initial comment, allowing for the fact that it may be private
my $privacy = 0;
if (Bugzilla->params->{"insidergroup"} 
    && Bugzilla->user->in_group(Bugzilla->params->{"insidergroup"})) 
{
    $privacy = $cgi->param('commentprivacy') ? 1 : 0;
}

trick_taint($comment);
$dbh->do(q{INSERT INTO longdescs (bug_id, who, bug_when, thetext,isprivate)
           VALUES (?, ?, ?, ?, ?)}, undef, ($id, $user->id, $timestamp,
                                            $comment, $privacy));

# Insert the cclist into the database
my $sth_cclist = $dbh->prepare(q{INSERT INTO cc (bug_id, who) VALUES (?,?)});
foreach my $ccid (@$cc_ids) {
    $sth_cclist->execute($id, $ccid);
}

my @all_deps;
my $sth_addkeyword = $dbh->prepare(q{
            INSERT INTO keywords (bug_id, keywordid) VALUES (?, ?)});
if (Bugzilla->user->in_group("editbugs")) {
    foreach my $keyword (@keyword_ids) {
        $sth_addkeyword->execute($id, $keyword);
    }
    if (@keyword_ids) {
        # Make sure that we have the correct case for the kw
        my $kw_ids = join(', ', @keyword_ids);
        my $list = $dbh->selectcol_arrayref(qq{
                                    SELECT name 
                                      FROM keyworddefs 
                                     WHERE id IN ($kw_ids)
                                  ORDER BY name});
        my $kw_list = join(', ', @$list);
        $dbh->do(q{UPDATE bugs 
                      SET delta_ts = ?, keywords = ? 
                    WHERE bug_id = ?}, undef, ($timestamp, $kw_list, $id));
    }
    if ($cgi->param('dependson') || $cgi->param('blocked')) {
        my %deps = (dependson => $depends_on_ids, blocked => $blocks_ids);
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

# We don't have to check if the user can see the bug, because a user filing
# a bug can always see it. You can't change reporter_accessible until
# after the bug is filed.

# Add an attachment if requested.
if (defined($cgi->upload('data')) || $cgi->param('attachurl')) {
    $cgi->param('isprivate', $cgi->param('commentprivacy'));
    Bugzilla::Attachment->insert_attachment_for_bug(!THROW_ERROR,
                                                    $bug, $user, $timestamp,
                                                    \$vars)
        || ($vars->{'message'} = 'attachment_creation_failed');

    # Determine if Patch Viewer is installed, for Diff link
    eval {
        require PatchReader;
        $vars->{'patchviewerinstalled'} = 1;
    };
}

# Add flags, if any. To avoid dying if something goes wrong
# while processing flags, we will eval() flag validation.
# This requires errors to die().
# XXX: this can go away as soon as flag validation is able to
#      fail without dying.
my $error_mode_cache = Bugzilla->error_mode;
Bugzilla->error_mode(ERROR_MODE_DIE);
eval {
    Bugzilla::Flag::validate($cgi, $id);
    Bugzilla::Flag::process($bug, undef, $timestamp, $cgi);
};
Bugzilla->error_mode($error_mode_cache);
if ($@) {
    $vars->{'message'} = 'flag_creation_failed';
    $vars->{'flag_creation_error'} = $@;
}

# Email everyone the details of the new bug 
$vars->{'mailrecipients'} = {'changer' => $user->login};

$vars->{'id'} = $id;
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

if ($token) {
    trick_taint($token);
    $dbh->do('UPDATE tokens SET eventdata = ? WHERE token = ?', undef, 
             ("createbug:$id", $token));
}

print $cgi->header();
$template->process("bug/create/created.html.tmpl", $vars)
  || ThrowTemplateError($template->error());


