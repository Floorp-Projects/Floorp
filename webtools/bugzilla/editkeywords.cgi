#!/usr/bonsaitools/bin/perl -w
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

use diagnostics;
use strict;

require "CGI.pl";

my $localtrailer = "<A HREF=\"editkeywords.cgi\">edit</A> more keywords";


#
# Displays a text like "a.", "a or b.", "a, b or c.", "a, b, c or d."
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

confirm_login();

print "Content-type: text/html\n\n";

unless (UserInGroup("editkeywords")) {
    PutHeader("Not allowed");
    print "Sorry, you aren't a member of the 'editkeywords' group.\n";
    print "And so, you aren't allowed to add, modify or delete keywords.\n";
    PutTrailer();
    exit;
}


my $action  = trim($::FORM{action}  || '');


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
                    COUNT(keywords.bug_id), keywords.bug_id
             FROM keyworddefs LEFT JOIN keywords ON keyworddefs.id = keywords.keywordid
             GROUP BY keyworddefs.id
             ORDER BY keyworddefs.name");
    while (MoreSQLData()) {
        my ($id, $name, $description, $bugs, $onebug) = FetchSQLData();
        $description ||= "<FONT COLOR=\"red\">missing</FONT>";
        $bugs ||= 'none';
        if (!$onebug) {
            # This is silly hackery for old versions of MySQL that seem to
            # return a count() of 1 even if there are no matching.  So, we 
            # ask for an actual bug number.  If it can't find any bugs that
            # match the keyword, then we set the count to be zero, ignoring
            # what it had responded.
            $bugs = 'none';
        }
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
    PutHeader("Add keyword");
    print "<FORM METHOD=POST ACTION=editkeywords.cgi>\n";
    print "<TABLE BORDER=0 CELLPADDING=4 CELLSPACING=0>\n";

    EmitFormElements(-1, '', '');

    print "</TABLE>\n<HR>\n";
    print "<INPUT TYPE=SUBMIT VALUE=\"Add\">\n";
    print "<INPUT TYPE=HIDDEN NAME=\"action\" VALUE=\"new\">\n";
    print "</FORM>";

    my $other = $localtrailer;
    $other =~ s/more/other/;
    PutTrailer($other);
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
    unlink "data/versioncache";

    print "OK, done.<p>\n";
    PutTrailer("<A HREF=\"editkeywords.cgi\">edit</A> more keywords or <A HREF=\"editkeywords.cgi?action=add\">add</a> another keyword");
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
    PutHeader("Update keyword");

    my $id = $::FORM{id};
    my $name  = trim($::FORM{name} || '');
    my $description  = trim($::FORM{description}  || '');

    Validate($name, $description);

    SendSQL("SELECT id FROM keyworddefs WHERE name = " . SqlQuote($name));

    my $tmp = FetchOneColumn();

    if ($tmp && $tmp != $id) {
        print "The keyword '$name' already exists. Please press\n";
        print "<b>Back</b> and try again.\n";
        PutTrailer($localtrailer);
        exit;
    }

    SendSQL("UPDATE keyworddefs SET name = " . SqlQuote($name) .
            ", description = " . SqlQuote($description) .
            " WHERE id = $id");

    print "Keyword updated.<BR>\n";

    &RebuildCacheWarning;
    # Make versioncache flush
    unlink "data/versioncache";

    PutTrailer($localtrailer);
    exit;
}


if ($action eq 'delete') {
    PutHeader("Delete keyword");
    my $id = $::FORM{id};

    SendSQL("SELECT name FROM keyworddefs WHERE id=$id");
    my $name = FetchOneColumn();

    if (!$::FORM{reallydelete}) {

        SendSQL("SELECT count(*)
                 FROM keywords
                 WHERE keywordid = $id");
        
        my $bugs = FetchOneColumn();
        
        if ($bugs) {
            
            
            print qq{
There are $bugs bugs which have this keyword set.  Are you <b>sure</b> you want
to delete the <code>$name</code> keyword?

<FORM METHOD=POST ACTION=editkeywords.cgi>
<INPUT TYPE=HIDDEN NAME="id" VALUE="$id">
<INPUT TYPE=HIDDEN NAME="action" VALUE="delete">
<INPUT TYPE=HIDDEN NAME="reallydelete" VALUE="1">
<INPUT TYPE=SUBMIT VALUE="Yes, really delete the keyword">
</FORM>
};

            PutTrailer($localtrailer);
            exit;
        }
    }

    SendSQL("DELETE FROM keywords WHERE keywordid = $id");
    SendSQL("DELETE FROM keyworddefs WHERE id = $id");

    print "Keyword $name deleted.\n";

    &RebuildCacheWarning;
    # Make versioncache flush
    unlink "data/versioncache";

    PutTrailer($localtrailer);
    exit;
}

PutHeader("Error");
print "I don't have a clue what you want.<BR>\n";

foreach ( sort keys %::FORM) {
    print "$_: $::FORM{$_}<BR>\n";
}



sub RebuildCacheWarning {

    print "<BR><BR><B>You have deleted or modified a keyword. You must rebuild the keyword cache!<BR></B>";
    print "You can rebuild the cache using sanitycheck.cgi. On very large installations of Bugzilla,<BR>";
    print "This can take several minutes.<BR><BR><B><A HREF=sanitycheck.cgi?rebuildkeywordcache=1>Rebuild cache</HREF><BR></B>";

}


