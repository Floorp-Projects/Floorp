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
#                 Gervase Markham <gerv@gerv.net>

use diagnostics;
use strict;
use lib ".";

use lib qw(.);

require "CGI.pl";

# Shut up misguided -w warnings about "used only once".  "use vars" just
# doesn't work for me.

sub sillyness {
    my $zz;
    $zz = $::userid;
    $zz = $::usergroupset;
    $zz = %::FORM;
}

# Use the template toolkit (http://www.template-toolkit.org/) to generate
# the user interface (HTML pages and mail messages) using templates in the
# "template/" subdirectory.
use Template;

# Create the global template object that processes templates and specify
# configuration parameters that apply to all templates processed in this script.
my $template = Template->new(
{
    # Colon-separated list of directories containing templates.
    INCLUDE_PATH => "template/custom:template/default",
    # Allow templates to be specified with relative paths.
    RELATIVE => 1,
    PRE_CHOMP => 1,
});

# Define the global variables and functions that will be passed to the UI 
# template.  Individual functions add their own values to this hash before
# sending them to the templates they process.
my $vars = 
{
    # Function for retrieving global parameters.
    'Param' => \&Param, 

    # Function for processing global parameters that contain references
    # to other global parameters.
    'PerformSubsts' => \&PerformSubsts,
    
    'quoteUrls' => \&quoteUrls,
    'time2str' => \&time2str,
    'str2time' => \&str2time,
};

ConnectToDatabase();
quietly_check_login();

GetVersionTable();

my $generic_query  = "
  SELECT bugs.bug_id, bugs.product, bugs.version, bugs.rep_platform,
  bugs.op_sys, bugs.bug_status, bugs.resolution, bugs.priority,
  bugs.bug_severity, bugs.component, assign.login_name, report.login_name,
  bugs.bug_file_loc, bugs.short_desc, bugs.target_milestone,
  bugs.qa_contact, bugs.status_whiteboard, bugs.keywords
  FROM bugs,profiles assign,profiles report
  WHERE assign.userid = bugs.assigned_to AND report.userid = bugs.reporter";

my $buglist = $::FORM{'buglist'} || 
              $::FORM{'bug_id'}  || 
              $::FORM{'id'}      || "";

my @bugs;

foreach my $bug_id (split(/[:,]/, $buglist)) {
    detaint_natural($bug_id) || next;
    SendSQL(SelectVisible("$generic_query AND bugs.bug_id = $bug_id",
                          $::userid, $::usergroupset));

    my %bug;
    my @row = FetchSQLData();

    foreach my $field ("bug_id", "product", "version", "rep_platform",
                       "op_sys", "bug_status", "resolution", "priority",
                       "bug_severity", "component", "assigned_to", "reporter",
                       "bug_file_loc", "short_desc", "target_milestone",
                       "qa_contact", "status_whiteboard", "keywords") 
    {
        $bug{$field} = shift @row;
    }
    
    if ($bug{'bug_id'}) {
        $bug{'comments'} = GetComments($bug{'bug_id'});
        $bug{'qa_contact'} = $bug{'qa_contact'} > 0 ? 
                                          DBID_to_name($bug{'qa_contact'}) : "";

        push (@bugs, \%bug);
    }
}

# Add the bug list of hashes to the variables
$vars->{'bugs'} = \@bugs;

$vars->{'use_keywords'} = 1 if (@::legal_keywords);

print "Content-type: text/html\n";
print "Content-disposition: inline; filename=bugzilla_bug_list.html\n\n";

# Generate and return the UI (HTML page) from the appropriate template.
$template->process("show/multiple.tmpl", $vars)
  || DisplayError("Template process failed: " . $template->error())
  && exit;
