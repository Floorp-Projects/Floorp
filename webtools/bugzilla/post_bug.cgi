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

use Bugzilla;
use Bugzilla::Constants;
require "CGI.pl";

use Bugzilla::Bug;

use Bugzilla::User;

# Shut up misguided -w warnings about "used only once". For some reason,
# "use vars" chokes on me when I try it here.
sub sillyness {
    my $zz;
    $zz = $::buffer;
    $zz = %::components;
    $zz = %::versions;
    $zz = @::legal_opsys;
    $zz = @::legal_platform;
    $zz = @::legal_priority;
    $zz = @::legal_product;
    $zz = @::legal_severity;
    $zz = %::target_milestone;
}

# Use global template variables.
use vars qw($vars $template);

my $user = Bugzilla->login(LOGIN_REQUIRED);

my $cgi = Bugzilla->cgi;

# do a match on the fields if applicable

&Bugzilla::User::match_field ({
    'cc'            => { 'type' => 'multi'  },
    'assigned_to'   => { 'type' => 'single' },
});

# The format of the initial comment can be structured by adding fields to the
# enter_bug template and then referencing them in the comment template.
my $comment;

$vars->{'form'} = \%::FORM;

my $format = GetFormat("bug/create/comment", $::FORM{'format'}, "txt");

$template->process($format->{'template'}, $vars, \$comment)
  || ThrowTemplateError($template->error());

# Check that if required a description has been provided
if (Param("commentoncreate") && !trim($::FORM{'comment'})) {
    ThrowUserError("description_required");
}
ValidateComment($comment);

my $product = $::FORM{'product'};
my $product_id = get_product_id($product);
if (!$product_id) {
    ThrowUserError("invalid_product_name",
                   { product => $product });
}

# Set cookies
my $cookiepath = Param("cookiepath");
if (exists $::FORM{'product'}) {
    if (exists $::FORM{'version'}) {
        $cgi->send_cookie(-name => "VERSION-$product",
                          -value => $cgi->param('version'),
                          -expires => "Fri, 01-Jan-2038 00:00:00 GMT");
    }
}

