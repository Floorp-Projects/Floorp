#!/usr/bin/perl -w
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
# The Initial Developer of the Original Code is Terry Weissman.
# Portions created by Terry Weissman are
# Copyright (C) 2000 Terry Weissman. All
# Rights Reserved.
#
# Contributor(s): Terry Weissman <terry@mozilla.org>

use strict;
use lib ".";

require "CGI.pl";

use Bugzilla::Config qw(:DEFAULT $datadir);

use vars qw($template $vars);

my $localtrailer = "<A HREF=\"editkeywords.cgi\">edit</A> more keywords";


#
# Displays a text like "a.", "a or b.", "a, b or c.", "a, b, c or d."
# 
# XXX This implementation of PutTrailer outputs a default link back to
# the query page instead of the index, which is inconsistent with other
# PutTrailer() implementations.
#

sub PutTrailer (@)
{
    my (@links) = ("Back to the <A HREF=\"query.cgi\">query page</A>", @_);

    my $count = $#links;
    my $num = 0;
    print "<P>\n";
    foreach (@links) {
        print $_;
        if ($num == $count) {
            print ".\n";
        }
        elsif ($num == $count-1) {
            print " or ";
        }
        else {
            print ", ";
        }
        $num++;
    }
    PutFooter();
}


#
# Displays the form to edit a keyword's parameters
#

sub EmitFormElements ($$$)
{
    my ($id, $name, $description) = @_;

    $name = value_quote($name);
    $description = value_quote($description);

    print qq{<INPUT TYPE="HIDDEN" NAME=id VALUE=$id>};

    print "  <TR><TH ALIGN=\"right\">Name:</TH>\n";
    print "  <TD><INPUT SIZE=64 MAXLENGTH=64 NAME=\"name\" VALUE=\"$name\"></TD>\n";
    print "</TR><TR>\n";

    print "  <TH ALIGN=\"right\">Description:</TH>\n";
    print "  <TD><TEXTAREA ROWS=4 COLS=64 WRAP=VIRTUAL NAME=\"description\">$description</TEXTAREA></TD>\n";
    print "</TR>\n";

}


sub Validate ($$) {
    my ($name, $description) = @_;
    if ($name eq "") {
        print "You must enter a non-blank name for the keyword. Please press\n";
        print "<b>Back</b> and try again.\n";
        PutTrailer($localtrailer);
        exit;
    }
    if ($name =~ /[\s,]/) {
        print "You may not use commas or whitespace in a keyword name.\n";
        print "Please press <b>Back</b> and try again.\n";
        PutTrailer($localtrailer);
        exit;
    }    
    if ($description eq "") {
        print "You must enter a non-blank description of the keyword.\n";
        print "Please press <b>Back</b> and try again.\n";
        PutTrailer($localtrailer);
        exit;
    }
}


#
# Preliminary checks:
#

ConnectToDatabase();
confirm_login();

print Bugzilla->cgi->header();

unless (UserInGroup("editkeywords")) {
    PutHeader("Not allowed");
    print "Sorry, you aren't a member of the 'editkeywords' group.\n";
    print "And so, you aren't allowed to add, modify or delete keywords.\n";
    PutTrailer();
    exit;
}


my $action  = trim($::FORM{action}  || '');
$vars->{'action'} = $action;

detaint_natural($::FORM{id});


