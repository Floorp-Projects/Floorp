# -*- Mode: perl; indent-tabs-mode: nil -*-
#

# Build.pm - the module which processes the updates posted by
# processmail (the data originally came from the clients building the
# trees) and creates the colored boxes which show what the state of
# the build was and display a link to the build log.


# $Revision: 1.63 $ 
# $Date: 2003/08/04 14:32:41 $ 
# $Author: kestes%walrus.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/lib/TinderDB/Build.pm,v $ 
# $Name:  $ 


# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is the Tinderbox build tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#

# complete rewrite by Ken Estes:
#	 kestes@staff.mail.com Old work.
#	 kestes@reefedge.com New work.
#	 kestes@walrus.com Home.
# Contributor(s): 




# a build record is an anonymous hash with these fields

# $buildrec = {    
#       binaryname => The full path name to the binary 
#                      which this build produced
#			(if applicable)
#	bloatdata => A string to be displayed to the users showing the 
#			 bloat memory test information.
#       buildname  => The name of the build 
#                      (including the hostname, and 
#			OS of the buildmachine)
#       status => The final status of the build 
#       		(success, build_failed, test_failed, etc)
#       info => A string to be displayed to users interested in this build.
#
#       timenow => The time that the status was last reported
#       starttime => The time the build started  (in time() format)
#       endtime   => The time the build ended (in time() format)
#       runtime   => The time it took the build to complete (in seconds)
#       deadtime   => The time between the last build ending and 
#                      this build starting (in seconds).  Most organizations 
#                      will have this be zero all the time.  Some shops may 
#                      run the build scrips through cron and this will cause 
#                      'deadtime'.
#
#	errors => The number of errors found in the build file, will be displayed 
#			in the build cell.
#	print => Any strings or links which were sent from the buildmachine 
#		 to be displayed in the build cell.
#       errorparser => The error parser to use when parsing the logfiles
#       full-log  => The basename of the log file contianing the full log
#       brief-log => The basename of the log file contianing the brief log
#       fulllog  => The full URL of the log file contianing the full log
#       brieflog => The full URL of the log file contianing the brief log
#      };


# $DATABASE{$tree}{$buildname}{'recs'} is an anonymous list
# of buildrecs orderd by build time.  The most recent rec is 0;

# the following build summary information is also stored in the database.

# $DATABASE{$tree}{$buildname}{'earliest_failure'}
#    If the build is broken then it is the first failed build which
#    followed a successful build. Otherwise it is not defined


# $DATABASE{$tree}{$buildname}{'recent_success'}
#    If the build is broken it is the first sucessful build following
#    the minumum of all earliest_failures.

# $DATABASE{$tree}{$buildname}{'average_buildtime'}
#     the average time a sucessful build takes(in seconds).

# $DATABASE{$tree}{$buildname}{'average_deadtime'}
#     the average deadtime (in seconds).

#  $METADATA{$tree}{'updates_since_trim'}+= 1



# While we do the traversal of the table to build the HTML page
# $PRINT_NEXT{$tree}{$buildname} keeps track of the current index into
# each columns list between calls to status_table_row.  We need this
# information since one build will span many cells.


# If the value: $IGNORE_BUILDS{$tree}{$buildname} is defined then the
# build column will not be displayed in the html.

# if the ocmmand line argument $main::NOIGNORE is defined then we turn
# off the ingore-builds feature.

#  The list @LATEST_STATUS holds the status (success, test-failed,
#  etc) for each build column which we do not ignore

#  The list @BUILD_NAMES holds the name for each build column which we
#  do not ignore


# Lets enumerate all the ways that the TinderDB abstraction is broken
# by the Build module:

# *) the summary functions will look in this namespace for
#	  @LATEST_STATUS
#	  @BUILD_NAMES

# *) read file to load $IGNORE_BUILDS in addition 
#	to the regular DB updates

# *) Peek at cmd/web argument $main::NOIGNORE

# *) admintree calls a few DB functions directly in build without
#        going through the DB interface for all DB's.




package TinderDB::Build;

# Load standard perl libraries
use File::Basename;

# Load Tinderbox libraries

use lib '#tinder_libdir#';

use HTMLPopUp;
use BuildStatus;
use TinderDB::BasicTxtDB;
use VCDisplay;
use Utils;
use TinderConfig;
use TinderDB::Notice;
#use TinderHeader;


$VERSION = '#tinder_version#';

@ISA = qw(TinderDB::BasicTxtDB);

if (defined ($TinderConfig::DISPLAY_BUILD_ERRORS)) {
    $DISPLAY_BUILD_ERRORS = $TinderConfig::DISPLAY_BUILD_ERRORS;
} else {
    $DISPLAY_BUILD_ERRORS = 1;
}


# The number of most recent builds used in computing the averages
# which estimate the build times.

$NUM_OF_AVERAGE = 10;

# We 'have a' notice so that we can put notices in the column to which they apply.
 
$NOTICE= TinderDB::Notice->new();

$DEBUG=1;

# Find the name of each build and the proper order to display them.
# No part of the code should peek at keys %{ $DATABASE{$tree} } directly.

sub build_names {
  my ($tree) = (@_);
  
  my @outrow = ();
  
  foreach $buildname (keys %{ $DATABASE{$tree} } ){
    
    # skip this column?
    
    (!($main::NOIGNORE)) &&
      ($IGNORE_BUILDS{$tree}{$buildname}) && 
        next;
    
    push @outrow, $buildname;
  }

  @outrow = TreeData::sort_tree_buildnames( 
                                           $tree,
                                           [@outrow],
                                          );
  
  return @outrow;
}


