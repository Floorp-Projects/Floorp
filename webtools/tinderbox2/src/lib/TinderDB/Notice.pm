# -*- Mode: perl; indent-tabs-mode: nil -*-
#

# Notice.pm - this module processes the notice updates posted by
# addnotice.cgi and create the status page column which displays the
# notices.


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

# $Revision: 1.28 $ 
# $Date: 2003/08/17 01:37:52 $ 
# $Author: kestes%walrus.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/lib/TinderDB/Notice.pm,v $ 
# $Name:  $ 




# We want to allow users to embed links and other HTML into their
# comments BUT we must filter user input to prevent this:

# http://www.ciac.org/ciac/bulletins/k-021.shtml

#   When a victim with scripts enabled in their browser reads this
#   message, the malicious code may be executed
#   unexpectedly. Scripting tags that can be embedded in this way
#   include <SCRIPT>, <OBJECT>, <APPLET>, and <EMBED>.

# note that since we want some tags to be allowed (href) but not
# others.  This requirement breaks the taint perl mechanisms for
# checking as we can not escape every '<>'.

# the data is stored as:
#  $DATABASE{$tree}{$time}{$author} = $message;
#  $METADATA{$tree}{'updates_since_trim'}+= 1

# where each $message has the required fields:

#	$record->{'time'} # the time the notice was posted in time() format
#	$record->{'mailaddr'} # the mail address of the user who posed the notice

# currently addnote.cgi saves additional debug information in these not
# used fields:

#	$record->{'tree'}  # the name of the tree this notice appiles to
#	$record->{'notice'} # the raw text of the notice
#	$record->{'localtime'} # the time as a user viewable string


package TinderDB::Notice;

# Load standard perl libraries
use File::Basename;


# Load Tinderbox libraries

use lib '#tinder_libdir#';

use Utils;
use VCDisplay;
use HTMLPopUp;
use TinderDB::BasicTxtDB;

$VERSION = ( qw $Revision: 1.28 $ )[1];

@ISA = qw(TinderDB::BasicTxtDB);


# The indicator that a notice is available for a given notice cell is
# configurable.  Traditionally it is a star gif however if you wish to
# run entirely without images I suggest you set it to "X".
# This is used in TinderDB::Notice.pm

$NOTICE_AVAILABLE = $TinderConfig::NOTICE_AVAILABLE || "X";


sub status_table_legend {
  my ($out)='';

  # print out what the cell will look like if there 'is' / 'is not' a
  # notice at this time.

  $out .= <<EOF;
        <td align=right valign=top>
	<table $TinderDB::LEGEND_BORDER>
		<thead><tr>
			<td align=center>Notes</td>
		</tr></thead>
		<tr>
			<td>Note posted: </td>
			<td align=center>$NOTICE_AVAILABLE</td>
		</tr>
		<tr>
			<td>No Note posted: </td>
			<td align=center>$HTMLPopUp::EMPTY_TABLE_CELL</td>
		</tr>
	</table>
        </td>
EOF

  $out .= "\t\n";

  return ($out);  
}


sub status_table_header {
  return ("\t<th>Notes</th>\n");
}



