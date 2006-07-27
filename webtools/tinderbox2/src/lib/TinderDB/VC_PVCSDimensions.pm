# -*- Mode: perl; indent-tabs-mode: nil -*-
#

# TinderDB::VC_PVCSDimensions - methods to query the Perforce Version
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


# $Revision: 1.6 $ 
# $Date: 2006/07/27 16:31:07 $ 
# $Author: bear%code-bear.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/lib/TinderDB/VC_PVCSDimensions.pm,v $ 
# $Name:  $ 



package TinderDB::VC_PVCSDimensions;


# The Perforce implemenation of the Version Control DB for Tinderbox.
# This column of the status table will report who has changed files in
# the Perforce Depot and what files they have changed.



#  We store the hash of all names who modified the tree at a
#  particular time as follows:

#  $DATABASE{$tree}{$time}{'author'}{$originator}{$ch_doc_id} =  
#              [ $lib_filename, $revision, $status, $title, ];

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


use PVCSGetData;
use BTData;
use TreeData;
use TinderHeader;
use HTMLPopUp;
use TinderDB::BasicTxtDB;
use TinderDB::Notice;
use Utils;
use VCDisplay;


$VERSION = ( qw $Revision: 1.6 $ )[1];

@ISA = qw(TinderDB::BasicTxtDB);


# name of the version control system
$VC_NAME = $TinderConfig::VC_NAME || "PVCS";

# how we recoginise bug number in the checkin comments.
$VC_BUGNUM_REGEXP = $TinderConfig::VC_BUGNUM_REGEXP ||
    '(_SCR_\d\d\d+)';

# We 'have a' notice so that we can put stars in our column.

$NOTICE= TinderDB::Notice->new();
$DEBUG = 1;


# return a string of the whole Database in a visually useful form.

sub get_all_pvcs_data {
  my ($self, $tree) = (@_);

#  my $treestate = TinderHeader::gettree_header('TreeState', $tree);

  # sort numerically descending
  my (@times) = sort {$b <=> $a} keys %{ $DATABASE{$tree} };

  my $out;
  $out .= "<HTML>\n";
  $out .= "<HEAD>\n";
  $out .= "\t<TITLE>PVCS Checkin Data as gathered by Tinderbox</TITLE>\n";
  $out .= "</HEAD>\n";
  $out .= "<BODY>\n";
  $out .= "<H3>PVCS Checkin Data as gathered by Tinderbox</H3>\n";
  $out .= "\n\n";
  $out .= "<TABLE BORDER=1 BGCOLOR='#FFFFFF' CELLSPACING=1 CELLPADDING=1>\n";
  $out .= "\t<TR>\n";
  $out .= "\t\t<TH>Time</TH>\n";
  $out .= "\t\t<TH>Tree State</TH>\n";
  $out .= "\t\t<TH>Originator</TH>\n";
  $out .= "\t\t<TH>Ch Doc ID</TH>\n";
  $out .= "\t\t<TH>Lib File</TH>\n";
  $out .= "\t\t<TH>Rev</TH>\n";
  $out .= "\t\t<TH>Status</TH>\n";
  $out .= "\t\t<TH>Title</TH>\n";
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

      my $pretty_time = HTMLPopUp::timeHTML($time);
      my $cell_time =
          HTMLPopUp::Link(
                          "name" => $time,
                          "linktxt" => $pretty_time,
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
          $out .= "\t\t<TD>$HTMLPopUp::EMPTY_TABLE_CELL</TD>\n";
          $out .= "\t\t<TD>$HTMLPopUp::EMPTY_TABLE_CELL</TD>\n";
          $out .= "\t\t<TD>$HTMLPopUp::EMPTY_TABLE_CELL</TD>\n";
          $out .= "\t</TR>\n";
      }

      if ( defined($DATABASE{$tree}{$time}{'author'}) ) {
          my ($recs) = $DATABASE{$tree}{$time}{'author'};
          my (@authors) = sort (keys %{ $recs });

          foreach $author (@authors) {
              foreach $change_num ( keys %{ $recs->{$author} } ) {
                  foreach $item_file ( @{ $recs->{$author}{$change_num} }) {

                  $out .= (
                         HTMLPopUp::Link(
                                         "name" => $time,
                                         ).
                         HTMLPopUp::Link(
                                         "name" => "change_".$change_num,
                                         ).
                           "\n");
                  
                  $out .= list2table_row( 
                                          $cell_time, $treestate, 
                                          $author, $change_num,
                                          @{ $item_file }
                                          );
                  $out .= "\n";
              } # $item_file
              }# change_num
          } # $author
      } # has authors
  } # time

  $out .= "</TABLE>\n";

  $out .= "\n\n";
  $out .= "This page was generated at: ";
  $out .= localtime($main::TIME);
  $out .= "\n\n";

  $out .= "</BODY>\n";
  $out .= "</HTML>\n";

  return $out;
}



