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

# complete rewrite by Ken Estes:
#	 kestes@staff.mail.com Old work.
#	 kestes@reefedge.com New work.
#	 kestes@walrus.com Home.
# Contributor(s): 




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
use HTMLPopUp;
use TinderDB::BasicTxtDB;

$VERSION = ( qw $Revision: 1.15 $ )[1];

@ISA = qw(TinderDB::BasicTxtDB);


# The indicator that a notice is available for a given notice cell is
# configurable.  Traditionally it is a star gif however if you wish to
# run entirely without images I suggest you set it to "X".
# This is used in TinderDB::Notice.pm

$NOTICE_AVAILABLE = "X";
#$NOTICE_AVAILABLE = "<img src='$FileStructure::IMAGES{star}' border=0>";


sub status_table_legend {
  my ($out)='';

  # print out what the cell will look like if there 'is' / 'is not' a
  # notice at this time.

  $out .= <<EOF;
        <td align=right valign=top>
	<table $TinderDB::LEGEND_BORDER>
		<thead><tr>
			<td align=center>Notices</td>
		</tr></thead>
		<tr>
			<td>Notice posted: </td>
			<td align=center>$NOTICE_AVAILABLE</td>
		</tr>
		<tr>
			<td>No Notice posted: </td>
			<td align=center>$HTMLPopUp::EMPTY_TABLE_CELL</td>
		</tr>
	</table>
        </td>
EOF

  $out .= "\t\n";

  return ($out);  
}


sub status_table_header {
  return ("\t<th>Notices</th>\n");
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
  my ($self, $row_times, $tree, ) = @_;

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

  $NEXT_DB = 0;
  while ( ($DB_TIMES[$NEXT_DB] > $earliest_data) &&    
          ($NEXT_DB < $#DB_TIMES) ) {
    $NEXT_DB++
  }

  return ;  
}


sub render_notice {
    my ($notice) = @_;
    
    my $remote_host = $notice->{'remote_host'};
    my $time = $notice->{'time'};
    my $note = $notice->{'note'};
    my $mailaddr = $notice->{'mailaddr'};
    
    my ($pretty_time) = HTMLPopUp::timeHTML($time);

    # If the user requests for us to lie about the time, we should
    # also tell when the note was really posted.

    my $localpostedtime;
    if ($notice->{'time'} != $notice->{'posttime'}) {
       $localpostedtime = (
                           "(Actually posted at: ". 
                           $notice->{'localpostedtime'}.
                           ")".
                           "");
    }

    my ($rendered_notice) = (
                             "\t\t<p>\n".
                             "\t\t\t[<b>".
                             HTMLPopUp::Link(
                                             "linktxt"=>$mailaddr,
                                             "href"=>"mailto:$mailaddr?subject=\"Tinderbox Notice\"",
                                             "name"=>"$time\.$mailaddr",
                                             ).
                           " - ".
                           HTMLPopUp::Link(
                                           "linktxt"=>$pretty_time,
                                           "href"=>"\#$time",
                                           ).
                           "</b>]".
                           $localpostedtime.
                           "\n".
                           "<!-- posted from remote host: $remote_host -->\n".
                           "\t\t</p>\n".
                           "\t\t<p>\n".
                           "$note\n".
                           "\t\t</p>\n"
                          );

    return $rendered_notice;
}

sub get_all_rendered_notices {
    my ($self, $tree, ) = @_;
    
    my $rendered_notices;
    
    my @db_times = sort {$b <=> $a} keys %{ $DATABASE{$tree} };

    foreach $time (@db_times) {    
        
        $localtime = localtime($time);
        $rendered_notices .= (
                              "\n\n".
                              HTMLPopUp::Link(
                                              "name"=>$time,
                                              "href"=>"\#$time",
                                              #"linktxt" => $localtime,
                                              ).
                              "<!-- $localtime -->".
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


sub status_table_row {
  my ($self, $row_times, $row_index, $tree, ) = @_;

  my @outrow = ();
  my @authors = ();

  # we assume that tree states only change rarely so there are very
  # few cells which have more then one state associated with them.
  # It does not matter what we do with those cells.

  my $rendered_notice = '';
  my $num_notices = 0;

  my ($first_notice_time) = $DB_TIMES[$NEXT_DB];

  while (1) {
    my ($time) = $DB_TIMES[$NEXT_DB];

    # find the DB entries which are needed for this cell
    ($time < $row_times->[$row_index]) && last;

    foreach $author (keys %{ $DATABASE{$tree}{$time} }) {
      my $new_notice = 
          render_notice($DATABASE{$tree}{$time}{$author});

      $rendered_notice .= "<p>".$new_notice."</p>";
      push @authors, $author;
      $num_notices++;
    }

    $NEXT_DB++;
    ($NEXT_DB > $#DB_TIMES) && last;
  }


  # create a url to a cgi script so that those who do not use pop up
  # menus can view the notice.

  my $href = (FileStructure::get_filename($tree, 'tree_URL').
              "/all_notices.html\#$first_notice_time");

  if ($rendered_notice) {

    # the popup window software is pretty sensitive to newlines and
    # terminating quotes.  Take those out of the message.


    @outrow =  (
                "\t<td>".
                HTMLPopUp::Link(
                                "linktxt" => $NOTICE_AVAILABLE,
                                "href" => $href,
                                "windowtxt" => $rendered_notice,
                                "windowtitle" => "Notice Board",
                                "windowheight" => (175 * $num_notices)+100,
                               ).
                "</td>");
  } else {
    @outrow = ("\t<!-- skipping: Notice: tree: $tree -->".
               "<td>$HTMLPopUp::EMPTY_TABLE_CELL</td>\n");

  }


  return @outrow; 
}



1;
