# -*- Mode: perl; indent-tabs-mode: nil -*-
#

# Build.pm - the module which processes the updates posted by
# processmail (the data originally came from the clients building the
# trees) and creates the colored boxes which show what the state of
# the build was and display a link to the build log.


# $Revision: 1.8 $ 
# $Date: 2000/11/09 19:55:52 $ 
# $Author: kestes%staff.mail.com $ 
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

# complete rewrite by Ken Estes, Mail.com (kestes@staff.mail.com).
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

use BuildStatus;
use TinderDB::BasicTxtDB;
use VCDisplay;
use Utils;

$VERSION = '#tinder_version#';

@ISA = qw(TinderDB::BasicTxtDB);


# Add an empty object, of this DB subclass, to end of the set of all
# HTML columns.  This registers the subclass with TinderDB and defines
# the order of the HTML columns.

push @TinderDB::HTML_COLUMNS, TinderDB::Build->new();



# find the name of each build 


sub build_names {
  my ($tree) = (@_);
  
  my @outrow = ();
  
  foreach $buildname (sort keys %{ $DATABASE{$tree} } ){
    
    # skip this column?
    
    (!($main::NOIGNORE)) &&
      ($IGNORE_BUILDS{$tree}{$buildname}) && 
        next;
    
    push @outrow, $buildname;
  }

  return @outrow;
}


# The admin tree program needs to know all the build names so it can
# set ignore_builds.

sub all_build_names {
  my ($self, $tree) = (@_);
  
  my (@outrow) = sort keys %{ $DATABASE{$tree} };

  return @outrow;
}




# return find the most recent status for each build of this tree