# Return the most recent times that we received treestate and checkin
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

  # If we delete too many duplicates then we lose information when
  # the database is trimmed. We need to keep some duplicates around
  # for debugging and for "redundancy". Only delete duplicates during
  # the last hour.  Notice we are still removing 90% of the
  # duplicates.

  # If we have three data points in a row, and all of them have the
  # same state and the oldest is less than an hour old, then we can
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
  
  (defined($METADATA{$tree}{'next_change_num'})) ||
      ($METADATA{$tree}{'next_change_num'} = 1);

  my $product_id = TreeData::TreeName2Branch($tree);
  my $workset_name = TreeData::TreeName2Module($tree);
  my $min_time = time2pvcsFormat($last_tree_data);

  my $sqltable = <<"EOSQL";

  SELECT 
      PCMS_ITEM_DATA."PRODUCT_ID", 
      PCMS_ITEM_DATA."CREATE_DATE", 
      PCMS_ITEM_DATA."ORIGINATOR", 
      PCMS_ITEM_DATA."LIB_FILENAME", 
      PCMS_ITEM_DATA."REVISION",  
      PCMS_ITEM_DATA."STATUS",
      PCMS_WORKSET_INFO."WORKSET_NAME",
      PCMS_CHDOC_DATA."CH_DOC_ID", 
      PCMS_CHDOC_DATA."TITLE" 
  FROM 
      "PCMS"."PCMS_ITEM_DATA" PCMS_ITEM_DATA, 
      "PCMS"."PCMS_WORKSET_INFO",
      "PCMS"."PCMS_CHDOC_DATA" PCMS_CHDOC_DATA,
      "PCMS"."PCMS_CHDOC_RELATED_ITEMS" PCMS_CHDOC_RELATED_ITEMS
  WHERE 
      PCMS_ITEM_DATA."ITEM_UID" = PCMS_CHDOC_RELATED_ITEMS."TO_ITEM_UID" AND 
      PCMS_ITEM_DATA."PRODUCT_ID" = PCMS_WORKSET_INFO."PRODUCT_ID" AND
      PCMS_CHDOC_RELATED_ITEMS."FROM_CH_UID" = PCMS_CHDOC_DATA."CH_UID" AND 
      PCMS_ITEM_DATA."PRODUCT_ID" = '$product_id' AND
      PCMS_WORKSET_INFO."WORKSET_NAME" = '$workset_name' AND
      PCMS_ITEM_DATA."CREATE_DATE" >= '$min_time' 
  ORDER BY 
      PCMS_CHDOC_DATA."CH_DOC_ID" DESC;

  exit;
 
EOSQL
    ;


  my $sep = PVCSGetData::get_oraselect_separator();

  my $sqlout = PVCSGetData::oraselect($sqltable);

  foreach $line (@{ $sqlout }) {

      chomp $line;
      ($line) || next;
      
      my (
          $product_id,
          $create_date,
          $originator,
          $lib_filename,
          $revision,
          $status,
          $workset_name,
          $ch_doc_id,
          $title,
          ) = split (/$sep/ , $line);
      
      $product_id = pvcs_trim_whitespace($product_id);
      $lib_filename = pvcs_trim_whitespace($lib_filename);
      $originator = pvcs_trim_whitespace($originator);
      $create_date  = pvcs_trim_whitespace($create_date);
      $revision = pvcs_trim_whitespace($revision);
      $status = pvcs_trim_whitespace($status);
      $workset_name = pvcs_trim_whitespace($workset_name);
      $ch_doc_id = pvcs_trim_whitespace($ch_doc_id);
      $status = pvcs_trim_whitespace($status);
      
      my $time = pvcs_date_str2time($create_date);
      
      push @{ $DATABASE{$tree}{$time}{'author'}{$originator}{$ch_doc_id} },
	      [ $lib_filename, $revision, $status, $title, ];
      
      $DATABASE{$tree}{$time}{'bugs'}{$ch_doc_id} = 1;

      $num_updates ++;      

  }

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

  my $all_vc_data = $self->get_all_pvcs_data($tree);
  my ($outfile) = (FileStructure::get_filename($tree, 'tree_HTML').
                   "/all_vc.html");
  main::overwrite_file($outfile, $all_vc_data);

    return $num_updates;
} # apply_db_updates


