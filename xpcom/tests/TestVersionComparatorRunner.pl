#!/usr/bin/perl -w
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
# The Original Code is Mozilla XPCOM Tests.
#
# The Initial Developer of the Original Code is
# Benjamin Smedberg <benjamin@smedbergs.us>.
#
# Portions created by the Initial Developer are Copyright (C) 2005
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

my $executable = shift;

@comparisons = (
  "0.9",
  "0.9.1",
  "1.0pre1",
  "1.0pre2",
  "1.0",
  "1.1pre",
  "1.1pre1a",
  "1.1pre1",
  "1.1pre10a",
  "1.1pre10",
  "1.1",
  "1.1.0.1",
  "1.1.1",
  "2.0",
  "2.1",
  "3.0.-1",
  "3.0"
);

@equality = (
  "1.1pre",
  "1.1pre0",
  "1.0+"
);

# Do the comparisons in both directions

sub run_testprog {
    my ($executable, $args, $expected) = @_;

    my $result = `$executable $args`;
    $result =~ s/\r|\n//g;

    if ($result ne $expected) {
	print STDERR "Testing '$args' expected '$expected', got '$result'.\n";
	exit 1;
    }
}

for (my $i = 0; $i < scalar(@comparisons) - 1; ++$i) {
    run_testprog($executable,
		 "$comparisons[$i] $comparisons[$i + 1]",
		 "$comparisons[$i] < $comparisons[$i + 1]");
    run_testprog($executable,
		 "$comparisons[$i + 1] $comparisons[$i]",
		 "$comparisons[$i + 1] > $comparisons[$i]");
}

for (my $i = 0; $i < scalar(@equality) - 1; ++$i) {
    run_testprog($executable,
		 "$equality[$i] $equality[$i + 1]",
		 "$equality[$i] = $equality[$i + 1]");
}

print "All comparisons OK\n";
exit 0;
