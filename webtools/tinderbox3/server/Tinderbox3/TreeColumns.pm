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

package Tinderbox3::TreeColumns;

use strict;

use Date::Format;
use Tinderbox3::Util;
use CGI::Carp qw/fatalsToBrowser/;

sub new {
	my $class = shift;
	$class = ref($class) || $class;
	my $this = {};
	bless $this, $class;

  my ($start_time, $end_time, $tree, $field_short_names, $field_processors,
      $field_handlers, $patch_str,
      $machine_id, $machine_name, $os, $os_version, $compiler, $clobber) = @_;

  $this->{START_TIME} = $start_time;
  $this->{END_TIME} = $end_time;
  $this->{TREE} = $tree;
  $this->{FIELD_SHORT_NAMES} = $field_short_names;
  $this->{FIELD_PROCESSORS} = $field_processors;
  $this->{FIELD_HANDLERS} = $field_handlers;
  $this->{MACHINE_ID} = $machine_id;
  $this->{MACHINE_NAME} = $machine_name;
  $this->{OS} = $os;
  $this->{OS_VERSION} = $os_version;
  $this->{COMPILER} = $compiler;
  $this->{CLOBBER} = $clobber;
  $this->{PATCH_STR} = $patch_str;

  $this->{AT_START} = 1;
  $this->{STARTED_PRINTING} = 0;
  $this->{PROCESSED_THROUGH} = -1;

  return $this;
}

sub check_boundaries {
  my $this = shift;
  my ($time) = @_;
  if ($time < $this->{START_TIME}) {
    return $this->{START_TIME};
  }
  if ($time > $this->{END_TIME}) {
    return $this->{END_TIME};
  }
  return $time;
}

sub add_event {
  my $this = shift;
  my ($event) = @_;
  # "Fix" the previous event if it ended after this one began
  if (defined($this->{EVENTS}) && scalar(@{$this->{EVENTS}})) {
    if ($this->{EVENTS}[@{$this->{EVENTS}} - 1]{status_time} >
        $event->{build_time}) {
      $this->{EVENTS}[@{$this->{EVENTS}} - 1]{status_time} = $event->{build_time};
    }
  }
  push @{$this->{EVENTS}}, $event;
}

sub first_event_time {
  my $this = shift;
  my ($event) = @_;
  return $this->check_boundaries($this->{AT_START} ?
                                 $this->{EVENTS}[0]{build_time} :
                                 $this->{EVENTS}[0]{status_time});
}

sub pop_first {
  my $this = shift;
  my $event = $this->{EVENTS}[0];
  if ($this->{AT_START}) {
    $this->{AT_START} = 0;
    return ($this->check_boundaries($event->{build_time}), [$event, 1], 1);
  } else {
    shift @{$this->{EVENTS}};
    $this->{AT_START} = 1;
    return ($this->check_boundaries($event->{status_time}), [$event, 0], 0);
  }
}

sub is_empty {
  my $this = shift;
  return !defined($this->{EVENTS}) || !scalar(@{$this->{EVENTS}});
}

sub column_header {
  my $this = shift;
  my ($rows, $column) = @_;
  # Get the status from the first column
  my $class = "";
  for (my $row=0; $row < @{$rows}; $row++) {
    my $col = $rows->[$row][$column];
    if (defined($col)) {
      for (my $i=@{$col} - 1; $i >= 0; $i--) {
        # Ignore "incomplete" status and pick up first complete status to show
        # state of entire tree
        if ($col->[$i][0]{status} >= 100 && $col->[$i][0]{status} < 300) {
          $class = " class=status" . $col->[$i][0]{status};
          last;
        }
      }
    }
  }
  return "<th rowspan=2$class>$this->{MACHINE_NAME} $this->{OS} $this->{OS_VERSION} @{[$this->{CLOBBER} ? 'Clbr' : 'Dep']}<br><a href='adminmachine.pl?machine_id=$this->{MACHINE_ID}'>Edit</a> | <a href='adminmachine.pl?action=kick_machine&machine_id=$this->{MACHINE_ID}'>Kick</a></th>";
}

sub column_header_2 {
  return "";
}