sub time2pvcsFormat {
  # convert time() format to the format which appears in perforce output
  my ($time) = @_;

  my ($sec,$min,$hour,$mday,$mon,
      $year,$wday,$yday,$isdst) =
        localtime($time);

  $year += 1900;
  $mon++;

  my $date_str = sprintf("%04u/%02u/%02u/%02u/%02u/%02u", 
                         $year, $mon, $mday, $hour, $min, $sec);

  return ($date_str);
}

sub pvcs_date_str2time {      
    my ($pvcs_date_str) = @_;
    
    my ($year, $mon, $mday, $hour, $min, $sec) = 
        split('/', $pvcs_date_str);

    $mon--;

    my ($time) = timelocal($sec,$min,$hour,$mday,$mon,$year);    
    
    return $time;
}


sub pvcs_trim_whitespace {
    my ($line) = (@_);

    $line =~ s{\t+}{}g;
    $line =~ s{\n+}{}g;
    $line =~ s{ +}{}g;

    return $line;    
}


# Return the data which would appear in a cell if we were to render
# it.  The checkin data comes back in a datastructure and we return
# the oldest treestate found in this cell.  The reason for another
# data structure is that this one is keyed on authors while the main
# data structure is keyed on time.

sub cell_data {
    my ($tree, $db_index, $last_time) = @_;

    my (%authors) = ();
    my $last_treestate;

    while (1) {
        my ($time) = $DB_TIMES[$db_index];
        
        # find the DB entries which are needed for this cell
        ($time < $last_time) && last;
 
        ($db_index >= $#DB_TIMES) && last;

        $db_index++;
        
        if (defined($DATABASE{$tree}{$time}{'treestate'})) {
            (
             defined($last_treestate) &&
             $last_treestate ne $DATABASE{$tree}{$time}{'treestate'} &&
             1) && last;

            $last_treestate = $DATABASE{$tree}{$time}{'treestate'};
        }

        # Inside each cell, we want all the posts by the same author to
        # appear together.  The previous data structure allowed us to find
        # out what data was in each cell.
        
        if (defined( $DATABASE{$tree}{$time}{'author'} )) {
            my $recs = $DATABASE{$tree}{$time}{'author'};
            foreach $author (keys %{ $recs }) {
                my @bugs = (keys %{ $recs->{$author} });
                $authors{$author} = [ @bugs ];
            }
        }
        
    } # while (1)

    return ($db_index, $last_treestate, \%authors, );
}


# Produce the HTML which goes with the already computed author data.

sub render_authors {
    my ($tree, $last_treestate, $authors, $mindate, $maxdate,) = @_;

    my @authors = sort keys %{$authors};
    
    my ($cell_color) = TreeData::TreeState2color($last_treestate);
    my ($char) = TreeData::TreeState2char($last_treestate);
    
    my $cell_options = '';
    my $text_browser_color_string;
    
    if ( ($last_treestate) && ($cell_color) ) {
        $cell_options = "bgcolor=$cell_color ";
        
        $text_browser_color_string = 
          HTMLPopUp::text_browser_color_string($cell_color, $char);
    }
    
    my $query_links = '';
    if ($text_browser_color_string) {
        $query_links.=  "\t\t".$text_browser_color_string."\n";
    }

    if ( scalar(@authors) ) {
        
        # find the times which bound the cell so that we can set up a
        # VC query.
        
        my ($format_maxdate) = HTMLPopUp::timeHTML($maxdate);
        my ($format_mindate) = HTMLPopUp::timeHTML($mindate);
        my ($time_interval_str) = "$format_maxdate to $format_mindate",

        # create a string of all VC data for displaying with the
        # checkin table

        my ($vc_info);
        foreach $key ('module','branch',) {
            my ($value) = $TreeData::VC_TREE{$tree}{$key};
            $vc_info .= "$key: $value <br>\n";
        }
        
        # define a table, to show what was checked in for each author
        
        foreach $author (@authors) {
            
            my $mailto_author = TreeData::VCName2MailAddress($author);

            my (@bugs) = @{ $authors->{$author} };   

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
                          "/all_vc.html#$mindate");

            $link_choices .= 
              HTMLPopUp::Link(
                              "href" => $href,
                              "linktxt" => "Tinderbox Checkin Data",
                              );

            $link_choices .= "<br>";


            foreach $bug_number (sort keys %{ $bugs }) {
                my $href = BTData::bug_id2bug_url($bug_number);
                $link_choices .= 
                    HTMLPopUp::Link(
                                    "href" => $href,
                                    "linktxt" => "\t\tBug: $bug_number",
                                    );
                $link_choices .= "<br>";
            }

            # We display the list of names in 'teletype font' so that the
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
                                   
                                   "linktxt" => "\t\t <tt>$author</tt>",
                                   %popup_args,
                                   );

            my $bug_links = '';
            foreach $bug_number (@bugs) {
                my $href = BTData::bug_id2bug_url($bug_number);
                $bug_links .= 
                    HTMLPopUp::Link(
                                    "href" => $href,
                                    "linktxt" => "\t\t <tt>$bug_number</tt>",
                                    
                                    %popup_args,
                                    );
            }
            $query_link .= ' &nbsp; ';
            $query_link .= $bug_links;
            $query_link .= ' &nbsp; ';
            # put each link on its own line and add good comments so we
            # can debug the HTML.
            
            my ($date_str) = localtime($mindate)."-".localtime($maxdate);
            
            if ($DEBUG) {
                $query_links .= (
                                 "\t\t<!-- VC_PVCSDimensions: ".
                                 ("Author: @authors, ".
                                  "Bug_Numbers: '@bugs', ".
                                  "Time: '$date_str', ".
                                  "Tree: $tree, ".
                                  "").
                                 "  -->\n".
                                 "");
            }

            $query_links .= "\t\t".$query_link."\n";
            
        } # foreach $author
    } # if @authors

    my $notice = $NOTICE->Notice_Link(
                                      $maxdate,
                                      $tree,
                                      $VC_NAME,
                                      );
    if ($notice) {
        $query_links.= "\t\t".$notice."\n";
    }

    if ($text_browser_color_string) {
        $query_links.=  "\t\t".$text_browser_color_string."\n";
    }

    @outrow = (
               "\t<!-- VC_PVCSDimensions: authors -->\n".               
               "\t<td align=center $cell_options>\n".
               $query_links.
               "\t</td>\n".
               "");
    
    return @outrow;
}