if (defined $::FORM{'maketemplate'}) {
    $vars->{'url'} = $::buffer;
    
    print $cgi->header();
    $template->process("bug/create/make-template.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

umask 0;

# Some sanity checking
if (!CanEnterProduct($product)) {
    ThrowUserError("entry_access_denied", {product => $product});
}

my $component_id = get_component_id($product_id, $::FORM{component});
$component_id || ThrowUserError("require_component");

if (!defined $::FORM{'short_desc'} || trim($::FORM{'short_desc'}) eq "") {
    ThrowUserError("require_summary");
}

# If bug_file_loc is "http://", the default, strip it out and use an empty
# value. 
$::FORM{'bug_file_loc'} = "" if $::FORM{'bug_file_loc'} eq 'http://';
    
my $sql_product = SqlQuote($::FORM{'product'});
my $sql_component = SqlQuote($::FORM{'component'});

# Default assignee is the component owner.
if ($::FORM{'assigned_to'} eq "") {
    SendSQL("SELECT initialowner FROM components " .
            "WHERE id = $component_id");
    $::FORM{'assigned_to'} = FetchOneColumn();
} else {
    $::FORM{'assigned_to'} = DBNameToIdAndCheck(trim($::FORM{'assigned_to'}));
}

my @bug_fields = ("version", "rep_platform",
                  "bug_severity", "priority", "op_sys", "assigned_to",
                  "bug_status", "bug_file_loc", "short_desc",
                  "target_milestone", "status_whiteboard");

if (Param("useqacontact")) {
    SendSQL("SELECT initialqacontact FROM components " .
            "WHERE id = $component_id");
    my $qa_contact = FetchOneColumn();
    if (defined $qa_contact && $qa_contact != 0) {
        $::FORM{'qa_contact'} = $qa_contact;
        push(@bug_fields, "qa_contact");
    }
}

if (UserInGroup("canedit") || UserInGroup("canconfirm")) {
    # Default to NEW if the user hasn't selected another status
    $::FORM{'bug_status'} ||= "NEW";
} else {
    # Default to UNCONFIRMED if we are using it, NEW otherwise
    $::FORM{'bug_status'} = $::unconfirmedstate;
    SendSQL("SELECT votestoconfirm FROM products WHERE id = $product_id");
    if (!FetchOneColumn()) {
        $::FORM{'bug_status'} = "NEW";
    }
}

if (!exists $::FORM{'target_milestone'}) {
    SendSQL("SELECT defaultmilestone FROM products WHERE name=$sql_product");
    $::FORM{'target_milestone'} = FetchOneColumn();
}

if (!Param('letsubmitterchoosepriority')) {
    $::FORM{'priority'} = Param('defaultpriority');
}

GetVersionTable();

# Some more sanity checking
CheckFormField(\%::FORM, 'product',      \@::legal_product);
CheckFormField(\%::FORM, 'rep_platform', \@::legal_platform);
CheckFormField(\%::FORM, 'bug_severity', \@::legal_severity);
CheckFormField(\%::FORM, 'priority',     \@::legal_priority);
CheckFormField(\%::FORM, 'op_sys',       \@::legal_opsys);
CheckFormField(\%::FORM, 'bug_status',   [$::unconfirmedstate, 'NEW']);
CheckFormField(\%::FORM, 'version',          $::versions{$product});
CheckFormField(\%::FORM, 'component',        $::components{$product});
CheckFormField(\%::FORM, 'target_milestone', $::target_milestone{$product});
CheckFormFieldDefined(\%::FORM, 'assigned_to');
CheckFormFieldDefined(\%::FORM, 'bug_file_loc');
CheckFormFieldDefined(\%::FORM, 'comment');

my @used_fields;
foreach my $field (@bug_fields) {
    if (exists $::FORM{$field}) {
        push (@used_fields, $field);
    }
}

if (exists $::FORM{'bug_status'} 
    && $::FORM{'bug_status'} ne $::unconfirmedstate) 
{
    push(@used_fields, "everconfirmed");
    $::FORM{'everconfirmed'} = 1;
}

$::FORM{'product_id'} = $product_id;
push(@used_fields, "product_id");
$::FORM{component_id} = $component_id;
push(@used_fields, "component_id");

my %ccids;
my @cc;

# Create the ccid hash for inserting into the db
# and the list for passing to Bugzilla::BugMail::Send
# use a hash rather than a list to avoid adding users twice
if (defined $::FORM{'cc'}) {
    foreach my $person (split(/[ ,]/, $::FORM{'cc'})) {
        if ($person ne "") {
            my $ccid = DBNameToIdAndCheck($person);
            if ($ccid && !$ccids{$ccid}) {
                $ccids{$ccid} = 1;
                push(@cc, $person);
            }
        }
    }
}
# Check for valid keywords and create list of keywords to be added to db
# (validity routine copied from process_bug.cgi)
my @keywordlist;
my %keywordseen;

if ($::FORM{'keywords'} && UserInGroup("editbugs")) {
    foreach my $keyword (split(/[\s,]+/, $::FORM{'keywords'})) {
        if ($keyword eq '') {
           next;
        }
        my $i = GetKeywordIdFromName($keyword);
        if (!$i) {
            ThrowUserError("unknown_keyword",
                           { keyword => $keyword });
        }
        if (!$keywordseen{$i}) {
            push(@keywordlist, $i);
            $keywordseen{$i} = 1;
        }
    }
}

# Check for valid dependency info. 
foreach my $field ("dependson", "blocked") {
    if (UserInGroup("editbugs") && defined($::FORM{$field}) &&
        $::FORM{$field} ne "") {
        my @validvalues;
        foreach my $id (split(/[\s,]+/, $::FORM{$field})) {
            next unless $id;
            ValidateBugID($id, 1);
            push(@validvalues, $id);
        }
        $::FORM{$field} = join(",", @validvalues);
    }
}
# Gather the dependecy list, and make sure there are no circular refs
my %deps;
if (UserInGroup("editbugs") && defined($::FORM{'dependson'})) {
    my $me = "blocked";
    my $target = "dependson";
    my %deptree;
    for (1..2) {
        $deptree{$target} = [];
        my %seen;
        foreach my $i (split('[\s,]+', $::FORM{$target})) {
            if (!exists $seen{$i}) {
                push(@{$deptree{$target}}, $i);
                $seen{$i} = 1;
            }
        }
        # populate $deps{$target} as first-level deps only.
        # and find remainder of dependency tree in $deptree{$target}
        @{$deps{$target}} = @{$deptree{$target}};
        my @stack = @{$deps{$target}};
        while (@stack) {
            my $i = shift @stack;
            SendSQL("select $target from dependencies where $me = " .
                    SqlQuote($i));
            while (MoreSQLData()) {
                my $t = FetchOneColumn();
                if (!exists $seen{$t}) {
                    push(@{$deptree{$target}}, $t);
                    push @stack, $t;
                    $seen{$t} = 1;
                } 
            }
        }
        
        if ($me eq 'dependson') {
            my @deps   =  @{$deptree{'dependson'}};
            my @blocks =  @{$deptree{'blocked'}};
            my @union = ();
            my @isect = ();
            my %union = ();
            my %isect = ();
            foreach my $b (@deps, @blocks) { $union{$b}++ && $isect{$b}++ }
            @union = keys %union;
            @isect = keys %isect;
            if (@isect > 0) {
                my $both;
                foreach my $i (@isect) {
                    $both = $both . GetBugLink($i, "#" . $i) . " ";
                }

                ThrowUserError("dependency_loop_multi",
                               { both => $both },
                               "abort");
            }
        }
        my $tmp = $me;
        $me = $target;
        $target = $tmp;
    }
}

# Build up SQL string to add bug.
my $sql = "INSERT INTO bugs " . 
  "(" . join(",", @used_fields) . ", reporter, creation_ts, " .
  "estimated_time, remaining_time) " .
  "VALUES (";

foreach my $field (@used_fields) {
    $sql .= SqlQuote($::FORM{$field}) . ",";
}

$comment =~ s/\r\n?/\n/g;     # Get rid of \r.
$comment = trim($comment);
# If comment is all whitespace, it'll be null at this point. That's
# OK except for the fact that it causes e-mail to be suppressed.
$comment = $comment ? $comment : " ";

$sql .= "$::userid, now(), ";

# Time Tracking
if (UserInGroup(Param("timetrackinggroup")) &&
    defined $::FORM{'estimated_time'}) {

    my $est_time = $::FORM{'estimated_time'};
    Bugzilla::Bug::ValidateTime($est_time, 'estimated_time');
    $sql .= SqlQuote($est_time) . "," . SqlQuote($est_time);
} else {
    $sql .= "0, 0";
}
$sql .= ")";

# Groups
my @groupstoadd = ();
foreach my $b (grep(/^bit-\d*$/, keys %::FORM)) {
    if ($::FORM{$b}) {
        my $v = substr($b, 4);
        $v =~ /^(\d+)$/
          || ThrowCodeError("group_id_invalid", undef, "abort");
        if (!GroupIsActive($v)) {
            # Prevent the user from adding the bug to an inactive group.
            # Should only happen if there is a bug in Bugzilla or the user
            # hacked the "enter bug" form since otherwise the UI 
            # for adding the bug to the group won't appear on that form.
            $vars->{'bit'} = $v;
            ThrowCodeError("inactive_group", undef, "abort");
        }
        SendSQL("SELECT user_id FROM user_group_map 
                 WHERE user_id = $::userid
                 AND group_id = $v
                 AND isbless = 0");
        my ($permit) = FetchSQLData();
        if (!$permit) {
            SendSQL("SELECT othercontrol FROM group_control_map
                     WHERE group_id = $v AND product_id = $product_id");
            my ($othercontrol) = FetchSQLData();
            $permit = (($othercontrol == CONTROLMAPSHOWN)
                       || ($othercontrol == CONTROLMAPDEFAULT));
        }
        if ($permit) {
            push(@groupstoadd, $v)
        }
    }
}

SendSQL("SELECT DISTINCT groups.id, groups.name, " .
        "membercontrol, othercontrol " .
        "FROM groups LEFT JOIN group_control_map " .
        "ON group_id = id AND product_id = $product_id " .
        " WHERE isbuggroup != 0 AND isactive != 0 ORDER BY description");
while (MoreSQLData()) {
    my ($id, $groupname, $membercontrol, $othercontrol ) = FetchSQLData();
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
SendSQL($sql);

SendSQL("select now()");
my $timestamp = FetchOneColumn();

# Get the bug ID back.
SendSQL("select LAST_INSERT_ID()");
my $id = FetchOneColumn();

# Add the group restrictions
foreach my $grouptoadd (@groupstoadd) {
    SendSQL("INSERT INTO bug_group_map (bug_id, group_id)
             VALUES ($id, $grouptoadd)");
}

# Add the comment
SendSQL("INSERT INTO longdescs (bug_id, who, bug_when, thetext) 
         VALUES ($id, $::userid, now(), " . SqlQuote($comment) . ")");

# Insert the cclist into the database
foreach my $ccid (keys(%ccids)) {
    SendSQL("INSERT INTO cc (bug_id, who) VALUES ($id, $ccid)");
}

my @all_deps;
if (UserInGroup("editbugs")) {
    foreach my $keyword (@keywordlist) {
        SendSQL("INSERT INTO keywords (bug_id, keywordid) 
                 VALUES ($id, $keyword)");
    }
    if (@keywordlist) {
        # Make sure that we have the correct case for the kw
        SendSQL("SELECT name FROM keyworddefs WHERE id IN ( " .
                join(',', @keywordlist) . ")");
        my @list;
        while (MoreSQLData()) {
            push (@list, FetchOneColumn());
        }
        SendSQL("UPDATE bugs SET keywords = " .
                SqlQuote(join(', ', @list)) .
                " WHERE bug_id = $id");
    }
    if (defined $::FORM{'dependson'}) {
        my $me = "blocked";
        my $target = "dependson";
        for (1..2) {
            foreach my $i (@{$deps{$target}}) {
                SendSQL("INSERT INTO dependencies ($me, $target) values " .
                        "($id, $i)");
                push(@all_deps, $i); # list for mailing dependent bugs
                # Log the activity for the other bug:
                LogActivityEntry($i, $me, "", $id, $user->id, $timestamp);
            }
            my $tmp = $me;
            $me = $target;
            $target = $tmp;
        }
    }
}

# Email everyone the details of the new bug 
$vars->{'mailrecipients'} = { 'cc' => \@cc,
                              'owner' => DBID_to_name($::FORM{'assigned_to'}),
                              'reporter' => Bugzilla->user->login,
                              'changer' => Bugzilla->user->login };

if (defined $::FORM{'qa_contact'}) {
    $vars->{'mailrecipients'}->{'qa'} = DBID_to_name($::FORM{'qa_contact'});
}

$vars->{'id'} = $id;
my $bug = new Bugzilla::Bug($id, $::userid);
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

print $cgi->header();
$template->process("bug/create/created.html.tmpl", $vars)
  || ThrowTemplateError($template->error());


