# -*- Mode: perl; indent-tabs-mode: nil -*-
#

# TinderDB::VC_Perforce - methods to query the Perforce Version
# Control system and find the users who have checked changes into the
# tree recently and renders this information into HTML for the status
# page.  This module depends on TreeData and is one of the few which
# can see its internal datastructures.  

# The TinderHeader::TreeState is queried so that users will be
# displayed in the color which represents the state of the tree at the
# time they checked their code in.  Ideally Perforce should have a
# notion of tree state so that when the development tree is in a
# restricted state the developers find that their ability to checkin
# is restricted as well.  For example when the tree is 'closed' no
# developer should be able to check in however QA and the Project
# Managment team should be able to allow indivual developers to check
# in code on a case by case basis.  The security model in Perforce
# does not make such regular changes to the security tables
# perticularly easy or safe to be performed by so many people.

# The only perforce commands this module runs are:
#
#		 'p4 describe -s $num'
#		 'p4 changes  -s \@$date_str\@now $filespec'

# 			which looks like this 

#		 'p4 describe -s 75437'
# 		 'p4 changes -s submitted @2003/05/10,@now //...'
#
# This means that tinderbox needs to run as a user which has Perforce
# 'list' privileges.

# Tinderbox works on the notion of "tree" which is a set of
# directories (under a particular branch).  Perforce has no easily
# identifiable analog.  We would like to query Perforce about changes
# only to the set of files which concerns us.  It is not clear if the
# filespec can be used with the 'describe -s' command to limit the
# information reported about about a change set.

# We had similar problems with CVS so we create this notion in
# Tinderbox using the variables
# $TreeData::VC_TREE{$tree}{'dir_pattern'} which define regular
# expressions of file names.  There must be at least one file in the
# change set which matches the dir pattern for us to assume that this
# change set applies to this tree.  If not we ignore the change set.

# This is not implemented yet since we have no experiance with what is
# needed but some ideas are left in the code, commented out, so that
# we can work with them later.



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

# complete rewrite by Ken Estes for contact info see the
#     mozilla/webtools/tinderbox2/Contact file.
# Contributor(s): 


# $Revision: 1.19 $ 
# $Date: 2003/08/17 01:37:53 $ 
# $Author: kestes%walrus.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/lib/TinderDB/VC_Perforce.pm,v $ 
# $Name:  $ 



package TinderDB::VC_Perforce;


# The Perforce implemenation of the Version Control DB for Tinderbox.
# This column of the status table will report who has changed files in
# the Perforce Depot and what files they have changed.



#  We store the hash of all names who modified the tree at a
#  particular time as follows:

#  $DATABASE{$tree}{$time}{'author'}{$author} = 
#            [\@affected_files, \@jobs_fixed, 
# 		$change_num, $workspace, $comment];

# Where each element of @affected_files looks like
#	 [$filename, $revision, $action];
#
# This was extracted from perforce data which looked like
#         ... //depot/filename#32 edit

# Where each element of @jobs_fixed, looks like
#         [$jobname, $author, $status, $comment];
#
# This was extracted from perforce data which looked like
# 		job000001 on 2001/05/27 by ken *closed*


# we also store information in the metadata structure

#   $METADATA{$tree}{'updates_since_trim'} += 1;
#

#  we remember which change sets we have information about and what we
#  will need to be quired to get new data.
#
#  $METADATA{$tree}{'next_change_num'});
#

# additionally information about changes in the tree state are stored
# in the variable:

#   $DATABASE{$tree}{$time}{'treestate'} = $state;

#
# where state is either 'Open' or 'Closed' or some other user defined string.
#
# Cell colors are controled by the functions:
#	TreeData::get_all_sorted_tree_states()
#	TreeData::TreeState2color($state)
#	TreeData::TreeState2char($state)


# Load standard perl libraries
use File::Basename;
use Time::Local;

# Load Tinderbox libraries

use lib '#tinder_libdir#';