if ($action eq "") {
    PutHeader("Select keyword");
    my $tableheader = qq{
<TABLE BORDER=1 CELLPADDING=4 CELLSPACING=0>
<TR BGCOLOR="#6666FF">
<TH ALIGN="left">Edit keyword ...</TH>
<TH ALIGN="left">Description</TH>
<TH ALIGN="left">Bugs</TH>
<TH ALIGN="left">Action</TH>
</TR>
};
    print $tableheader;
    my $line_count = 0;
    my $max_table_size = 50;

    SendSQL("SELECT keyworddefs.id, keyworddefs.name, keyworddefs.description,
                    COUNT(keywords.bug_id)
             FROM keyworddefs LEFT JOIN keywords ON keyworddefs.id = keywords.keywordid
             GROUP BY keyworddefs.id
             ORDER BY keyworddefs.name");
    while (MoreSQLData()) {
        my ($id, $name, $description, $bugs) = FetchSQLData();
        $description ||= "<FONT COLOR=\"red\">missing</FONT>";
        $bugs ||= 'none';
        if ($line_count == $max_table_size) {
            print "</table>\n$tableheader";
            $line_count = 0;
        }
        $line_count++;
            
        print qq{
<TR>
<TH VALIGN="top"><A HREF="editkeywords.cgi?action=edit&id=$id">$name</TH>
<TD VALIGN="top">$description</TD>
<TD VALIGN="top" ALIGN="right">$bugs</TD>
<TH VALIGN="top"><A HREF="editkeywords.cgi?action=delete&id=$id">Delete</TH>
</TR>
};
    }
    print qq{
<TR>
<TD VALIGN="top" COLSPAN=3>Add a new keyword</TD><TD><A HREF="editkeywords.cgi?action=add">Add</TD>
</TR>
</TABLE>
};
    PutTrailer();
    exit;
}
    

if ($action eq 'add') {
    print Bugzilla->cgi->header();

    $template->process("admin/keywords/create.html.tmpl",
                       $vars)
      || ThrowTemplateError($template->error());

    exit;
}

#
# action='new' -> add keyword entered in the 'action=add' screen
#

if ($action eq 'new') {
    PutHeader("Adding new keyword");

    # Cleanups and valididy checks

    my $name = trim($::FORM{name} || '');
    my $description  = trim($::FORM{description}  || '');

    Validate($name, $description);
    
    SendSQL("SELECT id FROM keyworddefs WHERE name = " . SqlQuote($name));

    if (FetchOneColumn()) {
        print "The keyword '$name' already exists. Please press\n";
        print "<b>Back</b> and try again.\n";
        PutTrailer($localtrailer);
        exit;
    }


    # Pick an unused number.  Be sure to recycle numbers that may have been
    # deleted in the past.  This code is potentially slow, but it happens
    # rarely enough, and there really aren't ever going to be that many
    # keywords anyway.

    SendSQL("SELECT id FROM keyworddefs ORDER BY id");

    my $newid = 1;

    while (MoreSQLData()) {
        my $oldid = FetchOneColumn();
        if ($oldid > $newid) {
            last;
        }
        $newid = $oldid + 1;
    }

    # Add the new keyword.
    SendSQL("INSERT INTO keyworddefs (id, name, description) VALUES ($newid, " .
            SqlQuote($name) . "," .
            SqlQuote($description) . ")");

    # Make versioncache flush
    unlink "$datadir/versioncache";

    print "OK, done.<p>\n";
    PutTrailer("<a href=\"editkeywords.cgi\">edit</a> more keywords",
        "<a href=\"editkeywords.cgi?action=add\">add</a> another keyword");
    exit;
}

    

#
# action='edit' -> present the edit keywords from
#
# (next action would be 'update')
#

if ($action eq 'edit') {
    PutHeader("Edit keyword");

    my $id  = trim($::FORM{id} || 0);
    # get data of keyword
    SendSQL("SELECT name,description
             FROM keyworddefs
             WHERE id=$id");
    my ($name, $description) = FetchSQLData();
    if (!$name) {
        print "Something screwy is going on.  Please try again.\n";
        PutTrailer($localtrailer);
        exit;
    }
    print "<FORM METHOD=POST ACTION=editkeywords.cgi>\n";
    print "<TABLE  BORDER=0 CELLPADDING=4 CELLSPACING=0>\n";

    EmitFormElements($id, $name, $description);
    
    print "<TR>\n";
    print "  <TH ALIGN=\"right\">Bugs:</TH>\n";
    print "  <TD>";
    SendSQL("SELECT count(*)
             FROM keywords
             WHERE keywordid = $id");
    my $bugs = '';
    $bugs = FetchOneColumn() if MoreSQLData();
    print $bugs || 'none';

    print "</TD>\n</TR></TABLE>\n";

    print "<INPUT TYPE=HIDDEN NAME=\"action\" VALUE=\"update\">\n";
    print "<INPUT TYPE=SUBMIT VALUE=\"Update\">\n";

    print "</FORM>";

    my $x = $localtrailer;
    $x =~ s/more/other/;
    PutTrailer($x);
    exit;
}


#
# action='update' -> update the keyword
#

if ($action eq 'update') {
    my $id = $::FORM{id};
    my $name  = trim($::FORM{name} || '');
    my $description  = trim($::FORM{description}  || '');

    Validate($name, $description);

    SendSQL("SELECT id FROM keyworddefs WHERE name = " . SqlQuote($name));

    my $tmp = FetchOneColumn();

    if ($tmp && $tmp != $id) {
        PutHeader("Update keyword");

        print "The keyword '$name' already exists. Please press\n";
        print "<b>Back</b> and try again.\n";
        PutTrailer($localtrailer);
        exit;
    }

    SendSQL("UPDATE keyworddefs SET name = " . SqlQuote($name) .
            ", description = " . SqlQuote($description) .
            " WHERE id = $id");

    # Make versioncache flush
    unlink "$datadir/versioncache";

    print Bugzilla->cgi->header();

    $vars->{'name'} = $name;
    $template->process("admin/keywords/rebuild-cache.html.tmpl",
                       $vars)
      || ThrowTemplateError($template->error());

    exit;
}


if ($action eq 'delete') {
    my $id = $::FORM{id};

    SendSQL("SELECT name FROM keyworddefs WHERE id=$id");
    my $name = FetchOneColumn();

    if (!$::FORM{reallydelete}) {
        SendSQL("SELECT count(*)
                 FROM keywords
                 WHERE keywordid = $id");
        
        my $bugs = FetchOneColumn();
        
        if ($bugs) {
            $vars->{'bug_count'} = $bugs;
            $vars->{'keyword_id'} = $id;
            $vars->{'name'} = $name;

            print Bugzilla->cgi->header();

            $template->process("admin/keywords/confirm-delete.html.tmpl",
                               $vars)
              || ThrowTemplateError($template->error());

            exit;
        }
    }

    SendSQL("DELETE FROM keywords WHERE keywordid = $id");
    SendSQL("DELETE FROM keyworddefs WHERE id = $id");

    # Make versioncache flush
    unlink "$datadir/versioncache";

    print Bugzilla->cgi->header();

    $vars->{'name'} = $name;
    $template->process("admin/keywords/rebuild-cache.html.tmpl",
                       $vars)
      || ThrowTemplateError($template->error());

    exit;
}

ThrowCodeError("action_unrecognized", $vars);