# The admin tree program needs to know all the build names so it can
# set ignore_builds.

# note that the admintree.cgi program calls this function directly.

sub all_build_names {
  my ($tree) = (@_);
  
  my (@build_names) = keys %{ $DATABASE{$tree} };
  my (@outrow) =  TreeData::sort_tree_buildnames( 
                                                 $tree,
                                                 [@build_names],
                                                );

  return @outrow;
}




# return find the most recent status for each build of this tree

sub latest_status {
  my ($tree) = (@_);
  
  my (@outrow) = ();
  my (@build_names) = build_names($tree);

  foreach $buildname (@build_names) {
    
    my ($last_status);
    foreach $db_index (0 .. $#{ $DATABASE{$tree}{$buildname}{'recs'} }) {
      
      my ($rec) = $DATABASE{$tree}{$buildname}{'recs'}[$db_index];
      my ($buildstatus) = $rec->{'status'};
      
      (BuildStatus::is_status_final($buildstatus))  ||
        next;

      $last_status = $buildstatus;
      last;
    } # foreach $db_index

    if ($last_status) {
      push @outrow, $last_status;
    } else {

      # If we really have no data try and get 
      # 'not running'/'in progress' information

      my ($rec) = $DATABASE{$tree}{$buildname}{'recs'}[0];
      push @outrow, $rec->{'status'};
    }

  } # foreach $buildname

  return @outrow;
}

# return find the most recent error count for each build of this tree

sub latest_errors {
  my ($tree) = (@_);
  
  if (!($DISPLAY_BUILD_ERRORS)) {
      # We're not collecting error status, return indication of that
      return ();
  }

  my (@outrow) = ();
  my (@build_names) = build_names($tree);

  foreach $buildname (@build_names) {

      my ($last_error);
      foreach $db_index (0 .. $#{ $DATABASE{$tree}{$buildname}{'recs'} }) {
      
        my ($rec) = $DATABASE{$tree}{$buildname}{'recs'}[$db_index];
        my ($builderrors) = $rec->{'errors'};
        my ($buildstatus) = $rec->{'status'};
      
        (BuildStatus::is_status_final($buildstatus))  ||
          next;

        $last_error = $builderrors;
        last;
      } # foreach $db_index

      if ($last_error) {
        push @outrow, $last_error;
      } else {

        # If we really have no data try and get 
        # 'not running'/'in progress' information

        my ($rec) = $DATABASE{$tree}{$buildname}{'recs'}[0];
        push @outrow, $rec->{'errors'};
      }

  } # foreach $buildname

  return @outrow;
}



# where can people attach notices to?
# Really this is the names the columns produced by this DB

sub notice_association {
    my ($self, $tree) = (@_);
    return build_names($tree);
}

#  Prepare information for popupwindows on row headers and also the
#  link to bonsai giving the aproximate times that the build was
#  broken.

sub gettree_header {
  my ($self, $tree) = (@_);
  
  my ($out) = '';

  (TreeData::tree_exists($tree)) ||
    die("Tree: $tree, not defined.");
  
  # this is not working the way I want it to.  Individual columns are
  # good but the max/min to combine multiple columns is broken.  We
  # need to introduce the notion of "adjecent cell" to get this to
  # work right. I will debug it later for now just exit this function.

  return $out;

  # find our best guess as to when the tree broke.

  my (@earliest_failure) = ();
  my (@recent_success) = ();

  my (@build_names) = build_names($tree);

  foreach $buildname (@build_names){
    
    my $earliest_failure = undef;

    foreach $db_index (0 .. $#{ $DATABASE{$tree}{$buildname}{'recs'} }) {
      
      my ($rec) = $DATABASE{$tree}{$buildname}{'recs'}[$db_index];
      my ($buildstatus) = $rec->{'status'};

      (BuildStatus::is_status_final($buildstatus)) ||
        next;

      if ($buildstatus eq 'success') {
        $earliest_failure = $rec->{'endtime'};
        last;
      }
      
    } # each $db_index

    push @earliest_failure, $earliest_failure;
    $DATABASE{$tree}{$buildname}{'earliest_failure'} = $earliest_failure;
  }

  # find the oldest time when any columns current failures began

  my $earliest_failure = main::min(@earliest_failure);

  defined($earliest_failure) || return ;

  # find our best guess as to when the tree was last good.
  
  (@build_names) = build_names($tree);
  foreach $buildname (@build_names) {
    
    my $recent_success = undef;
    foreach $db_index (0 .. $#{ $DATABASE{$tree}{$buildname}{'recs'} } ) {
      
      my ($rec) = $DATABASE{$tree}{$buildname}{'recs'}[$db_index];
      my ($buildstatus) = $rec->{'status'};
      my ($starttime) = $rec->{'starttime'};
      
      if ( defined($earliest_failure) && 
           ($earliest_failure < $starttime) ) {
        next;
      }
      
      (BuildStatus::is_status_final($buildstatus))  ||
        next;
      
      if ($buildstatus eq 'success') {
        $recent_success = $starttime;
        last;
      }
      
    } # each $db_index
    
    push @recent_success, $recent_success;
    $DATABASE{$tree}{$buildname}{'recent_success'} = $recent_success;
  }
  
  my $recent_success = main::max(@recent_success);
  defined($recent_success) || return ;

  my ($link) = '';

  if ( $recent_success < $earliest_failure ) {
    
    my ($txt) = ("Suspect that build broke in: [ ".
                 HTMLPopUp::timeHTML($recent_success).", ".
                 HTMLPopUp::timeHTML($earliest_failure)." ]");

    my ($link) = VCDisplay::query(
                                   'tree'=> $tree,
                                   'mindate'=> $recent_success,
                                   'maxdate'=> $earliest_failure,
                                   'linktxt'=> $txt,
                                  );
    
    $out .= $link;
  }

  return $out;
}