sub cell {
  my $this = shift;
  my ($rows, $row_num, $column) = @_;

  my $cell = $rows->[$row_num][$column];

  #
  # If this is the first time cell() has been called, we look for the real
  # starting row, and if it has an in-progress status, we move the end event
  # into the current cell so it shows continuous like it should.
  #
  if (!$this->{STARTED_PRINTING}) {
    if (!defined($cell)) {
      for (my $i = $row_num-1; $i >= 0; $i--) {
        if (defined($rows->[$i][$column])) {
          my $last_event = @{$rows->[$i][$column]} - 1;
          if ($rows->[$i][$column][$last_event][0]{status} < 100) {
            # Take the top event from that cell and put it in this one
            push @{$cell}, pop @{$rows->[$i][$column]};
            if (!@{$rows->[$i][$column]}) {
              $rows->[$i][$column] = undef;
            }
            last;
          }
        }
      }
    }
    # XXX uncomment for debug
    if (defined($cell)) {
      # die if it is a start cell
      if ($cell->[@{$cell} - 1][1]) {
        die "Start cell without end cell found!";
      }
    }
    # XXX end debug

    $this->{STARTED_PRINTING} = 1;
  }

  #
  # Print the cell
  #
  if (defined($cell)) {
    #
    # If the last event in this cell is an end event, we print a td all the way
    # down to and including the corresponding start cell.
    #
    # XXX If there is a start/end/start[/end] situation, print the other
    # start/end as well
    #
    my $top_event_info = $cell->[@{$cell} - 1];
    my ($top_event, $top_is_start) = @{$top_event_info};
    my $retval = "";
    if (!$top_is_start) {
      # Search for the start tag (only need to search if the end tag is the only
      # one in this cell)
      my $rowspan;
      if (@{$cell} == 1) {
        my $i;
        for ($i = $row_num - 1; $i >= 0; $i--) {
          if (defined($rows->[$i][$column])) {
            last;
          }
        }
        $rowspan = $row_num - $i + 1;
        # XXX uncomment to debug
        if ($i == -1) {
          die "End tag without start tag found!";
        }
        # XXX end debug
      } else {
        $rowspan = 1;
      }

      $retval = "<td class='status$top_event->{status}'";
      $retval .= ($rowspan == 1 ? "" : " rowspan=$rowspan") . ">";

      # Print "L" (log and add comment)
      {
        my $popup_str = <<EOM;
<b>$this->{MACHINE_NAME}</b><br>
<a href='showlog.pl?machine_id=$this->{MACHINE_ID}&logfile=$top_event->{logfile}'>Show Log</a><br>
<a href='buildcomment.pl?tree=$this->{TREE}&machine_id=$this->{MACHINE_ID}&build_time=$top_event->{build_time}'>Add Comment</a>
EOM
        $retval .= "<a href='showlog.pl?machine_id=$this->{MACHINE_ID}&logfile=$top_event->{logfile}' onclick='return do_popup(event, \"log\", \"" . escape_html(escape_js($popup_str)) . "\")'>L</a>\n";
      }

      # Print comment star
      if (defined($top_event->{comments}) && @{$top_event->{comments}} > 0) {
        my $popup_str = "<strong>Comments</strong> (<a href='buildcomment.pl?tree=$this->{TREE}&machine_id=$this->{MACHINE_ID}&build_time=$top_event->{build_time}'>Add Comment</a>)<br>";
        foreach my $comment (sort { $b->[2] <=> $a->[2] } @{$top_event->{comments}}) {
          $popup_str .= "<a href='mailto:$comment->[0]'>$comment->[0]</a> - " . time2str("%H:%M", $comment->[2]) .
                        "<br><p><code>$comment->[1]</code></p>";
        }


        $retval .= "<a href='#' onclick='return do_popup(event, \"comments\", \"" . escape_html(escape_js($popup_str)) . "\")'><img src='star.gif'></a>\n";
      }

      $retval .= "<br>\n";

      {
        my $build_time = ($top_event->{status_time} - $top_event->{build_time});
        my $build_time_str = "";
        if ($build_time > 60*60) {
          $build_time_str .= int($build_time / (60*60)) . "h";
          $build_time %= 60*60;
        }
        if ($build_time > 60) {
          $build_time_str .= int($build_time / 60) . "m";
          $build_time %= 60;
        }
        if (!$build_time_str) {
          $build_time_str = $build_time . "s";
        }
        $retval .= "<b>Time:</b> $build_time_str<br>\n";
      }
      $retval .= "<b>Status:</b> $top_event->{status}<br>\n";
      foreach my $field (@{$top_event->{fields}}) {
        my $processor = $this->{FIELD_PROCESSORS}{$field->[0]};
        $processor = "default" if !$processor;
        my $handler = $this->{FIELD_HANDLERS}{$processor};
        $retval .= $handler->process_field($this, $field->[0], $field->[1]);
      }
      $retval .= "</td>";

      $this->{PROCESSED_THROUGH} = $row_num - $rowspan + 1;
    }

    #
    # If there are multiple events in the cell and the first event is an end
    # event, we move it into the next cell so it will be printed there.
    #
    if (@{$cell} > 1 && !$cell->[0][1]) {
      # XXX uncomment to debug
      if ($row_num == 0) {
        die "End tag without start tag found!";
      }
      # XXX end debug
      push @{$rows->[$row_num-1][$column]}, shift @{$cell};
    }

    return $retval;
  } elsif ($row_num < $this->{PROCESSED_THROUGH} ||
           $this->{PROCESSED_THROUGH} == -1) {
    # Print empty cell large enough to cover empty rows
    my $i;
    for ($i = $row_num-1; $i >= 0; $i--) {
      if (defined($rows->[$i][$column])) {
        last;
      }
    }
    $this->{PROCESSED_THROUGH} = $i + 1;
    my $rowspan = (($row_num - $i) > 1) ? " rowspan=" . ($row_num - $i) : "";
    return "<td$rowspan></td>\n";
  }
  return "";
}


