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

package Tinderbox3::ShowBuilds;

use strict;
use Date::Format;
use Tinderbox3::Header;
use Tinderbox3::TreeColumns;
use Tinderbox3::BonsaiColumns;
use Tinderbox3::BuildTimeColumn;

sub print_showbuilds {
  my ($p, $dbh, $fh, $tree, $start_time, $end_time,
      $min_row_size, $max_row_size) = @_;

  #
  # Get tree and patch info
  #
  my $tree_info = $dbh->selectrow_arrayref("SELECT field_short_names, field_processors, header, footer, special_message, sheriff, build_engineer, cvs_co_date, status, min_row_size, max_row_size FROM tbox_tree WHERE tree_name = ?", undef, $tree);
  if (!$tree_info) {
    die "Tree $tree does not exist!";
  }
  my ($field_short_names, $field_processors_str, $header, $footer,
      $special_message, $sheriff, $build_engineer, $cvs_co_date, $status,
      $default_min_row_size, $default_max_row_size) = @{$tree_info};
  $min_row_size = $default_min_row_size if !defined($min_row_size);
  $max_row_size = $default_max_row_size if !defined($max_row_size);
  my %field_processors;
  # Create the handlers for the different fields
  require Tinderbox3::FieldProcessors::default;
  my %field_handlers = ( default => new Tinderbox3::FieldProcessors::default );
  foreach my $field_processor (split /,/, $field_processors_str) {
    my ($field, $processor) = split /=/, $field_processor;
    $field_processors{$field} = $processor;
    # Check if the processor is OK to put in an eval statement
    if ($processor =~ /^([A-Za-z]+)$/) {
      my $code = "require Tinderbox3::FieldProcessors::$1; \$field_handlers{$1} = new Tinderbox3::FieldProcessors::$1();";
      eval $code;
    }
  }

  # Get sizes in seconds for easy comparisons
  $min_row_size *= 60;
  $max_row_size *= 60;

  #
  # Construct the a href and such for the patches
  # XXX do this lazily in case there are no patches
  #
  my %patch_str;
  {
    my $sth = $dbh->prepare("SELECT patch_id, patch_name, patch_ref, patch_ref_url, in_use FROM tbox_patch WHERE tree_name = ?");
    $sth->execute($tree);
    while (my $row = $sth->fetchrow_arrayref) {
      my $str;
      my $class;
      if (!$row->[4]) {
        $class = " class=obsolete";
      } else {
        $class = "";
      }
      $str = "<span$class><a href='get_patch.pl?patch_id=$row->[0]'>$row->[1]</a>";
      if ($row->[2]) {
        $str .= " (";
        $str .= "<a href='$row->[3]'>" if ($row->[3]);
        $str .= $row->[2];
        $str .= "</a>" if ($row->[3]);
        $str .= ")";
      }
      $str .= "</span>";
      $patch_str{$row->[0]} = $str;
    }
  }

  print $fh "",insert_dynamic_data($header, $tree, $sheriff, $build_engineer, $cvs_co_date, $status, \%patch_str, $start_time, $end_time);
  print $fh "",insert_dynamic_data($special_message, $tree, $sheriff, $build_engineer, $cvs_co_date, $status, \%patch_str, $start_time, $end_time);

  print_tree($p, $dbh, $fh, $tree, $start_time, $end_time, $field_short_names,
             \%field_processors, \%field_handlers, $min_row_size, $max_row_size,
             \%patch_str);

  print $fh "",insert_dynamic_data($footer, $tree, $sheriff, $build_engineer, $cvs_co_date, $status, \%patch_str, $start_time, $end_time);

}


sub insert_dynamic_data {
  my ($str, $tree, $sheriff, $build_engineer, $cvs_co_date, $status, $patch_str, $start_time, $end_time) = @_;

  $str =~ s/#TREE#/$tree/g;
  my $time = time2str("%c %Z", time);
  $str =~ s/#TIME#/$time/g;
  $str =~ s/#SHERIFF#/$sheriff/g;
  $str =~ s/#BUILD_ENGINEER#/$build_engineer/g;
  $cvs_co_date = 'current' if !$cvs_co_date;
  $str =~ s/#CVS_CO_DATE#/$cvs_co_date/g;
  $str =~ s/#STATUS#/$status/g;
  $str =~ s/#START_TIME_MINUS\((\d+)\)#/$start_time - $1/eg;
  $str =~ s/#END_TIME#/$end_time/g;

  if ($str =~ /#PATCHES#/) {
    my $patches_str = "";
    if (keys %{$patch_str}) {
      $patches_str = join(', ', values %{$patch_str});
    } else {
      $patches_str = "None";
    }
    $str =~ s/#PATCHES#/$patches_str/g;
  }

  return $str;
}

