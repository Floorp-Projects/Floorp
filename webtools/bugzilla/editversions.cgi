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
#
# Direct any questions on this source code to
#
# Holger Schurig <holgerschurig@nikocity.de>

use strict;
use lib ".";

require "CGI.pl";
require "globals.pl";

use Bugzilla::Constants;
use Bugzilla::Config qw(:DEFAULT $datadir);

# TestProduct:  just returns if the specified product does exists
# CheckProduct: same check, optionally  emit an error text
# TestVersion:  just returns if the specified product/version combination exists
# CheckVersion: same check, optionally emit an error text

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

sub TestVersion ($$)
{
    my ($prod,$ver) = @_;

    # does the product exist?
    SendSQL("SELECT products.name,value
             FROM versions, products
             WHERE versions.product_id=products.id AND products.name=" . SqlQuote($prod) . " and value=" . SqlQuote($ver));
    return FetchOneColumn();
}

sub CheckVersion ($$)
{
    my ($prod,$ver) = @_;

    # do we have the version?
    unless ($ver) {
        print "Sorry, you haven't specified a version.";
        PutTrailer();
        exit;
    }

    CheckProduct($prod);

    unless (TestVersion $prod,$ver) {
        print "Sorry, version '$ver' for product '$prod' does not exist.";
        PutTrailer();
        exit;
    }
}


#
# Displays the form to edit a version
#

sub EmitFormElements ($$)
{
    my ($product, $version) = @_;

    print "  <TH ALIGN=\"right\">Version:</TH>\n";
    print "  <TD><INPUT SIZE=64 MAXLENGTH=64 NAME=\"version\" VALUE=\"" .
        value_quote($version) . "\">\n";
    print "      <INPUT TYPE=HIDDEN NAME=\"product\" VALUE=\"" .
        value_quote($product) . "\"></TD>\n";
}


#
# Displays a text like "a.", "a or b.", "a, b or c.", "a, b, c or d."
#

sub PutTrailer (@)
{
    my (@links) = ("Back to the <A HREF=\"query.cgi\">query page</A>", @_);
    SendSQL("UNLOCK TABLES");

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

Bugzilla->login(LOGIN_REQUIRED);

print Bugzilla->cgi->header();

unless (UserInGroup("editcomponents")) {
    PutHeader("Not allowed");
    print "Sorry, you aren't a member of the 'editcomponents' group.\n";
    print "And so, you aren't allowed to add, modify or delete versions.\n";
    PutTrailer();
    exit;
}


#
# often used variables
#
my $cgi = Bugzilla->cgi;
my $product = trim($cgi->param('product') || '');
my $version = trim($cgi->param('version') || '');
my $action  = trim($cgi->param('action')  || '');
my $localtrailer;
if ($version) {
    $localtrailer = "<A HREF=\"editversions.cgi?product=" . url_quote($product) . "\">edit</A> more versions";
} else {
    $localtrailer = "<A HREF=\"editversions.cgi\">edit</A> more versions";
}



#
# product = '' -> Show nice list of versions
#

unless ($product) {
    PutHeader("Select product");

    SendSQL("SELECT products.name,products.description,'xyzzy'
             FROM products 
             GROUP BY products.name
             ORDER BY products.name");
    print "<TABLE BORDER=1 CELLPADDING=4 CELLSPACING=0><TR BGCOLOR=\"#6666FF\">\n";
    print "  <TH ALIGN=\"left\">Edit versions of ...</TH>\n";
    print "  <TH ALIGN=\"left\">Description</TH>\n";
    print "  <TH ALIGN=\"left\">Bugs</TH>\n";
    #print "  <TH ALIGN=\"left\">Edit</TH>\n";
    print "</TR>";
    while ( MoreSQLData() ) {
        my ($product, $description, $bugs) = FetchSQLData();
        $description ||= "<FONT COLOR=\"red\">missing</FONT>";
        $bugs ||= "none";
        print "<TR>\n";
        print "  <TD VALIGN=\"top\"><A HREF=\"editversions.cgi?product=", url_quote($product), "\"><B>$product</B></A></TD>\n";
        print "  <TD VALIGN=\"top\">$description</TD>\n";
        print "  <TD VALIGN=\"top\">$bugs</TD>\n";
        #print "  <TD VALIGN=\"top\"><A HREF=\"editversions.cgi?action=edit&product=", url_quote($product), "\">Edit</A></TD>\n";
    }
    print "</TR></TABLE>\n";

    PutTrailer();
    exit;
}

#
# action='' -> Show nice list of versions
#

unless ($action) {
    PutHeader("Select version of $product");
    CheckProduct($product);
    my $product_id = get_product_id($product);

    SendSQL("SELECT value
             FROM versions
             WHERE product_id=$product_id
             ORDER BY value");

    print "<TABLE BORDER=1 CELLPADDING=4 CELLSPACING=0><TR BGCOLOR=\"#6666FF\">\n";
    print "  <TH ALIGN=\"left\">Edit version ...</TH>\n";
    #print "  <TH ALIGN=\"left\">Bugs</TH>\n";
    print "  <TH ALIGN=\"left\">Action</TH>\n";
    print "</TR>";
    while ( MoreSQLData() ) {
        my $version = FetchOneColumn();
        print "<TR>\n";
        print "  <TD VALIGN=\"top\"><A HREF=\"editversions.cgi?product=", url_quote($product), "&version=", url_quote($version), "&action=edit\"><B>$version</B></A></TD>\n";
        #print "  <TD VALIGN=\"top\">$bugs</TD>\n";
        print "  <TD VALIGN=\"top\"><A HREF=\"editversions.cgi?product=", url_quote($product), "&version=", url_quote($version), "&action=del\"><B>Delete</B></A></TD>\n";
        print "</TR>";
    }
    print "<TR>\n";
    print "  <TD VALIGN=\"top\">Add a new version</TD>\n";
    print "  <TD VALIGN=\"top\" ALIGN=\"middle\"><A HREF=\"editversions.cgi?product=", url_quote($product) . "&action=add\">Add</A></TD>\n";
    print "</TR></TABLE>\n";

    PutTrailer();
    exit;
}




#
# action='add' -> present form for parameters for new version
#
# (next action will be 'new')
#

if ($action eq 'add') {
    PutHeader("Add version of $product");
    CheckProduct($product);
    my $product_id = get_product_id($product);

    #print "This page lets you add a new version to a bugzilla-tracked product.\n";

    print "<FORM METHOD=POST ACTION=editversions.cgi>\n";
    print "<TABLE BORDER=0 CELLPADDING=4 CELLSPACING=0><TR>\n";

    EmitFormElements($product, $version);

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
# action='new' -> add version entered in the 'action=add' screen
#

if ($action eq 'new') {
    PutHeader("Adding new version");
    CheckProduct($product);
    my $product_id = get_product_id($product);

    # Cleanups and valididy checks

    unless ($version) {
        print "You must enter a text for the new version. Please press\n";
        print "<b>Back</b> and try again.\n";
        PutTrailer($localtrailer);
        exit;
    }
    if (TestVersion($product,$version)) {
        print "The version '$version' already exists. Please press\n";
        print "<b>Back</b> and try again.\n";
        PutTrailer($localtrailer);
        exit;
    }

    # Add the new version
    SendSQL("INSERT INTO versions ( " .
          "value, product_id" .
          " ) VALUES ( " .
          SqlQuote($version) . ", $product_id)");

    # Make versioncache flush
    unlink "$datadir/versioncache";

    print "OK, done.<p>\n";
    PutTrailer("<A HREF=\"editversions.cgi?product=$product&amp;action=add\">add</a> another version or $localtrailer");
    exit;
}




#
# action='del' -> ask if user really wants to delete
#
# (next action would be 'delete')
#

if ($action eq 'del') {
    PutHeader("Delete version of $product");
    CheckVersion($product, $version);
    my $product_id = get_product_id($product);

    SendSQL("SELECT count(bug_id)
             FROM bugs
             WHERE product_id = $product_id
               AND version = " . SqlQuote($version));
    my $bugs = FetchOneColumn();

    print "<TABLE BORDER=1 CELLPADDING=4 CELLSPACING=0>\n";
    print "<TR BGCOLOR=\"#6666FF\">\n";
    print "  <TH VALIGN=\"top\" ALIGN=\"left\">Part</TH>\n";
    print "  <TH VALIGN=\"top\" ALIGN=\"left\">Value</TH>\n";

    print "</TR><TR>\n";
    print "  <TH ALIGN=\"left\" VALIGN=\"top\">Product:</TH>\n";
    print "  <TD VALIGN=\"top\">$product</TD>\n";
    print "</TR><TR>\n";
    print "  <TH ALIGN=\"left\" VALIGN=\"top\">Version:</TH>\n";
    print "  <TD VALIGN=\"top\">$version</TD>\n";
    print "</TR><TR>\n";
    print "  <TH ALIGN=\"left\" VALIGN=\"top\">Bugs:</TH>\n";
    print "  <TD VALIGN=\"top\">", $bugs || 'none' , "</TD>\n";
    print "</TR></TABLE>\n";

    print "<H2>Confirmation</H2>\n";

    if ($bugs) {
        if (!Param("allowbugdeletion")) {
            print "Sorry, there are $bugs bugs outstanding for this version.
You must reassign those bugs to another version before you can delete this
one.";
            PutTrailer($localtrailer);
            exit;
        }
        print "<TABLE BORDER=0 CELLPADDING=20 WIDTH=\"70%\" BGCOLOR=\"red\"><TR><TD>\n",
              "There are bugs entered for this version!  When you delete this ",
              "version, <B><BLINK>all</BLINK></B> stored bugs will be deleted, too. ",
              "You could not even see the bug history for this version anymore!\n",
              "</TD></TR></TABLE>\n";
    }

    print "<P>Do you really want to delete this version?<P>\n";
    print "<FORM METHOD=POST ACTION=editversions.cgi>\n";
    print "<INPUT TYPE=SUBMIT VALUE=\"Yes, delete\">\n";
    print "<INPUT TYPE=HIDDEN NAME=\"action\" VALUE=\"delete\">\n";
    print "<INPUT TYPE=HIDDEN NAME=\"product\" VALUE=\"" .
        value_quote($product) . "\">\n";
    print "<INPUT TYPE=HIDDEN NAME=\"version\" VALUE=\"" .
        value_quote($version) . "\">\n";
    print "</FORM>";

    PutTrailer($localtrailer);
    exit;
}



#
# action='delete' -> really delete the version
#

if ($action eq 'delete') {
    PutHeader("Deleting version of $product");
    CheckVersion($product,$version);
    my $product_id = get_product_id($product);

    # lock the tables before we start to change everything:

    SendSQL("LOCK TABLES attachments WRITE,
                         bugs WRITE,
                         bugs_activity WRITE,
                         versions WRITE,
                         dependencies WRITE");

    # According to MySQL doc I cannot do a DELETE x.* FROM x JOIN Y,
    # so I have to iterate over bugs and delete all the indivial entries
    # in bugs_activies and attachments.

    if (Param("allowbugdeletion")) {

        SendSQL("SELECT bug_id
             FROM bugs
             WHERE product_id=$product_id
               AND version=" . SqlQuote($version));
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
             WHERE product_id = $product_id
               AND version=" . SqlQuote($version));
        print "Bugs deleted.<BR>\n";
    }

    SendSQL("DELETE FROM versions
             WHERE product_id = $product_id
               AND value=" . SqlQuote($version));
    print "Version deleted.<P>\n";

    unlink "$datadir/versioncache";
    PutTrailer($localtrailer);
    exit;
}



#
# action='edit' -> present the edit version form
#
# (next action would be 'update')
#

if ($action eq 'edit') {
    PutHeader("Edit version of $product");
    CheckVersion($product,$version);
    my $product_id = get_product_id($product);

    print "<FORM METHOD=POST ACTION=editversions.cgi>\n";
    print "<TABLE BORDER=0 CELLPADDING=4 CELLSPACING=0><TR>\n";

    EmitFormElements($product, $version);

    print "</TR></TABLE>\n";

    print "<INPUT TYPE=HIDDEN NAME=\"versionold\" VALUE=\"" .
        value_quote($version) . "\">\n";
    print "<INPUT TYPE=HIDDEN NAME=\"action\" VALUE=\"update\">\n";
    print "<INPUT TYPE=SUBMIT VALUE=\"Update\">\n";

    print "</FORM>";

    my $other = $localtrailer;
    $other =~ s/more/other/;
    PutTrailer($other);
    exit;
}



#
# action='update' -> update the version
#

if ($action eq 'update') {
    PutHeader("Update version of $product");

    my $versionold = trim($cgi->param('versionold') || '');

    CheckVersion($product,$versionold);
    my $product_id = get_product_id($product);

    # Note that the order of this tests is important. If you change
    # them, be sure to test for WHERE='$version' or WHERE='$versionold'

    SendSQL("LOCK TABLES bugs WRITE,
                         versions WRITE,
                         products READ");

    if ($version ne $versionold) {
        unless ($version) {
            print "Sorry, I can't delete the version text.";
            PutTrailer($localtrailer);
            exit;
        }
        if (TestVersion($product,$version)) {
            print "Sorry, version '$version' is already in use.";
            PutTrailer($localtrailer);
            exit;
        }
        SendSQL("UPDATE bugs
                 SET version=" . SqlQuote($version) . ",
                 delta_ts = delta_ts
                 WHERE version=" . SqlQuote($versionold) . "
                   AND product_id = $product_id");
        SendSQL("UPDATE versions
                 SET value=" . SqlQuote($version) . "
                 WHERE product_id = $product_id
                   AND value=" . SqlQuote($versionold));
        unlink "$datadir/versioncache";
        print "Updated version.<BR>\n";
    }

    PutTrailer($localtrailer);
    exit;
}



#
# No valid action found
#

PutHeader("Error");
print "I don't have a clue what you want.<BR>\n";
