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

# complete rewrite by Ken Estes for contact info see the
#     mozilla/webtools/tinderbox2/Contact file.
# Contributor(s): 


# $Revision: 1.68 $ 
# $Date: 2003/08/17 01:37:53 $ 
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

#  If the log message contains something which looks like a bug
#  number, then we store that string in a separate data structure.

#   $DATABASE{$tree}{$time}{'bugs'}{$bug_number} = 1;

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

use BTData;
use TreeData;
use TinderHeader;
use BonsaiData;
use TinderDB::BasicTxtDB;
use TinderDB::Notice;
use Utils;
use HTMLPopUp;
use VCDisplay;


$VERSION = ( qw $Revision: 1.68 $ )[1];

@ISA = qw(TinderDB::BasicTxtDB);

# name of the version control system
$VC_NAME = $TinderConfig::VC_NAME || "Bonsai";

# how we recoginise bug number in the checkin comments.
$VC_BUGNUM_REGEXP = $TinderConfig::VC_BUGNUM_REGEXP ||
    '(\d\d\d+)';

# We 'have a' notice so that we can put stars in our column.

$NOTICE= TinderDB::Notice->new();
$DEBUG = 1;

# return a string of the whole Database in a visually useful form.

sub get_all_bonsai_data {
  my ($self, $tree) = (@_);

  my $treestate = TinderHeader::gettree_header('TreeState', $tree);

  # sort numerically descending
  my (@times) = sort {$b <=> $a} keys %{ $DATABASE{$tree} };

  my $out;
  $out .= "<HTML>\n";
  $out .= "<HEAD>\n";
  $out .= "\t<TITLE>Bonasi Checkin Data as gathered by Tinderbox</TITLE>\n";
  $out .= "</HEAD>\n";
  $out .= "<BODY>\n";
  $out .= "<H3>Bonasi Checkin Data as gathered by Tinderbox</H3>\n";
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
                  my ($log) = $recs->{$author}{$file};
                  $out .= "\t\t<TD>$file</TD>\n";
                  $out .= "\t\t<TD>$log</TD>\n";
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
  $DATABASE{$tree}{$main::TIME}{'treestate'} = $new_tree_state;

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
      
      if ($log =~ m/$VC_BUGNUM_REGEXP/) {
          my $bug_number = $1;
          $DATABASE{$tree}{$time}{'bugs'}{$bug_number} = 1;
      }
      
  } # foreach update

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

  my $all_vc_data = $self->get_all_bonsai_data($tree);
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


# Return the data which would appear in a cell if we were to render
# it.  The checkin data comes back in a datastructure and we return
# the oldest treestate found in this cell.  The reason for another
# data structure is that this one is keyed on authors while the main
# data structure is keyed on time.

sub cell_data {
    my ($tree, $db_index, $last_time) = @_;

    my (%authors) = ();
    my (%bugs) = ();
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
        
        if (defined( $DATABASE{$tree}{$time}{'author'} )) {
            my $recs = $DATABASE{$tree}{$time}{'author'};
            foreach $author (keys %{ $recs }) {
                foreach $file (keys %{ $recs->{$author} }) {
                    my ($log) = $recs->{$author}{$file};
                    $authors{$author}{$time}{$file} = $log;
                }
            }
        }
        
        if (defined( $DATABASE{$tree}{$time}{'bugs'} )) {
            $recs = $DATABASE{$tree}{$time}{'bugs'};
            foreach $bug (keys %{ $recs }) {
                $bugs{$bug} =1;
            }
        }

    } # while (1)

    return ($db_index, $last_treestate, \%authors, \%bugs,);
}



# Produce the HTML which goes with the already computed author data.

sub render_authors {
    my ($tree, $last_treestate, $authors, $mindate, $maxdate,) = @_;

    # Now invert the data structure.
    my %authors = %{$authors};
    
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
            
            # This is a Netscape.com/Mozilla.org specific CVS/Bonsai
            # issue. Most users do not have '%' in their CVS names. Do
            # not display the full mail address in the status column,
            # it takes up too much space.
            # Keep only the user name.

            my $display_author=$author;
            $display_author =~ s/\%.*//;
            
            my $mailto_author=$author;
            $mailto_author = TreeData::VCName2MailAddress($author);
            

            my (@times) = sort {$b <=> $a} keys %{ $authors{$author} };   
            my $checkin_page_reference = $times[0];
            my @bug_numbers;
            # sort numerically descending
            foreach $time (@times) {
                foreach $file (keys %{ $authors{$author}{$time}}) {
                    my ($log) = $authors{$author}{$time}{$file};
                    if ($log =~ m/$VC_BUGNUM_REGEXP/) {
                        push @bug_numbers, $1;
                    }
                }
            }
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
                                 "\t\t<!-- VC_Bonsai: ".
                                 ("Author: $author, ".
                                  "Bug_Numbers: '@bug_numbers', ".
                                  "Time: '$date_str', ".
                                  "Tree: $tree, ".
                                  "").
                                 "  -->\n".
                                 "");
            }

            $query_links .= "\t\t".$query_link."\n";
            
        } # foreach %author
    } # if %authors

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
               "\t<!-- VC_Bonsai: authors -->\n".               
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
        push @outrow, ("\t<!-- VC_Bonsai: empty data. ".
                       "tree: $tree, ".
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
  # at the top of the page.  We make the first cell bigger then the
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
          push @outrow, ("\t<!-- VC_Bonsai: skipping. ".
                         "tree: $tree, ".
                         "additional_skips: ".
                         ($NEXT_ROW{$tree} -  $row_index).", ".
                         " -->\n");
      }
      return @outrow;
  }

  
  # Find all the authors who changed code at any point in this cell
  # Find the tree state for this cell.

  my ($db_index, $last_treestate, $authors, $bugs,) =
      cell_data($tree, $NEXT_DB{$tree}, $row_times->[$row_index]);

  # Track the treestate between calls with a global variable.

  # This initalization may not be correct.  If we are creating a page
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
         ($row_index+$rowspan < $#{ $row_times }) &&

         ( (!($next_authors)) || (!(scalar(%{$next_authors}))) ) &&

         (
          !defined($next_treestate) ||
          ( $LAST_TREESTATE{$tree} eq $next_treestate)
          )


         ) {
      
      $db_index = $next_db_index;
      $rowspan++ ;
      
      ($next_db_index, $next_treestate, $next_authors, $next_bugs) =
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


1;