use BTData;
use TreeData;
use TinderHeader;
use HTMLPopUp;
use TinderDB::BasicTxtDB;
use TinderDB::Notice;
use Utils;
use VCDisplay;


$VERSION = ( qw $Revision: 1.19 $ )[1];

@ISA = qw(TinderDB::BasicTxtDB);


# name of the version control system
$VC_NAME = $TinderConfig::VC_NAME || "Perforce";

# how we recoginise bug number in the checkin comments.
$VC_BUGNUM_REGEXP = $TinderConfig::VC_BUGNUM_REGEXP ||
    '(\d\d\d+)';

# We 'have a' notice so that we can put stars in our column.

$NOTICE= TinderDB::Notice->new();
$DEBUG = 1;

$ENV{'P4PORT'} = $TinderConfig::PERFORCE_PORT || 1666;


# return a string of the whole Database in a visually useful form.

sub get_all_perforce_data {
  my ($self, $tree) = (@_);

  my $treestate = TinderHeader::gettree_header('TreeState', $tree);

  # sort numerically descending
  my (@times) = sort {$b <=> $a} keys %{ $DATABASE{$tree} };

  my $out;
  $out .= "<HTML>\n";
  $out .= "<HEAD>\n";
  $out .= "\t<TITLE>Perforce Checkin Data as gathered by Tinderbox</TITLE>\n";
  $out .= "</HEAD>\n";
  $out .= "<BODY>\n";
  $out .= "<H3>Perforce Checkin Data as gathered by Tinderbox</H3>\n";
  $out .= "\n\n";
  $out .= "<TABLE BORDER=1 BGCOLOR='#FFFFFF' CELLSPACING=1 CELLPADDING=1>\n";
  $out .= "\t<TR>\n";
  $out .= "\t\t<TH>Time</TH>\n";
  $out .= "\t\t<TH>Tree State</TH>\n";
  $out .= "\t\t<TH>Author</TH>\n";
  $out .= "\t\t<TH>File</TH>\n";
  $out .= "\t\t<TH>Log</TH>\n";
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
                          ) ;
      
      ($names) &&
          ($cell_time .= "\n".$names."\t\t");


      if (defined($DATABASE{$tree}{$time}{'treestate'})) {
          $treestate = $DATABASE{$tree}{$time}{'treestate'};

          $out .= "\t<TR>\n";
          $out .= "\t\t<TD>$cell_time</TD>\n";
          $out .= "\t\t<TD ALIGN=center >$treestate</TD>\n";
          $out .= "\t\t<TD>$HTMLPopUp::EMPTY_TABLE_CELL</TD>\n";
          $out .= "\t\t<TD>$HTMLPopUp::EMPTY_TABLE_CELL</TD>\n";
          $out .= "\t\t<TD>$HTMLPopUp::EMPTY_TABLE_CELL</TD>\n";
          $out .= "\t</TR>\n";
      }

      if ( defined($DATABASE{$tree}{$time}{'author'}) ) {
          my ($recs) = $DATABASE{$tree}{$time}{'author'};
          my (@authors) = sort (keys %{ $recs });

          foreach $author (@authors) {
              my($affected_files_ref, $jobs_fixed_ref, 
                 $change_num, $workspace, $comment) = 
                     @{ $recs->{$author} };
              my $localtime = localtime($time);

              $out .= (
                       HTMLPopUp::Link(
                                       "name" => $time,
                                       ).
                       HTMLPopUp::Link(
                                       "name" => "change".$change_num,
                                       ).
                       "\n");

              $out .= (
                       "Change number: $change_num <br>\n".
                       "at $localtime<br>\n".
                       "by $author <br>\n".
                       "on workspace: $workspace<br>\n".
                       "<br>\n".
                       "$comment<br>\n".
                       "<br>\n".
                       "\n");
                       
              my (@affected_header) = get_affected_files_header();

#              ($affected_table, $affected_num_rows, $affected_max_length) = 
#                  struct2table(\@affected_header, $author, $affected_files_ref);

              my (@jobs_header) =  get_jobs_fixed_header();
#
#      ($jobs_table, $jobs_num_rows, $jobs_max_length) = 
#          struct2table(\@jobs_header, $author, $jobs_fixed);
#

              $out .= "\n";
              $out .= $affected_table;
              $out .= "\n";

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

  my ($last_tree_data, $second2last_tree_data, $last_vc_data);

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
    
    (!defined($last_vc_data)) &&
      (defined($DATABASE{$tree}{$time}{'author'})) &&
	($last_vc_data = $time);
    
    
    # do not iterate through the whole histroy.  Stop after we have
    # the data we need.
    
    (defined($last_vc_data)) &&
      (defined($second2last_vc_data)) &&
	(defined($last_tree_data)) &&
	  last;

  } # foreach $time (@times) 


  return ($last_tree_data, $second2last_tree_data, $last_vc_data);
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

  my ($affected_files, $jobs_fixed);
  my (@authors,  @change_nums, @workspaces, );
  
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
           
           my($affected_files_ref, $jobs_fixed_ref, 
              $change_num, $workspace, $comment) = 
                  @{ $DATABASE{$tree}{$time}{'author'}{$author} };
           
           push @authors, $author;
           push @change_nums, $change_num;
           push @workspaces, $workspace;
       }
   }

   if (defined( $DATABASE{$tree}{$time}{'bugs'} )) {
       $recs = $DATABASE{$tree}{$time}{'bugs'};
       foreach $bug (keys %{ $recs }) {
           $bugs{$bug} =1;
       }
   }
   
} # while (1)
  
  @workspaces = main::uniq(@workspace);

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
       $cell_options = "bgcolor=$cell_color ";

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

  my $query_links = '';
  if ($text_browser_color_string) {
      $query_links.=  "\t\t".$text_browser_color_string."\n";
  }

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
      my ($time_interval_str) = "$format_maxdate to $format_mindate",
      
      # create a string of all VC data for displaying with the checkin table
      
      my ($vc_info);

      my $filespec = TreeData::Tree2Filespec($tree);
      $vc_info .= "Filespec: $filespec <br>\n";

      $vc_info .= "Changes: @change_nums <br>\n";
      
      foreach $author (@authors) {
          
          my $display_author=$author;
            
          my $mailto_author=$author;
          $mailto_author = TreeData::VCName2MailAddress($author);
          
          @bug_numbers = main::uniq(@bug_numbers);
          
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
          $link_choices .= 
              VCDisplay::query(
                               'tree' => $tree,
                               'mindate' => $mindate - $main::SECONDS_PER_DAY,
                               'maxdate' => $maxdate,
                               'who' => $author,
                               
                               "linktxt" => "Check-ins within 24 hours",
                               );
          
          $link_choices .= "<br>";
          $link_choices .= 
              VCDisplay::query(
                               'tree' => $tree,
                               'mindate' => $mindate - $main::SECONDS_PER_WEEK,
                               'maxdate' => $maxdate,
                               'who' => $author,
                               
                               "linktxt" => "Check-ins within 7 days",
                               );
          
          $link_choices .= "<br>";
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
          
          foreach $bug_number (@bug_numbers) {
              my $href = BTData::bug_id2bug_url($bug_number);
                $link_choices .= 
                    HTMLPopUp::Link(
                                    "href" => $href,
                                    "linktxt" => "\t\tBug: $bug_number",
                                    );
                $link_choices .= "<br>";
          }
          
          # we display the list of names in 'teletype font' so that the
          # names do not bunch together. It seems to make a difference if
          # there is a <cr> between each link or not, but it does make a
          # difference if we close the <tt> for each author or only for
          # the group of links.
          
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
          
          my $bug_links = '';
          foreach $bug_number (@bug_numbers) {
              my $href = BTData::bug_id2bug_url($bug_number);
              $bug_links .= 
                  HTMLPopUp::Link(
                                  "href" => $href,
                                  "linktxt" => "\t\t<tt>$bug_number</tt>",
                                  
                                  %popup_args,
                                    );
          }
          $query_link .= $bug_links;
          
          # put each link on its own line and add good comments so we
          # can debug the HTML.
          
          my ($date_str) = localtime($mindate)."-".localtime($maxdate);
          
          if ($DEBUG) {
              $query_links .= (
                               "\t\t<!-- VC_Perforce: ".
                               ("Author: $author, ".
                                "Bug_Numbers: '@bug_numbers', ".
                                "Time: '$date_str', ".
                                "Tree: $tree, ".
                                "").
                               "  -->\n".
                               "");
          }
          
          $query_links .= "\t\t".$query_link."\n";
          
      } # foreach @author
  } # if @authors
  
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
             "\t<!-- VC_Perforce: authors -->\n".               
             "\t<td align=center $cell_options>\n".
             $query_links.
             "\t</td>\n".
             "");
  
  return @outrow; 
}



