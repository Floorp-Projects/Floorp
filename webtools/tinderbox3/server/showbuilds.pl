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

use strict;
use CGI;
use Tinderbox3::DB;
use Tinderbox3::ShowBuilds;

my $p = new CGI;
my $dbh = get_dbh();

my $tree = $p->param('tree') || "";
my ($start_time, $end_time);
if ($p->param('start_time')) {
  $start_time = $p->param('start_time');
  if ($start_time > time) {
    $start_time = time;
  }
  $end_time = $start_time + ($p->param('interval') || (24*60*60));
  if ($end_time > time) {
    $end_time = time;
  }
} else {
  $end_time = time;
  $start_time = $end_time - 24*60*60;
}

my $min_row_size = $p->param('min_row_size');
my $max_row_size = $p->param('max_row_size');

print $p->header;
Tinderbox3::ShowBuilds::print_showbuilds($p, $dbh, *STDOUT, $tree, $start_time,
                                         $end_time, $min_row_size, $max_row_size);

$dbh->disconnect;