sub apply_db_updates {
  my ($self, $tree,) = @_;
  
  my ($filename) = $self->update_file($tree);
  my $dirname= File::Basename::dirname($filename);
  my $prefixname = File::Basename::basename($filename);
  my (@sorted_files) = $self->readdir_file_prefix( $dirname, 
                                                   $prefixname);

  scalar(@sorted_files) || return 0;

  foreach $update_file (@sorted_files) {

    # This require will set a variable called $record with all
    # the info from this build update.

    my ($record) = Persistence::load_structure("$update_file");

    ($record) ||
      die("Error reading Notice update file '$update_file'.\n");

    my $time = $record->{'time'};
    my $mailaddr = $record->{'mailaddr'};

    $DATABASE{$tree}{$time}{$mailaddr} = $record;

  }

  scalar(@sorted_files) || return 0;

  $METADATA{$tree}{'updates_since_trim'}+=   
    scalar(@sorted_files);

  if ( ($METADATA{$tree}{'updates_since_trim'} >
        $TinderDB::MAX_UPDATES_SINCE_TRIM)
     ) {
    $METADATA{$tree}{'updates_since_trim'}=0;
    trim_db_history(@_);
  }

  # be sure to save the updates to the database before we delete their
  # files.

  $self->savetree_db($tree);

  $self->unlink_files(@sorted_files);
  
  $all_rendered_notices = $self->get_all_rendered_notices($tree);
  my ($outfile) = (FileStructure::get_filename($tree, 'tree_HTML').
                   "/all_notices.html");
    
  main::overwrite_file($outfile, $all_rendered_notices);


  return scalar(@sorted_files);
}



# remove all records from the database which are older then last_time.

sub trim_db_history {
  my ($self, $tree, ) = (@_);

  my ($last_time) =  $main::TIME - $TinderDB::TRIM_SECONDS;
  # sort numerically ascending
  my (@times) =  sort {$a <=> $b} keys %{ $DATABASE{$tree} };

  foreach $time (@times){

    ($time >= $last_time) && last;

    delete $DATABASE{$tree}{$time};
  }

  return ;
}


# clear data structures in preparation for printing a new table

