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

package Tinderbox3::BuildTimeColumn;

use strict;
use Date::Format;

sub new {
	my $class = shift;
	$class = ref($class) || $class;
	my $this = {};
	bless $this, $class;

  my ($p, $dbh) = @_;

  return $this;
}

sub column_header {
  return "<th>Build Time</th>";
}

sub column_header_2 {
  return "<td>Click time to see changes since then</td>";
}

sub cell {
  my $this = shift;
  my ($rows, $row_num, $column) = @_;
  my $class;
  if (time2str("%H", $rows->[$row_num][$column]) % 2 == 1) {
    $class = "time";
  } else {
    $class = "time_alt";
  }
  return "<td class=$class>" . time2str("%D %R", $rows->[$row_num][$column]) . "</td>";
}

1

