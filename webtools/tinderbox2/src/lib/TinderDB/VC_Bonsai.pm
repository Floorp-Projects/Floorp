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


# $Revision: 1.56 $ 
# $Date: 2002/05/07 20:36:26 $ 
# $Author: kestes%walrus.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/lib/TinderDB/VC_Bonsai.pm,v $ 
# $Name:  $ 



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
#	TreeData::get_all_sorted_tree_states()
#	TreeData::TreeState2color($state)
#	TreeData::TreeState2char($state)


# Load standard perl libraries
use File::Basename;
use Time::Local;

# Load Tinderbox libraries

use lib '#tinder_libdir#';

use TinderHeader;
use BonsaiData;
use BTData;
use TinderDB::BasicTxtDB;
use Utils;
use HTMLPopUp;
use TreeData;
use VCDisplay;


$VERSION = ( qw $Revision: 1.56 $ )[1];

@ISA = qw(TinderDB::BasicTxtDB);

# name of the version control system
$VC_NAME = $TinderConfig::VC_NAME || "CVS";

# how we recoginise bug number in the checkin comments.
$VC_BUGNUM_REGEXP = $TinderConfig::VC_BUGNUM_REGEXP ||
    "(\d\d\d+)";

$EMPTY_TABLE_CELL = $HTMLPopUp::EMPTY_TABLE_CELL;


# Print out the Database in a visually useful form so that I can
# debug timing problems.  This is not called by any code. I use this
# in the debugger.

sub debug_database {
  my ($self, $tree) = (@_);

  # sort numerically descending
  my (@times) = sort {$b <=> $a} keys %{ $DATABASE{$tree} };


  my $last_treestate = '';
  foreach $time (@times) {

      if (defined($DATABASE{$tree}{$time}{'treestate'})) {
          $localtime = localtime($time);
          $tree_state = $DATABASE{$tree}{$time}{'treestate'};

#          ($tree_state eq $last_treestate) && next;
          print "$localtime:\t $tree_state \n";
          $last_treestate = $tree_state;
      }

      if ( defined($DATABASE{$tree}{$time}{'author'}) ) {
          $localtime = localtime($time);
          @authors = sort (keys %{ $DATABASE{$tree}{$time}{'author'} });
          print "$localtime:\t\t @authors \n";
      }

  }

  return ;
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
    $self->trim_db_history($tree);
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

  foreach $state (TreeData::get_all_sorted_tree_states()) {
    my ($cell_color) = TreeData::TreeState2color($state);
    my ($char) = TreeData::TreeState2char($state);
    my ($description) = TreeData::TreeStates2descriptions($state);
    my $description = "$state: $description";
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


sub status_table_header {
  return ("\t<th>$VC_NAME</th>\n");
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
            $last_treestate = $DATABASE{$tree}{$time}{'treestate'};
        }

        # Inside each cell, we want all the posts by the same author to
        # appear together.  The previous data structure allowed us to find
        # out what data was in each cell.
        
        my $recs = $DATABASE{$tree}{$time}{'author'};
        foreach $author (keys %{ $recs }) {
            foreach $file (keys %{ $recs->{$author} }) {
                my ($log) = $recs->{$author}{$file};
                $authors{$author}{$time}{$file} = $log;
            }
        }
    } # while (1)

    return ($db_index, $last_treestate, \%authors);
}



# Produce the HTML which goes with the already computed author data.

sub render_authors {
    my ($tree, $last_treestate, $authors, $mindate, $maxdate,) = @_;

    # Now invert the data structure.
    my %authors = %{$authors};
    
    my ($cell_color) = TreeData::TreeState2color($last_treestate);
    my ($char) = TreeData::TreeState2char($last_treestate);
    
    my $cell_options;
    my $text_browser_color_string;
    
    if ( ($last_treestate) && ($cell_color) ) {
        $cell_options = "bgcolor=$cell_color ";
        
        $text_browser_color_string = 
          HTMLPopUp::text_browser_color_string($cell_color, $char);
    }
    
    my $query_links = '';
    $query_links.=  "\t\t".$text_browser_color_string."\n";
    
    if ( scalar(%authors) ) {
        
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
            
            my @bug_numbers;
            # sort numerically descending
            foreach $time ( sort {$b <=> $a} keys %{ $authors{$author} }) {
                foreach $file (keys %{ $authors{$author}{$time}}) {
                    $num_rows++;
                    my ($log) = $authors{$author}{$time}{$file};
                    if ($log =~ m/$VC_BUGNUM_REGEXP/) {
                        push @bug_numbers, $1;
                    }
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

            # This is a Netscape.com/Mozilla.org specific CVS/Bonsai
            # issue. Most users do not have '%' in their CVS names. Do
            # not display the full mail address in the status column,
            # it takes up too much space.
            # Keep only the user name.

            my $display_author=$author;
            $display_author =~ s/\%.*//;
            
            # we display the list of names in 'teletype font' so that the
            # names do not bunch together. It seems to make a difference if
            # there is a <cr> between each link or not, but it does make a
            # difference if we close the <tt> for each author or only for
            # the group of links.
            
            my (%popup_args) = (
                                
                                "windowtxt" => $table,
                                "windowtitle" => ("$VC_NAME Info ".
                                                  "Author: $author ".
                                                  "$time_interval_str "),
                                
                                "windowheight" => ($num_rows * 50) + 100,
                                "windowwidth" => ($max_length * 10) + 100,
                                );
            
            my $mailto_author=$author;
            $mailto_author =~ s/\%/\@/;
            
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
                                  "href" => "mailto: $mailto_author",
                                  "linktxt" => "\t\t<tt>$display_author</tt>",
                                  
                                  %popup_args,
                                  );
            } else {
                
                $query_link .= 
                  VCDisplay::query(
                                   'tree' => $tree,
                                   'mindate' => $mindate,
                                   'maxdate' => $maxdate,
                                   'who' => $author,
                                   
                                   "linktxt" => "\t\t<tt>$display_author</tt>",
                                   %popup_args,
                                   );
            }
            
            @bug_numbers = main::uniq(@bug_numbers);
            foreach $bug_number (@bug_numbers) {
                my $href = BTData::bug_id2bug_url($bug_number);
                $query_link .= 
                  HTMLPopUp::Link(
                                  "href" => $href,
                                  "linktxt" => "\t\t<tt>$bug_number</tt>",
                                  
                                  %popup_args,
                                  );
            }
            
            # put each link on its own line and add good comments so we
            # can debug the HTML.
            
            my ($date_str) = localtime($mindate)."-".localtime($maxdate);
            
            $query_links .= (
                             "\t\t<!-- VC_Bonsai: ".
                             ("Author: $author, ".
                              "Bug_Numbers: '@bug_numbers', ".
                              "Time: '$date_str', ".
                              "Tree: $tree, ".
                              "").
                             "  -->\n".
                             "");
            
            $query_links .= "\t\t".$query_link."\n";
            
        } # foreach %author
    } # if %authors

    $query_links.=  "\t\t".$text_browser_color_string."\n";
    
    @outrow = (
               "\t<!-- VC_Bonsai: authors -->\n".               
               "\t<td align=center $cell_options>\n".
               $query_links.
               "\t</td>\n".
               "");
    
    return @outrow;
}


