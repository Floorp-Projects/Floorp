# -*- Mode: perl; indent-tabs-mode: nil -*-
#

# TinderDB::BT_GENERIC - A generic method to display the changes to
# the bug tracking system.  I am hoping that we will not need to have
# one specialized method per bug ticket system but can instead use
# this method for all commonly used (Bug Tracking, Modification
# Request) systems.  This module will display bugs which have been
# 'closed' (developers close bugs when their latest version control
# check in fixes them) or 'reopened' (since that means that
# developers closed the bugs incorrectly).  BTData.pm contains all the
# local settings which configure this module.


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


# $Revision: 1.24 $ 
# $Date: 2003/08/17 01:37:52 $ 
# $Author: kestes%walrus.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/lib/TinderDB/BT_Generic.pm,v $ 
# $Name:  $ 



package TinderDB::BT_Generic;

#   We store the hash of all names who modified the tree at a
#   particular time as follows:

#   $DATABASE{$tree}{$timenow}{$status}{$bug_id} = $record;

# Where $rec is an anonymous hash of name value pairs from the bug
# tracking system.

# we also store information in the metadata structure

#   $METADATA{$tree}{'updates_since_trim'} += 1;
#


# Load standard perl libraries
use File::Basename;
use Time::Local;

# Load Tinderbox libraries

use lib '#tinder_libdir#';

use TinderDB::BasicTxtDB;
use BTData;
use Utils;
use HTMLPopUp;
use TreeData;
use VCDisplay;
use TinderDB::Notice;


$VERSION = ( qw $Revision: 1.24 $ )[1];

@ISA = qw(TinderDB::BasicTxtDB);


# name of the bug tracking system
$BT_NAME = $TinderConfig::BT_NAME || "BT";

# We 'have a' notice so that we can put stars in our column.
 
$NOTICE= TinderDB::Notice->new();

# get the recent data from where the MDA puts the parsed data.


sub apply_db_updates {
  my ($self, $tree, ) = @_;
  
  my ($filename) = $self->update_file($tree);
  my ($dirname)= File::Basename::dirname($filename);
  my ($prefixname) = File::Basename::basename($filename);
  my (@sorted_files) = $self->readdir_file_prefix( $dirname, 
                                                   $prefixname);

  scalar(@sorted_files) || return 0;

  foreach $file (@sorted_files) {
    my ($record) = Persistence::load_structure($file);

    ($record) ||
      die("Error reading Bug Tracking update file '$file'.\n");

    my($timenow) =  $record->{'tinderbox_timenow'};
    
    my($status) = $record->{'tinderbox_status'};
    my($bug_id) = $record->{'tinderbox_bug_id'};
    
    # sanity check the record, taint checks are done in processmail.
    {
      ($tree eq $record->{'tinderbox_tree'}) ||
        die("Error in updatefile: $file, ".
            "Tree: $tree, not equal to Tree: $record->{'tree'}.");

      (main::is_time_valid($timenow)) ||
        die("Error in updatefile: $file, ".
            "timenow: $timenow, is not a valid time.");
   }  


    # Add the record to the datastructure

    # Notice: We store the raw $status and do not use
    # BTData::STATUS_PROGRESS to convert it to our display format:
    # $progress.  This is so that the progress mapping can change
    # later and not invalidate old data.

    $DATABASE{$tree}{$timenow}{$status}{$bug_id} = $record;

  } # $update_file 

  $METADATA{$tree}{'updates_since_trim'}+=   
    scalar(@sorted_files);

  if ( ($METADATA{$tree}{'updates_since_trim'} >
        $TinderDB::MAX_UPDATES_SINCE_TRIM)
     ) {
    $METADATA{$tree}{'updates_since_trim'}=0;
    $self->trim_db_history(@_);
  }

  # Be sure to save the updates to the database before we delete their
  # files.

  $self->savetree_db($tree);

  $self->unlink_files(@sorted_files);
  
  return scalar(@sorted_files);
}


sub status_table_legend {
  my ($out)='';

  # I am not sure the best way to explain this to our users

  return ($out);  
}



