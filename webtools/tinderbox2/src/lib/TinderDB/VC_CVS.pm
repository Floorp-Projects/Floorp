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

# $Revision: 1.34 $ 
# $Date: 2003/08/04 17:15:15 $ 
# $Author: kestes%walrus.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/lib/TinderDB/VC_CVS.pm,v $ 
# $Name:  $ 


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
# implementations should use the checkin comments if available.

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


$VERSION = ( qw $Revision: 1.34 $ )[1];

@ISA = qw(TinderDB::BasicTxtDB);



$CURRENT_YEAR = 1900 + (gmtime(time()))[5];

# name of the version control system
$VC_NAME = $TinderConfig::VC_NAME || "CVS";

# How we recoginise bug number in the checkin comments.
# Except of course we can not get the comments from the CVS log
# command.  so this is not used.

$VC_BUGNUM_REGEXP = $TinderConfig::VC_BUGNUM_REGEXP ||
    '(\d\d\d+)';

# We 'have a' notice so that we can put stars in our column.

$NOTICE= TinderDB::Notice->new();
$DEBUG = 1;



sub parse_cvs_time { 
  # convert cvs times into unix times.
  my ($date_str, $time_str) = @_;

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

  if ( $date_str =~ m/-/ ) {
    ($year, $mon, $mday,) = split(/-/, $date_str);
  } else {
    $year = $CURRENT_YEAR;
    ($mon, $mday,) = split(/\//, $date_str);
  }

  ($hours, $min,) = split(/:/, $time_str);
  
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


# Print out the Database in a visually useful form.

sub get_all_cvs_data {
  my ($self, $tree) = (@_);

  my $treestate = TinderHeader::gettree_header('TreeState', $tree);

  # sort numerically descending
  my (@times) = sort {$b <=> $a} keys %{ $DATABASE{$tree} };

  my $out;
  $out .= "<HTML>\n";
  $out .= "<HEAD>\n";
  $out .= "\t<TITLE>CVS Checkin Data as gathered by Tinderbox</TITLE>\n";
  $out .= "</HEAD>\n";
  $out .= "<BODY>\n";
  $out .= "<H3>CVS Checkin Data as gathered by Tinderbox</H3>\n";
  $out .= "\n\n";
  $out .= "<TABLE BORDER=1 BGCOLOR='#FFFFFF' CELLSPACING=1 CELLPADDING=1>\n";
  $out .= "\t<TR>\n";
  $out .= "\t\t<TH>Time</TH>\n";
  $out .= "\t\t<TH>Tree State</TH>\n";
  $out .= "\t\t<TH>Author</TH>\n";
  $out .= "\t\t<TH>File</TH>\n";
  $out .= "\t</TR>\n";

  # we want to be able to make links into this page either with the
  # times of checkins or of times which are round numbers.

  my $rounded_increment = $main::SECONDS_PER_MINUTE * 5;
  my $rounded_time = main::round_time($times[0]);

  # Why are the names so confusing in this code?
  # Netscape does not scroll to the middle of a large table if we
  # put names between the rows, however it will scroll if we name
  # the contents of a cell.
  
  foreach $time (@times) {

      # Allow us to create links which point to times which may not
      # appear in the data.  These links should correspond to the cell
      # spacing in the status table.

      my $names = '';
      while ($rounded_time > $time) { 
          my $comment = "<!-- ".localtime($rounded_time)." -->";
          $names .= (
                     "\t\t\t".
                     HTMLPopUp::Link(
                                     "name" => $rounded_time,
                                     "linktxt" => $comment,
                                     ).
                     "\n");
          $rounded_time -= $rounded_increment;
      }

      # Allow us to create links which point to any row.

      my $localtime = localtime($time);
      my $cell_time =
          HTMLPopUp::Link(
                          "name" => $time,
                          "linktxt" => $localtime,
                          );

      ($names) &&
          ($cell_time .= "\n".$names."\t\t");

      if (defined($DATABASE{$tree}{$time}{'treestate'})) {
          my $localtime = localtime($time);
          $treestate = $DATABASE{$tree}{$time}{'treestate'};

          $out .= "\t<TR>\n";
          $out .= "\t\t<TD>$cell_time</TD>\n";
          $out .= "\t\t<TD ALIGN=center >$treestate</TD>\n";
          $out .= "\t\t<TD>$HTMLPopUp::EMPTY_TABLE_CELL</TD>\n";
          $out .= "\t\t<TD>$HTMLPopUp::EMPTY_TABLE_CELL</TD>\n";
          $out .= "\t</TR>\n";
      }

      if ( defined($DATABASE{$tree}{$time}{'author'}) ) {
          my ($recs) = $DATABASE{$tree}{$time}{'author'};
          my $localtime = localtime($time);
          my (@authors) = sort (keys %{ $recs });

          foreach $author (@authors) {
              my @files =  sort (keys %{ $recs->{$author} });
              my $rowspan = scalar (@files);
              my $cell_options = "ALIGN=center ROWSPAN=$rowspan";
              $out .= "\t<TR>\n";
              $out .= "\t\t<TD $cell_options>$cell_time</TD>\n";
              $out .= "\t\t<TD $cell_options>$treestate</TD>\n";
              $out .= "\t\t<TD $cell_options>$author</TD>\n";
              my $num;
              foreach $file (@files) {
                  ($num) &&
                      ($out .= "\t<TR>\n");
                  $num ++;
                  $out .= "\t\t<TD>$file</TD>\n";
                  $out .= "\t</TR>\n";
              } # $file
          } # $author
      }

  } # $time
  $out .= "</TABLE>\n";
  $out .= "\n\n";
  $out .= "This page was generated at: ";
  $out .= localtime($main::TIME);
  $out .= "\n\n";
  $out .= "</BODY>\n";
  $out .= "</HTML>\n";
  return $out;
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

      # Careful with this split, Some users put spaces in their
      # filenames. Otherwise we would do this

      #  my ($rectype, $date, $time, $tzone, $author, 
      #      $revision, $file, $repository_dir, $eqeq, $dest_dir)
      #      = split(/\s+/, $line);

      my (@line) = split(/\s+/, $line);

      my ($rectype) = shift @line; 
      my ($date) = shift @line; 
      my ($time) = shift @line; 
      my ($tzone) = shift @line; 
      my ($author) = shift @line; 
      my ($revision) = shift @line; 

      my ($dest_dir) = pop @line; 
      my ($eqeq) = pop @line; 
      my ($repository_dir) = pop @line; 
   
      my ($file)= "@line";

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

  ($num_updates) ||
      return 0;

  $METADATA{$tree}{'updates_since_trim'} += $num_updates;

  if ( ($METADATA{$tree}{'updates_since_trim'} >
        $TinderDB::MAX_UPDATES_SINCE_TRIM)
     ) {
    $METADATA{$tree}{'updates_since_trim'}=0;
    $self->trim_db_history($tree);
  }

  $self->savetree_db($tree);

  # VCDisplay needs to know the filename that we write to, so that the
  # None.pm can reference this file.  However VC display is called by
  # Build, Time as well as this VC column.  So all the VC
  # implementations must store their data into a file with the same
  # name.

  my $all_vc_data = $self->get_all_cvs_data($tree);
  my ($outfile) = (FileStructure::get_filename($tree, 'tree_HTML').
                   "/all_vc.html");
  main::overwrite_file($outfile, $all_vc_data);

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

  foreach $state (TreeData::get_all_sorted_tree_states()) {
    my ($cell_color) = TreeData::TreeState2color($state);
    my ($char) = TreeData::TreeState2char($state);
    my ($description) = TreeData::TreeStates2descriptions($state);
    $description = "$state: $description";
    my $text_browser_color_string = 
      HTMLPopUp::text_browser_color_string($cell_color, $char);

    $description = (

                    $text_browser_color_string.
                    $description.
                    $text_browser_color_string.
                    
                    "");
    
    $out .= ("\t\t<tr bgcolor=\"$cell_color\">".
             "<td>$description</td></tr>\n");
  }

  $out .= "\t</table>\n";
  $out .= "\t</td>\n";


  return ($out);  
}



# where can people attach notices to?
# Really this is the names the columns produced by this DB

sub notice_association {
    return $VC_NAME;
}


sub status_table_header {
  return ("\t<th>$VC_NAME</th>\n");
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

  my (@authors) = ();
  my $checkin_page_reference;
  
  while (1) {
   my ($time) = $DB_TIMES[$NEXT_DB];

    # find the DB entries which are needed for this cell
    ($time < $row_times->[$row_index]) && last;

    $NEXT_DB++;

    if (defined($DATABASE{$tree}{$time}{'treestate'})) {
      $LAST_TREESTATE = $DATABASE{$tree}{$time}{'treestate'};
    }

   if (defined($DATABASE{$tree}{$time}{'author'})) {
       push @authors,  (keys %{ $DATABASE{$tree}{$time}{'author'} });
       if (!(defined($checkin_page_reference))) {
           $checkin_page_reference = $time;
       }
   }

  } # while (1)

  @authors = main::uniq(@authors);

  # If there is no treestate, then the tree state has not changed
  # since an early time.  The earliest time was assigned a state in
  # apply_db_updates().  It is possible that there are no treestates at
  # all this should not prevent the VC column from being rendered.

  if (!($LAST_TREESTATE)) {
      $LAST_TREESTATE = $TinderHeader::HEADER2DEFAULT_HTML{'TreeState'};
  }

  my ($cell_color) = TreeData::TreeState2color($LAST_TREESTATE);
  my ($char) = TreeData::TreeState2char($LAST_TREESTATE);

  my $cell_options = '';
  my $text_browser_color_string;
  my $empty_cell_contents = $HTMLPopUp::EMPTY_TABLE_CELL;

  if ( ($LAST_TREESTATE) && ($cell_color) ) {
       my ($cell_options) = "bgcolor=$cell_color ";

       $text_browser_color_string = 
         HTMLPopUp::text_browser_color_string($cell_color, $char);

       # for those who like empty cells to be truly empty, we need to
       # be sure that they see the different cell colors when they
       # change.

       if (
           ($cell_color !~ m/white/) &&
           (!($text_browser_color_string)) &&
           (!($empty_cell_contents)) &&
            1) {
               $empty_cell_contents = "&nbsp;";
           }
  }
  
  my ($query_links) = '';
  $query_links.=  "\t\t".$text_browser_color_string."\n";

  if ( scalar(@authors) ) {
    
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

    foreach $author (@authors) {
            
        # This is a Netscape.com/Mozilla.org specific CVS/Bonsai
        # issue. Most users do not have '%' in their CVS names. Do
        # not display the full mail address in the status column,
        # it takes up too much space.
        # Keep only the user name.
        
        my $display_author=$author;
        $display_author =~ s/\%.*//;
        
        my $mailto_author=$author;
        $mailto_author = TreeData::VCName2MailAddress($author);
        
        # The Link Choices inside the popup.
        
        my $link_choices = "Checkins by <b>$author</b><br>";
        $link_choices .= " for $vc_info \n<br>";
        $link_choices .= 
            VCDisplay::query(
                             'tree' => $tree,
                             'mindate' => $mindate,
                             'maxdate' => $maxdate,
                             'who' => $author,
                             
                             "linktxt" => "This check-in",
                             );
        
        $link_choices .= "<br>";
#        $link_choices .= 
#            VCDisplay::query(
#                             'tree' => $tree,
#                             'mindate' => $mindate - $main::SECONDS_PER_DAY,
#                             'maxdate' => $maxdate,
#                             'who' => $author,
#                             
#                             "linktxt" => "Check-ins within 24 hours",
#                             );
#        
#        $link_choices .= "<br>";
#        $link_choices .= 
#              VCDisplay::query(
#                               'tree' => $tree,
#                               'mindate' => $mindate - $main::SECONDS_PER_WEEK,
#                               'maxdate' => $maxdate,
#                               'who' => $author,
#                               
#                               "linktxt" => "Check-ins within 7 days",
#                               );
#
#        $link_choices .= "<br>";

        $link_choices .= 
            HTMLPopUp::Link(
                            "href" => "mailto:$mailto_author",
                            "linktxt" => "Send Mail to $author",
                            );
        
        $link_choices .= "<br>";

        my ($href) = (FileStructure::get_filename($tree, 'tree_URL').
                      "/all_vc.html#$checkin_page_reference");
        
        $link_choices .= 
            HTMLPopUp::Link(
                            "href" => $href,
                            "linktxt" => "Tinderbox Checkin Data",
                            );
        
        $link_choices .= "<br>";
        
            my (%popup_args) = (
                                "windowtxt" => $link_choices,
                                "windowtitle" => ("$VC_NAME Info ".
                                                  "Author: $author ".
                                                  "$time_interval_str "),
                                );
        
               my ($query_link) = "";
            
            $query_link .= 
                  VCDisplay::query(
                                   'tree' => $tree,
                                   'mindate' => $mindate,
                                   'maxdate' => $maxdate,
                                   'who' => $author,
                                   
                                   "linktxt" => "\t\t<tt>$display_author</tt>",
                                   %popup_args,
                                   );

     # put each link on its own line and add good comments so we
        # can debug the HTML.
        
        my ($date_str) = localtime($mindate)."-".localtime($maxdate);
        
            if ($DEBUG) {
                $query_links .= (
                                 "\t\t<!-- VC_CVS: ".
                                 ("Author: $author, ".
                                  "Time: '$date_str', ".
                                  "Tree: $tree, ".
                                  "").
                                 "  -->\n".
                                 "");
            }
        
        $query_links .= "\t\t".$query_link."\n";
        
    } # foreach %author
    
    my $notice = $NOTICE->Notice_Link(
                                      $maxdate,
                                      $tree,
                                      $VC_NAME,
                                      );
    if ($notice) {
        $query_links.= "\t\t".$notice."\n";
    }

    $query_links.=  "\t\t".$text_browser_color_string."\n";

    @outrow = (
               "\t<td align=center $cell_color>\n".
               $query_links.
               "\t</td>\n".
               "");
    
  } else {

    @outrow = ("\t<!-- skipping: VC_Bonsai: tree: $tree -->".
               "<td align=center $cell_options>$empty_cell_contents</td>\n");
  }
  
  
  return @outrow; 
}



1;

