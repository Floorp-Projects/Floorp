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

package Tinderbox3::XML;

use strict;
use CGI;

require Exporter;
our @ISA = qw(Exporter);
our @EXPORT = qw(die_xml_error);

sub die_xml_error {
  my ($p, $dbh, $error) = @_;
  print $p->header("text/xml");
  print "<error>", $p->escapeHTML($error), "</error>\n";
  $dbh->disconnect;
  exit(0);
}

1
