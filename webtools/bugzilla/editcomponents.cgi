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
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Holger
# Schurig. Portions created by Holger Schurig are
# Copyright (C) 1999 Holger Schurig. All
# Rights Reserved.
#
# Contributor(s): Holger Schurig <holgerschurig@nikocity.de>
#               Terry Weissman <terry@mozilla.org>
#
# Direct any questions on this source code to
#
# Holger Schurig <holgerschurig@nikocity.de>

use diagnostics;
use strict;

require "CGI.pl";
require "globals.pl";

# Shut up misguided -w warnings about "used only once".  For some reason,
# "use vars" chokes on me when I try it here.

sub sillyness {
    my $zz;
    $zz = $::buffer;
}


my $dobugcounts = (defined $::FORM{'dobugcounts'});



# TestProduct:    just returns if the specified product does exists
# CheckProduct:   same check, optionally  emit an error text
# TestComponent:  just returns if the specified product/component combination exists
# CheckComponent: same check, optionally emit an error text

sub TestProduct ($)
{
    my $prod = shift;

    # does the product exist?
    SendSQL("SELECT product
             FROM products
             WHERE product=" . SqlQuote($prod));
    return FetchOneColumn();
}

sub CheckProduct ($)
{
    my $prod = shift;

    # do we have a product?
    unless ($prod) {
        print "Sorry, you haven't specified a product.";
        PutTrailer();
        exit;
    }

    unless (TestProduct $prod) {
        print "Sorry, product '$prod' does not exist.";
        PutTrailer();
        exit;
    }
}

sub TestComponent ($$)
{
    my ($prod,$comp) = @_;

    # does the product exist?
    SendSQL("SELECT program,value
             FROM components
             WHERE program=" . SqlQuote($prod) . " and value=" . SqlQuote($comp));
    return FetchOneColumn();
}

sub CheckComponent ($$)
{
    my ($prod,$comp) = @_;

    # do we have the component?
    unless ($comp) {
        print "Sorry, you haven't specified a component.";
        PutTrailer();
        exit;
    }

    CheckProduct($prod);

    unless (TestComponent $prod,$comp) {
        print "Sorry, component '$comp' for product '$prod' does not exist.";
        PutTrailer();
        exit;
    }
}


#
# Displays the form to edit component parameters
#

sub EmitFormElements ($$$$$)
{
    my ($product, $component, $initialownerid, $initialqacontactid, $description) = @_;

    my ($initialowner, $initialqacontact) = ($initialownerid ? DBID_to_name ($initialownerid) : '',
                                             $initialqacontactid ? DBID_to_name ($initialqacontactid) : '');

    print "  <TH ALIGN=\"right\">Component:</TH>\n";
    print "  <TD><INPUT SIZE=64 MAXLENGTH=255 NAME=\"component\" VALUE=\"" .
        value_quote($component) . "\">\n";
    print "      <INPUT TYPE=HIDDEN NAME=\"product\" VALUE=\"" .
        value_quote($product) . "\"></TD>\n";

    print "</TR><TR>\n";
    print "  <TH ALIGN=\"right\">Description:</TH>\n";
    print "  <TD><TEXTAREA ROWS=4 COLS=64 WRAP=VIRTUAL NAME=\"description\">" .
        value_quote($description) . "</TEXTAREA></TD>\n";

    print "</TR><TR>\n";
    print "  <TH ALIGN=\"right\">Initial owner:</TH>\n";
    print "  <TD><INPUT TYPE=TEXT SIZE=64 MAXLENGTH=255 NAME=\"initialowner\" VALUE=\"" .
        value_quote($initialowner) . "\"></TD>\n";

    if (Param('useqacontact')) {
        print "</TR><TR>\n";
        print "  <TH ALIGN=\"right\">Initial QA contact:</TH>\n";
        print "  <TD><INPUT TYPE=TEXT SIZE=64 MAXLENGTH=255 NAME=\"initialqacontact\" VALUE=\"" .
            value_quote($initialqacontact) . "\"></TD>\n";
    }
}


#
# Displays a text like "a.", "a or b.", "a, b or c.", "a, b, c or d."
#

sub PutTrailer (@)
{
    my (@links) = ("Back to the <A HREF=\"query.cgi\">query page</A>", @_);

    my $count = $#links;
    my $num = 0;
    print "<P>\n";
    if (!$dobugcounts) {
        print qq{<a href="editcomponents.cgi?dobugcounts=1&$::buffer">};
        print qq{Redisplay table with bug counts (slower)</a><p>\n};
    }
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
# Preliminary checks:
#

confirm_login();

print "Content-type: text/html\n\n";

unless (UserInGroup("editcomponents")) {
    PutHeader("Not allowed");
    print "Sorry, you aren't a member of the 'editcomponents' group.\n";
    print "And so, you aren't allowed to add, modify or delete components.\n";
    PutTrailer();
    exit;
}


#
# often used variables
#
my $product   = trim($::FORM{product}   || '');
my $component = trim($::FORM{component} || '');
my $action    = trim($::FORM{action}    || '');
my $localtrailer;
if ($product) {
    $localtrailer = "<A HREF=\"editcomponents.cgi?product=" . url_quote($product) . "\">edit</A> more components";
} else {
    $localtrailer = "<A HREF=\"editcomponents.cgi\">edit</A> more components";
}



#
# product = '' -> Show nice list of products
#

unless ($product) {
    PutHeader("Select product");

    if ($dobugcounts){
        SendSQL("SELECT products.product,products.description,COUNT(bug_id)
             FROM products LEFT JOIN bugs
               ON products.product=bugs.product
             GROUP BY products.product
             ORDER BY products.product");
    } else {
        SendSQL("SELECT products.product,products.description
             FROM products 
             ORDER BY products.product");
    }
    print "<TABLE BORDER=1 CELLPADDING=4 CELLSPACING=0><TR BGCOLOR=\"#6666FF\">\n";
    print "  <TH ALIGN=\"left\">Edit components of ...</TH>\n";
    print "  <TH ALIGN=\"left\">Description</TH>\n";
    if ($dobugcounts) {
        print "  <TH ALIGN=\"left\">Bugs</TH>\n";
    }
    #print "  <TH ALIGN=\"left\">Edit</TH>\n";
    print "</TR>";
    while ( MoreSQLData() ) {
        my ($product, $description, $bugs) = FetchSQLData();
        $description ||= "<FONT COLOR=\"red\">missing</FONT>";
        print "<TR>\n";
        print "  <TD VALIGN=\"top\"><A HREF=\"editcomponents.cgi?product=", url_quote($product), "\"><B>$product</B></A></TD>\n";
        print "  <TD VALIGN=\"top\">$description</TD>\n";
        if ($dobugcounts) {
            $bugs ||= "none";
            print "  <TD VALIGN=\"top\">$bugs</TD>\n";
        }
        #print "  <TD VALIGN=\"top\"><A HREF=\"editproducts.cgi?action=edit&product=", url_quote($product), "\">Edit</A></TD>\n";
    }
    print "</TR></TABLE>\n";

    PutTrailer();
    exit;
}



#
# action='' -> Show nice list of components
#

unless ($action) {
    PutHeader("Select component of $product");
    CheckProduct($product);

    if ($dobugcounts) {
        SendSQL("SELECT value,description,initialowner,initialqacontact,COUNT(bug_id)
             FROM components LEFT JOIN bugs
               ON components.program=bugs.product AND components.value=bugs.component
             WHERE program=" . SqlQuote($product) . "
             GROUP BY value");
    } else {
        SendSQL("SELECT value,description,initialowner,initialqacontact
             FROM components 
             WHERE program=" . SqlQuote($product) . "
             GROUP BY value");
    }        
    print "<TABLE BORDER=1 CELLPADDING=4 CELLSPACING=0><TR BGCOLOR=\"#6666FF\">\n";
    print "  <TH ALIGN=\"left\">Edit component ...</TH>\n";
    print "  <TH ALIGN=\"left\">Description</TH>\n";
    print "  <TH ALIGN=\"left\">Initial owner</TH>\n";
    print "  <TH ALIGN=\"left\">Initial QA contact</TH>\n"
        if Param('useqacontact');
    print "  <TH ALIGN=\"left\">Bugs</TH>\n"
        if $dobugcounts;
    print "  <TH ALIGN=\"left\">Delete</TH>\n";
    print "</TR>";
    my @data;
    while (MoreSQLData()) {
        push @data, [FetchSQLData()];
    }
    foreach (@data) {
        my ($component,$desc,$initialownerid,$initialqacontactid, $bugs) = @$_;

        $desc             ||= "<FONT COLOR=\"red\">missing</FONT>";
        my $initialowner = $initialownerid ? DBID_to_name ($initialownerid) : "<FONT COLOR=\"red\">missing</FONT>";
        my $initialqacontact = $initialqacontactid ? DBID_to_name ($initialqacontactid) : "<FONT COLOR=\"red\">missing</FONT>";
        print "<TR>\n";
        print "  <TD VALIGN=\"top\"><A HREF=\"editcomponents.cgi?product=", url_quote($product), "&component=", url_quote($component), "&action=edit\"><B>$component</B></A></TD>\n";
        print "  <TD VALIGN=\"top\">$desc</TD>\n";
        print "  <TD VALIGN=\"top\">$initialowner</TD>\n";
        print "  <TD VALIGN=\"top\">$initialqacontact</TD>\n"
                if Param('useqacontact');
        if ($dobugcounts) {
            $bugs ||= 'none';
            print "  <TD VALIGN=\"top\">$bugs</TD>\n";
        }
        print "  <TD VALIGN=\"top\"><A HREF=\"editcomponents.cgi?product=", url_quote($product), "&component=", url_quote($component), "&action=del\"><B>Delete</B></A></TD>\n";
        print "</TR>";
    }
    print "<TR>\n";
    my $span = 3;
    $span++ if Param('useqacontact');
    $span++ if $dobugcounts;
    print "  <TD VALIGN=\"top\" COLSPAN=$span>Add a new component</TD>\n";
    print "  <TD VALIGN=\"top\" ALIGN=\"middle\"><A HREF=\"editcomponents.cgi?product=", url_quote($product) . "&action=add\">Add</A></TD>\n";
    print "</TR></TABLE>\n";

    PutTrailer();
    exit;
}


$dobugcounts = 1;               # Stupid hack to force further PutTrailer()
                                # calls to not offer a "bug count" option.


#
# action='add' -> present form for parameters for new component
#
# (next action will be 'new')
#

if ($action eq 'add') {
    PutHeader("Add component of $product");
    CheckProduct($product);

    #print "This page lets you add a new product to bugzilla.\n";

    print "<FORM METHOD=POST ACTION=editcomponents.cgi>\n";
    print "<TABLE BORDER=0 CELLPADDING=4 CELLSPACING=0><TR>\n";

    EmitFormElements($product, '', 0, 0, '');

    print "</TR></TABLE>\n<HR>\n";
    print "<INPUT TYPE=SUBMIT VALUE=\"Add\">\n";
    print "<INPUT TYPE=HIDDEN NAME=\"action\" VALUE=\"new\">\n";
    print "</FORM>";

    my $other = $localtrailer;
    $other =~ s/more/other/;
    PutTrailer($other);
    exit;
}



#
# action='new' -> add component entered in the 'action=add' screen
#

if ($action eq 'new') {
    PutHeader("Adding new component of $product");
    CheckProduct($product);

    # Cleanups and valididy checks

    unless ($component) {
        print "You must enter a name for the new component. Please press\n";
        print "<b>Back</b> and try again.\n";
        PutTrailer($localtrailer);
        exit;
    }
    if (TestComponent($product,$component)) {
        print "The component '$component' already exists. Please press\n";
        print "<b>Back</b> and try again.\n";
        PutTrailer($localtrailer);
        exit;
    }

    my $description = trim($::FORM{description} || '');

    if ($description eq '') {
        print "You must enter a description for the component '$component'. Please press\n";
        print "<b>Back</b> and try again.\n";
        PutTrailer($localtrailer);
        exit;
    }

    my $initialowner = trim($::FORM{initialowner} || '');
	#
	# Now validating to make sure it's too an existing account
	#
	DBNameToIdAndCheck($initialowner);

    if ($initialowner eq '') {
        print "You must enter an initial owner for the component '$component'. Please press\n";
        print "<b>Back</b> and try again.\n";
        PutTrailer($localtrailer);
        exit;
    }

    my $initialownerid = DBname_to_id ($initialowner);
    if (!$initialownerid) {
        print "You must use an existing Bugzilla account as initial owner for the component
'$component'. Please press\n";
        print "<b>Back</b> and try again.\n";
        PutTrailer($localtrailer);
        exit;
      }

    my $initialqacontact = trim($::FORM{initialqacontact} || '');
    my $initialqacontactid = DBname_to_id ($initialqacontact);
    if (Param('useqacontact')) {
        if ($initialqacontact eq '') {
            print "You must enter an initial QA contact for the component '$component'. Please press\n";
            print "<b>Back</b> and try again.\n";
            PutTrailer($localtrailer);
            exit;
        }

        if (!$initialqacontactid) {
            print "You must use an existing Bugzilla account as initial QA contact for the component '$component'. Please press\n";
            print "<b>Back</b> and try again.\n";
            PutTrailer($localtrailer);
            exit;
        }
	#
	# Now validating to make sure it's too an existing account
	#
	DBNameToIdAndCheck($initialqacontact);
    }

    # Add the new component
    SendSQL("INSERT INTO components ( " .
          "program, value, description, initialowner, initialqacontact " .
          " ) VALUES ( " .
          SqlQuote($product) . "," .
          SqlQuote($component) . "," .
          SqlQuote($description) . "," .
          SqlQuote($initialownerid) . "," .
          SqlQuote($initialqacontactid) . ")");

    # Make versioncache flush
    unlink "data/versioncache";

    print "OK, done.<p>\n";
    if ($product) {
        PutTrailer("<A HREF=\"editcomponents.cgi?product=" . url_quote($product) . "\">edit</A> more components or <A HREF=\"editcomponents.cgi?product=". url_quote($product) . "&action=add\">Add</A> another component");
    } else {
        PutTrailer("<A HREF=\"editcomponents.cgi\">edit</A> more components or <A HREF=\"editcomponents.cgi?action=add\">Add</A> another component");
    }
    exit;
}



#
# action='del' -> ask if user really wants to delete
#
# (next action would be 'delete')
#

if ($action eq 'del') {
    PutHeader("Delete component of $product");
    CheckComponent($product, $component);

    # display some data about the component
    SendSQL("SELECT products.product,products.description,
                products.milestoneurl,products.disallownew,
                components.program,components.value,components.initialowner,
                components.initialqacontact,components.description
             FROM products
             LEFT JOIN components on product=program
             WHERE product=" . SqlQuote($product) . "
               AND   value=" . SqlQuote($component) );


    my ($product,$pdesc,$milestoneurl,$disallownew,
        $dummy,$component,$initialownerid,$initialqacontactid,$cdesc) = FetchSQLData();

    my $initialowner = $initialownerid ? DBID_to_name ($initialownerid) : "<FONT COLOR=\"red\">missing</FONT>";
    my $initialqacontact = $initialqacontactid ? DBID_to_name ($initialqacontactid) : "<FONT COLOR=\"red\">missing</FONT>";
    my $milestonelink = $milestoneurl ? "<A HREF=\"$milestoneurl\">$milestoneurl</A>"
                                      : "<FONT COLOR=\"red\">missing</FONT>";
    $pdesc            ||= "<FONT COLOR=\"red\">missing</FONT>";
    $disallownew        = $disallownew ? 'closed' : 'open';
    $cdesc            ||= "<FONT COLOR=\"red\">missing</FONT>";
    
    print "<TABLE BORDER=1 CELLPADDING=4 CELLSPACING=0><TR BGCOLOR=\"#6666FF\">\n";
    print "  <TH VALIGN=\"top\" ALIGN=\"left\">Part</TH>\n";
    print "  <TH VALIGN=\"top\" ALIGN=\"left\">Value</TH>\n";

    print "</TR><TR>\n";
    print "  <TD VALIGN=\"top\">Component:</TD>\n";
    print "  <TD VALIGN=\"top\">$component</TD>";

    print "</TR><TR>\n";
    print "  <TD VALIGN=\"top\">Component description:</TD>\n";
    print "  <TD VALIGN=\"top\">$cdesc</TD>";

    print "</TR><TR>\n";
    print "  <TD VALIGN=\"top\">Initial owner:</TD>\n";
    print "  <TD VALIGN=\"top\">$initialowner</TD>";

    if (Param('useqacontact')) {
        print "</TR><TR>\n";
        print "  <TD VALIGN=\"top\">Initial QA contact:</TD>\n";
        print "  <TD VALIGN=\"top\">$initialqacontact</TD>";
    }
    SendSQL("SELECT count(bug_id)
             FROM bugs
             WHERE product=" . SqlQuote($product) . "
                AND component=" . SqlQuote($component));

    print "</TR><TR>\n";
    print "  <TD VALIGN=\"top\">Component of product:</TD>\n";
    print "  <TD VALIGN=\"top\">$product</TD>\n";

    print "</TR><TR>\n";
    print "  <TD VALIGN=\"top\">Description:</TD>\n";
    print "  <TD VALIGN=\"top\">$pdesc</TD>\n";

    if (Param('usetargetmilestone')) {
         print "</TR><TR>\n";
         print "  <TD VALIGN=\"top\">Milestone URL:</TD>\n";
         print "  <TD VALIGN=\"top\">$milestonelink</TD>\n";
    }

    print "</TR><TR>\n";
    print "  <TD VALIGN=\"top\">Closed for bugs:</TD>\n";
    print "  <TD VALIGN=\"top\">$disallownew</TD>\n";

    print "</TR><TR>\n";
    print "  <TD VALIGN=\"top\">Bugs</TD>\n";
    print "  <TD VALIGN=\"top\">";
    my $bugs = FetchOneColumn();
    print $bugs || 'none';


    print "</TD>\n</TR></TABLE>";

    print "<H2>Confirmation</H2>\n";

    if ($bugs) {
        if (!Param("allowbugdeletion")) {
            print "Sorry, there are $bugs bugs outstanding for this component. 
You must reassign those bugs to another component before you can delete this
one.";
            PutTrailer($localtrailer);
            exit;
        }
        print "<TABLE BORDER=0 CELLPADDING=20 WIDTH=\"70%\" BGCOLOR=\"red\"><TR><TD>\n",
              "There are bugs entered for this component!  When you delete this ",
              "component, <B><BLINK>all</BLINK></B> stored bugs will be deleted, too. ",
              "You could not even see the bug history for this component anymore!\n",
              "</TD></TR></TABLE>\n";
    }

    print "<P>Do you really want to delete this component?<P>\n";

    print "<FORM METHOD=POST ACTION=editcomponents.cgi>\n";
    print "<INPUT TYPE=SUBMIT VALUE=\"Yes, delete\">\n";
    print "<INPUT TYPE=HIDDEN NAME=\"action\" VALUE=\"delete\">\n";
    print "<INPUT TYPE=HIDDEN NAME=\"product\" VALUE=\"" .
        value_quote($product) . "\">\n";
    print "<INPUT TYPE=HIDDEN NAME=\"component\" VALUE=\"" .
        value_quote($component) . "\">\n";
    print "</FORM>";

    PutTrailer($localtrailer);
    exit;
}



#
# action='delete' -> really delete the component
#

if ($action eq 'delete') {
    PutHeader("Deleting component of $product");
    CheckComponent($product,$component);

    # lock the tables before we start to change everything:

    SendSQL("LOCK TABLES attachments WRITE,
                         bugs WRITE,
                         bugs_activity WRITE,
                         components WRITE,
                         dependencies WRITE");

    # According to MySQL doc I cannot do a DELETE x.* FROM x JOIN Y,
    # so I have to iterate over bugs and delete all the indivial entries
    # in bugs_activies and attachments.

    if (Param("allowbugdeletion")) {
        SendSQL("SELECT bug_id
             FROM bugs
             WHERE product=" . SqlQuote($product) . "
               AND component=" . SqlQuote($component));
        while (MoreSQLData()) {
            my $bugid = FetchOneColumn();

            PushGlobalSQLState();
            SendSQL("DELETE FROM attachments WHERE bug_id=$bugid");
            SendSQL("DELETE FROM bugs_activity WHERE bug_id=$bugid");
            SendSQL("DELETE FROM dependencies WHERE blocked=$bugid");
            PopGlobalSQLState();
        }
        print "Attachments, bug activity and dependencies deleted.<BR>\n";


        # Deleting the rest is easier:

        SendSQL("DELETE FROM bugs
             WHERE product=" . SqlQuote($product) . "
               AND component=" . SqlQuote($component));
        print "Bugs deleted.<BR>\n";
    }

    SendSQL("DELETE FROM components
             WHERE program=" . SqlQuote($product) . "
               AND value=" . SqlQuote($component));
    print "Components deleted.<P>\n";
    SendSQL("UNLOCK TABLES");

    unlink "data/versioncache";
    PutTrailer($localtrailer);
    exit;
}



#
# action='edit' -> present the edit component form
#
# (next action would be 'update')
#

if ($action eq 'edit') {
    PutHeader("Edit component of $product");
    CheckComponent($product,$component);

    # get data of component
    SendSQL("SELECT products.product,products.description,
                products.milestoneurl,products.disallownew,
                components.program,components.value,components.initialowner,
                components.initialqacontact,components.description
             FROM products
             LEFT JOIN components on product=program
             WHERE product=" . SqlQuote($product) . "
               AND   value=" . SqlQuote($component) );

    my ($product,$pdesc,$milestoneurl,$disallownew,
        $dummy,$component,$initialownerid,$initialqacontactid,$cdesc) = FetchSQLData();

    my $initialowner = $initialownerid ? DBID_to_name ($initialownerid) : '';
    my $initialqacontact = $initialqacontactid ? DBID_to_name ($initialqacontactid) : '';

    print "<FORM METHOD=POST ACTION=editcomponents.cgi>\n";
    print "<TABLE BORDER=0 CELLPADDING=4 CELLSPACING=0><TR>\n";

    #+++ display product/product description

    EmitFormElements($product, $component, $initialownerid, $initialqacontactid, $cdesc);

    print "</TR><TR>\n";
    print "  <TH ALIGN=\"right\">Bugs:</TH>\n";
    print "  <TD>";
    SendSQL("SELECT count(*)
             FROM bugs
             WHERE product=" . SqlQuote($product) .
            " and component=" . SqlQuote($component));
    my $bugs = '';
    $bugs = FetchOneColumn() if MoreSQLData();
    print $bugs || 'none';

    print "</TD>\n</TR></TABLE>\n";

    print "<INPUT TYPE=HIDDEN NAME=\"componentold\" VALUE=\"" .
        value_quote($component) . "\">\n";
    print "<INPUT TYPE=HIDDEN NAME=\"descriptionold\" VALUE=\"" .
        value_quote($cdesc) . "\">\n";
    print "<INPUT TYPE=HIDDEN NAME=\"initialownerold\" VALUE=\"" .
        value_quote($initialowner) . "\">\n";
    print "<INPUT TYPE=HIDDEN NAME=\"initialqacontactold\" VALUE=\"" .
        value_quote($initialqacontact) . "\">\n";
    print "<INPUT TYPE=HIDDEN NAME=\"action\" VALUE=\"update\">\n";
    print "<INPUT TYPE=SUBMIT VALUE=\"Update\">\n";

    print "</FORM>";

    my $other = $localtrailer;
    $other =~ s/more/other/;
    PutTrailer($other);
    exit;
}



#
# action='update' -> update the component
#

if ($action eq 'update') {
    PutHeader("Update component of $product");

    my $componentold        = trim($::FORM{componentold}        || '');
    my $description         = trim($::FORM{description}         || '');
    my $descriptionold      = trim($::FORM{descriptionold}      || '');
    my $initialowner        = trim($::FORM{initialowner}        || '');
    my $initialownerold     = trim($::FORM{initialownerold}     || '');
    my $initialqacontact    = trim($::FORM{initialqacontact}    || '');
    my $initialqacontactold = trim($::FORM{initialqacontactold} || '');

    CheckComponent($product,$componentold);

    # Note that the order of this tests is important. If you change
    # them, be sure to test for WHERE='$component' or WHERE='$componentold'

    SendSQL("LOCK TABLES bugs WRITE,
                         components WRITE, profiles READ");

    if ($description ne $descriptionold) {
        unless ($description) {
            print "Sorry, I can't delete the description.";
            PutTrailer($localtrailer);
	    SendSQL("UNLOCK TABLES");
            exit;
        }
        SendSQL("UPDATE components
                 SET description=" . SqlQuote($description) . "
                 WHERE program=" . SqlQuote($product) . "
                   AND value=" . SqlQuote($componentold));
        print "Updated description.<BR>\n";
    }


    if ($initialowner ne $initialownerold) {
        unless ($initialowner) {
            print "Sorry, I can't delete the initial owner.";
	    SendSQL("UNLOCK TABLES");
            PutTrailer($localtrailer);
            exit;
        }

        my $initialownerid = DBname_to_id($initialowner);
        unless ($initialownerid) {
            print "Sorry, you must use an existing Bugzilla account as initial owner.";
            SendSQL("UNLOCK TABLES");
            PutTrailer($localtrailer);
            exit;
        }

        SendSQL("UPDATE components
                 SET initialowner=" . SqlQuote($initialownerid) . "
                 WHERE program=" . SqlQuote($product) . "
                   AND value=" . SqlQuote($componentold));
        print "Updated initial owner.<BR>\n";
    }

    if (Param('useqacontact') && $initialqacontact ne $initialqacontactold) {
        unless ($initialqacontact) {
            print "Sorry, I can't delete the initial QA contact.";
	    SendSQL("UNLOCK TABLES");
            PutTrailer($localtrailer);
            exit;
        }

        my $initialqacontactid = DBname_to_id($initialqacontact);
        unless ($initialqacontactid) {
            print "Sorry, you must use an existing Bugzilla account as initial QA contact.";
            SendSQL("UNLOCK TABLES");
            PutTrailer($localtrailer);
            exit;
        }

        SendSQL("UPDATE components
                 SET initialqacontact=" . SqlQuote($initialqacontactid) . "
                 WHERE program=" . SqlQuote($product) . "
                   AND value=" . SqlQuote($componentold));
        print "Updated initial QA contact.<BR>\n";
    }


    if ($component ne $componentold) {
        unless ($component) {
            print "Sorry, I can't delete the product name.";
            PutTrailer($localtrailer);
	    SendSQL("UNLOCK TABLES");
            exit;
        }
        if (TestComponent($product,$component)) {
            print "Sorry, component name '$component' is already in use.";
            PutTrailer($localtrailer);
	    SendSQL("UNLOCK TABLES");
            exit;
        }

        SendSQL("UPDATE bugs
                 SET component=" . SqlQuote($component) . "
                 WHERE component=" . SqlQuote($componentold) . "
                   AND product=" . SqlQuote($product));
        SendSQL("UPDATE components
                 SET value=" . SqlQuote($component) . "
                 WHERE value=" . SqlQuote($componentold) . "
                   AND program=" . SqlQuote($product));

        unlink "data/versioncache";
        print "Updated product name.<BR>\n";
    }
    SendSQL("UNLOCK TABLES");

    PutTrailer($localtrailer);
    exit;
}



#
# No valid action found
#

PutHeader("Error");
print "I don't have a clue what you want.<BR>\n";

foreach ( sort keys %::FORM) {
    print "$_: $::FORM{$_}<BR>\n";
}
