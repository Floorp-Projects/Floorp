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
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Terry Weissman <terry@mozilla.org>

use vars %::FORM;

use diagnostics;
use strict;

require "CGI.pl";

ConnectToDatabase();
GetVersionTable();

quietly_check_login();

######################################################################
# Begin Data/Security Validation
######################################################################

# If this installation uses bug groups to restrict access to products,
# only show the user products that don't have their own bug group or
# those whose bug group the user is a member of.  Otherwise, if this 
# installation doesn't use bug groups, show the user all legal products.
my @products;
if ( Param("usebuggroups") ) {
  @products = grep( !GroupExists($_) || UserInGroup($_) , @::legal_product );
} else {
  @products = @::legal_product;
}

if ( defined $::FORM{'product'} ) {
  # Make sure the user specified a valid product name.  Note that
  # if the user specifies a valid product name but is not authorized
  # to access that product, they will receive a different error message
  # which could enable people guessing product names to determine
  # whether or not certain products exist in Bugzilla, even if they
  # cannot get any other information about that product.
  grep( $::FORM{'product'} eq $_ , @::legal_product )
    || DisplayError("The product name is invalid.")
    && exit;

  # Make sure the user is authorized to access this product.
  if ( Param("usebuggroups") && GroupExists($::FORM{'product'}) ) {
    UserInGroup($::FORM{'product'})
      || DisplayError("You are not authorized to access that product.")
      && exit;
  }
}

######################################################################
# End Data/Security Validation
######################################################################

print "Content-type: text/html\n\n";

my $product = $::FORM{'product'};
if (!defined $product || lsearch(\@products, $product) < 0) {

    PutHeader("Bugzilla component description");
    print "
<FORM>
Please specify the product whose components you want described.
<P>
Product: <SELECT NAME=product>
";
    print make_options(\@products);
    print "
</SELECT>
<P>
<INPUT TYPE=\"submit\" VALUE=\"Submit\">
</FORM>
";
    PutFooter();
    exit;
}


PutHeader("Bugzilla component description", "Bugzilla component description",
          $product);

print "
<TABLE>
<tr>
<th align=left>Component</th>
<th align=left>Default owner</th>
";

my $emailsuffix = Param("emailsuffix");
my $useqacontact = Param("useqacontact");

my $cols = 2;
if ($useqacontact) {
    print "<th align=left>Default qa contact</th>";
    $cols++;
}

my $colbut1 = $cols - 1;

print "</tr>";

SendSQL("select value, initialowner, initialqacontact, description from components where program = " . SqlQuote($product) . " order by value");

my @data;
while (MoreSQLData()) {
    push @data, [FetchSQLData()];
}
foreach (@data) {
    my ($component, $initialownerid, $initialqacontactid, $description) = @$_;

    my ($initialowner, $initialqacontact) = ($initialownerid ? DBID_to_name ($initialownerid) : '',
                                             $initialqacontactid ? DBID_to_name ($initialqacontactid) : '');

    print qq|
<tr><td colspan=$cols><hr></td></tr>
<tr><td rowspan=2>$component</td>
<td><a href="mailto:$initialowner$emailsuffix">$initialowner</a></td>
|;
    if ($useqacontact) {
        print qq|
<td><a href="mailto:$initialqacontact$emailsuffix">$initialqacontact</a></td>
|;
    }
    print "</tr><tr><td colspan=$colbut1>$description</td></tr>\n";
}

print "<tr><td colspan=$cols><hr></td></tr></table>\n";

PutFooter();