# Create one cell (possibly taking up many rows) which will show
# that:
#       No authors have checked in during this time.
#       The tree state has not changed during this time.
#       There were no notices posted during this time.

sub render_empty_cell {
    my ($last_treestate, $till_time, $rowspan, $tree) = @_;

    my $local_till_time = localtime($till_time);
    my ($cell_color) = TreeData::TreeState2color($last_treestate);
    my ($char) = TreeData::TreeState2char($last_treestate);
    
    my $cell_options = '';
    my $text_browser_color_string;
    
    if ( ($last_treestate) && ($cell_color) ) {
        $cell_options = "bgcolor=$cell_color ";
        
        $text_browser_color_string = 
          HTMLPopUp::text_browser_color_string($cell_color, $char) ;
    }

    my $cell_contents;
    if ($text_browser_color_string) {
        $cell_contents .= $text_browser_color_string;
    }

    if (!($cell_contents)) {
        $cell_contents = "\n\t\t".$HTMLPopUp::EMPTY_TABLE_CELL."\n";
    }

    my @outrow =();
    if ($DEBUG) {
        push @outrow, ("\t<!-- VC_PVCSDimensions: empty data. ".
                       "tree: $tree, ".
                       "last_treestate: $last_treestate, ".
                       "filling till: $local_till_time, ".
                       "-->\n");
    }
            
    push @outrow, (
                   "\t\t<td align=center rowspan=$rowspan $cell_options>".
                   "$cell_contents</td>\n");

    return @outrow;
}



