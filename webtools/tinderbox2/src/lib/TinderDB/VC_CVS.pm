# -*- Mode: perl; indent-tabs-mode: nil -*-
#

# TinderDB::VC_CVS - methods to query the (raw) CVS Version Control
# system and find the users who have checked changes into the tree
# recently and renders this information into HTML for the status page.
# This module depends on TreeData.  The TinderHeader::TreeState is
# queried so that users will be displayed in the color which
# represents the state of the tree at the time they checked their code
# in.  If you are using bonsai you should not use this module, use
# TinderDB::VC_Bonsai instead.


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



package TinderDB::VC_CVS;

# the raw CVS implemenation of the Version Control DB for Tinderbox.
# This column of the status table will report who has changed files in
# the CVS repository and what files they have changed.

# Remember CVS does all its work in GMT (UCT), Tinderbox does all its work in
# local time.  Thus this file needs to perform the conversions.

# The 'cvs history' command does not give correct info about
# modifications to the repository (types= 'ARM') when passed a module
# name, it was only designed to work on individual files.  If you use
# the -m option you will not find any info about checkins. We leave
# off the module name and take the info for the whole repository
# instead. If this becomes a bother we can reject information about
# updates which do not match a pattern.  In this respect the pattern
# becomes a proxy for the module.  The pattern is stored in
# $TreeData::VC_TREE{$tree}{'dir_pattern'}.  

# There is nothing I can think of to get information about which
# branch the changes were checked in on.  The history command also has
# no notion of branches, but that is a common problem with the way CVS
# does branching anyway.

# What we really want it the checkin comments but history does not
# give us that information and it would be too expensive to run cvs
# again for each file.  The file name is good enough but other VC
# implementations should use the checkin comments if availible.

#   We store the hash of all names who modified the tree at a
#   particular time as follows:

#   $DATABASE{$tree}{$time}{'author'}{$author}{$file} = 1;

# additionally information about changes in the tree state are stored
# in the variable:

#   $DATABASE{$tree}{$time}{'treestate'} = $state;

# we also store information in the metadata structure

#   $METADATA{$tree}{'updates_since_trim'} += 1;
#
# where state is either 'Open' or 'Closed' or some other user defined string.
#
# Cell colors are controled by the functions:
#	TreeData::get_all_tree_states()
#	TreeData::TreeState2color($state)


# Most of the real time for the tinder.cgi process is waiting for CVS
# to tell us about the history.  

# dprofpp says that:
#	 %64.8 of real time which is 66.25 seconds out of 102.15 Seconds
#	 was spent in 3 calls to TinderDB::VC::apply_db_updates()

# It may be a good idea to separate this out into a different process.
# We may want to have a process which gets the history data and passes
# it to tinder.cgi via the same type of updates as the build accepts.
# Then both the tinder.cgi and this new cvs process could be run out
# of cron periodically and the webpages would not be delayed if CVS
# takes a long time to respond this might improve the response time of
# the web updates.  Also the OS process scheduler could do a better
# job of scheduling if there were two different processes one which
# was IO bound and one which was CPU bound.  A separate CVS process
# may improve the quality of the output the cvs info page says that
# there is an option `-w'

#     Show only the records for modifications done from the same
#     working directory where `history' is executing.

# I sugest making the separation a configurable option so some users
# can have VC_CVS run in push mode and others in pull mode.

# It would even be better if bonsai could be convinced set up a
# subscription service for trees.  Then Bonsai could mail tinderbox
# the updates which apply to the branches of interest and tinderbox
# would not have to pool and it would get the branch information which
# it needs.


# Load standard perl libraries
use File::Basename;
use Time::Local;

# Load Tinderbox libraries

use lib '#tinder_libdir#';

use TinderDB::BasicTxtDB;
use Utils;
use HTMLPopUp;
use TreeData;
use VCDisplay;


$VERSION = ( qw $Revision: 1.17 $ )[1];

@ISA = qw(TinderDB::BasicTxtDB);

# Add an empty object, of this DB subclass, to end of the set of all
# HTML columns.  This registers the subclass with TinderDB and defines
# the order of the HTML columns.

push @TinderDB::HTML_COLUMNS, TinderDB::VC_CVS->new();



$CURRENT_YEAR = 1900 + (gmtime(time()))[5];

# name of the version control system
$VC_NAME = $TinderDB::VC_NAME || "CVS";




