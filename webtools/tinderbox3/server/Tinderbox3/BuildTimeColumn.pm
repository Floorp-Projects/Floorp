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