# Compute information which is relevant to the whole tree not a
# particular build.  This needs to be called each time the DATABASE is
# loaded.  This data depends on the ignore_builds file so can not be
# stored in the DATABASE but must be recomputed each time we run.
# Perhaps the gobal variables should someday move into METADATA for
# eases of maintinance, but will still need to be recomputed at the
# same places in the code.

sub compute_metadata {
  my ($self, $tree,) = @_;

  # notice that the reason we can not just inherit the loadtree_db()
  # from @ISA is precicely all the things which break the DB
  # abstraction.

  # order is important for these operations.
  my ($ignore_builds) = TinderHeader::gettree_header('IgnoreBuilds', $tree);

  foreach $buildname (split(/,/, $ignore_builds)) {
    $IGNORE_BUILDS{$tree}{$buildname} = 1;
  }

  eval {
    local $SIG{'__DIE__'} = \&null;
    @BUILD_NAMES = build_names($tree);
    @LATEST_STATUS = latest_status($tree);
  };

  return ;
}


sub loadtree_db {
  my ($self, $tree, ) = (@_);

  $self->SUPER::loadtree_db($tree);

  $self->compute_metadata($tree);

  return ;
}


# remove all records from the database which are older then
# $TinderDB::TRIM_SECONDS.  Since we are making a pass over all
# data this is a good time to find the average run time of the build.
# Both of these operations need not be run everytime the database is
# updated.