sub parse_cvs_time { 
  # convert cvs times into unix times.
  my ($date, $time) = @_;

  # Can you believe that some CVS versions gives no information about
  # the YEAR?

  # We do not even  get a two digit year.
  # Luckally for us we are only interested in history in our recent
  # past, within the last year.

  my ($year, $mon, $mday, $hours, $min,) = ();

  # some versions of CVS give data in the format:
  #        2001-01-04
  # others use:
  #        01/04

  if ( $date =~ m/-/ ) {
    ($year, $mon, $mday,) = split(/-/, $date);
  } else {
    $year = $CURRENT_YEAR;
    ($mon, $mday,) = split(/\//, $date);
  }

  ($hours, $min,) = split(/:/, $time);
  
  # The perl conventions for these variables is 0 origin while the
  # "display" convention for these variables is 1 origin.  
  $mon--;
  
  # This calculation may use the wrong year.
  my $sec = 0;

  my ($time) = timegm($sec,$min,$hours,$mday,$mon,$year);    

  # This fix is needed every year on Jan 1. On that day $time is
  # nearly a year in the future so is much bigger then $main::TIME.

  if ( ($time - $main::TIME) > $main::SECONDS_PER_MONTH) {
    $time = timegm($sec,$min,$hours,$mday,$mon,$year - 1);    
  }

  # check that the result is reasonable.

  if ( (($main::TIME - $main::SECONDS_PER_YEAR) > $time) || 
       (($main::TIME + $main::SECONDS_PER_MONTH) < $time) ) {
    die("CVS reported time: $time ".scalar(gmtime($time)).
        " which is more then a year away from now or in the future.\n");
  }

  return $time;
}


sub time2cvsformat {
  # convert time() format to cvs input format
  my ($time) = @_;

  my ($sec,$min,$hour,$mday,$mon,
      $year,$wday,$yday,$isdst) =
        gmtime($time);
  $mon++;
  $year += 1900;
  my ($cvs_date_str) = sprintf("%02u/%02u/%04u %02u:%02u:%02u GMT",
                               $mon, $mday, $year, $hour, $min, $sec);

  return $cvs_date_str;
}


# remove all records from the database which are older then last_time.

sub trim_db_history {
  my ($self, $tree,) = (@_);

  my ($last_time) =  $main::TIME - $TinderDB::TRIM_SECONDS;

  # sort numerically ascending
  my (@times) = sort {$a <=> $b} keys %{ $DATABASE{$tree} };
  foreach $time (@times) {
    ($time >= $last_time) && last;

    delete $DATABASE{$tree}{$time};
  }

  return ;
}


# Return the most recent times that we recieved treestate and checkin
# data.

sub find_last_data {
  my ($tree) = @_;

  # sort numerically descending
  my (@times) = sort {$b <=> $a} keys %{ $DATABASE{$tree} };

  my ($last_tree_data, $second2last_tree_data, $last_cvs_data);

  foreach $time (@times) {

    # We must check $second2last_tree_data before $last_tree_data or
    # we may end up with both pointing to the same entry.

    (defined($last_tree_data)) &&
      (!defined($second2last_tree_data)) &&
	(defined($DATABASE{$tree}{$time}{'treestate'})) &&
	  ($second2last_tree_data = $time);

    (!defined($last_tree_data)) &&
      (defined($DATABASE{$tree}{$time}{'treestate'})) &&
	($last_tree_data = $time);
    
    (!defined($last_cvs_data)) &&
      (defined($DATABASE{$tree}{$time}{'author'})) &&
	($last_cvs_data = $time);
    
    
    # do not iterate through the whole histroy.  Stop after we have
    # the data we need.
    
    (defined($last_cvs_data)) &&
      (defined($second2last_cvs_data)) &&
	(defined($last_tree_data)) &&
	  last;

  } # foreach $time (@times) 


  return ($last_tree_data, $second2last_tree_data, $last_cvs_data);
}



# get the recent data from CVS and the treestate file.

sub apply_db_updates {
  my ($self, $tree,) = @_;
  
  my ($new_tree_state) = TinderHeader::gettree_header('TreeState', $tree);
  my ($last_tree_data, $second2last_tree_data, $last_cvs_data) = 
    find_last_data($tree);

  # Store the latest treestate in the database along with the checkin
  # data.

  # Take this opportunity to perform an optimization of the database.

  # We purge duplicate 'treestate' entries (entries at consecutive
  # times which have the same state) to keep the DB size down.
  # Data::Dumper takes a long time and reducing the data that it needs
  # to process really helps speed things up.

  # If we delete too many duplicates then we loose information when
  # the database is trimmed. We need to keep some duplicates arround
  # for debuging and for "redundancy". Only delete duplicates during
  # the last hour.  Notice we are still removing 90% of the
  # duplicates.

  # If we have three data points in a row, and all of them have the
  # same state and the oldest is less then an hour old, then we can
  # delete the middle state.  While writing this code I kept trying to
  # make do with only one older state being remembered.  The problem
  # is that if you keep deleting the oldest member you always have
  # exactly one entry which is only five minutes old.

  if ( 
      defined($last_tree_data) && 
      defined($second2last_tree_data) &&

      ($new_tree_state eq $DATABASE{$tree}{$last_tree_data}{'treestate'}) &&
      ($new_tree_state eq $DATABASE{$tree}{$second2last_tree_data}{'treestate'}) &&

      ( ($main::TIME - $second2last_tree_data) < $main::SECONDS_PER_HOUR )
     ) {
    delete $DATABASE{$tree}{$last_tree_data}{'treestate'};
    
    (scalar(%{ $DATABASE{$tree}{$last_tree_data} }) == 0) &&
      delete $DATABASE{$tree}{$last_tree_data};
  }

  $DATABASE{$tree}{$main::TIME}{'treestate'} = $new_tree_state;
   
  ($last_cvs_data) ||
   ($last_cvs_data = $main::TIME - $TinderDB::TRIM_SECONDS );
  
  my ($cvs_date_str) = time2cvsformat($last_cvs_data);

  my ($num_updates) = 0;

  # see comments about CVS history at the top of this file for details
  # about this commmand and limitations in CVS.

  # for testing here is a good command at my site
  #  cvs -d /devel/java_repository history -c -a -D '400000 seconds ago' 

  my (@cmd) = ('cvs', 
               '-d', $TreeData::VC_TREE{$tree}{'root'}, 
               'history',
               '-c', '-a', '-D', $cvs_date_str,);


  # We have a good chance that all the trees we are interested in will
  # be in the same CVS repository.  Since we are not using the module
  # name or branch in the CVS call all the VC data will be identical.
  # Calls to CVS can take a very long time, use a cached result if
  # possible.


  my (@cvs_output) = main::cache_cmd(@cmd);

  if ($cvs_output[0] !~  m/^No records selected.\n/) {

    foreach $line (@cvs_output) {

      # we are now parsing lines which look like this:
      # (Duplicate whitespace trimmed and lines wrapped for clarity.)

      # M 01/03 20:25 +0000 cporto  1.1.2.2 serviceagreement.txt              
      #     html/docs  == <remote>
      # A 01/05 20:42 +0000 tha 1.1  Makefile 
      #    storedprocedure/gwbulk == <remote>

      # some versions of CVS give data in the format:
      #        2001-01-04
      # others use:
      #        01/04


      my ($rectype, $date, $time, $tzone, $author, 
          $revision, $file, $repository_dir, $eqeq, $dest_dir)
        = split(/\s+/, $line);
      
      # Ignore directories which are not in our module.  Since we are not
      # CVS we can only guess what these might be based on patterns.
      
      # This is to correct a bug (lack of feature) in CVS history command.

      ($TreeData::VC_TREE{$tree}{'dir_pattern'}) && 
        (!($repository_dir =~ 
           m!$TreeData::VC_TREE{$tree}{'dir_pattern'}!)) &&
             next;

      # We can not ignore changes which are not on our branch without
      # using bonsai CVS does not really support any analysis of
      # branches.  This is what bonsai was designed for.

      
      # We are only interested in records which indicate that the
      # database has been modified.  We leave out tagging as this is a
      # build function and our build ids should not appear on the
      # developer pages.

      ( $rectype =~ m/^([AMR])$/ ) || next;
      
      $time = parse_cvs_time($date, $time);

      {
        
        # double check that we understand the format, and untaint our
        # variables.
        
        if ( $eqeq ne '==' ) {
          die("Can not parse CVS history output, ".
              "'==' not in right place,: '$line'\n")
        }
        $eqeq = '==';
        if ( $tzone ne '+0000' ) {
          die("Can not parse CVS history output, ".
              "expected TZ: '+0000': '$line'\n")
        }
        $tzone = '+0000';
        if ( $rectype !~ m/^([MACFROGWUT])$/ ) {
          die("Can not parse CVS history output ".
              "rectype: $rectype unknown: '$line'\n")
        }
        $rectype = $1;
        
        if ( (($main::TIME - $main::SECONDS_PER_YEAR) > $time) || 
             (($main::TIME + $main::SECONDS_PER_MONTH) < $time) ) {
          die("CVS reported time: $time ".gmtime($time).
              " which is more then a year away from now.\n")
        }

        $author = main::extract_user($author);
      }
      
      $DATABASE{$tree}{$time}{'author'}{$author}{"$repository_dir/$file"}=1;
      $num_updates ++;

    } # foreach $line
  } # any updates

  $METADATA{$tree}{'updates_since_trim'} += $num_updates;

  if ( ($METADATA{$tree}{'updates_since_trim'} >
        $TinderDB::MAX_UPDATES_SINCE_TRIM)
     ) {
    $METADATA{$tree}{'updates_since_trim'}=0;
    trim_db_history(@_);
  }

  $self->savetree_db($tree);

  return $num_updates;
}


sub status_table_legend {
  my ($out)='';

# print all the possible tree states in a cell with the color
  $out .= "\t<td align=right valign=top>\n";
  $out .= "\t<table $TinderDB::LEGEND_BORDER>\n";
  
  $out .= ("\t\t<thead><tr><td align=center>".
           "$VC_NAME Cell Colors".
           "</td></tr></thead>\n");

  foreach $state (TreeData::get_all_tree_states()) {
    my ($color) = TreeData::TreeState2color($state);
    $out .= ("\t\t<tr bgcolor=\"$color\">".
             "<td>Tree State: $state</td></tr>\n");
  }

  $out .= "\t</table>\n";
  $out .= "\t</td>\n";


  return ($out);  
}


sub status_table_header {
  return ("\t<th>$VC_NAME checkins</th>\n");
}



# clear data structures in preparation for printing a new table

sub status_table_start {
  my ($self, $row_times, $tree, ) = @_;
  
  # create an ordered list of all times which any data is stored
  
  # sort numerically descending
  @DB_TIMES = sort {$b <=> $a} keys %{ $DATABASE{$tree} };

  # adjust the $NEXT_DB to skip data which came after the first cell
  # at the top of the page.  We make the first cell bigger then the
  # rest to allow for some overlap between pages.

  my ($first_cell_seconds) = 2*($row_times->[0] - $row_times->[1]);
  my ($earliest_data) = $row_times->[0] + $first_cell_seconds;

  $NEXT_DB = 0;
  while ( ($DB_TIMES[$NEXT_DB] > $earliest_data) &&    
          ($NEXT_DB < $#DB_TIMES) ) {
    $NEXT_DB++
  }

  $LAST_TREESTATE = '';

  return ;  
}





sub status_table_row {
  my ($self, $row_times, $row_index, $tree, ) = @_;

  my (@outrow) = ();

  # we assume that tree states only change rarely so there are very
  # few cells which have more then one state associated with them.
  # It does not matter what we do with those cells.
  
  # find all the authors who changed code at any point in this cell
  # find the tree state for this cell.

  my (%authors) = ();
  
  while (1) {
   my ($time) = $DB_TIMES[$NEXT_DB];

    # find the DB entries which are needed for this cell
    ($time < $row_times->[$row_index]) && last;

    $NEXT_DB++;

    if (defined($DATABASE{$tree}{$time}{'treestate'})) {
      $LAST_TREESTATE = $DATABASE{$tree}{$time}{'treestate'};
    }

    # Now invert the data structure.
   
    # Inside each cell, we want all the posts by the same author to
    # appear together.  The previous data structure allowed us to find
    # out what data was in each cell.
   if (defined($DATABASE{$tree}{$time}{'author'})) {
     foreach $author (keys %{ $DATABASE{$tree}{$time}{'author'} }) {
       foreach $file (keys %{ $DATABASE{$tree}{$time}{'author'}{$author} }) {
         $authors{$author}{$time}{$file} = 1;
       }
     }
   }

  } # while (1)

  # If there is no treestate, then the tree state has not changed
  # since an early time.  The earliest time was assigned a state in
  # apply_db_updates().  It is possible that there are no treestates at
  # all this should not prevent the VC column from being rendered.

  my ($color) = TreeData::TreeState2color($LAST_TREESTATE);

  ($LAST_TREESTATE) && ($color) &&
    ($color = "bgcolor=$color");
  
  my ($query_links) = '';
  if ( scalar(%authors) ) {
    
    # find the times which bound the cell so that we can set up a
    # VC query.

    my ($mindate) = $row_times->[$row_index];

    my ($maxdate);
    if ($row_index > 0){
      $maxdate = $row_times->[$row_index - 1];
    } else {
      $maxdate = $main::TIME;
    }
    my ($format_maxdate) = HTMLPopUp::timeHTML($maxdate);
    my ($format_mindate) = HTMLPopUp::timeHTML($mindate);
    my ($time_interval_str) = "$format_maxdate to $format_mindate";

    # create a string of all VC data for displaying with the checkin table

    my ($vc_info);
    foreach $key ('module',) {
      my ($value) = $TreeData::VC_TREE{$tree}{$key};
      $vc_info .= "$key: $value <br>\n";
    }

    # I wish we could give only the information for a particular
    # branch, since we can not document that this is for all branches

    $vc_info .= "branch: all <br>\n";

    # define a table, to show what was checked in for each author

    foreach $author (sort keys %authors) {
      my ($table) = '';
      my ($num_rows) = 0;
      my ($max_length) = 0;
      
      $table .= (
                 "Checkins by <b>$author</b> <br> for $vc_info \n".
                 "<table border cellspacing=2>\n".
                 "");
      
      # add table headers
      $table .= (
                 "\t<tr>".
                 "<th>Time</th>".
                 "<th>File</th>".
                 "</tr>\n".
                 "");
      
      # sort numerically descending
      foreach $time ( sort {$b <=> $a} keys %{ $authors{$author} }) {
        foreach $file (keys %{ $authors{$author}{$time}}) {
          $num_rows++;
          $max_length = main::max($max_length , length($file));
          $table .= (
                     "\t<tr>".
                     "<td>".HTMLPopUp::timeHTML($time)."</td>".
                     "<td>".$file."</td>".
                     "</tr>\n".
                     "");
        }
      }
      $table .= "</table>";

      # we display the list of names in 'teletype font' so that the
      # names do not bunch together. It seems to make a difference if
      # there is a <cr> between each link or not, but it does make a
      # difference if we close the <tt> for each author or only for
      # the group of links.

      my (%popup_args) = (
                          "linktxt" => "\t\t<tt>$author</tt>",
                          
                          "windowtxt" => $table,
                          "windowtitle" => ("$VC_NAME Info ".
                                            "Author: $author ".
                                            "$time_interval_str "),

                          "windowheight" => ($num_rows * 50) + 100,
                          "windowwidth" => ($max_length * 10) + 100,
                         );

      # If you have a VCDisplay implementation you should make the
      # link point to its query method otherwise you want a 'mailto:'
      # link

      my ($query_link) = "";
      if ( 
          ($TinderConfig::VCDisplayImpl) && 
          ($TinderConfig::VCDisplayImpl =~ 'None') 
         ) {

        $query_link .= 
          HTMLPopUp::Link(
                          "href" => "mailto: $author",
                          
                          %popup_args,
                         );
      } else {
        
        $query_link .= 
          VCDisplay::query(
                           'tree' => $tree,
                           'mindate' => $mindate,
                           'maxdate' => $maxdate,
                           'who' => $author,
                           
                           %popup_args,
                             );
      }

      # put each link on its own line and add good comments so we
      # can debug the HTML.

      $query_link = "\t\t".$query_link."\n";
      my ($date_str) = localtime($mindate)."-".localtime($maxdate);

      $query_links .= (
                       "\t\t<!-- VC: ".("Author: $author, ".
                                        "Time: '$date_str', ".
                                        "Tree: $tree, ".
                                       "").
                       "  -->\n".
                       "");

      $query_links .= $query_link;
    }
    
    @outrow = (
               "\t<td align=center $color>\n".
               $query_links.
               "\t</td>\n".
               "");
    
  } else {
    
    @outrow = ("\t<!-- skipping: VC: tree: $tree -->".
               "<td align=center $color>$HTMLPopUp::EMPTY_TABLE_CELL</td>\n");
  }
  
  
  return @outrow; 
}



1;