#
# Method to get a the TreeColumns objects for a tree
#
sub get_tree_column_queues {
  my ($p, $dbh, $start_time, $end_time, $tree, $field_short_names, $field_processors, $field_handlers, $patch_str) = @_;
  #
  # Get the list of machines
  #
  my $sth = $dbh->prepare("SELECT machine_id, machine_name, os, os_version, compiler, clobber, visible FROM tbox_machine WHERE tree_name = ?");
  $sth->execute($tree);

  my %columns;
  while (my $row = $sth->fetchrow_arrayref) {
    $columns{$row->[0]} = new Tinderbox3::TreeColumns($start_time, $end_time, $tree, $field_short_names, $field_processors, $field_handlers, $patch_str, @{$row});
  }

  if (!keys %columns) {
    return ();
  }

  #
  # Dump the relevant events into the columns
  #
  $sth = $dbh->prepare(
    "SELECT b.machine_id, " . Tinderbox3::DB::sql_get_timestamp("b.build_time") . ",
            " . Tinderbox3::DB::sql_get_timestamp("b.status_time") . ", b.status, b.log
       FROM tbox_build b
      WHERE b.machine_id IN (" . join(", ", map { "?" } keys %columns) . ")
            AND b.status_time >= " . Tinderbox3::DB::sql_abstime("?") . "
            AND b.build_time <= " . Tinderbox3::DB::sql_abstime("?") . "
    ORDER BY b.build_time, b.machine_id");
  $sth->execute(keys %columns, $start_time, $end_time);
  while (my $build = $sth->fetchrow_arrayref) {
    my $machine_id = $build->[0];
    my $build_time = $build->[1];
    my $event = { build_time => $build->[1], status_time => $build->[2],
               status => $build->[3], logfile => $build->[4], fields => [] };

    my $fields = $dbh->selectall_arrayref("SELECT name, value FROM tbox_build_field WHERE machine_id = ? AND build_time = " . Tinderbox3::DB::sql_abstime("?"), undef, $machine_id, $build_time);
    foreach my $field (@{$fields}) {
      push @{$event->{fields}}, [ $field->[0], $field->[1] ];
    }

    my $comments = $dbh->selectall_arrayref("SELECT login, build_comment, " . Tinderbox3::DB::sql_get_timestamp("comment_time") . " FROM tbox_build_comment WHERE machine_id = ? AND build_time = " . Tinderbox3::DB::sql_abstime("?"), undef, $machine_id, $build_time);
    foreach my $comment (@{$comments}) {
      push @{$event->{comments}}, [ $comment->[0], $comment->[1], $comment->[2] ];
    }

    $columns{$machine_id}->add_event($event);
  }

  return sort { $a->{MACHINE_NAME} cmp $b->{MACHINE_NAME} } (map { defined($_->{EVENTS}) ? ($_) : () } values %columns);
}


1
