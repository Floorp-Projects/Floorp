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
# Contributor(s): Sam Ziegler <sam@ziegler.org>
# Terry Weissman <terry@mozilla.org>
# Mark Hamby <mhamby@logicon.com>

# Code derived from editcomponents.cgi, reports.cgi

use diagnostics;
use strict;

require "CGI.pl";

# Shut up misguided -w warnings about "used only once":

use vars @::legal_product;

confirm_login();

print "Content-type: text/html\n\n";

if (!UserInGroup("editcomponents")) {
    print "<H1>Sorry, you aren't a member of the 'editcomponents' group.</H1>\n";
    print "And so, you aren't allowed to add a new component.\n";
    exit;
}


PutHeader("Add Component");

print "This page lets you add a component to bugzilla.\n";

unlink "data/versioncache";
GetVersionTable();

my $prodcode = "P0";

my $product_popup = make_options (\@::legal_product, $::legal_product[0]);

print "
      <form method=post action=doaddcomponent.cgi>

      <TABLE>
      <TR>
              <th align=right>Component:</th>
              <TD><input size=60 name=\"component\" value=\"\"></TD>
      </TR>
      <TR>
              <TH  align=right>Program:</TH>
              <TD><SELECT NAME=\"product\">
                      $product_popup
                      </SELECT></TD>
      </TR>
      <TR>
              <TH  align=right>Description:</TH>
              <TD><input size=60 name=\"description\" value=\"\"></TD>
      </TR>
      <TR>
              <TH  align=right>Initial owner:</TH>
              <TD><input size=60 name=\"initialowner\" value=\"\"></TD>
      </TR>
      ";

if (Param('useqacontact')) {
      print "
              <TR>
                      <TH  align=right>Initial QA contact:</TH>
                      <TD><input size=60 name=\"initialqacontact\" value=\"\"></TD>
              </TR>
              ";
}

print "
      </table>
      <hr>
      ";

print "<input type=submit value=\"Submit changes\">\n";

print "</form>\n";

print "<p><a href=query.cgi>Skip all this, and go back to the query page</a>\n";