sub status_table_header {
  my $out = '';

  my (@columns) = BTData::get_all_columns();

  foreach $column (@columns) {  
    $out .= "\t<th>$BT_NAME $column</th>\n";
  }

  return ($out);
}

# where can people attach notices to?
# Really this is the names of the columns produced by this DB

sub notice_association {
    my ($self, $tree, ) = @_;
    my (@columns) = BTData::get_all_columns();
    return @columns;
}


sub render_bug {
    my ($rec) = @_;
    
    my $tree = $rec->{'tinderbox_tree'};
    my $status = $rec->{'tinderbox_status'};
    my $column = $BTData::STATUS_PROGRESS{$status};


    my ($table) = '';
    my ($num_rows) = 0;
    my ($max_length) = 0;
    
    # display all the interesting fields
    
    foreach $field (@BTData::DISPLAY_FIELDS) {
        
        my ($value) = $rec->{$field};
        
        # many fields tend to be empty because it diffs the bug
        # reports and only reports the lines which change and a few
        # lines of context.
        
        ($value) || 
            next;
        
        # $max_length = main::max($max_length , length($value));
        $num_rows++;
        $table .= (
                   "\t".
                   "<tt>$field</tt>".
                   ": ".
                   $value.
                   "<br>\n".
                   "");
    } # foreach $field 
    
    ($table) ||
        return ;

    $table = (
              "Ticket updated at: ".
              localtime($time).
              "<br>\n".
              $table.
              "");
    
    # a link to the cgibin page which displays the bug
    
    my ($bug_href) = BTData::rec2bug_url($rec);

    my $link_choices;
    $link_choices .= "<br>";
    $link_choices .= 
      HTMLPopUp::Link(
                      "href" => $bug_href,
                      "linktxt" => "Visit ticket: $bug_id",
                      );
    
    $link_choices .= "<br>";

    # fix the size so that long summaries do not cause our window
    # to get too large.
    
    $max_length = 40;
    
    my ($window_title) = "$BT_NAME Info bug_id: $bug_id";
    
    # we display the list of names in 'teletype font' so that the
    # names do not bunch together. It seems to make a difference if
    # there is a <cr> between each link or not, but it does make a
    # difference if we close the <tt> for each author or only for
    # the group of links.
    my ($query_link) = 
      HTMLPopUp::Link(
                      "linktxt" => "<tt>$bug_id</tt>",
                      "href" => $bug_href,
                      
                      "windowtxt" => $table.$link_choices,
                      "windowtitle" => $window_title,
                      "windowheight" => ($num_rows * 25) + 100,
                      "windowwidth" => ($max_length * 15) + 100,
                      );
    
    # put each link on its own line and add good comments so we
    # can debug the HTML.
    
    my $out = (
               "\t\t<!-- BT: ".("bug_id: $bug_id, ".
                                "Time: '".localtime($time)."', ".
                                "Column: $column, ".
                                "Status: $status, ".
                                "Tree: $tree, ".
                                "").
               "  -->\n".
               "\t\t".$query_link."\n".
               "");
    

    return $out;
}


# Return all the bug_ids which changed at any point between the two
# row times which represent this cell.  Further the data must be in
# the column we are currently rendering, that means that the status
# must map into the correct column.