sub latest_status {
  my ($tree) = (@_);
  
  my (@outrow) = ();
  
  foreach $buildname (sort keys %{ $DATABASE{$tree} } ){
    
    # skip this column?

    (!($main::NOIGNORE))  &&
      ($IGNORE_BUILDS{$tree}{$buildname}) && 
        next;
    
    my ($last_status);
    foreach $db_index (0 .. $#{ $DATABASE{$tree}{$buildname}{'recs'} }) {
      
      my ($rec) = $DATABASE{$tree}{$buildname}{'recs'}[$db_index];
      my ($buildstatus) = $rec->{'status'};
      
      if ( ($buildstatus eq 'not_running') ||
           ($buildstatus eq 'building') ) {
        next;
      }
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



#  Prepare information for popupwindows on row headers and also the
#  link to bonsai giving the aproximate times that the build was
#  broken.

sub gettree_header {
  my ($self, $tree) = (@_);
  
  # this is not working the way I want it to.  I will debug it later.

  return '';

  my ($out) = '';

  (TreeData::tree_exists($tree)) ||
    die("Tree: $tree, not defined.");
  
  # find our best guess as to when the tree broke.

  my (@earliest_failure) = ();
  my (@recent_success) = ();

  foreach $buildname (sort keys %{ $DATABASE{$tree} } ){
    
    # skip this column?

    !($main::NOIGNORE)  &&
      $IGNORE_BUILDS{$tree}{$buildname} && 
        next;
    
    my $earliest_failure = undef;

    foreach $db_index (0 .. $#{ $DATABASE{$tree}{$buildname}{'recs'} }) {
      
      my ($rec) = $DATABASE{$tree}{$buildname}{'recs'}[$db_index];
      my ($buildstatus) = $rec->{'status'};

      if ( ($buildstatus eq 'not_running') ||
           ($buildstatus eq 'building') ) {
        next ;
      }

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
  
  foreach $buildname (sort keys %{ $DATABASE{$tree} } ){
    
    # skip this column?
    
    !($main::NOIGNORE)  &&
      $IGNORE_BUILDS{$tree}{$buildname} && 
        next;
    
    my $recent_success = undef;
    foreach $db_index (0 .. $#{ $DATABASE{$tree}{$buildname}{'recs'} } ) {
      
      my ($rec) = $DATABASE{$tree}{$buildname}{'recs'}[$db_index];
      my ($buildstatus) = $rec->{'status'};
      my ($starttime) = $rec->{'starttime'};
      
      if ( defined($earliest_failure) && 
           ($earliest_failure < $starttime) ) {
        next;
      }
      
      if ( ($buildstatus eq 'not_running') ||
           ($buildstatus eq 'building') ) {
        next ;
      }
      
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
                                   'mindate'=> $earliest_failure,
                                   'maxdate'=> $recent_success,
                                   'linktxt'=> $txt,
                                  );
    
    $out .= $link;
  }

  my (@ignoring_builds) = sort keys %{ $IGNORE_BUILDS{$tree} };
  
  if (@ignoring_builds) {
    $out .= "Currently ignoring builds: @ignoring_builds \n";
    $num_lines++;
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

  foreach $buildname (split(/\s+/, $ignore_builds)) {
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
  my (@run_times) = ();
  my (@dead_times) = ();

  foreach $buildname (sort keys %{ $DATABASE{$tree} } ){

    my ($last_index) = undef;
    my $recs = $DATABASE{$tree}{$buildname}{'recs'};
    foreach $db_index (0 .. $#{ $recs }) {
      
      my ($rec) = $recs->[$db_index];

      if ( ($rec->{'status'} eq 'success') && 
           ($rec->{'runtime'}) ) {
        push @run_times, $rec->{'runtime'};
      }
      
      if ( $rec->{'deadtime'} ) {
        push @dead_times, $rec->{'deadtime'};
      }
      
      if ($rec->{'starttime'} < $last_time) {
        $last_index = $db_index;
        last;
      }
      
    }

    # medians are a more robust statistical estimator then the mean.
    # They will give us better answers then a typical "average"

    my $run_avg = main::median(@run_times);
    ($run_avg) &&
      ( $DATABASE{$tree}{$buildname}{'average_buildtime'} = $run_avg);
    
    my $dead_avg = main::median(@dead_times);
    ($dead_avg) &&
      ( $DATABASE{$tree}{$buildname}{'average_deadtime'} = $dead_avg);
    
    # the trim DB step

    if (defined($last_index)) {
      my @new_table = @{ $recs }[0 .. $last_index];
      $DATABASE{$tree}{$buildname}{'recs'} = [ @new_table ];
    }

  }
  return ;
}



sub status_table_legend {
  my ($out)='';

# print all the possible links which can be included in a build

$out .=<<EOF;
        <td align=right valign=top>
	<table $TinderDB::LEGEND_BORDER>
	<thead><tr>
		<td align=right>Build</td>
		<td align=left>Cell Links</td>
	</tr></thead>
              <tr><td align=center><TT>L</TT></td>
                  <td>= Show Build Logs</td></tr>
              <tr><td align=center><TT>C</TT></td>
                  <td>= Show Previous Build Checkins</td></tr>
              <tr><td align=center><TT>B</TT></td>
                  <td>= Get Binaries</td></tr>
              <tr><td align=center><TT>Lk:XXX</TT></td>
                  <td>=  (bytes leaked)</td></tr>
              <tr><td align=center><TT>Bl:XXX</TT></td>
                  <td>=  (bytes allocated, bloat)</td></tr>
	</table>
        </td>
EOF
  ;

  my @build_states = BuildStatus::get_all_sorted_status();

  my $state_rows;

  foreach $state (@build_states) {
    my ($color) = BuildStatus::status2html_colors($state);
    my ($description) = BuildStatus::status2descriptions($state);
    $state_rows .= (
                    "\t\t<tr bgcolor=\"$color\">\n".
                    "\t\t<td>$description</td>\n".
                    "\t\t</tr>\n"
                   );
    
  }

$out .=<<EOF;
        <td align=right valign=top>
	<table $TinderDB::LEGEND_BORDER>
		<thead>
		<tr><td align=center>Build Cell Colors</td></tr>
		</thead>
$state_rows
	</table>
        </td>
EOF
  ;


  return ($out);
}


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
    
    my $txt ='';    
    my $num_lines;

    $txt .= "time now: &nbsp;".&HTMLPopUp::timeHTML($main::TIME)."<br>";
    $num_lines++;

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

    $txt .= "<p>";
    $num_lines++;

    if ($avg_buildtime) {
      my $min = sprintf ("%.0f",         # round
                         $avg_buildtime/60);
      $txt .= "avg_buildtime (minutes): &nbsp;$min<br>";
      $num_lines++;
    }
    
    if ($avg_deadtime) {
      my $min = sprintf ("%.0f",         # round
                         $avg_deadtime/60);
      $txt .= "avg_deadtime (minutes): &nbsp;$min<br>";
      $num_lines++;
    }
    
    if ($current_endtime) {
      my ($min) =  sprintf ("%.0f",         # round
                            ($main::TIME  - $current_endtime)/60);
      $txt .= "previous end_time: &nbsp;";
      $txt .= &HTMLPopUp::timeHTML($current_endtime)."<br>";
      $txt .= "current dead_time (minutes): &nbsp;$min<br>";
      $num_lines += 2;
    } elsif ($current_starttime) {
      my ($min) =  sprintf ("%.0f",         # round
                            ($main::TIME  - $current_starttime)/60);
      $txt .= "current start_time: &nbsp;";
      $txt .= &HTMLPopUp::timeHTML($current_starttime)."<br>";
      $txt .= "current build_time (minutes): &nbsp;$min<br>";
      $num_lines += 2;
    }

    my $estimated_remaining = undef;

    if (0){
    } elsif ( ($avg_deadtime) && ($avg_buildtime) && ($current_endtime) ) {
      $estimated_remaining = ($avg_deadtime + $avg_buildtime) + 
        ($current_endtime - $main::TIME);
    } elsif ( ($avg_buildtime) && ($current_starttime) ) {
      $estimated_remaining = ($avg_buildtime) + 
        ($current_starttime - $main::TIME);
    }

    if ($estimated_remaining) {
      my $min =  sprintf ("%.0f",         # round
                          ($estimated_remaining/60) );
      $txt .= "time_remaining (estimate): &nbsp;$min<br>";
      $num_lines++;
    }

    $buildname =~ s/Clobber/Clbr/g;
    $buildname =~ s/Depend/Dep/g;    

    my $title = "Build Status Buildname: $buildname";

    my $link = HTMLPopUp::Link(
                               "windowtxt"=>$txt,
                               "windowtitle" => $title,
                               "linktxt"=>$buildname,
                               "windowheight" => (25 * $num_lines),
                               "href"=>"",
                         );

    my $font_face = "<font face='Arial,Helvetica' size=-1>";

    my ($bg) = BuildStatus::status2html_colors($latest_status);
    my ($header) = ("\t<th rowspan=1 bgcolor=$bg>".
                    $font_face.
                    "$link</font></th>\n");
    
    if (
        ($latest_status ne 'success') &&
        ($form{crap})
       ) {
      # I do not wish to support this flames format going forward
      $bg = $FileStucture::IMAGES{flames};
      $header = ("\t<th rowspan=1 bgcolor=000000 background='$bg'>".
                 $font_face.
                 "<font color=white>$link</font></font></th>\n");
    }
    
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

  foreach $update_file (@sorted_files) {
    my $full_file = "$dirname/$update_file";
    my ($record) = Persistence::load_structure($full_file);

    ($record) ||
      die("Error reading Build update file '$full_file'.\n");

    my ($build) = $record->{'buildname'};
    my ($buildstatus) = $record->{'status'};
    my ($starttime) = $record->{'starttime'};
    my ($timenow) =  $record->{'timenow'};

    # sanity check the record, taint checks are done in processmail.
    {
      BuildStatus::is_status_valid($buildstatus) ||
        die("Error in updatefile: $full_file, Status not valid");
      
      ($tree eq $record->{'tree'}) ||
        die("Error in updatefile: $full_file, ".
            "Tree: $tree, equal to Tree: $record->{'tree'}.");

      (main::is_time_valid($starttime)) ||
        die("Error in updatefile: $full_file, ".
            "starttime: $starttime, is not a valid time.");

      (main::is_time_valid($timenow)) ||
        die("Error in updatefile: $full_file, ".
            "timenow: $timenow, is not a valid time.");

      ($starttime < $timenow) ||
        die("Error in updatefile: $full_file, ".
            "starttime: $starttime, is less then timenow: $timenow.");

   }  

    # ignore updates which arrive out of order
    # or which start too fast

    if ( defined($DATABASE{$tree}{$build}{'recs'}) ) {

         ($record->{'starttime'} < 
          $DATABASE{$tree}{$build}{'recs'}[0]{'starttime'}) &&
         next;

         # Keep the spacing between builds greater then our HTML grid
         # spacing.  There can be very frequent updates for any build
         # but different builds must be spaced apart.

         my ($safe_separation) = (TinderDB::TABLE_SPACING + 
                                $main::SECONDS_PER_MINUTE);
         
         my ($separation) = ($record->{'starttime'} - 
                           $DATABASE{$tree}{$build}{'recs'}[0]{'starttime'});
         
         ($separation < $safe_separation) &&
         next;
    }

    # The time which the previous build started
    my ($previous_rec) = $DATABASE{$tree}{$build}{'recs'}[0];

    # Is this report an update to the current build? If so we do not
    # want two entries for the same build. Remove old entry
    
    if ( defined($DATABASE{$tree}{$build}{'recs'}) &&
         ($record->{'starttime'} == $previous_rec->{'starttime'})  
       ) {
      shift @{ $DATABASE{$tree}{$build}{'recs'} };
    }

    # add the record to the datastructure

    if ( defined( $DATABASE{$tree}{$build}{'recs'} ) ) {
      unshift @{ $DATABASE{$tree}{$build}{'recs'} }, $record;
    } else { 
      $DATABASE{$tree}{$build}{'recs'} = [ $record ];
    }    

    # If there is a final disposition then we need to add a bunch of
    # other data which depends on what is already availible.
    
    if ($buildstatus ne 'not_running') {

      if (
          ($buildstatus ne 'success') &&
          ($previous_rec->{'starttime'})
         ) {

        # If the current build is broken, show what to see what has
        # changed in VC during the last build.

        $record->{'previousbuildtime'} = $previous_rec->{'starttime'};

      }

      $record->{'runtime'} = ( $record->{'timenow'} - 
                               $record->{'starttime'} );

      $record->{'deadtime'} = ( $record->{'starttime'} - 
                                $previous_rec->{'endtime'} );

      $record->{'endtime'} = $record->{'timenow'};

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
                sprintf("%.2f", ($record->{'runtime'}/60)).
                " (minutes)<br>");
      if ($record->{'deadtime'} > 0) {
        $info .= ("deadtime: ".
                  sprintf("%.2f", ($record->{'deadtime'}/60)).
                  " (minutes)<br>");
      }
      $info .= "buildstatus: $record->{'status'}<br>";
      $info .= "buildname: $record->{'buildname'}<br>";
      $info .= "tree: $record->{'tree'}<br>";

      $record->{'info'} = $info;

      # run status dependent hook.
      BuildStatus::run_status_handler($record);

      $record = '';

    }
    
  } # $update_file 

  $METADATA{$tree}{'updates_since_trim'}+=   
    scalar(@sorted_files);

  if ( ($METADATA{$tree}{'updates_since_trim'} >
        $TinderDB::MAX_UPDATES_SINCE_TRIM)
     ) {
    $METADATA{$tree}{'updates_since_trim'}=0;
    trim_db_history(@_);
  }


  $self->compute_metadata($tree);

  # be sure to save the updates to the database before we delete their
  # files.

  $self->savetree_db($tree);

  $self->unlink_files($dirname, @sorted_files);
  
  return scalar(@sorted_files);
}


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
  
  foreach $buildname (sort keys %{ $DATABASE{$tree} } ) {
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





sub status_table_row {
  my ($self, $row_times, $row_index, $tree, ) = @_;

  ( scalar(@LATEST_STATUS) && scalar(@BUILD_NAMES) ) ||
    return ();


  my @outrow = ();

  foreach $buildname (sort keys %{ $DATABASE{$tree} } ) {

    my ($db_index) = $NEXT_DB{$tree}{$buildname};
    my ($current_rec) = $DATABASE{$tree}{$buildname}{'recs'}[$db_index];

    # skip this column?

    if (
        ( (!($main::NOIGNORE)) && ($IGNORE_BUILDS{$tree}{$buildname}) ) || 
        ( $NEXT_ROW{$tree}{$buildname} !=  $row_index )
       ) {
      
      push @outrow, ("\t<!-- skipping: Build: ".
                     "tree: $tree, ".
                     "build: $buildname, ".
                     "additional_skips: ".
                     ($NEXT_ROW{$tree}{$buildname} -  $row_index).", ".
                     "previous_end: ".localtime($current_rec->{'timenow'}).", ".
                     " -->\n");
      next;
    }
    
    # create a dummy cell for missing build?

    if  ( $current_rec->{'timenow'} < $row_times->[$row_index] ) {

      my ($rowspan) = 1;
      while ( 
             ( ($row_index + $rowspan) <= $#{$row_times}) &&
             ( $current_rec->{'timenow'}  <  
               $row_times->[$row_index + $rowspan] ) 
            ) {
        $rowspan++ ;
      }

      my ($cell_color) = BuildStatus::status2html_colors('not_running');
      my ($cell_options) = ("rowspan=$rowspan ".
                            "bgcolor=$cell_color ");

      push @outrow, ("\t<!-- not_running: Build:".
                     "tree: $tree, ".
                     "build: $buildname, ".
                     "previous_end: ".localtime($current_rec->{'timenow'}).", ".
                     "-->\n".

                     "\t\t<td align=center $cell_options>".
                     "$HTMLPopUp::EMPTY_TABLE_CELL</td>\n");
      $NEXT_ROW{$tree}{$buildname} =  $row_index + $rowspan;
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
    
    my ($cell_color) = BuildStatus::status2html_colors($current_rec->{'status'});

    my ($cell_options) = ("rowspan=$rowspan ".
                          "bgcolor=$cell_color ");

    my ($links) = '';
    my ($title) = "Build Info Buildname: $buildname";

    # Build Log Link

    # We wish to encourage people to use the brief log.  If they need
    # the full log they can get there from the brief log page.

    if ($current_rec->{'brieflog'}) {
      $links.= "\t\t".
        HTMLPopUp::Link(
                        "linktxt"=>"L", 
                        # the mail processor tells us the URL to
                        # retreive the log files.
                        "href"=>$current_rec->{'brieflog'},
                        "windowtxt"=>$current_rec->{'info'}, 
                        "windowtitle" =>$title,
                       )."\n";
    }
    
    # Binary file Link
    
    if ($current_rec->{'binaryname'}) {
      $links.= "\t\t".HTML::Link(
                                 "linktxt"=>"B",
                                 "href"=>$current_rec->{'binaryname'},
                                 "windowtxt"=>$current_rec->{'info'}, 
                                 "windowtitle" =>$title,
                                )."\n";
    }
    
    # Bloat Data Link

    if ($current_rec->{'bloatdata'}) {
      $links.= "\t\t".
        HTMLPopUp::Link(
                        "windowtxt"=>$current_rec->{'info'}, 
                        "windowtitle" =>$title,
                        "linktxt"=>$current_rec->{'bloat_data'},
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

      my ($mindate) = $current_rec->{'previousbuildtime'};
      my ($maxdate) = $current_rec->{'starttime'};

      $links .= (
                 "\n". 
                 VCDisplay::query(
                                   'linktxt'=> "C",
                                   'tree' => $tree,
                                   'mindate' => $mindate,
                                   'maxdate' => $maxdate,
                                   "windowtxt"=>$current_rec->{'info'}, 
                                   "windowtitle" =>$title,
                                  ).
                 "\n"
                );
    }

    push @outrow, ( "\t<!-- cell for build: $buildname, tree: $tree -->\n".
                    "\t<td align=center $cell_options><tt>\n".
                   $links."\t".
                   "</tt></td>\n");
    

    $NEXT_ROW{$tree}{$buildname} = $row_index + $rowspan;
    $NEXT_DB{$tree}{$buildname} += 1;
  }

  return @outrow;
}



1;