sub trim_db_history {
  my ($self, $tree, ) = (@_);
  
  my ($last_time) =  $main::TIME - $TinderDB::TRIM_SECONDS;

  my ($oldest_inactive_time) = $main::TIME - $main::SECONDS_PER_DAY;

  my (@all_build_names);

  # compute averages.

  @all_build_names = all_build_names($tree);
  foreach $buildname (@all_build_names) {

    my ($last_index) = undef;
    my (@run_times) = ();
    my (@dead_times) = ();

    my $recs = $DATABASE{$tree}{$buildname}{'recs'};
    foreach $db_index (0 .. $#{ $recs }) {
      
      my ($rec) = $recs->[$db_index];

      # If the runtime/deadtime is zero, it should still be counted in
      # the average.  All we care about is 'success'.

      if ( 
           (defined($rec->{'runtime'})) &&
           ($rec->{'status'} eq 'success') && 
           ($#run_times < $NUM_OF_AVERAGE) &&
           1) {
        push @run_times, $rec->{'runtime'};
      }
      
      if ( 
           (defined($rec->{'deadtime'})) &&
           ($rec->{'status'} eq 'success') && 
           ($#dead_times < $NUM_OF_AVERAGE) &&
           1) {
        push @dead_times, $rec->{'deadtime'};
      }
      
      if ($rec->{'starttime'} < $last_time) {
        $last_index = $db_index;
        last;
      }
      
    }

    # We do not use the very first datapoint as it is probably
    # incomplete.

    pop @run_times;
    pop @dead_times;

    # medians are a more robust statistical estimator then the mean.
    # They will give us better answers then a typical "average"

    delete $DATABASE{$tree}{$buildname}{'average_buildtime'};
    my $run_avg = main::median(@run_times);
    ($run_avg) &&
      ( $DATABASE{$tree}{$buildname}{'average_buildtime'} = $run_avg);
    
    delete $DATABASE{$tree}{$buildname}{'average_deadtime'};
    my $dead_avg = main::median(@dead_times);
    ($dead_avg) &&
      ( $DATABASE{$tree}{$buildname}{'average_deadtime'} = $dead_avg);
    
    # the trim DB step

    if (defined($last_index)) {
      my @new_table = @{ $recs }[0 .. $last_index];
      $DATABASE{$tree}{$buildname}{'recs'} = [ @new_table ];
    }

    # columns which have not received data in a long while are purged.

    my ($rec) = $DATABASE{$tree}{$buildname}{'recs'}[0];
    
    if ( 
         !(defined($rec)) ||
         ( $rec->{'starttime'} < $oldest_inactive_time ) ||
         0) {
        delete $DATABASE{$tree}{$buildname};
    }

  }
  return ;
}



# return a list of all the times where an even occured.

sub event_times_vec {
  my ($self, $start_time, $end_time, $tree) = (@_);

  my @times;

  my @build_names = build_names($tree);
  foreach $buildname (@build_names) {
      my ($num_recs) = $#{ $DATABASE{$tree}{$buildname}{'recs'} };
      foreach $i (0 .. $num_recs) {

          my $rec = $DATABASE{$tree}{$buildname}{'recs'}[$i];
          push @times, $rec->{'starttime'}, $rec->{'endtime'};

      }
  }

  # sort numerically descending
  @times = sort {$b <=> $a} @times;

  my @out;
  my $old_time = 0;
  foreach $time (@times) {
      ($time == $old_time) && next;
      ($time <= $start_time) || next;
      ($time <= $end_time) && last;

      $old_time = $time;
      push @out, $time;
  }

  return @out;
}

# return a list of all the start times.

sub start_times_vec {
  my ($self, $start_time, $end_time, $tree) = (@_);

  my @times;

  my @build_names = build_names($tree);
  foreach $buildname (@build_names) {
      my ($num_recs) = $#{ $DATABASE{$tree}{$buildname}{'recs'} };
      foreach $i (0 .. $num_recs) {

          my $rec = $DATABASE{$tree}{$buildname}{'recs'}[$i];
          push @times, $rec->{'starttime'};

      }
  }

  # sort numerically descending
  @times = sort {$b <=> $a} @times;

  my @out;
  my $old_time = 0;
  foreach $time (@times) {
      ($time == $old_time) && next;
      ($time <= $start_time) || next;
      ($time <= $end_time) && last;

      $old_time = $time;
      push @out, $time;
  }

  return @out;
}



# Print out the Database in a visually useful form so that I can
# debug timing problems.  This is not called by any code. I use this
# in the debugger.

sub debug_database {
  my ($self, $tree) = (@_);

  my @build_names = build_names($tree);
  foreach $buildname (@build_names) {
      print "$buildname\n";
      my ($num_recs) = $#{ $DATABASE{$tree}{$buildname}{'recs'} };
      foreach $i (0 .. $num_recs) {

          my $rec = $DATABASE{$tree}{$buildname}{'recs'}[$i];

          my $starttime = localtime($rec->{'starttime'});
          my $endtime = localtime($rec->{'endtime'});
          my $runtime = main::round($rec->{'runtime'}/$main::SECONDS_PER_MINUTE);
          my $deadtime = main::round($rec->{'deadtime'}/$main::SECONDS_PER_MINUTE);
          my $previousbuildtime = localtime($rec->{'previousbuildtime'});
          my $status = $rec->{'status'};

          print "\tendtime: $endtime\n";
          print "\tstarttime: $starttime\n";
          print "\tpreviousbuildtime: $previousbuildtime\n";
          print "\t\t\t\truntime: $runtime deadtime: $deadtime status: $status\n";
      }
  }

  return ;
}



sub status_table_legend {
  my ($out)='';

  # print user defined legend, this is typicaly all the possible links
  # which can be included in a build log.

  my $print_legend = BuildStatus::TinderboxPrintLegend();

$out .=<<EOF;
        <td align=right valign=top>
	<table $TinderDB::LEGEND_BORDER>
	<thead><tr>
		<td align=right>Build</td>
		<td align=left>Cell Links</td>
	</tr></thead>
              <tr><td align=center><TT>l</TT></td>
                  <td>= Brief Build Log</td></tr>
              <tr><td align=center><TT>L</TT></td>
                  <td>= Full Build Log</td></tr>
              <tr><td align=center><TT>C</TT></td>
                  <td>= Builds new Contents</td></tr>
              <tr><td align=center><TT>B</TT></td>
                  <td>= Get Binaries</td></tr>
                      $print_legend
	</table>
        </td>
EOF
  ;

  my @build_states = BuildStatus::get_all_sorted_status();

  my $state_rows;

  foreach $state (@build_states) {
    my ($cell_color) = BuildStatus::status2html_colors($state);
    my ($description) = BuildStatus::status2descriptions($state);
    my ($char) = BuildStatus::status2hdml_chars($state);
    my $text_browser_color_string = 
      HTMLPopUp::text_browser_color_string($cell_color, $char);

    $description = (

                    $text_browser_color_string.
                    $description.
                    $text_browser_color_string.

                    "");

    $state_rows .= (
                    "\t\t<tr bgcolor=\"$cell_color\">\n".
                    "\t\t<td>$description</td>\n".
                    "\t\t</tr>\n"
                   );
    
  }

$out .=<<EOF;
        <td align=right valign=top>
	<table $TinderDB::LEGEND_BORDER>
		<thead>
		<tr><td align=center>Build State Cell Colors</td></tr>
		</thead>
$state_rows
	</table>
        </td>
EOF
  ;


  return ($out);
}

# return the legend for the Summaries pages which are designed for
# text browsers.

sub hdml_legend {
  my ($out);
  my ($state_rows);

  my @build_states = BuildStatus::get_all_sorted_status();

  foreach $state (@build_states) {
    my ($char) = BuildStatus::status2hdml_chars($state);
    my ($description) = BuildStatus::status2descriptions($state);

    $state_rows .= "\t$char : $description<br>\n";
    
  }
  
  $out .=<<EOF
<DISPLAY NAME=help>
	Legend:<br>
$state_rows
</DISPLAY>
EOF
  ;

  return $out;
}



sub status_table_header {
  my ($self,  $tree, ) = @_;

  ( scalar(@BUILD_NAMES) ) ||
    return ();

  my (@outrow);

  foreach  $i (0 .. $#BUILD_NAMES) {
    
    my ($buildname) = $BUILD_NAMES[$i];
    my ($latest_status) = $LATEST_STATUS[$i];
    
    # skip this column?

    (!($main::NOIGNORE))  &&
      ($IGNORE_BUILDS{$tree}{$buildname}) && 
        next;

    # create popup text discribing how this build is progressing

    my $avg_buildtime = $DATABASE{$tree}{$buildname}{'average_buildtime'};
    my $avg_deadtime = $DATABASE{$tree}{$buildname}{'average_deadtime'};
    my $current_starttime = $DATABASE{$tree}{$buildname}{'recs'}[0]{'starttime'};
    my $current_endtime = $DATABASE{$tree}{$buildname}{'recs'}[0]{'endtime'};
    my $current_status = $DATABASE{$tree}{$buildname}{'recs'}[0]{'status'};
    my $previous_endtime = $DATABASE{$tree}{$buildname}{'recs'}[1]{'endtime'};
    my $current_finnished = BuildStatus::is_status_final($current_status);

    my $txt ='';    
    my $num_lines;

    $txt .= "time now: &nbsp;".&HTMLPopUp::timeHTML($main::TIME)."<br>";
    $num_lines++;

    if ($current_endtime) {
      $txt .= "previous end_time: &nbsp;";
      $txt .= &HTMLPopUp::timeHTML($previous_endtime)."<br>";
    }

    my $earliest_failure = $DATABASE{$tree}{$buildname}{'earliest_failure'};
    if ($earliest_failure){
      my $time = HTMLPopUp::timeHTML($earliest_failure);
      $txt .= "earliest_failure: &nbsp;$time<br>";
      $num_lines++;
    }
    my $recent_success = $DATABASE{$tree}{$buildname}{'recent_success'};
    if ($recent_success) {
      my $time = HTMLPopUp::timeHTML($recent_success);
      $txt .= "recent_success: &nbsp;$time<br>";
      $num_lines++;
    }

    $num_lines++;

    if ($avg_buildtime) {
        my $min = main::round($avg_buildtime/$main::SECONDS_PER_MINUTE);
      $txt .= "avg_buildtime (minutes): &nbsp;$min<br>";
      $num_lines++;
    }
    
    if ($avg_deadtime) {
        my $min =  main::round($avg_deadtime/$main::SECONDS_PER_MINUTE);
        $txt .= "avg_deadtime (minutes): &nbsp;$min<br>";
        $num_lines++;
    }
    
    $num_lines++;

    my $estimated_remaining = undef;

    if ($current_finnished) {
      my ($min) =  main::round(
                               ($main::TIME - $current_endtime)/
                               $main::SECONDS_PER_MINUTE
                               );
      $txt .= "current dead_time (minutes): &nbsp;$min<br>";
      $num_lines += 2;

      if ($avg_buildtime) {

        # The build has not started so must estimate at least
        # $avg_buildtime even if we have waited a long time already.
        # If we have waited a little time the estimate is:
        # $avg_buildtime + $avg_deadtime

          my ($estimated_dead_remaining) = 0;
          if ( ($avg_deadtime) && ($main::TIME - $current_endtime)) {
              $estimated_dead_remaining = 
                  main::max( ($avg_deadtime - 
                              ($main::TIME - $current_endtime)), 
                             0);
          }

        $estimated_remaining = ($estimated_dead_remaining + $avg_buildtime);
      }

    } elsif ($current_starttime) {
        my ($min) = main::round(
                                ($main::TIME  - $current_starttime)/
                                $main::SECONDS_PER_MINUTE
                                );
        $txt .= "current start_time: &nbsp;";
        $txt .= &HTMLPopUp::timeHTML($current_starttime)."<br>";
        $txt .= "current build_time (minutes): &nbsp;$min<br>";
        $num_lines += 2;

        if ($avg_buildtime) {
            $estimated_remaining = ($avg_buildtime) - 
                ($main::TIME - $current_starttime);
        }

    }

    if ($estimated_remaining) {
        my $min = main::round($estimated_remaining/
                                $main::SECONDS_PER_MINUTE);
      my $estimate_end_time = $main::TIME + $estimated_remaining;
      $txt .= "time_remaining (estimate): &nbsp;$min<br>";
      $num_lines++;

      $txt .= "estimated_end_time: &nbsp;";
      $txt .= &HTMLPopUp::timeHTML($estimate_end_time)."<br>";
      $num_lines++;
    }

    my $title = "Build Status Buildname: $buildname";

    my ($bg) = BuildStatus::status2html_colors($latest_status);
    my ($background) = '';
    my ($background_gif) = BuildStatus::status2header_background_gif(
                                     $latest_status);
    if ( ($FileStructure::GIF_URL) && ($background_gif) ) {
        $background = "background='$FileStructure::GIF_URL/$background_gif'";

        # the flames gif which is popular at netscape is causing me
        # trouble.  when I use it, I have trouble reading the column
        # heading text in the cell.  I may have to find a different
        # gif or clever font setting to make this legable.

        $buildname = (
#                      "<font color=white>".
                      $buildname.
#                      "</font>".
                      "");
    }

    my $link = HTMLPopUp::Link(
                               "windowtxt"=>$txt,
                               "windowtitle" => $title,
                               "linktxt"=> $buildname,
                               "href"=>"",
                         );

    my ($header) = ("\t<th rowspan=1 bgcolor=$bg $background>".
                    "<font face='Arial,Helvetica' size=-1>".
                    $link.
                    "</font></th>\n");

    push @outrow, ($header);

  } # foreach $i
  
  return @outrow;
}


sub apply_db_updates {
  my ($self, $tree, ) = @_;
  
  # If the cell immediately after us is defined, then we can have
  # a previousbuildtime.  New builds always start right after old
  # builds finish.  The only time there is a pause, is when the
  # old build broke right away.  Hence we can use the difference
  # in start time as the time for the build.  If this is wrong the
  # build broke early and we do not care about the runtime.  Throw
  # away updates from newbuilds which start before mintime is up.

  my ($filename) = $self->update_file($tree);
  my ($dirname)= File::Basename::dirname($filename);
  my ($prefixname) = File::Basename::basename($filename);
  my (@sorted_files) = $self->readdir_file_prefix( $dirname, 
                                                   $prefixname);

  scalar(@sorted_files) || return 0;

  foreach $file (@sorted_files) {
    my ($record) = Persistence::load_structure($file);

    ($record) ||
      die("Error reading Build update file '$file'.\n");

    my ($build) = $record->{'buildname'};
    my ($buildstatus) = $record->{'status'};
    my ($starttime) = $record->{'starttime'};
    my ($timenow) =  $record->{'timenow'};

    my ($previous_rec) = $DATABASE{$tree}{$build}{'recs'}[0];

    # sanity check the record, taint checks are done in processmail.
    {
      BuildStatus::is_status_valid($buildstatus) ||
        die("Error in updatefile: $file, Status not valid");
      
      ($tree eq $record->{'tree'}) ||
        die("Error in updatefile: $file, ".
            "Tree: $tree, equal to Tree: $record->{'tree'}.");

      (main::is_time_valid($starttime)) ||
        die("Error in updatefile: $file, ".
            "starttime: $starttime, is not a valid time.");

      (main::is_time_valid($timenow)) ||
        die("Error in updatefile: $file, ".
            "timenow: $timenow, is not a valid time.");

      ($starttime <= $timenow) ||
        die("Error in updatefile: $file, ".
            "starttime: $starttime, is less then timenow: $timenow.");

   }  

    if ( defined($previous_rec->{'starttime'}) ) {
      
      my ($safe_separation) = ($TinderDB::TABLE_SPACING * 
                               $main::SECONDS_PER_MINUTE);

      # add a few seconds for safety
      $safe_separation += 100; 
      
      my ($separation) = ($record->{'starttime'} - 
                          $previous_rec->{'starttime'});
         
      my ($different_builds) = ($record->{'starttime'} !=
                                $previous_rec->{'starttime'});

      # Keep the spacing between builds greater then our HTML grid
      # spacing.  There can be very frequent updates for any build
      # but different builds must be spaced apart.

      # If updates start too fast, remove older build from database.

      if ($different_builds) {
          if ($separation < $safe_separation) {

              # I would like the admins to know about this condition,
              # but if they can not fix it for political reasons then
              # we do not want to clutter up the log with this warning.

#              print main::LOG (
#                               "Not enough separation between builds. ".
#                               "safe_separation: $safe_separation ".
#                               "separation: $separation ".
#                               "tree: $tree ".
#                               "build: $build \n".
#                               "");

              # Remove old entry
              shift @{ $DATABASE{$tree}{$build}{'recs'} };
              $previous_rec = $DATABASE{$tree}{$build}{'recs'}[0];
          }

          # Some build machines are buggy and new builds appear to
          # start before old builds finish. Fix the incoming data
          # here so that builds do not overlap. Do not tamper with
          # the start time since many different mail messages will
          # have this new start time.  Instead we fix the end time
          # of the last build.
          
          if ( 
               ($previous_rec->{'endtime'}) &&
               ($record->{'starttime'} < $previous_rec->{'endtime'}) 
               ) {
              $previous_rec->{'endtime'} = $record->{'starttime'};
          }
       } 
    }

    # Is this report for the same build as the [0] entry? If so we do not
    # want two entries for the same build. Must throw out either
    # update or record in the database.

    if ( defined($previous_rec->{'starttime'}) &&
         ($record->{'starttime'} == $previous_rec->{'starttime'})
         ) {
        
        if (BuildStatus::is_status_final($previous_rec->{'status'})) {
            # Ignore the new entry if old entry was final.
            next;
        }
        
        # Remove old entry if it is not final.

        shift @{ $DATABASE{$tree}{$build}{'recs'} };
    } 

    # Add the record to the datastructure.
    
    if ( defined( $DATABASE{$tree}{$build}{'recs'} ) ) {
      unshift @{ $DATABASE{$tree}{$build}{'recs'} }, $record;
    } else { 
      $DATABASE{$tree}{$build}{'recs'} = [ $record ];
    }    

    # If there is a final disposition then we need to add a bunch of
    # other data which depends on what is already available.
    
      if ($previous_rec->{'starttime'}) {

        # show what has new has made it into this build, it is also what
        # has changed in VC during the last build.

        $record->{'previousbuildtime'} = $previous_rec->{'starttime'};

      }

      $record->{'runtime'} = (
                              $record->{'timenow'} - 
                              $record->{'starttime'} 
                              );

      if ($previous_rec->{'endtime'}) {
          $record->{'deadtime'} = ( 
                                    $record->{'starttime'} - 
                                    $previous_rec->{'endtime'} 
                                    );
      }

      # fix for mozilla.org issues

      if ($record->{'deadtime'}) {
          $record->{'deadtime'} = main::max( 0, $record->{'deadtime'} );
      }

      if (!defined($record->{'endtime'})) {
          $record->{'endtime'} = $record->{'timenow'};
      }

      # construct text to be displayed to users interested in this cell

      my ($info) = '';
      $info .= ("endtime: ".
                &HTMLPopUp::timeHTML($record->{'endtime'}).
                "<br>");
      $info .= ("starttime: ".
                &HTMLPopUp::timeHTML($record->{'starttime'}).
                "<br>");

      # round the division

      $info .= ("runtime: ".
              main::round($record->{'runtime'}/$main::SECONDS_PER_MINUTE).
                " (minutes)<br>");
      if ($record->{'deadtime'}) {
          $info .= ("deadtime: ".
                  main::round($record->{'deadtime'}/$main::SECONDS_PER_MINUTE).
                    " (minutes)<br>");
      }
      $info .= "buildstatus: $record->{'status'}<br>";
      $info .= "buildname: $record->{'buildname'}<br>";
      $info .= "tree: $record->{'tree'}<br>";

      $record->{'info'} = $info;

      # run status dependent hook.
      BuildStatus::run_status_handler($record);

      $record = '';

  } # $update_file 

  $METADATA{$tree}{'updates_since_trim'}+=   
    scalar(@sorted_files);

  if ( ($METADATA{$tree}{'updates_since_trim'} >
        $TinderDB::MAX_UPDATES_SINCE_TRIM)
     ) {
    $METADATA{$tree}{'updates_since_trim'}=0;
    $self->trim_db_history($tree);
  }


  $self->compute_metadata($tree);

  # be sure to save the updates to the database before we delete their
  # files.

  $self->savetree_db($tree);

  $self->unlink_files(@sorted_files);
  
  return scalar(@sorted_files);
} # apply_db_updates 


# clear data structures in preparation for printing a new table

sub status_table_start {
  my ($self, $row_times, $tree, ) = @_;

  # We set $NEXT_ROW{$tree}{$buildname} = 0; since we wish to begin at
  # the first element of $row_times.

  # adjust the $NEXT_DB to skip data which came after the first cell
  # at the top of the page.  We make the first cell bigger then the
  # rest to allow for some overlap between pages.

  my ($first_cell_seconds) = 2*($row_times->[0] - $row_times->[1]);
  my ($earliest_data) = $row_times->[0] + $first_cell_seconds;
  
  my (@all_build_names) = all_build_names($tree);
  foreach $buildname (@all_build_names) {

    $NOTICE->status_table_start($row_times, $tree, $buildname);

    my ($db_index) = 0;
    my ($current_rec) = $DATABASE{$tree}{$buildname}{'recs'}[$db_index];
    while ( 
           ( $current_rec->{'starttime'} > $earliest_data ) ||
           ( $current_rec->{'timenow'}   > $earliest_data ) 
          ) {
      $db_index++;
      $current_rec = $DATABASE{$tree}{$buildname}{'recs'}[$db_index];
    }

    $NEXT_DB{$tree}{$buildname} = $db_index;
    $NEXT_ROW{$tree}{$buildname} = 0;
  } 

  return ;  
}


# create the popup window links for this cell.
sub buildcell_links {
    my  ($current_rec, $tree, $buildname) = @_;


    # the list of links with popup windows we are creating
    my ($links) = '';

    # a copy of the list of links which will appear inside the
    # popupwindow
    my ($index_links) = '';

    my ($title) = "Build Info Buildname: $buildname";

    # Build Log Link

    # We wish to encourage people to use the brief log.  If they need
    # the full log they can get there from the brief log page.

    if ($current_rec->{'brieflog'}) {
      $index_links.= "\t\t\t".
          HTMLPopUp::Link(
                          "linktxt"=>"View Summary Log", 
                          # the mail processor tells us the URL to
                          # retreive the log files.
                          "href"=>$current_rec->{'brieflog'},
                          )."<br>\n";
    }
    
    if ($current_rec->{'fulllog'}) {
      $index_links.= "\t\t\t".
        HTMLPopUp::Link(
                        "linktxt"=>"View Full Log", 
                        # the mail processor tells us the URL to
                        # retreive the log files.
                        "href"=>$current_rec->{'fulllog'},
                       )."<br>\n";
  }
    
    # Binary file Link
    
    if ($current_rec->{'binaryname'}) {
          $index_links.= "\t\t\t".HTMLPopUp::Link(
                                              "linktxt"=>"Get Binary File",
                                              "href"=>$current_rec->{'binaryname'},
                                              )."<br>\n";
    }
    
    if ( $current_rec->{'previousbuildtime'} ) {

      # If the current build is broken, show what to see what has
      # changed in VC during the last build.

      my ($maxdate) = $current_rec->{'starttime'};
      my ($mindate) = $current_rec->{'previousbuildtime'};

      $index_links .= (
                 "\t\t\t". 
                 VCDisplay::query(
                                   'linktxt'=> "Changes since last build",
                                   'tree' => $tree,
                                   'mindate' => $mindate,
                                   'maxdate' => $maxdate,
                                  ).
                       "<br>\n"
                       );
  }
    # Build Log Link

    # We wish to encourage people to use the brief log.  If they need
    # the full log they can get there from the brief log page.

    if ($current_rec->{'brieflog'}) {
      $links.= "\t\t\t".
        HTMLPopUp::Link(
                        "linktxt"=>"l", 
                        # the mail processor tells us the URL to
                        # retreive the log files.
                        "href"=>$current_rec->{'brieflog'},
                        "windowtxt"=>$current_rec->{'info'}.$index_links, 
                        "windowtitle" =>$title,
                       )."\n";
    }
    
    if ($current_rec->{'fulllog'}) {
      $links.= "\t\t\t".
        HTMLPopUp::Link(
                        "linktxt"=>"L", 
                        # the mail processor tells us the URL to
                        # retreive the log files.
                        "href"=>$current_rec->{'fulllog'},
                        "windowtxt"=>$current_rec->{'info'}.$index_links, 
                        "windowtitle" =>$title,
                       )."\n";
  }
    
    # Binary file Link
    my $binary_ref = (
                      FileStructure::get_filename($tree, build_din_dir) . 
                      $current_rec->{'binaryname'}
                      );

    if ($current_rec->{'binaryname'}) {
        $links.= "\t\t\t".
            HTMLPopUp::Link(
                            "linktxt"=>"B",
                            "href"=> $binary_ref,
                            "windowtxt"=>$current_rec->{'info'}.$index_links, 
                            "windowtitle" =>$title,
                            )."\n";
    }
    
    # Bloat Data Link

    if ($current_rec->{'bloatdata'}) {
      $links.= "\t\t\t".
        HTMLPopUp::Link(
                        "windowtxt"=>$current_rec->{'info'}, 
                        "windowtitle" =>$title,
                        "linktxt"=>$current_rec->{'bloat_data'}.$index_links,
                        "href"=>(
                                 "$FileStructure::URLS{'gunzip'}?".
                                 "tree=$tree&".
                                 "brief-log=$current_rec->{'brieflog'}"
                                ),
                       )."\n";
    }
    
    # What Changed Link
    if ( $current_rec->{'previousbuildtime'} ) {

      # If the current build is broken, show what to see what has
      # changed in VC during the last build.

      my ($maxdate) = $current_rec->{'starttime'};
      my ($mindate) = $current_rec->{'previousbuildtime'};

      $links .= (
                 "\t\t\t". 
                 VCDisplay::query(
                                   'linktxt'=> "C",
                                   'tree' => $tree,
                                   'mindate' => $mindate,
                                   'maxdate' => $maxdate,
                                   "windowtxt"=>$current_rec->{'info'}.$index_links, 
                                   "windowtitle" =>$title,
                                  ).
                 "\n"
                );
    }

    # Error count (not a link, but hey)

    if ( ($DISPLAY_BUILD_ERRORS) && ($current_rec->{'errors'}) ) {
        $links .= (
                   "\t\t\t<br>errs: ". 
                   $current_rec->{'errors'}."\n".
                   "");
    }

    if ($current_rec->{'print'}) {
        $links .= (
                   "\t\t\t". 
                   $current_rec->{'print'}.
                   "\n".
                   "");
    }

    return $links;
}

sub status_table_row {
  my ($self, $row_times, $row_index, $tree, ) = @_;

  ( scalar(@LATEST_STATUS) && scalar(@BUILD_NAMES) ) ||
    return ();


  my @outrow = ();

  my (@build_names) = build_names($tree);
  foreach $buildname (@build_names) {

    my ($db_index) = $NEXT_DB{$tree}{$buildname};
    my ($current_rec) = $DATABASE{$tree}{$buildname}{'recs'}[$db_index];

    # skip this column?

    if ( $NEXT_ROW{$tree}{$buildname} !=  $row_index ) {
        
        if ($DEBUG) {
            push @outrow, ("\t<!-- skipping: Build: ".
                           "tree: $tree, ".
                           "build: $buildname, ".
                           "additional_skips: ".
                           ($NEXT_ROW{$tree}{$buildname} -  $row_index).", ".
                           "previous_end: ".localtime($current_rec->{'timenow'}).", ".
                           " -->\n");
        }
      next;
    }
    
    # create a dummy cell for missing build?

    if  ( $current_rec->{'timenow'} < $row_times->[$row_index] ) {

      my ($rowspan) = 0;
      while ( 
             ( ($row_index + $rowspan) < $#{$row_times}) &&
             ( $current_rec->{'timenow'}  <  
               $row_times->[$row_index + $rowspan] ) 
            ) {
        $rowspan++ ;
      }

      $NEXT_ROW{$tree}{$buildname} = $row_index + $rowspan;

      my ($cell_color) = BuildStatus::status2html_colors('not_running');
      my ($cell_options) = ("rowspan=$rowspan ".
                            "bgcolor=$cell_color ");
      my ($lc_time) = localtime($current_rec->{'timenow'});

      my $notice = $NOTICE->Notice_Link(
                                        $row_times->[$row_index + $rowspan],
                                        $tree,
                                        $buildname,
                                        );
      my $cell_contents;
      if ($notice) {
          $cell_contents = (
                            "\t\t\t". 
                            $notice.
                            "\n".
                            "");
      } else {
          $cell_contents = $HTMLPopUp::EMPTY_TABLE_CELL;
      }

      if ($DEBUG) {
          push @outrow, ("\t<!-- not_running: Build:".
                         "tree: $tree, ".
                         "build: $buildname, ".
                         "previous_end: $lc_time, ".
                         "-->\n");
      }
      
      push @outrow, ("\t\t<td align=center $cell_options>".
                     "$cell_contents</td>\n");

      next;
    }


    # This record will fill at least one cell.  Should it fill more?
    # Count the number of row bottoms that this build crosses.

    my ($rowspan) = 1;
    while (  
           ( ($row_index + $rowspan) <= $#{$row_times}) &&
           ( $row_times->[ $row_index + $rowspan ] >
             $current_rec->{'starttime'}) 
          ) {
      $rowspan++ ;
    }

    my ($status) = $current_rec->{'status'};
    my ($cell_color) = BuildStatus::status2html_colors($status);

    my ($cell_options) = ("rowspan=$rowspan ".
                          "bgcolor=$cell_color ");

    my ($char) = BuildStatus::status2hdml_chars($status);
    my $text_browser_color_string = 
      HTMLPopUp::text_browser_color_string($cell_color, $char);

    my $links;
    if ($text_browser_color_string) {
        $links.=  "\t\t\t".$text_browser_color_string."\n";
    }

    $links .= buildcell_links($current_rec, $tree, $buildname);

    # put link to view Notices
    my $notice = $NOTICE->Notice_Link(
                                      $current_rec->{'starttime'},
                                      $tree,
                                      $buildname,
                                      );
    if ($notice) {
        $links .= "\t\t\t$notice\n";
    }
    
    if ($text_browser_color_string) {
        $links.=  "\t\t\t".$text_browser_color_string."\n";
    }

    push @outrow, ( "\t<!-- cell for build: $buildname, tree: $tree -->\n".
                    "\t\t<td align=center $cell_options><tt>\n".
                   $links.
                    "\t\t".
                   "</tt></td>\n");
    

    $NEXT_ROW{$tree}{$buildname} = $row_index + $rowspan;
    $NEXT_DB{$tree}{$buildname} += 1;
  }

  return @outrow;
}



1;

