#!/usr/bonsaitools/bin/perl -wT
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

use diagnostics;
use strict;
use lib qw(.);

require "CGI.pl";
require "bug_form.pl";

# Shut up misguided -w warnings about "used only once". For some reason,
# "use vars" chokes on me when I try it here.
sub sillyness {
    my $zz;
    $zz = $::buffer;
    $zz = $::usergroupset;
    $zz = %::COOKIE;
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

confirm_login();


# The format of the initial comment can be structured by adding fields to the
# enter_bug template and then referencing them in the comment template.
my $comment;

$vars->{'form'} = \%::FORM;

# We can't use ValidateOutputFormat here because it defaults to HTML.
my $template_name = "bug/create/comment";
$template_name .= ($::FORM{'format'} ? "-$::FORM{'format'}" : "");

$template->process("$template_name.txt.tmpl", $vars, \$comment)
  || ThrowTemplateError($template->error());

ValidateComment($comment);

my $product = $::FORM{'product'};

# Set cookies
my $cookiepath = Param("cookiepath");
if (exists $::FORM{'product'}) {
    if (exists $::FORM{'version'}) {           
        print "Set-Cookie: VERSION-$product=$::FORM{'version'} ; \
               path=$cookiepath ; expires=Sun, 30-Jun-2029 00:00:00 GMT\n"; 
    }
}

if (defined $::FORM{'maketemplate'}) {
    $vars->{'url'} = $::buffer;
    
    print "Content-type: text/html\n\n";
    $template->process("bug/create/make-template.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

umask 0;
ConnectToDatabase();

# Some sanity checking
if(Param("usebuggroupsentry") && GroupExists($product)) {
    if(!UserInGroup($product)) {
        DisplayError("Sorry; you do not have the permissions necessary to enter
                      a bug against this product.", "Permission Denied");
        exit;
    }
}

if (!$::FORM{'component'}) {
    DisplayError("You must choose a component that corresponds to this bug.
                  If necessary, just guess.");
    exit;                  
}

if (!defined $::FORM{'short_desc'} || trim($::FORM{'short_desc'}) eq "") {
    DisplayError("You must enter a summary for this bug.");
    exit;
}

# If bug_file_loc is "http://", the default, strip it out and use an empty
# value. 
$::FORM{'bug_file_loc'} = "" if $::FORM{'bug_file_loc'} eq 'http://';
    
my $sql_product = SqlQuote($::FORM{'product'});
my $sql_component = SqlQuote($::FORM{'component'});

# Default assignee is the component owner.
if ($::FORM{'assigned_to'} eq "") {
    SendSQL("SELECT initialowner FROM components " .
            "WHERE program=$sql_product AND value=$sql_component");
    $::FORM{'assigned_to'} = FetchOneColumn();
} else {
    $::FORM{'assigned_to'} = DBNameToIdAndCheck($::FORM{'assigned_to'});
}

my @bug_fields = ("product", "version", "rep_platform",
                  "bug_severity", "priority", "op_sys", "assigned_to",
                  "bug_status", "bug_file_loc", "short_desc", "component",
                  "target_milestone");

if (Param("useqacontact")) {
    SendSQL("SELECT initialqacontact FROM components " .
            "WHERE program=$sql_product AND value=$sql_component");
    my $qa_contact = FetchOneColumn();
    if (defined $qa_contact && $qa_contact != 0) {
        $::FORM{'qa_contact'} = $qa_contact;
        push(@bug_fields, "qa_contact");
    }
}

if (exists $::FORM{'bug_status'}) {
    # Ignore the given status, so that we can set it to UNCONFIRMED
    # or NEW, depending on votestoconfirm if either the given state was
    # unconfirmed (so that a user can't override the below check), or if
    # the user doesn't have permission to change the default status anyway
    if ($::FORM{'bug_status'} eq $::unconfirmedstate
        || (!UserInGroup("canedit") && !UserInGroup("canconfirm"))) {
        delete $::FORM{'bug_status'};
    }
}

if (!exists $::FORM{'bug_status'}) {
    $::FORM{'bug_status'} = $::unconfirmedstate;
    SendSQL("SELECT votestoconfirm FROM products WHERE product=$sql_product");
    if (!FetchOneColumn()) {
        $::FORM{'bug_status'} = "NEW";
    }
}

if (!exists $::FORM{'target_milestone'}) {
    SendSQL("SELECT defaultmilestone FROM products WHERE product=$sql_product");
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

# Build up SQL string to add bug.
my $sql = "INSERT INTO bugs " . 
  "(" . join(",", @used_fields) . ", reporter, creation_ts, groupset) " . 
  "VALUES (";

foreach my $field (@used_fields) {
    $sql .= SqlQuote($::FORM{$field}) . ",";
}

$comment =~ s/\r\n?/\n/g;     # Get rid of \r.
$comment = trim($comment);
# If comment is all whitespace, it'll be null at this point. That's
# OK except for the fact that it causes e-mail to be suppressed.
$comment = $comment ? $comment : " ";

$sql .= "$::userid, now(), (0";

# Groups
foreach my $b (grep(/^bit-\d*$/, keys %::FORM)) {
    if ($::FORM{$b}) {
        my $v = substr($b, 4);
        $v =~ /^(\d+)$/
          || ThrowCodeError("One of the group bits submitted was invalid.",
                                                                undef, "abort");
        if (!GroupIsActive($v)) {
            # Prevent the user from adding the bug to an inactive group.
            # Should only happen if there is a bug in Bugzilla or the user
            # hacked the "enter bug" form since otherwise the UI 
            # for adding the bug to the group won't appear on that form.
            ThrowCodeError("Attempted to add bug to an inactive group, " . 
                           "identified by the bit '$v'.", undef, "abort");
        }
        $sql .= " + $v";    # Carefully written so that the math is
                            # done by MySQL, which can handle 64-bit math,
                            # and not by Perl, which I *think* can not.
    }
}

$sql .= ") & $::usergroupset)\n";

# Lock tables before inserting records for the new bug into the database
# if we are using a shadow database to prevent shadow database corruption
# when two bugs get created at the same time.
SendSQL("LOCK TABLES bugs WRITE, longdescs WRITE, cc WRITE") if Param("shadowdb");

# Add the bug report to the DB.
SendSQL($sql);

# Get the bug ID back.
SendSQL("select LAST_INSERT_ID()");
my $id = FetchOneColumn();

# Add the comment
SendSQL("INSERT INTO longdescs (bug_id, who, bug_when, thetext) 
         VALUES ($id, $::userid, now(), " . SqlQuote($comment) . ")");

my %ccids;
my $ccid;
my @cc;

# Add the CC list
if (defined $::FORM{'cc'}) {
    foreach my $person (split(/[ ,]/, $::FORM{'cc'})) {
        if ($person ne "") {
            $ccid = DBNameToIdAndCheck($person);
            if ($ccid && !$ccids{$ccid}) {
                SendSQL("INSERT INTO cc (bug_id, who) VALUES ($id, $ccid)");
                $ccids{$ccid} = 1;
                push(@cc, $person);
            }
        }
    }
}

SendSQL("UNLOCK TABLES") if Param("shadowdb");

# Assemble the -force* strings so this counts as "Added to this capacity"
my @ARGLIST = ();
if (@cc) {
    push (@ARGLIST, "-forcecc", join(",", @cc));
}

push (@ARGLIST, "-forceowner", DBID_to_name($::FORM{assigned_to}));

if (defined $::FORM{'qa_contact'}) {
    push (@ARGLIST, "-forceqacontact", DBID_to_name($::FORM{'qa_contact'}));
}

push (@ARGLIST, "-forcereporter", DBID_to_name($::userid));

push (@ARGLIST, $id, $::COOKIE{'Bugzilla_login'});

# Send mail to let people know the bug has been created.
# See attachment.cgi for explanation of why it's done this way.
my $mailresults = '';
open(PMAIL, "-|") or exec('./processmail', @ARGLIST);
$mailresults .= $_ while <PMAIL>;
close(PMAIL);

# Tell the user all about it
$vars->{'id'} = $id;
$vars->{'mail'} = $mailresults;
$vars->{'type'} = "created";

print "Content-type: text/html\n\n";
$template->process("bug/create/created.html.tmpl", $vars)
  || ThrowTemplateError($template->error());

$::FORM{'id'} = $id;

show_bug("header is already done");