sub status_table_start {
  my ($self, $row_times, $tree, $association) = @_;

  # create an ordered list of all times which any data is stored

  # sort numerically descending
  @DB_TIMES = sort {$b <=> $a} keys %{ $DATABASE{$tree} };

  # We may be building a table of yesterdays data.  In this case we do
  # not want the first row to have all of todays data in it.

  # Adjust the $NEXT_DB to skip data which came after the first cell
  # at the top of the page.  We make the first cell bigger then the
  # rest to allow for some overlap between pages which start and stop
  # on the same time.

  my ($first_cell_seconds) = 2*($row_times->[0] - $row_times->[1]);
  my ($earliest_data) = $row_times->[0] + $first_cell_seconds;

  (defined($association)) ||
      ($association = '');

  $NEXT_DB{$tree}{$association} = 0;
  while ( ($DB_TIMES[$NEXT_DB{$tree}{$association}] > $earliest_data) &&    
          ($NEXT_DB{$tree}{$association} < $#DB_TIMES) ) {
    $NEXT_DB{$tree}{$association}++
  }

  return ;  
}



sub cell_data {
    my ($tree, $db_index, $last_time, $association) = @_;
    
    my %authors = ();
    
    my $first_notice_time;
    
    while (1) {
        my ($time) = $DB_TIMES[$db_index];

        # find the DB entries which are needed for this cell
        ($time < $last_time) && last;
        ($db_index >= $#DB_TIMES) && last;
        
        $db_index++;

        foreach $author (keys %{ $DATABASE{$tree}{$time} }) {
            my $rec = $DATABASE{$tree}{$time}{$author};
            if (
                (defined($association)) &&
                (!(defined($rec->{'associations'}->{$association})))
                ) {
                next;
            }
            $authors{$author} = $rec;
            if (!($first_notice_time)) {
                $first_notice_time = $time;
            }
        }
        
    } # while (1)
    
    
    return ($db_index, $first_notice_time, \%authors);
}


# Given a notice return a formatted HTML representation of the text.

sub render_notice {
    my ($notice) = @_;
    
    my $remote_host = $notice->{'remote_host'};
    my $time = $notice->{'time'};
    my $note = $notice->{'note'};
    my $mailaddr = $notice->{'mailaddr'};
    
    my ($pretty_time) = HTMLPopUp::timeHTML($time);
    my $mailto = "mailto:$mailaddr?subject=Tinderbox Notice";
    my $hidden_info = (
                       "\t\t\t<!-- ".
                       "posted from remote host: $remote_host".
                       " at time: $notice->{'localposttime'} ".
                       "-->\n".
                       "");

    # If the user requests for us to lie about the time, we should
    # also tell when the note was really posted.

    my $localpostedtime = '';
    if ( 
         ($notice->{'posttime'} - $notice->{'time'}) > 
         $main::SECONDS_PER_HOUR
         ) {
        $localpostedtime = (
                            "(Actually posted at: ". 
                            $notice->{'localposttime'}.
                            ")<br>\n".
                            "");
    }
    
    my $rendered_association = '';
    my $assocations_ref = $notice->{'associations'};
    if ($assocations_ref) {
        foreach $association (sort keys %{ $assocations_ref }) {
            $rendered_association .= (
                                      "\t\t\t<tt>-".
                                      $association.
                                      "-</tt><br>\n"
                                      );
         }
     }

     my ($rendered_notice) = (
                              "\t\t<br>\n".
                              "\t\t\t[<b>".
                              HTMLPopUp::Link(
                                              "linktxt"=>$mailaddr,
                                              "href"=>$mailto,
                                              "name"=>"$time\.$mailaddr",
                                              ).
                            " - ".
                            HTMLPopUp::Link(
                                            "linktxt"=>$pretty_time,
                                            "href"=>"\#$time",
                                            ).
                            "</b>]<br>\n".
                              $localpostedtime.
                              $rendered_association.
                              $hidden_info.
                              "\t\t<p>\n".
                              "$note\n".
                              "\t\t</p>\n"
                              );

    return $rendered_notice;
 }


 # return all the notices so we can write to the disk this data. Those
 # users with text browsers can view this data without popups.

 sub get_all_rendered_notices {
     my ($self, $tree, ) = @_;

     my $rendered_notices;

     my @db_times = sort {$b <=> $a} keys %{ $DATABASE{$tree} };

     foreach $time (@db_times) {    

         my ($pretty_time) = HTMLPopUp::timeHTML($time);

         my $checkin_links;
         $checkin_links .= VCDisplay::query(
                                            'tree' => $tree,
                                            'mindate' => $time,
                                            'linktxt' => "Checkins since $pretty_time",
                                            );
         $checkin_links .= "<br>\n";
         $checkin_links .= VCDisplay::query(
                                            'tree' => $tree,
                                            'maxdate' => $time,
                                            'mindate' => $time-$main::SECONDS_PER_HOUR,
                                            'linktxt' => "Checkins before $pretty_time",
                                            );
         $checkin_links .= "<br>\n";

         # allow us to reference individual notices in the file
         $rendered_notices .= (
                               "\n\n".
                               HTMLPopUp::Link(
                                               "name"=>$time,
                                               "href"=>"\#$time",
                                               ).
                               "<!-- $pretty_time -->".
                               "\n".
                               "");

         foreach $author (keys %{ $DATABASE{$tree}{$time} }) {
             my $new_notice = 
                 render_notice($DATABASE{$tree}{$time}{$author});

             $rendered_notices .= "<p>".$new_notice."</p>";
         } # foreach $author

     } # foreach $time


     return $rendered_notices;
 }

 # Given a data structure which represents all applicable notices for a
 # cell render the popup and graphic image which is appropriate. This
 # is used by the code which renders the whole cell in <td>'s.


 sub rendered_cell_contents {
     my ($tree, $authors, $first_notice_time, ) = @_;

     my $rendered_notices;
     my $num_notices =0;
     foreach $author (sort keys %{ $authors }) {
         my $new_notice = 
             render_notice($authors->{$author});

         $rendered_notices .= "<p>".$new_notice."</p>";
         $num_notices++
     } # foreach $author


     ($rendered_notices) ||
         return ;

     # create a url to a cgi script so that those who do not use pop up
     # menus can view the notice.

     my $href = (FileStructure::get_filename($tree, 'tree_URL').
                 "/all_notices.html\#$first_notice_time");
    
     my $index_link =  (
                        "\t\t".
                        HTMLPopUp::Link(
                                        "linktxt" => "All Notices for this tree",
                                        "href" => $href,
                                        ).
                        "<br>\n".
                        "");

    # the popup window software is pretty sensitive to newlines and
    # terminating quotes.  Take those out of the message.

    my $title = (
                 "Notice Board: ".
                 HTMLPopUp::timeHTML($first_notice_time)
                 );

    $out =  (
                "\t\t".
                HTMLPopUp::Link(
                                
                                # If notice available is an image then
                                # we need the spaces to make the popup
                                # on mouse over work.
                                
                                "linktxt" => " $NOTICE_AVAILABLE ",
                                "href" => $href,
                                "windowtxt" => $rendered_notices.$index_link,
                                "windowtitle" => $title,
                                "windowheight" => (175 * $num_notices)+100,
                                ).
                "\n".
                "");

    return $out;
}

# Create one cell (possibly taking up many rows) which will show
# that no authors have checked in during this time.

sub render_empty_cell {
    my ($tree, $till_time, $rowspan) = @_;

    my $local_till_time = localtime($till_time);
    my $out;
    $out .= ("\t<!-- Notice: empty data. ".
             "tree: $tree, ".
             "filling till: '$local_till_time', ".
             "-->\n");
    $out .=(
            "\t\t<td align=center rowspan=$rowspan>".
            "$HTMLPopUp::EMPTY_TABLE_CELL</td>\n"
            );

    return $out;
}


sub Notice_Link {
    my ($self, $last_time, $tree, $association) = @_;

    my $start_time = $NEXT_DB{$tree}{$association};
    (defined($start_time)) || 
        ($start_time = 0);

    my ($db_index, $first_notice_time, $authors) =
        cell_data($tree, 
                  $start_time,
                  $last_time, 
                  $association);

    $NEXT_DB{$tree}{$association} = $db_index;

    my $html = rendered_cell_contents($tree, 
                                      $authors, $first_notice_time);

    return $html;
}


sub status_table_row {
    my ($self, $row_times, $row_index, $tree,) = @_;
    
    my @html;

    # skip this column because it is part of a multi-row missing data
    # cell?

    $association = '';
    if ( 
         (defined($NEXT_ROW{$tree}{$association})) &&
         ( $NEXT_ROW{$tree}{$association} != $row_index )
         ) {
        
        push @html, ("\t<!-- Notice: skipping. ".
                     "tree: $tree, ".
                     "additional_skips: ".
                     ($NEXT_ROW{$tree}{$association} - $row_index).", ".
                     " -->\n");
        return @html;
    }

    my $last_time = $row_times->[$row_index];

    my ($db_index, $first_notice_time, $authors) =
        cell_data($tree, 
                  $NEXT_DB{$tree}{$association},
                  $last_time,
                  '');
    
    # If there is data for this cell render it and return.

    if (scalar(%{$authors})) {
        
        $NEXT_DB{$tree}{$association} = $db_index;
        $NEXT_ROW{$tree}{$association} = $row_index + 1;
        @html = rendered_cell_contents(
                                       $tree, 
                                       $authors, $first_notice_time);

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
           ($row_index+$rowspan <  $#{ $row_times }) &&

           (!(scalar(%{$authors}))) 
           ) {
        
        $db_index = $next_db_index;
        $rowspan++ ;
        my $last_time = $row_times->[$row_index+$rowspan];

        ($next_db_index, $first_notice_time, $authors) =
            cell_data($tree, 
                      $db_index, 
                      $last_time, 
                      '');
    }
    
    $NEXT_ROW{$tree}{$association} = $row_index + $rowspan;
    $NEXT_DB{$tree}{$association} = $db_index;
    
    @html= render_empty_cell($tree, 
                             $row_times->[$row_index+$rowspan], 
                             $rowspan);
    
    return @html;
}




1;