# Create one cell (possibly taking up many rows) which will show
# that no authors have checked in during this time.

sub render_empty_cell {
    my ($last_treestate, $rowspan) = @_;

    my ($cell_color) = TreeData::TreeState2color($last_treestate);
    my ($char) = TreeData::TreeState2char($last_treestate);
    
    my $cell_options;
    my $text_browser_color_string;
    
    if ( ($last_treestate) && ($cell_color) ) {
        $cell_options = "bgcolor=$cell_color ";
        
        $text_browser_color_string = 
          HTMLPopUp::text_browser_color_string($cell_color, $char) ;
    }
    
    my $cell_contents =  $text_browser_color_string || $EMPTY_TABLE_CELL;

    return ("\t<!-- VC_Bonsai: empty data. ".
            "tree: $tree, ".
            "-->\n".
            
            "\t\t<td align=center rowspan=$rowspan $cell_options>".
            "$cell_contents</td>\n");
}



# clear data structures in preparation for printing a new table

sub status_table_start {
  my ($self, $row_times, $tree, ) = @_;
  
  # create an ordered list of all times which any data is stored
  
  # sort numerically descending
  @DB_TIMES = sort {$b <=> $a} keys %{ $DATABASE{$tree} };

  # NEXT_DB is my index into the list of all times.


  # adjust the $NEXT_DB to skip data which came after the first cell
  # at the top of the page.  We make the first cell bigger then the
  # rest to allow for some overlap between pages.

  my ($first_cell_seconds) = 2*($row_times->[0] - $row_times->[1]);
  my ($earliest_data) = $row_times->[0] + $first_cell_seconds;

  $NEXT_DB{$tree} = 0;
  while ( ($DB_TIMES[$NEXT_DB{$tree}] > $earliest_data) &&    
          ($NEXT_DB{$tree} < $#DB_TIMES) ) {
    $NEXT_DB{$tree}++
  }

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
      
      push @outrow, ("\t<!-- VC_Bonsai: skipping. ".
                     "tree: $tree, ".
                     "additional_skips: ".
                     ($NEXT_ROW{$tree} -  $row_index).", ".
                     "previous_end: ".localtime($current_rec->{'timenow'}).", ".
                     " -->\n");
   return @outrow;
  }

  
  # find all the authors who changed code at any point in this cell
  # find the tree state for this cell.

  my ($db_index, $last_treestate, $authors) =
      cell_data($tree, $NEXT_DB{$tree}, $row_times->[$row_index]);
  
  $LAST_TREESTATE{$tree} = $last_treestate || $LAST_TREESTATE{$tree};
  $last_treestate = $LAST_TREESTATE{$tree};
  
  if (%{$authors}) {

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
  } else {

      # Create a multi-row dummy cell for missing data.
      # Cell stops if there is author data in the following cell or the
      # treestate changes.
      
      my $rowspan = 0;
      my ($next_db_index, $next_treestate, $next_authors);
      $next_db_index = $db_index;

      while (
             !(%{$next_authors}) &&
             (
              !defined($next_treestate) ||
              ($last_treestate eq $next_treestate)
              )
             ) {

          $db_index = $next_db_index;
          $rowspan++ ;

          ($next_db_index, $next_treestate, $next_authors) =
              cell_data($tree, $db_index, $row_times->[$row_index+$rowspan]);
          
      }

      $NEXT_ROW{$tree} = $row_index + $rowspan;
      $NEXT_DB{$tree} = $db_index;

      my @html= render_empty_cell($LAST_TREESTATE{$tree}, $rowspan);
      return @html;
  }

  # not reached
}


1;