# clear data structures in preparation for printing a new table

sub status_table_start {
  my ($self, $row_times, $tree, ) = @_;
  
  # create an ordered list of all times which any data is stored
  
  # sort numerically descending
  @DB_TIMES = sort {$b <=> $a} keys %{ $DATABASE{$tree} };

  # NEXT_DB is my index into the list of all times.


  # adjust the $NEXT_DB to skip data which came after the first cell
  # at the top of the page.  We make the first cell bigger than the
  # rest to allow for some overlap between pages.

  my ($first_cell_seconds) = 2*($row_times->[0] - $row_times->[1]);
  my ($earliest_data) = $row_times->[0] + $first_cell_seconds;

  $NEXT_DB{$tree} = 0;
  while ( ($DB_TIMES[$NEXT_DB{$tree}] > $earliest_data) &&    
          ($NEXT_DB{$tree} < $#DB_TIMES) ) {
    $NEXT_DB{$tree}++
  }

  $NOTICE->status_table_start($row_times, $tree, $VC_NAME);

  # we do not store a treestate with every database entry.
  # remember the treestate as we travel through the database.

  $LAST_TREESTATE{$tree} = '';

  # Sometimes our output will span several rows.  This will track the
  # next row in which we create more HTML.

  $NEXT_ROW{$tree} = 0;

  return ;  
}


sub status_table_row {
  my ($self, $row_times, $row_index, $tree, ) = @_;

  my (@outrow) = ();

  # skip this column because it is part of a multi-row missing data
  # cell?

  if ( $NEXT_ROW{$tree} != $row_index ) {
      if ($DEBUG) {
          push @outrow, ("\t<!-- VC_PVCSDimensions: skipping. ".
                         "tree: $tree, ".
                         "additional_skips: ".
                         ($NEXT_ROW{$tree} -  $row_index).", ".
                         " -->\n");
      }
      return @outrow;
  }

  
  # Find all the authors who changed code at any point in this cell
  # Find the tree state for this cell.

  my ($db_index, $last_treestate, $authors) =
      cell_data($tree, $NEXT_DB{$tree}, $row_times->[$row_index]);

  # Track the treestate between calls with a global variable.

  # This initialization may not be correct.  If we are creating a page
  # for a few days ago, when we start making the page, we do not know
  # the treestate. It is not so easy to find out the right tree state
  # for the first few rows, so fake it with the current state.

  $LAST_TREESTATE{$tree} = ( 
                             $last_treestate || 
                             $LAST_TREESTATE{$tree} || 
                             TinderHeader::gettree_header('TreeState', $tree) 
                             );

  if (scalar(%{$authors})) {

      $NEXT_DB{$tree} = $db_index;
      $NEXT_ROW{$tree} = $row_index + 1;

      my ($mindate) = $row_times->[$row_index], 
      my ($maxdate);
      if ($row_index > 0){
          $maxdate = $row_times->[$row_index - 1];
      } else {
          $maxdate = $main::TIME;
      }

      my @html = render_authors(
                                $tree,
                                $LAST_TREESTATE{$tree},
                                $authors,
                                $mindate,
                                $maxdate,
                                );
      return @html;
  }

  # Create a multi-row dummy cell for missing data.
  # Cell stops if there is author data in the following cell or the
  # treestate changes.
  
  my $rowspan = 0;
  my ($next_db_index, $next_treestate, $next_authors);
  $next_db_index = $db_index;
    
  while (
         ($row_index+$rowspan <= $#{ $row_times }) &&

         ( (!($next_authors)) || (!(scalar(%{$next_authors}))) ) &&

         (
          !defined($next_treestate) ||
          ( $LAST_TREESTATE{$tree} eq $next_treestate)
          )

         ) {
      
      $db_index = $next_db_index;
      $rowspan++ ;
      
      ($next_db_index, $next_treestate, $next_authors) =
          cell_data($tree, $db_index, 
                    $row_times->[$row_index+$rowspan]);
      
  }
  
  $NEXT_ROW{$tree} = $row_index + $rowspan;
  $NEXT_DB{$tree} = $db_index;
  
  my @html= render_empty_cell($LAST_TREESTATE{$tree}, 
                              $row_times->[$row_index+$rowspan], 
                              $rowspan, $tree);
  return @html;
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

1;