sub cell_data {
    my ($tree, $db_index, $last_time, $column) = @_;
    
    
    my (%bug_ids) = ();
    
    while (1) {
        my ($time) = $DB_TIMES[$db_index];
        
        # find the DB entries which are needed for this cell
        ($time < $last_time) && last;
        ($db_index >= $#DB_TIMES) && last;
        
        $db_index++;
        foreach $status (keys %{ $DATABASE{$tree}{$time} }) {
  
          ($column eq $BTData::STATUS_PROGRESS{$status}) ||
                next;

            my ($out) = '';
            foreach $bug_id (sort keys %{ $DATABASE{$tree}{$time}{$status} }) {

                # keep most recent bug report
                ($bug_ids{$bug_id}) && next;

                my ($rec) = $DATABASE{$tree}{$time}{$status}{$bug_id};
                $bug_ids{$bug_id} = $rec;
                
            } # foreach $bug_id
            
        } # foreach $status
    } # while (1)

    return ($db_index, \%bug_ids);
}


# Create one cell (possibly taking up many rows) which will show
# that no authors have checked in during this time.

sub render_empty_cell {
    my ($tree, $till_time, $rowspan, $column) = @_;

    my $notice = $NOTICE->Notice_Link(
                                      $till_time,
                                      $tree,
                                      $column,
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
 
    my $local_till_time = localtime($till_time);
    return ("\t<!-- BT_Generic: empty data. ".
            "tree: $tree, column: $column, ".
            "filling till: '$local_till_time', ".
            "-->\n".
            
            "\t\t<td align=center rowspan=$rowspan>".
            "$cell_contents</td>\n");
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

  my (@columns) = BTData::get_all_columns();

  foreach $column (@columns) {  
      $NOTICE->status_table_start($row_times, $tree, $column);

      $NEXT_DB{$tree}{$column} = 0;
      while ( ($DB_TIMES[$NEXT_DB{$tree}{$column}] > $earliest_data) &&    
              ($NEXT_DB{$tree}{$column} < $#DB_TIMES) ) {
          $NEXT_DB{$tree}{$column}++;
      }
  }

  return ;  
}



sub render_one_column {
    my ($self, $row_times, $row_index, $tree, $column) = @_;
    my @html;

    # skip this column because it is part of a multi-row missing data
    # cell?
    
    if ( $NEXT_ROW{$tree}{$column} != $row_index ) {
        
        push @html, ("\t<!-- BT_Generic: skipping. ".
                     "tree: $tree, ".
                     "additional_skips: ".
                     ($NEXT_ROW{$tree}{$column} - $row_index).", ".
                     " -->\n");
        return @html;
    }
    
    my ($db_index,  $bug_ids) =
        cell_data($tree, 
                  $NEXT_DB{$tree}{$column}, 
                  $row_times->[$row_index], 
                  $column);
    
    if (scalar(%{$bug_ids})) {
        
        $NEXT_DB{$tree}{$column} = $db_index;
        $NEXT_ROW{$tree}{$column} = $row_index + 1;
        foreach $bug_id (sort keys %{$bug_ids}) {
            my $html = render_bug($bug_ids->{$bug_id});
            push @html, $html;
        }

        $notice .= (
                    "\t\t\t". 
                    $NOTICE->Notice_Link(
                                         $DB_TIMES[$NEXT_DB{$tree}{$column}],
                                         $tree,
                                         $column,
                                         ).
                    "\n".
                    "");    

        my $out = (
                   "\t<td align=center>\n".
                   "@html".
                   "\t</td>\n".
                   "");

        return $out;          
    }

    # Create a multi-row dummy cell for missing data.
    # Cell stops if there is data in the following cell.
    
    my $rowspan = 0;
    my ($next_db_index);
    $next_db_index = $db_index;
    
    while (
           ($row_index+$rowspan <=  $#{ $row_times }) &&

           (!(scalar(%{$bug_ids}))) 
           ) {
        
        $db_index = $next_db_index;
        $rowspan++ ;
        
          ($next_db_index, $bug_ids) =
              cell_data($tree, 
                        $db_index, 
                        $row_times->[$row_index+$rowspan], 
                        $column);
    }
    
    $NEXT_ROW{$tree}{$column} = $row_index + $rowspan;
    $NEXT_DB{$tree}{$column} = $db_index;
    
    @html= render_empty_cell($tree, 
                             $row_times->[$row_index+$rowspan], 
                             $rowspan, $column);

    return @html;
}


sub status_table_row {
    my ($self, $row_times, $row_index, $tree, ) = @_;
    
    my (@outrow) = ();
    
    my (@columns) = BTData::get_all_columns();
    foreach $column (@columns) {
        push @outrow, render_one_column(
                                        $self, $row_times, 
                                        $row_index, $tree, 
                                        $column);
    }

    return @outrow;
}


1;