# convert perforce string time into unix time.
# currently I see times of the form 
# (this function will change if perforce changes how it reports the time)
#          year/month/mday hours:min:seconds

sub parse_perforce_time { 

  my ($date_str, $time_str) = @_;

  # untaint
  $date_str =~ s![^0-9\/\:]+!!g;
  $time_str =~ s![^0-9\/\:]+!!g;

  my ($year, $mon, $mday) = split(/\//, $date_str);

  my ($hours, $min, $sec) = split(/:/, $time_str);
  
  # The perl conventions for these variables is 0 origin while
  # the "display" convention for these variables is 1 origin.
  
  $mon--;
  
  my ($time) = timelocal($sec,$min,$hours,$mday,$mon,$year);    

  if ( !(main::is_time_valid($time)) ) {
      die("Perforce reported time: $time ".scalar(localtime($time)).
          " : '$date $time' ".
          " is not valid. \n");
  }

  return $time;
}



sub apply_db_updates {
  my ($self, $tree,) = @_;

  my ($new_tree_state) = TinderHeader::gettree_header('TreeState', $tree);
  my ($last_tree_data, $second2last_tree_data, $last_vc_data) = 
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

  my ($num_updates) = 0;

  ($last_tree_data) ||
   ($last_tree_data = $main::TIME - $TinderDB::TRIM_SECONDS );
  
  @change_sets = get_new_change_sets($last_tree_data, $tree);

  (defined($METADATA{$tree}{'next_change_num'})) ||
      ($METADATA{$tree}{'next_change_num'} = 1);

  my $change_num = $METADATA{$tree}{'next_change_num'};

  # We can put more then one change number in a
  # request and they will all be answered.  Send multiple
  # requests per call to save the cost of creating a new
  # process.

  while (scalar(@change_sets)) {
      my @cmd_change_sets = splice @change_sets, 0, 20;
      my $biggest_change_num = $cmd_change_sets[$#cmd_change_sets];
      
      my (@cmd) = ('p4', 'describe', '-s', @cmd_change_sets);
      
      store_cmd_output(@cmd);        
      
      while (does_cmd_nextline_match("^Change ")) {
          
            $METADATA{$tree}{'next_change_num'}++;
            $num_updates++;

            # process the data in this change set

            my ($change_num, $author, $time, $workspace) = 
                parse_update_header_line();
            my ($comment) = parse_update_comment();
            my (@jobs_fixed) = parse_update_jobs_fixed();
            my (@affected_files) = parse_update_affected_files();

            $DATABASE{$tree}{$time}{'author'}{$author} = 
                [\@affected_files, \@jobs_fixed, 
                 $change_num, $workspace, $comment];

            if ($comment =~ m/$VC_BUGNUM_REGEXP/) {
                my $bug_number = $1;
                $DATABASE{$tree}{$time}{'bugs'}{$bug_number} = 1;
            }

        } # each change

  } # each p4 subprocess

    $METADATA{$tree}{'updates_since_trim'} += $num_updates;

    if ( ($METADATA{$tree}{'updates_since_trim'} >
          $TinderDB::MAX_UPDATES_SINCE_TRIM)
         ) {
        $METADATA{$tree}{'updates_since_trim'}=0;
        $self->trim_db_history($tree);
    }
    
    $self->savetree_db($tree);
    
  # VCDisplay needs to know the filename that we write to, so that the
  # None.pm can reference this file.  However VCDisplay is called by
  # Build, Time as well as this VC column and we would rather not have
  # VCDisplay depend on anything in the VC_* code.  So all the VC
  # implementations must store their data into a file with the same
  # name. It is highly unlikely that one user will run VC_CVS.pm and
  # VC_Bonsai.pm module together.

  my $all_vc_data = $self->get_all_perforce_data($tree);
  my ($outfile) = (FileStructure::get_filename($tree, 'tree_HTML').
                   "/all_vc.html");
  main::overwrite_file($outfile, $all_vc_data);

    return $num_updates;
} # apply_db_updates

# Find the change sets which are new since the last time we checked
# and are relevent for this tree. Unfortunatly the 'p4 changes'
# command will not tell us which files have changed.  We will call the
# 'p4 describe' command to get more detailed information.

sub get_new_change_sets {
  my ($date, $tree, ) = @_;

  my @date_time_str = time2perforceFormat($date);
  my $date_str = $date_time_str[0];

  # use this command to pick a starting changeset
  # p4 changes -s submitted @2003/05/10,@now //...

  $filespec = TreeData::Tree2Filespec($tree);
  my (@cmd) = (
               'p4', 'changes', '-s', 
               '@'.$date_str.',@now', 
               $filespec,
               );
  
  my (@p4_output) = main::cache_cmd(@cmd);
  
  my @change_set;
  foreach $line (@p4_output) {
      ($line =~ m/^Change (\d+) on/) &&
          push @change_set, $1;
  }
  
  return @change_set;
}


# This set of functions are used by apply_db_updates() and the
# 'parse_update_' functions to retrieve the output of the VC one line
# at a time so that it can be parsed.

sub store_cmd_output {
    my (@cmd) = @_;
    
    # We have a good chance that all the trees we are interested in will
    # be in the same depot.  Since we are not using the module
    # name or branch in the depot call all the VC data will be identical.
    # Use a cached result if possible.

    @CMD_OUTPUT = main::cache_cmd(@cmd);
    return ;
} # store_cmd_output


sub does_cmd_nextline_match {
    my ($pattern) = @_;

    return scalar($CMD_OUTPUT[0] =~ m/$pattern/);
} # does_cmd_nextline_match


sub get_cmd_nextline {

    $line = shift @CMD_OUTPUT;
    return $line;
} # get_cmd_nextline


# The set of functions beginning 'parse_update_' are used to parse
# parts of the output of perforce and are called via
# apply_db_updates().  This set of functions is highly dependent on
# how perforce reports the data from a call of:

#
#		 'p4 describe -s $num'
#

# Each function knows how to parse one section of the output and
# remove the data which has been parsed from the datastructure which
# holds the output.  If the section is optional and is not provided
# then the function will recognize that there is not work for it
# during this call and will return without performing any work.  The
# functions return the data that they parsed and do not set any
# variables with the values found.


sub is_line_blank {
    my ($line) = @_;
    return scalar($line =~ m/^\n$/);
} # is_line_blank


sub parse_update_empty_line {
    
    my ($line) = get_cmd_nextline();
    
    (is_line_blank($line)) ||
        die("Perforce reported unexpected non blank line: '$line'\n");
    
    return ;
} # parse_update_empty_line



sub parse_update_comment {
    my ($comment);
    
    parse_update_empty_line();
    my ($line) = get_cmd_nextline();

    while ( !(is_line_blank($line)) ) {
        
        ($line =~ m/^\t/) ||
            die("Perforce comments are ".
                "expected to begin with a tab: '$line'\n");
        $comment .= $line;
        $line = get_cmd_nextline();
    }
    
    # convert tabs and \n to spaces, 
    # collapse duplicate spaces, 
    # untaint
    
    $comment =~ s/\s+/ /g;
    $comment = main::extract_printable_chars($comment);
    
    return $comment;
} # parse_update_comment



sub parse_update_header_line {
    
    (does_cmd_nextline_match('^Change ')) ||
        die("Can not parse Perforce describe output ".
            "expected string: 'Change ', '$line'\n");
    
    my ($line) = get_cmd_nextline();

    # The first line always looks like this:
    #
    # 	Change 576 by kestes@cirrus on 2001/05/27 12:18:17
    #
    
    my ($change, $change_num, $by, $author, $on, $date, $time,) = 
        split(/\s+/, $line);
    my $workspace;
    
    {
        # untaint the input and assure we parsed it correctly.
        
        if ( $change ne 'Change' ) {
            die("Can not parse Perforce describe output, ".
                "expected string Change: '$change': '$line'\n");
        }
        
        if ( $by ne 'by' ) {
            die("Can not parse Perforce describe output, ".
                "expected string by: '$by': '$line'\n");
        }
        
        if ( $on ne 'on' ) {
            die("Can not parse Perforce describe output, ".
                "expected string on: '$on': '$line'\n");
        }
        
        $change_num = main::extract_digits($change_num);

        $author = main::extract_user($author);
        ($author, $workspace) = split ('@', $author);
        
        $time = parse_perforce_time($date, $time);
        
        if ( !(main::is_time_valid($time)) ) {
            die("Perforce reported time: $time ".scalar(localtime($time)).
                " : '$date $time' ".
                " is not valid. \n");
        }
    }
    return ($change_num, $author, $time, $workspace,);
} # parse_update_header_line


sub parse_update_jobheader_line {

   my ($line) = get_cmd_nextline();

    # the line we are parsing looks like this.
    #
    # 		job000001 on 2001/05/27 by ken *closed*
    # 
    # Remember: the jobname is any sequence of non-whitespace
    # characters

    my ($jobname, $on, $date, $by, $author, $status) = 
        split(/\s+/, $line);
    
    {
        # untaint the input and assure we parsed it correctly.
        
        if ( $on ne 'on' ) {
            die("Can not parse Perforce describe output, ".
                "expected string on: '$on': '$line'\n");
        }

        if ( $by ne 'by' ) {
            die("Can not parse Perforce describe output, ".
                "expected string by: '$by': '$line'\n");
        }
        
        $jobname = main::extract_printable_chars($jobname);
        $author = main::extract_user($author);
        $status = main::extract_printable_chars($status);

        if ($status =~ m/\*(.*)\*/) {
            # strip the enclosing asterix's
            $status = $1
        }

        # Ignore the date it is not very accurate and thus not
        # interesting.
    }

    return($jobname,$author,$status);
} # parse_update_jobheader_line



sub parse_update_jobs_fixed {

    # before we begin parsing ensure we have the right section of
    # output.

    (does_cmd_nextline_match('^Jobs fixed')) ||
        return ;

    # its good, throw away the line and start parsing.

    my ($line) = get_cmd_nextline();

    parse_update_empty_line();
    
    my (@jobs_fixed);

    while ( !(does_cmd_nextline_match('\.\.\.$')) ) {

        my ($jobname,$author,$status) = parse_update_jobheader_line();
        my ($comment) = parse_update_comment();
        push @jobs_fixed, [$jobname, $author, $status, $comment];
        
    }

    return @jobs_fixed;
} # parse_update_jobs_fixed
        
sub get_jobs_fixed_header {
    # this is the header to use when parsing the 
    # jobs_fixed datastructure.
    my (@header) = qw(JobName Author Status Comment);
    return @header;
}




sub parse_update_affected_files {
    # before we begin parsing ensure we have the right section of
    # output.

    (does_cmd_nextline_match('^Affected files')) ||
        return ;

    # its good, throw away the line and start parsing.

    my ($line) = get_cmd_nextline();

    parse_update_empty_line();
    $line = get_cmd_nextline();

    my (@affected_files);

    while ( !(is_line_blank($line)) ) {
        
        # The lines looks like this:
        #
        # ... //depot/filename#32 edit
        # ... //depot/filename with spaces#32 edit
        
        my ($elipsis, $filename_and_revision, $action,);

        @line = split(/\s+/, $line);

        $action = pop @line;
        $elipsis = shift @line;
        $filename_and_revision = "@line";

        my ($filename, $revision,) = 
            split(/\#/, $filename_and_revision);
        
        $filename = main::extract_printable_chars($filename);
        $action = main::extract_printable_chars($action);
        
        if ($elipsis ne '...') {
            die("Can not parse Perforce describe output ".
                "expected string: '...', '$line'\n");
        }
        
        ($revision =~ m/\d+/) ||
            die("Can not parse Perforce describe output ".
                "expected revisions number following filename: ".
                "'filename_and_revision' : '$line'\n");

        ($filename =~ m/[\/\\]/) ||
            die("Can not parse Perforce describe output ".
                "expected filename to have forward or backward slash: ".
                "'filename' : '$line'\n");

        push @affected_files, [$filename, $revision, $action,];
        $line = get_cmd_nextline();
    }
    
    return @affected_files;
} # parse_update_affected_files


sub get_affected_files_header {
    # this is the header to use when parsing the 
    # affect_files datastructure.
    my (@header) = qw(Filename Revision Action Comment);
    return @header;
}


# The two temporary data structures: $affected_files, $jobs_fixed
# in status_table_row() have a similar structure.
# We use this function to turn their data into a html table.

sub struct2table {
    my ($header, $author, $struct,) = @_;
    my (@header) = @{$header};
    
    my $table = '';
    my $num_rows = 0;
    my $max_length = 0;

    $table .= "<br>\n";
    $table .= "<table border cellspacing=2>\n";
    $table .= list2table_header("Time", "Change", @header);
    
    my (@times) = keys %{$struct->{$author}};
    foreach $time (@times) {
        my (@change_num) = keys %{ $struct->{$author}{$time} };
        foreach $change_num (@change_num) {
            # the list of all changes with this change number
            my @rows = @{ $struct->{$author}{$time}{$change_num} };
            foreach $row (@rows) {
                
                my ($format_time) = HTMLPopUp::timeHTML($time);
                # one change
                my (@row) = @{$row};
                $table .= list2table_row($format_time,$change_num,@row);
                $num_rows++;
                $max_length = main::max(
                                        $max_length, 
                                        length("@row"),
                                        );
            }
        }
    }
    
    $table .= "</table>\n";
    
    return ($table, $num_rows, $max_length);
}

sub list2table_header {
    my (@list) = @_;
    
    my ($header) = (
                    "\t<tr>".
                    "<th>".
                    join("</th><th>",@list).
                    "</th>".
                    "</tr>\n".
                    "");
    return $header;
}


sub list2table_row {
    my (@list) = @_;
    
    my ($row) = (
                 "\t<tr>".
                 "<td>".
                 join("</td><td>", @list).
                 "</td>".
                 "</tr>\n".
                 "");
    
    return $row;
}

sub time2perforceFormat {
  # convert time() format to the format which appears in perforce output
  my ($time) = @_;

  my ($sec,$min,$hour,$mday,$mon,
      $year,$wday,$yday,$isdst) =
        localtime($time);

  $mon++;
  $year += 1900;

  my $date_str = sprintf("%04u/%02u/%02u",
                         $year, $mon, $mday);
  
  my $time_str = sprintf("%02u:%02u:%02u",
                         $hour, $min, $sec);

  return ($date_str, $time_str);
}

sub perforce_date_str2time {      
    my ($perforce_date_str) = @_;
    
    my ($year, $mon, $mday,) = split('/', $perforce_date_str);
    my ($hour, $min, $sec) = 0;

    # The perl conventions for these variables is 0 origin while
    # the "display" convention for these variables is 1 origin.
    
    $mon--;
    
    my ($time) = timelocal($sec,$min,$hour,$mday,$mon,$year);    
    
    return $time;
}


1;
