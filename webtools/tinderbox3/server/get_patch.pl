#!/usr/bin/perl -wT -I.
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Tinderbox 3.
#
# The Initial Developer of the Original Code is
# John Keiser (john@johnkeiser.com).
# Portions created by the Initial Developer are Copyright (C) 2004
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# ***** END LICENSE BLOCK *****

use CGI;
use Tinderbox3::DB;
use Tinderbox3::XML;
my $p = new CGI();
my $dbh = get_dbh();

my $patch_id = $p->param('patch_id') || "";
if (!$patch_id) {
  die_xml_error($p, $dbh, "Must specify patch id!");
}


#
# Get data for response
#
my $patch = $dbh->selectrow_arrayref("SELECT patch FROM tbox_patch WHERE patch_id = ?", undef, $patch_id);
if (!defined($patch)) {
  die_xml_error($p, $dbh, "Could not get tree!");
}

print $p->header("text/plain");

print $patch->[0];

$dbh->disconnect;
