# -*- Mode: perl; indent-tabs-mode: nil -*-
#

# TinderDB::VC_Bonsai - methods to query the bonsai interface to CVS
# Version Control system and find the users who have checked changes
# into the tree recently and renders this information into HTML for
# the status page.  This module depends on TreeData and is one of the
# few which can see its internal datastructures.  The
# TinderHeader::TreeState is queried so that users will be displayed
# in the color which represents the state of the tree at the time they
# checked their code in.  This module uses the library BonsaiData.pm
# to provide an abstract programatic interface to Bonsai.  We assume
# that the Bonsai tree names are the same as the Tinderbox tree names.
# This module is very similar to VC_CVS and the two files should be
# kept in sync.


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





package TinderDB::VC_Bonsai;


# the Bonsai (Mozilla.org's SQL interface to CVS) implemenation of the
# Version Control DB for Tinderbox.  This column of the status table
# will report who has changed files in the CVS repository and what
# files they have changed.

# Remember CVS does all its work in GMT (UCT), Tinderbox does all its work in
# local time.  Thus this file needs to perform the conversions.



#   We store the hash of all names who modified the tree at a
#   particular time as follows:

#   $DATABASE{$tree}{$time}{'author'}{$author}{$file} = $log;

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


# Load standard perl libraries
use File::Basename;
use Time::Local;

# Load Tinderbox libraries

use lib '#tinder_libdir#';

use BonsaiData;
use TinderDB::BasicTxtDB;
use Utils;
use HTMLPopUp;
use TreeData;
use VCDisplay;


$VERSION = ( qw $Revision: 1.8 $ )[1];

@ISA = qw(TinderDB::BasicTxtDB);

# Add an empty object, of this DB subclass, to end of the set of all
# HTML columns.  This registers the subclass with TinderDB and defines
# the order of the HTML columns.

push @TinderDB::HTML_COLUMNS, TinderDB::VC_Bonsai->new();



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
  
  my ($num_updates) = 0;

 # The data is returned from bonsai as a list of lists.  Each row is
 # one list.  We want to handle the data as little as possible in this
 # module, our only goals is to isolate the bonsai interface.  So we
 # will pass back the data directly to the caller with no intermediate
 # passes over it.

 # We are provided a hash table to make indexing the check-ins easier
 # and more self documenting.

  my ($results, $i) = 
    BonsaiData::get_checkin_data(
                                 $tree, 
                                 $TreeData::VC_TREE{$tree}{'module'}, 
                                 $TreeData::VC_TREE{$tree}{'branch'}, 
                                 $last_cvs_data,
                                );

  foreach $r (@{ $results }) {
      
      my ($time)   = $r->[$$i{'time'}];
      my ($author) = $r->[$$i{'author'}];
      my ($dir)    = $r->[$$i{'dir'}];
      my ($file)   = $r->[$$i{'file'}];
      my ($log)    = $r->[$$i{'log'}];
      
      $DATABASE{$tree}{$time}{'author'}{$author}{"$dir/$file"} = 
          $log;
      $num_updates ++;
      
  } # foreach update

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
           "VC Cell Colors".
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
  return ("\t<th>VC checkins</th>\n");
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

    if ($DATABASE{$tree}{$time}{'treestate'}) {
      $LAST_TREESTATE = $DATABASE{$tree}{$time}{'treestate'};
    }

   # Now invert the data structure.

   # Inside each cell, we want all the posts by the same author to
   # appear together.  The previous data structure allowed us to find
   # out what data was in each cell.

    foreach $author (keys %{ $DATABASE{$tree}{$time}{'author'} }) {
      foreach $file (keys %{ $DATABASE{$tree}{$time}{'author'}{$author} }) {
          my ($log) = $DATABASE{$tree}{$time}{'author'}{$author}{$file};
          $authors{$author}{$time}{$file} = $log;
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
  
  my $query_links = '';
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
    my ($time_interval_str) = "$format_maxdate to $format_mindate",

    # create a string of all VC data for displaying with the checkin table

    my ($vc_info);
    foreach $key ('module','branch',) {
      my ($value) = $TreeData::VC_TREE{$tree}{$key};
      $vc_info .= "$key: $value <br>\n";
    }

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
                 "<th>Log</th>".
                 "</tr>\n".
                 "");
      
      # sort numerically descending
      foreach $time ( sort {$b <=> $a} keys %{ $authors{$author} }) {
        foreach $file (keys %{ $authors{$author}{$time}}) {
          $num_rows++;
          my ($log) = $authors{$author}{$time}{$file};
          $max_length = main::max(
                                  $max_length , 
                                  (length($file) + length($log)),
                                  );
          $table .= (
                     "\t<tr>".
                     "<td>".HTMLPopUp::timeHTML($time)."</td>".
                     "<td>".$file."</td>".
                     "<td>".$log."</td>".
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
                          "windowtitle" => ("VC Info ".
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