sub print_tree {
  my ($p, $dbh, $fh, $tree, $start_time, $end_time, $field_short_names,
      $field_processors, $field_handlers, $min_row_size, $max_row_size,
      $patch_str) = @_;

  # Get the information we will be laying out in the table
  my @event_queues;
  push @event_queues, new Tinderbox3::BuildTimeColumn($p, $dbh);
  push @event_queues, Tinderbox3::BonsaiColumns::get_bonsai_column_queues($p, $dbh, $start_time, $end_time, $tree);
  push @event_queues, Tinderbox3::TreeColumns::get_tree_column_queues($p, $dbh, $start_time, $end_time, $tree, $field_short_names, $field_processors, $field_handlers, $patch_str);

  my $row_num = -1;
  my @rows;
  EVENTLOOP:
  while (1) {
    #
    # Get the oldest event from a queue
    #
    my ($event_time, $event, $please_split);
    my $column;
    {
      my $most_recent_queue = -1;
      my $most_recent_time = -1;
      for (my $queue_num = 1; $queue_num < @event_queues; $queue_num++) {
        my $queue = $event_queues[$queue_num];
        if (!$queue->is_empty()) {
          if ($most_recent_time == -1 ||
              $queue->first_event_time() < $most_recent_time) {
            $most_recent_queue = $queue_num;
            $most_recent_time = $queue->first_event_time();
          }
        }
      }
      # Break if there were no non-empty queues
      if ($most_recent_time < 0) {
        last EVENTLOOP;
      }
      ($event_time, $event, $please_split) = $event_queues[$most_recent_queue]->pop_first();
      if ($event_time != $most_recent_time) {
        die "Event time not what was expected!";
      }
      $column = $most_recent_queue;
    }

    #
    # If there are no rows yet, create the first row with this event time
    #
    if ($row_num == -1) {
      push @rows, [ $event_time ];
      $row_num++;
    } else {
      #
      # If event is outside the maximum boundary, start adding rows of
      # max_row_size to compensate
      #
      # XXX potential problem: one really wants cells to start at events
      # whenever possible, and when we use this algorithm, if the event in
      # question happens to be inside the minimum row time, we will not split
      # for it so the cell will not start at the event.  This can be compensated
      # for by building these new cells *down* from the cell in question, but
      # it would require more strange cases than I care to deal with right now
      # so I'm not coding it.  JBK
      #
      if ($max_row_size > 0) {
        while ($event_time > ($rows[$row_num][0] + $max_row_size)) {
          push @rows, [ $rows[$row_num][0] + $max_row_size ];
          $row_num++;
        }
      }

      #
      # If event has asked to split, and is outside the minimum boundary (so
      # that we *can* split, split the row.
      #
      if ($please_split && $event_time > ($rows[$row_num][0] + $min_row_size)) {
        push @rows, [ $event_time ];
        $row_num++;
      }
    }

    #
    # Finally, add the event to the current row.
    #
    push @{$rows[$row_num][$column]}, $event;
  }

  #
  # Ensure there is at least one row
  #
  if ($row_num < 0) {
    push @rows, [ $start_time ];
    $row_num++;
  }

  #
  # Add extra rows if the tinderbox does not go up to the end time
  #
  if ($max_row_size > 0) {
    while ($end_time > ($rows[$row_num][0] + $max_row_size)) {
      push @rows, [ $rows[$row_num][0] + $max_row_size ];
      $row_num++;
    }
  }

  #
  # Add extra rows if the tinderbox does not start at the start time
  #
  if ($start_time < $rows[0][0]) {
    if ($max_row_size > 0) {
      do {
        unshift @rows, [ $rows[0][0] - $max_row_size ];
      } while ($start_time < $rows[0][0]);
      # Fix the last row to be start time ;)
      if ($rows[0][0] < $start_time) {
        $rows[0][0] = $start_time;
      }
    } else {
      unshift @rows, [ $start_time ];
    }
  }

  #
  # Print head of table
  #
  print $fh "<table class=tinderbox>\n";
  print $fh "<thead><tr>\n";
  for (my $queue_num = 0; $queue_num < @event_queues; $queue_num++) {
    print $fh $event_queues[$queue_num]->column_header(\@rows, $queue_num);
  }
  print $fh "</tr>\n";

  print $fh "<tr>\n";
  for (my $queue_num = 0; $queue_num < @event_queues; $queue_num++) {
    print $fh $event_queues[$queue_num]->column_header_2(\@rows, $queue_num);
  }
  print $fh "</tr></thead>\n";

  #
  # Print body of table
  #
  print $fh "<tbody>\n";
  for(my $row_num = (@rows - 1); $row_num >= 0; $row_num--) {
    print $fh "<tr>";
    for (my $queue_num = 0; $queue_num < @event_queues; $queue_num++) {
      print $fh $event_queues[$queue_num]->cell(\@rows, $row_num, $queue_num);
    }
    print $fh "</tr>\n";
  }
  print $fh "</tbody>\n";
  print $fh "</table>\n";
}

1
