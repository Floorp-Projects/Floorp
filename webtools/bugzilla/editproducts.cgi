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
#               Dawn Endico <endico@mozilla.org>
#               Joe Robins <jmrobins@tgix.com>
#
# Direct any questions on this source code to
#
# Holger Schurig <holgerschurig@nikocity.de>

use strict;
use lib ".";

require "CGI.pl";
require "globals.pl";

# Shut up misguided -w warnings about "used only once".  "use vars" just
# doesn't work for me.

sub sillyness {
    my $zz;
    $zz = $::unconfirmedstate;
}


# TestProduct:  just returns if the specified product does exists
# CheckProduct: same check, optionally  emit an error text

sub TestProduct ($)
{
    my $prod = shift;

    # does the product exist?
    SendSQL("SELECT name
             FROM products
             WHERE name=" . SqlQuote($prod));
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


#
# Displays the form to edit a products parameters
#

sub EmitFormElements ($$$$$$$$)
{
    my ($product, $description, $milestoneurl, $disallownew,
        $votesperuser, $maxvotesperbug, $votestoconfirm, $defaultmilestone)
        = @_;

    $product = value_quote($product);
    $description = value_quote($description);

    print "  <TH ALIGN=\"right\">Product:</TH>\n";
    print "  <TD><INPUT SIZE=64 MAXLENGTH=64 NAME=\"product\" VALUE=\"$product\"></TD>\n";
    print "</TR><TR>\n";

    print "  <TH ALIGN=\"right\">Description:</TH>\n";
    print "  <TD><TEXTAREA ROWS=4 COLS=64 WRAP=VIRTUAL NAME=\"description\">$description</TEXTAREA></TD>\n";

    $defaultmilestone = value_quote($defaultmilestone);
    if (Param('usetargetmilestone')) {
        $milestoneurl = value_quote($milestoneurl);
        print "</TR><TR>\n";
        print "  <TH ALIGN=\"right\">URL describing milestones for this product:</TH>\n";
        print "  <TD><INPUT TYPE=TEXT SIZE=64 MAXLENGTH=255 NAME=\"milestoneurl\" VALUE=\"$milestoneurl\"></TD>\n";

        print "</TR><TR>\n";
        print "  <TH ALIGN=\"right\">Default milestone:</TH>\n";
        
        print "  <TD><INPUT TYPE=TEXT SIZE=20 MAXLENGTH=20 NAME=\"defaultmilestone\" VALUE=\"$defaultmilestone\"></TD>\n";
    } else {
        print qq{<INPUT TYPE=HIDDEN NAME="defaultmilestone" VALUE="$defaultmilestone">\n};
    }


    print "</TR><TR>\n";
    print "  <TH ALIGN=\"right\">Closed for bug entry:</TH>\n";
    my $closed = $disallownew ? "CHECKED" : "";
    print "  <TD><INPUT TYPE=CHECKBOX NAME=\"disallownew\" $closed VALUE=\"1\"></TD>\n";

    print "</TR><TR>\n";
    print "  <TH ALIGN=\"right\">Maximum votes per person:</TH>\n";
    print "  <TD><INPUT SIZE=5 MAXLENGTH=5 NAME=\"votesperuser\" VALUE=\"$votesperuser\"></TD>\n";

    print "</TR><TR>\n";
    print "  <TH ALIGN=\"right\">Maximum votes a person can put on a single bug:</TH>\n";
    print "  <TD><INPUT SIZE=5 MAXLENGTH=5 NAME=\"maxvotesperbug\" VALUE=\"$maxvotesperbug\"></TD>\n";

    print "</TR><TR>\n";
    print "  <TH ALIGN=\"right\">Number of votes a bug in this product needs to automatically get out of the <A HREF=\"bug_status.html#status\">UNCONFIRMED</A> state:</TH>\n";
    print "  <TD><INPUT SIZE=5 MAXLENGTH=5 NAME=\"votestoconfirm\" VALUE=\"$votestoconfirm\"></TD>\n";
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

ConnectToDatabase();
confirm_login();

print "Content-type: text/html\n\n";

unless (UserInGroup("editcomponents")) {
    PutHeader("Not allowed");
    print "Sorry, you aren't a member of the 'editcomponents' group.\n";
    print "And so, you aren't allowed to add, modify or delete products.\n";
    PutTrailer();
    exit;
}



#
# often used variables
#
my $product = trim($::FORM{product} || '');
my $action  = trim($::FORM{action}  || '');
my $localtrailer = "<A HREF=\"editproducts.cgi\">edit</A> more products";



#
# action='' -> Show nice list of products
#

unless ($action) {
    PutHeader("Select product");

    SendSQL("SELECT products.name,description,disallownew,
                    votesperuser,maxvotesperbug,votestoconfirm,COUNT(bug_id)
             FROM products LEFT JOIN bugs ON products.id = bugs.product_id
             GROUP BY products.name
             ORDER BY products.name");
    print "<TABLE BORDER=1 CELLPADDING=4 CELLSPACING=0><TR BGCOLOR=\"#6666FF\">\n";
    print "  <TH ALIGN=\"left\">Edit product ...</TH>\n";
    print "  <TH ALIGN=\"left\">Description</TH>\n";
    print "  <TH ALIGN=\"left\">Status</TH>\n";
    print "  <TH ALIGN=\"left\">Votes<br>per<br>user</TH>\n";
    print "  <TH ALIGN=\"left\">Max<br>Votes<br>per<br>bug</TH>\n";
    print "  <TH ALIGN=\"left\">Votes<br>to<br>confirm</TH>\n";
    print "  <TH ALIGN=\"left\">Bugs</TH>\n";
    print "  <TH ALIGN=\"left\">Action</TH>\n";
    print "</TR>";
    while ( MoreSQLData() ) {
        my ($product, $description, $disallownew, $votesperuser,
            $maxvotesperbug, $votestoconfirm, $bugs) = FetchSQLData();
        $description ||= "<FONT COLOR=\"red\">missing</FONT>";
        $disallownew = $disallownew ? 'closed' : 'open';
        $bugs        ||= 'none';
        print "<TR>\n";
        print "  <TD VALIGN=\"top\"><A HREF=\"editproducts.cgi?action=edit&product=", url_quote($product), "\"><B>$product</B></A></TD>\n";
        print "  <TD VALIGN=\"top\">$description</TD>\n";
        print "  <TD VALIGN=\"top\">$disallownew</TD>\n";
        print "  <TD VALIGN=\"top\" ALIGN=\"right\">$votesperuser</TD>\n";
        print "  <TD VALIGN=\"top\" ALIGN=\"right\">$maxvotesperbug</TD>\n";
        print "  <TD VALIGN=\"top\" ALIGN=\"right\">$votestoconfirm</TD>\n";
        print "  <TD VALIGN=\"top\" ALIGN=\"right\">$bugs</TD>\n";
        print "  <TD VALIGN=\"top\"><A HREF=\"editproducts.cgi?action=del&product=", url_quote($product), "\">Delete</A></TD>\n";
        print "</TR>";
    }
    print "<TR>\n";
    print "  <TD VALIGN=\"top\" COLSPAN=7>Add a new product</TD>\n";
    print "  <TD VALIGN=\"top\" ALIGN=\"middle\"><A HREF=\"editproducts.cgi?action=add\">Add</A></TD>\n";
    print "</TR></TABLE>\n";

    PutTrailer();
    exit;
}




#
# action='add' -> present form for parameters for new product
#
# (next action will be 'new')
#

if ($action eq 'add') {
    PutHeader("Add product");

    #print "This page lets you add a new product to bugzilla.\n";

    print "<FORM METHOD=POST ACTION=editproducts.cgi>\n";
    print "<TABLE BORDER=0 CELLPADDING=4 CELLSPACING=0><TR>\n";

    EmitFormElements('', '', '', 0, 0, 10000, 0, "---");

    print "</TR><TR>\n";
    print "  <TH ALIGN=\"right\">Version:</TH>\n";
    print "  <TD><INPUT SIZE=64 MAXLENGTH=255 NAME=\"version\" VALUE=\"unspecified\"></TD>\n";

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
# action='new' -> add product entered in the 'action=add' screen
#

if ($action eq 'new') {
    PutHeader("Adding new product");

    # Cleanups and validity checks

    unless ($product) {
        print "You must enter a name for the new product. Please press\n";
        print "<b>Back</b> and try again.\n";
        PutTrailer($localtrailer);
        exit;
    }
    if (TestProduct($product)) {
        print "The product '$product' already exists. Please press\n";
        print "<b>Back</b> and try again.\n";
        PutTrailer($localtrailer);
        exit;
    }

    my $version = trim($::FORM{version} || '');

    if ($version eq '') {
        print "You must enter a version for product '$product'. Please press\n";
        print "<b>Back</b> and try again.\n";
        PutTrailer($localtrailer);
        exit;
    }

    my $description  = trim($::FORM{description}  || '');
    my $milestoneurl = trim($::FORM{milestoneurl} || '');
    my $disallownew = 0;
    $disallownew = 1 if $::FORM{disallownew};
    my $votesperuser = $::FORM{votesperuser};
    $votesperuser ||= 0;
    my $maxvotesperbug = $::FORM{maxvotesperbug};
    $maxvotesperbug = 10000 if !defined $maxvotesperbug;
    my $votestoconfirm = $::FORM{votestoconfirm};
    $votestoconfirm ||= 0;
    my $defaultmilestone = $::FORM{defaultmilestone} || "---";

    # Add the new product.
    SendSQL("INSERT INTO products ( " .
            "name, description, milestoneurl, disallownew, votesperuser, " .
            "maxvotesperbug, votestoconfirm, defaultmilestone" .
            " ) VALUES ( " .
            SqlQuote($product) . "," .
            SqlQuote($description) . "," .
            SqlQuote($milestoneurl) . "," .
            $disallownew . "," .
            "$votesperuser, $maxvotesperbug, $votestoconfirm, " .
            SqlQuote($defaultmilestone) . ")");
    SendSQL("SELECT LAST_INSERT_ID()");
    my $product_id = FetchOneColumn();
    SendSQL("INSERT INTO versions ( " .
          "value, product_id" .
          " ) VALUES ( " .
          SqlQuote($version) . "," .
          $product_id . ")" );

    SendSQL("INSERT INTO milestones (product_id, value) VALUES (" .
            $product_id . ", " . SqlQuote($defaultmilestone) . ")");

    # If we're using bug groups, then we need to create a group for this
    # product as well.  -JMR, 2/16/00
    if(Param("usebuggroups")) {
        # Next we insert into the groups table
        SendSQL("INSERT INTO groups " .
                "(name, description, isbuggroup, last_changed) " .
                "VALUES (" .
                SqlQuote($product) . ", " .
                SqlQuote("Access to bugs in the $product product") . ", 1, NOW())");
        SendSQL("SELECT last_insert_id()");
        my $gid = FetchOneColumn();
        my $admin = GroupNameToId('admin');
        SendSQL("INSERT INTO group_group_map (member_id, grantor_id, isbless)
                 VALUES ($admin, $gid, 0)");
        SendSQL("INSERT INTO group_group_map (member_id, grantor_id, isbless)
                 VALUES ($admin, $gid, 1)");
        
    }

    # Make versioncache flush
    unlink "data/versioncache";

    print "OK, done.<p>\n";
    PutTrailer($localtrailer, "<a href=\"editcomponents.cgi?action=add&product=" . url_quote($product) . "\">add</a> components to this new product.");
    exit;
}



#
# action='del' -> ask if user really wants to delete
#
# (next action would be 'delete')
#

if ($action eq 'del') {
    PutHeader("Delete product");
    CheckProduct($product);

    # display some data about the product
    SendSQL("SELECT id, description, milestoneurl, disallownew
             FROM products
             WHERE name=" . SqlQuote($product));
    my ($product_id, $description, $milestoneurl, $disallownew) = FetchSQLData();
    my $milestonelink = $milestoneurl ? "<a href=\"$milestoneurl\">$milestoneurl</a>"
                                      : "<font color=\"red\">missing</font>";
    $description ||= "<FONT COLOR=\"red\">description missing</FONT>";
    $disallownew = $disallownew ? 'closed' : 'open';
    
    print "<TABLE BORDER=1 CELLPADDING=4 CELLSPACING=0>\n";
    print "<TR BGCOLOR=\"#6666FF\">\n";
    print "  <TH VALIGN=\"top\" ALIGN=\"left\">Part</TH>\n";
    print "  <TH VALIGN=\"top\" ALIGN=\"left\">Value</TH>\n";

    print "</TR><TR>\n";
    print "  <TD VALIGN=\"top\">Product:</TD>\n";
    print "  <TD VALIGN=\"top\">$product</TD>\n";

    print "</TR><TR>\n";
    print "  <TD VALIGN=\"top\">Description:</TD>\n";
    print "  <TD VALIGN=\"top\">$description</TD>\n";

    if (Param('usetargetmilestone')) {
        print "</TR><TR>\n";
        print "  <TD VALIGN=\"top\">Milestone URL:</TD>\n";
        print "  <TD VALIGN=\"top\">$milestonelink</TD>\n";
    }


    print "</TR><TR>\n";
    print "  <TD VALIGN=\"top\">Closed for bugs:</TD>\n";
    print "  <TD VALIGN=\"top\">$disallownew</TD>\n";

    print "</TR><TR>\n";
    print "  <TD VALIGN=\"top\">Components:</TD>\n";
    print "  <TD VALIGN=\"top\">";
    SendSQL("SELECT name,description
             FROM components
             WHERE product_id=$product_id");
    if (MoreSQLData()) {
        print "<table>";
        while ( MoreSQLData() ) {
            my ($component, $description) = FetchSQLData();
            $description ||= "<FONT COLOR=\"red\">description missing</FONT>";
            print "<tr><th align=right valign=top>$component:</th>";
            print "<td valign=top>$description</td></tr>\n";
        }
        print "</table>\n";
    } else {
        print "<FONT COLOR=\"red\">missing</FONT>";
    }

    print "</TD>\n</TR><TR>\n";
    print "  <TD VALIGN=\"top\">Versions:</TD>\n";
    print "  <TD VALIGN=\"top\">";
    SendSQL("SELECT value
             FROM versions
             WHERE product_id=$product_id
             ORDER BY value");
    if (MoreSQLData()) {
        my $br = 0;
        while ( MoreSQLData() ) {
            my ($version) = FetchSQLData();
            print "<BR>" if $br;
            print $version;
            $br = 1;
        }
    } else {
        print "<FONT COLOR=\"red\">missing</FONT>";
    }

    #
    # Adding listing for associated target milestones - matthew@zeroknowledge.com
    #
    if (Param('usetargetmilestone')) {
        print "</TD>\n</TR><TR>\n";
        print "  <TH ALIGN=\"right\" VALIGN=\"top\"><A HREF=\"editmilestones.cgi?product=", url_quote($product), "\">Edit milestones:</A></TH>\n";
        print "  <TD>";
        SendSQL("SELECT value
                 FROM milestones
                 WHERE product_id=$product_id
                 ORDER BY sortkey,value");
        if(MoreSQLData()) {
            my $br = 0;
            while ( MoreSQLData() ) {
                my ($milestone) = FetchSQLData();
                print "<BR>" if $br;
                print $milestone;
                $br = 1;
            }
        } else {
            print "<FONT COLOR=\"red\">missing</FONT>";
        }
    }

    print "</TD>\n</TR><TR>\n";
    print "  <TD VALIGN=\"top\">Bugs:</TD>\n";
    print "  <TD VALIGN=\"top\">";
    SendSQL("SELECT count(bug_id),product_id
             FROM bugs
             GROUP BY product_id
             HAVING product_id=$product_id");
    my $bugs = FetchOneColumn();
    print $bugs || 'none';


    print "</TD>\n</TR></TABLE>";

    print "<H2>Confirmation</H2>\n";

    if ($bugs) {
        if (!Param("allowbugdeletion")) {
            print "Sorry, there are $bugs bugs outstanding for this product.
You must reassign those bugs to another product before you can delete this
one.";
            PutTrailer($localtrailer);
            exit;
        }
        print "<TABLE BORDER=0 CELLPADDING=20 WIDTH=\"70%\" BGCOLOR=\"red\"><TR><TD>\n",
              "There are bugs entered for this product!  When you delete this ",
              "product, <B><BLINK>all</BLINK><B> stored bugs will be deleted, too. ",
              "You could not even see a bug history anymore!\n",
              "</TD></TR></TABLE>\n";
    }

    print "<P>Do you really want to delete this product?<P>\n";
    print "<FORM METHOD=POST ACTION=editproducts.cgi>\n";
    print "<INPUT TYPE=SUBMIT VALUE=\"Yes, delete\">\n";
    print "<INPUT TYPE=HIDDEN NAME=\"action\" VALUE=\"delete\">\n";
    print "<INPUT TYPE=HIDDEN NAME=\"product\" VALUE=\"" .
        value_quote($product) . "\">\n";
    print "</FORM>";

    PutTrailer($localtrailer);
    exit;
}



#
# action='delete' -> really delete the product
#

if ($action eq 'delete') {
    PutHeader("Deleting product");
    CheckProduct($product);
    my $product_id = get_product_id($product);

    # lock the tables before we start to change everything:

    SendSQL("LOCK TABLES attachments WRITE,
                         bugs WRITE,
                         bugs_activity WRITE,
                         components WRITE,
                         dependencies WRITE,
                         versions WRITE,
                         products WRITE,
                         groups WRITE,
                         profiles WRITE,
                         milestones WRITE,
                         flaginclusions WRITE,
                         flagexclusions WRITE");

    # According to MySQL doc I cannot do a DELETE x.* FROM x JOIN Y,
    # so I have to iterate over bugs and delete all the indivial entries
    # in bugs_activies and attachments.

    if (Param("allowbugdeletion")) {
        SendSQL("SELECT bug_id
             FROM bugs
             WHERE product_id=$product_id");
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
             WHERE product_id=$product_id");
        print "Bugs deleted.<BR>\n";
    }

    SendSQL("DELETE FROM components
             WHERE product_id=$product_id");
    print "Components deleted.<BR>\n";

    SendSQL("DELETE FROM versions
             WHERE product_id=$product_id");
    print "Versions deleted.<P>\n";

    # deleting associated target milestones - matthew@zeroknowledge.com
    SendSQL("DELETE FROM milestones
             WHERE product_id=$product_id");
    print "Milestones deleted.<BR>\n";

    SendSQL("DELETE FROM flaginclusions
             WHERE product_id=$product_id");
    SendSQL("DELETE FROM flagexclusions
             WHERE product_id=$product_id");
    print "Flag inclusions and exclusions deleted.<BR>\n";

    SendSQL("DELETE FROM products
             WHERE id=$product_id");
    print "Product '$product' deleted.<BR>\n";


    SendSQL("UNLOCK TABLES");

    unlink "data/versioncache";
    PutTrailer($localtrailer);
    exit;
}



#
# action='edit' -> present the edit products from
#
# (next action would be 'update')
#

if ($action eq 'edit') {
    PutHeader("Edit product");
    CheckProduct($product);

    # get data of product
    SendSQL("SELECT id,description,milestoneurl,disallownew,
                    votesperuser,maxvotesperbug,votestoconfirm,defaultmilestone
             FROM products
             WHERE name=" . SqlQuote($product));
    my ($product_id,$description, $milestoneurl, $disallownew,
        $votesperuser, $maxvotesperbug, $votestoconfirm, $defaultmilestone) =
        FetchSQLData();

    print "<FORM METHOD=POST ACTION=editproducts.cgi>\n";
    print "<TABLE  BORDER=0 CELLPADDING=4 CELLSPACING=0><TR>\n";

    EmitFormElements($product, $description, $milestoneurl, 
                     $disallownew, $votesperuser, $maxvotesperbug,
                     $votestoconfirm, $defaultmilestone);
    
    print "</TR><TR VALIGN=top>\n";
    print "  <TH ALIGN=\"right\"><A HREF=\"editcomponents.cgi?product=", url_quote($product), "\">Edit components:</A></TH>\n";
    print "  <TD>";
    SendSQL("SELECT name,description
             FROM components
             WHERE product_id=$product_id");
    if (MoreSQLData()) {
        print "<table>";
        while ( MoreSQLData() ) {
            my ($component, $description) = FetchSQLData();
            $description ||= "<FONT COLOR=\"red\">description missing</FONT>";
            print "<tr><th align=right valign=top>$component:</th>";
            print "<td valign=top>$description</td></tr>\n";
        }
        print "</table>\n";
    } else {
        print "<FONT COLOR=\"red\">missing</FONT>";
    }


    print "</TD>\n</TR><TR>\n";
    print "  <TH ALIGN=\"right\" VALIGN=\"top\"><A HREF=\"editversions.cgi?product=", url_quote($product), "\">Edit versions:</A></TH>\n";
    print "  <TD>";
    SendSQL("SELECT value
             FROM versions
             WHERE product_id=$product_id
             ORDER BY value");
    if (MoreSQLData()) {
        my $br = 0;
        while ( MoreSQLData() ) {
            my ($version) = FetchSQLData();
            print "<BR>" if $br;
            print $version;
            $br = 1;
        }
    } else {
        print "<FONT COLOR=\"red\">missing</FONT>";
    }

    #
    # Adding listing for associated target milestones - matthew@zeroknowledge.com
    #
    if (Param('usetargetmilestone')) {
        print "</TD>\n</TR><TR>\n";
        print "  <TH ALIGN=\"right\" VALIGN=\"top\"><A HREF=\"editmilestones.cgi?product=", url_quote($product), "\">Edit milestones:</A></TH>\n";
        print "  <TD>";
        SendSQL("SELECT value
                 FROM milestones
                 WHERE product_id=$product_id
                 ORDER BY sortkey,value");
        if(MoreSQLData()) {
            my $br = 0;
            while ( MoreSQLData() ) {
                my ($milestone) = FetchSQLData();
                print "<BR>" if $br;
                print $milestone;
                $br = 1;
            }
        } else {
            print "<FONT COLOR=\"red\">missing</FONT>";
        }
    }

    print "</TD>\n</TR><TR>\n";
    print "  <TH ALIGN=\"right\">Bugs:</TH>\n";
    print "  <TD>";
    SendSQL("SELECT count(bug_id),product_id
             FROM bugs
             GROUP BY product_id
             HAVING product_id=$product_id");
    my $bugs = '';
    $bugs = FetchOneColumn() if MoreSQLData();
    print $bugs || 'none';

    print "</TD>\n</TR></TABLE>\n";

    print "<INPUT TYPE=HIDDEN NAME=\"productold\" VALUE=\"" .
        value_quote($product) . "\">\n";
    print "<INPUT TYPE=HIDDEN NAME=\"descriptionold\" VALUE=\"" .
        value_quote($description) . "\">\n";
    print "<INPUT TYPE=HIDDEN NAME=\"milestoneurlold\" VALUE=\"" .
        value_quote($milestoneurl) . "\">\n";
    print "<INPUT TYPE=HIDDEN NAME=\"disallownewold\" VALUE=\"$disallownew\">\n";
    print "<INPUT TYPE=HIDDEN NAME=\"votesperuserold\" VALUE=\"$votesperuser\">\n";
    print "<INPUT TYPE=HIDDEN NAME=\"maxvotesperbugold\" VALUE=\"$maxvotesperbug\">\n";
    print "<INPUT TYPE=HIDDEN NAME=\"votestoconfirmold\" VALUE=\"$votestoconfirm\">\n";
    $defaultmilestone = value_quote($defaultmilestone);
    print "<INPUT TYPE=HIDDEN NAME=\"defaultmilestoneold\" VALUE=\"$defaultmilestone\">\n";
    print "<INPUT TYPE=HIDDEN NAME=\"action\" VALUE=\"update\">\n";
    print "<INPUT TYPE=SUBMIT VALUE=\"Update\">\n";

    print "</FORM>";

    my $x = $localtrailer;
    $x =~ s/more/other/;
    PutTrailer($x);
    exit;
}



#
# action='update' -> update the product
#

if ($action eq 'update') {
    PutHeader("Update product");

    my $productold          = trim($::FORM{productold}          || '');
    my $description         = trim($::FORM{description}         || '');
    my $descriptionold      = trim($::FORM{descriptionold}      || '');
    my $disallownew         = trim($::FORM{disallownew}         || '');
    my $disallownewold      = trim($::FORM{disallownewold}      || '');
    my $milestoneurl        = trim($::FORM{milestoneurl}        || '');
    my $milestoneurlold     = trim($::FORM{milestoneurlold}     || '');
    my $votesperuser        = trim($::FORM{votesperuser}        || 0);
    my $votesperuserold     = trim($::FORM{votesperuserold}     || 0);
    my $maxvotesperbug      = trim($::FORM{maxvotesperbug}      || 0);
    my $maxvotesperbugold   = trim($::FORM{maxvotesperbugold}   || 0);
    my $votestoconfirm      = trim($::FORM{votestoconfirm}      || 0);
    my $votestoconfirmold   = trim($::FORM{votestoconfirmold}   || 0);
    my $defaultmilestone    = trim($::FORM{defaultmilestone}    || '---');
    my $defaultmilestoneold = trim($::FORM{defaultmilestoneold} || '---');

    my $checkvotes = 0;

    CheckProduct($productold);
    my $product_id = get_product_id($productold);

    if ($maxvotesperbug !~ /^\d+$/ || $maxvotesperbug <= 0) {
        print "Sorry, the max votes per bug must be a positive integer.";
        PutTrailer($localtrailer);
        exit;
    }

    # Note that we got the $product_id using $productold above so it will
    # remain static even after we rename the product in the database.

    SendSQL("LOCK TABLES products WRITE,
                         versions READ,
                         groups WRITE,
                         profiles WRITE,
                         milestones READ");

    if ($disallownew ne $disallownewold) {
        $disallownew ||= 0;
        SendSQL("UPDATE products
                 SET disallownew=$disallownew
                 WHERE id=$product_id");
        print "Updated bug submit status.<BR>\n";
    }

    if ($description ne $descriptionold) {
        unless ($description) {
            print "Sorry, I can't delete the description.";
            SendSQL("UNLOCK TABLES");
            PutTrailer($localtrailer);
            exit;
        }
        SendSQL("UPDATE products
                 SET description=" . SqlQuote($description) . "
                 WHERE id=$product_id");
        print "Updated description.<BR>\n";
    }

    if (Param('usetargetmilestone') && $milestoneurl ne $milestoneurlold) {
        SendSQL("UPDATE products
                 SET milestoneurl=" . SqlQuote($milestoneurl) . "
                 WHERE id=$product_id");
        print "Updated mile stone URL.<BR>\n";
    }


    if ($votesperuser ne $votesperuserold) {
        SendSQL("UPDATE products
                 SET votesperuser=$votesperuser
                 WHERE id=$product_id");
        print "Updated votes per user.<BR>\n";
        $checkvotes = 1;
    }


    if ($maxvotesperbug ne $maxvotesperbugold) {
        SendSQL("UPDATE products
                 SET maxvotesperbug=$maxvotesperbug
                 WHERE id=$product_id");
        print "Updated max votes per bug.<BR>\n";
        $checkvotes = 1;
    }


    if ($votestoconfirm ne $votestoconfirmold) {
        SendSQL("UPDATE products
                 SET votestoconfirm=$votestoconfirm
                 WHERE id=$product_id");
        print "Updated votes to confirm.<BR>\n";
        $checkvotes = 1;
    }


    if ($defaultmilestone ne $defaultmilestoneold) {
        SendSQL("SELECT value FROM milestones " .
                "WHERE value = " . SqlQuote($defaultmilestone) .
                "  AND product_id = $product_id");
        if (!FetchOneColumn()) {
            print "Sorry, the milestone $defaultmilestone must be defined first.";
            SendSQL("UNLOCK TABLES");
            PutTrailer($localtrailer);
            exit;
        }
        SendSQL("UPDATE products " .
                "SET defaultmilestone = " . SqlQuote($defaultmilestone) .
                "WHERE id=$product_id");
        print "Updated default milestone.<BR>\n";
    }

    my $qp = SqlQuote($product);
    my $qpold = SqlQuote($productold);

    if ($product ne $productold) {
        unless ($product) {
            print "Sorry, I can't delete the product name.";
            SendSQL("UNLOCK TABLES");
            PutTrailer($localtrailer);
            exit;
        }
        if (TestProduct($product)) {
            print "Sorry, product name '$product' is already in use.";
            SendSQL("UNLOCK TABLES");
            PutTrailer($localtrailer);
            exit;
        }

        SendSQL("UPDATE products SET name=$qp WHERE id=$product_id");
        # Need to do an update to groups as well.  If there is a corresponding
        # bug group, whether usebuggroups is currently set or not, we want to
        # update it so it will match in the future.  If there is no group, this
        # update statement will do nothing, so no harm done.  -JMR, 3/8/00
        SendSQL("UPDATE groups " .
                "SET name = $qp, " .
                "description = " .
                SqlQuote("Access to bugs in the $product product") .
                " WHERE name = $qpold");
        
        print "Updated product name.<BR>\n";
    }
    unlink "data/versioncache";
    SendSQL("UNLOCK TABLES");

    if ($checkvotes) {
        print "Checking existing votes in this product for anybody who now has too many votes.";
        if ($maxvotesperbug < $votesperuser) {
            SendSQL("SELECT votes.who, votes.bug_id " .
                    "FROM votes, bugs " .
                    "WHERE bugs.bug_id = votes.bug_id " .
                    " AND bugs.product_id = $product_id " .
                    " AND votes.count > $maxvotesperbug");
            my @list;
            while (MoreSQLData()) {
                my ($who, $id) = (FetchSQLData());
                push(@list, [$who, $id]);
            }
            foreach my $ref (@list) {
                my ($who, $id) = (@$ref);
                RemoveVotes($id, $who, "The rules for voting on this product has changed;\nyou had too many votes for a single bug.");
                my $name = DBID_to_name($who);
                print qq{<br>Removed votes for bug <A HREF="show_bug.cgi?id=$id">$id</A> from $name\n};
            }
        }
        SendSQL("SELECT votes.who, votes.count FROM votes, bugs " .
                "WHERE bugs.bug_id = votes.bug_id " .
                " AND bugs.product_id = $product_id");
        my %counts;
        while (MoreSQLData()) {
            my ($who, $count) = (FetchSQLData());
            if (!defined $counts{$who}) {
                $counts{$who} = $count;
            } else {
                $counts{$who} += $count;
            }
        }
        foreach my $who (keys(%counts)) {
            if ($counts{$who} > $votesperuser) {
                SendSQL("SELECT votes.bug_id FROM votes, bugs " .
                        "WHERE bugs.bug_id = votes.bug_id " .
                        " AND bugs.product_id = $product_id " .
                        " AND votes.who = $who");
                while (MoreSQLData()) {
                    my $id = FetchSQLData();
                    RemoveVotes($id, $who,
                                "The rules for voting on this product has changed; you had too many\ntotal votes, so all votes have been removed.");
                    my $name = DBID_to_name($who);
                    print qq{<br>Removed votes for bug <A HREF="show_bug.cgi?id=$id">$id</A> from $name\n};
                }
            }
        }
        SendSQL("SELECT bug_id FROM bugs " .
                "WHERE product_id = $product_id " .
                "  AND bug_status = '$::unconfirmedstate' " .
                "  AND votes >= $votestoconfirm");
        my @list;
        while (MoreSQLData()) {
            push(@list, FetchOneColumn());
        }
        foreach my $id (@list) {
            SendSQL("SELECT who FROM votes WHERE bug_id = $id");
            my $who = FetchOneColumn();
            CheckIfVotedConfirmed($id, $who);
        }

    }

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
