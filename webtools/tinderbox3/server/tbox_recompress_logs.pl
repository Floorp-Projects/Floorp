#!/usr/bin/perl -w -I.
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

use Getopt::Long;

my %args;
$args{uncompressed_hours} = 24;
GetOptions(\%args, "uncompressed_hours:i");

foreach my $file (glob("xml/logs/*/*.log")) {
  my @file_stat = stat($file);
  my $file_mtime = $file_stat[9];
  if ((time - $file_mtime) >= $args{uncompressed_hours}*60*60) {
    system("gzip", "-9", $file);
  }
}
