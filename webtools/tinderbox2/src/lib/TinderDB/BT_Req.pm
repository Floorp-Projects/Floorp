# -*- Mode: perl; indent-tabs-mode: nil -*-
#

# TinderDB::BT_Req - A modified version of BT_Generic to allow
# tinderbox to interface with the Req bug tracking system.
# (http://www.draga.com/~jwise/minireq/). Eventually the common code
# should be factored out into separate classes but I do not have time
# now and the files are only about 400 lines long.  The main
# difference is that Req requires us to parse a log file for each
# tree.  Thus we pull the data into our datastructures instead of
# having the mail system push it in.  

# The current design of these BT modules does not allow me to have two
# copies of the same module (ie two colums both of them BT but
# configured to use different bug tracking system). This is due to
# global variables in the module and the way we configure the system
# in TinderConfig and BTData.

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



# $Revision: 1.8 $ 
# $Date: 2003/08/17 01:37:52 $ 
# $Author: kestes%walrus.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/lib/TinderDB/BT_Req.pm,v $ 
# $Name:  $ 


package TinderDB::BT_Req;


#   We store the hash of all names who modified the tree at a
#   particular time as follows:

#   $DATABASE{$tree}{$timenow}{$status}{$bug_id} = $record;

# Where $rec is an anonymous hash of name value pairs from the bug
# tracking system.

# we also store information in the metadata structure

#   $METADATA{$tree}{'updates_since_trim'} += 1;
#

# Load standard perl libraries
use Time::Local;

# Load Tinderbox libraries

use lib '#tinder_libdir#';

use HTMLPopUp;
use MailProcess;
use TinderDB::BasicTxtDB;
use TreeData;
use ReqData;
use Utils;
use VCDisplay;



$VERSION = ( qw $Revision: 1.8 $ )[1];

@ISA = qw(TinderDB::BasicTxtDB);


# name of the bug tracking system
$REQ_NAME = $TinderConfig::REQ_NAME || "Req";

# Return the oldest time we have data for.
# This should not be older then the time we would keep if we 
# were to trim the database now.

sub find_last_data {
    my ($tree) = @_;

    my $oldest_allowed_time =  $main::TIME - $TinderDB::TRIM_SECONDS;

    my $last_tree_data = (sort keys %{ $DATABASE{$tree} })[0];

    ($last_tree_data < $oldest_allowed_time) &&
	($last_tree_data = $oldest_allowed_time);
	
    return $last_tree_data;
}


# untaint the variable and values parsed from trouble ticket mail.

sub clean_bug_input {
  my ($var, $value) = @_;

  # untaint data for safety

  $var = main::extract_printable_chars($var);

  # the input is HTML so it is a good idea to escape it
  
  $value = main::extract_html_chars($value);
  $value = HTMLPopUp::escapeHTML($value);

  # remove spaces at beginning and end of lines

  $value =~ s/\s+$//;
  $value =~ s/^\s+//;

  $var =~ s/\s+$//;
  $var =~ s/^\s+//;


  # There are spaces in some of the variable names,
  # convert these to underlines to make processing easier.

  $var =~ s/\s+/_/g;

  return ($var, $value);
}



# get the recent data from the Req log file.  There is one log file
# per tree.


sub apply_db_updates {
  my ($self, $tree, ) = @_;
  
  my $queue_name = ReqData::tree2queue($tree);
  my @req_logs =  ReqData::tree2logfiles($tree);
  my $added_lines = 0; 
  my $last_tree_data = find_last_data($tree);
  my $year = 1900 + (localtime(time()))[5];

  foreach $req_log (@req_logs) {

      # We may not have req running on every branch so the log files may
      # not exist.
      
      (-r $req_log) || 
          next;
      
      open (REQ_LOG, "<$req_log") ||
          die("Could not open req logfile: $req_log\n");
  
      foreach $line (<REQ_LOG>) {
          
          
          # the log file looks like this.
          
          # FW: CVS update: reef/distrib/sets (#2) created via mail by Rich@reefedge.com. 12:32:37, Nov 15
          # FW: CVS update: reef/distrib/sets (#2) given to rich by jim. 13:46:13, Nov 15
          
          
          if ( $line =~ m!
               ^(.+)			# the subject can contain spaces
               \s+\(\#(\d+)\)\s+	# bug number in parentheses
               (.*)		 	# the action taken, can contain spaces
               \s+by\s+			#
               ([A-Za-z0-9\.\@]+)\.\s+	# author (may contain '@', '.',) 
               # with a period terminator
               (\d+)\:(\d+)\:(\d+)\,\s	# time in colon delimited format 
               # with coma terminator
               (\w+)\s(\d+)\n$		# three letter month and month day
               !x ) {
              
              my %tinderbox = (
                               'Subject' => $1, 
                               'Ticket_Num' => $2, 
                               'Complete_Action' => $3,
                               'Action' => $3, 
                               'Author' => $4, 
                               'Hour' => $5, 
                               'Minute' => $6, 
                               'Second' => $7, 
                               'Month' => $8, 
                               'Month_Day' => $9,
                               
                               'Tree' => $tree,
                               'Queue' => $queue_name,
                               );
              
              my $month = MailProcess::monthstr2mon($tinderbox{'Month'});
              
              my ($timenow) = timelocal
                  (
                   $tinderbox{'Second'},
                   $tinderbox{'Minute'},
                   $tinderbox{'Hour'},
                   $tinderbox{'Month_Day'},
                   $month,
                   $year,
                   );
              
              $tinderbox{'Timenow'} = $timenow;
              
              # we wish for all actions to be listed in our progress table
              # so they can not encode user names.
              # Examples of actions as parsed above:
              # 	given to rich 
              # 	user set to jim@reefedge.com 
              
              $tinderbox{'Action'} =~ s/ to .*$//;
              $tinderbox{'Action'} =~ s/ via mail//;
              
              # skip records which are already in the database.
	  
              ($timenow >= $last_tree_data) ||
                  next;
              
              # remove special characters and convert any spaces to '_'
              
              foreach $key (keys %tinderbox) {
                  $value = $tinderbox{$key};
                  ($key, $value) = clean_bug_input($key, $value);
                  $tinderbox{$key} = $value;
              }
              
              $tinderbox{'tinderbox_timenow'} = $timenow;
              $tinderbox{'tinderbox_status'} = $tinderbox{'Action'};
              $tinderbox{'tinderbox_bug_id'} = $tinderbox{'Ticket_Num'};
              
              $DATABASE
              {$tree}
              {$timenow}
              {$tinderbox{'Action'}}
              {$tinderbox{'Ticket_Num'}} = \%tinderbox;
              
              $added_lines++;
              
          } # if matches regexp
      
      } # foreach $line
      
      close(REQ_LOG) ||
          die("Could not close req logfile: $req_log\n");
      
  } # foreach $req_log

  ($added_lines) ||
      return 0;
  
  $METADATA{$tree}{'updates_since_trim'}+= $added_lines;
  
  if ( ($METADATA{$tree}{'updates_since_trim'} >
        $TinderDB::MAX_UPDATES_SINCE_TRIM)
       ) {
      $METADATA{$tree}{'updates_since_trim'}=0;
      $self->trim_db_history(@_);
  }
  
  $self->savetree_db($tree);
  
  return $added_lines;
}


sub status_table_legend {
  my ($out)='';

  # I am not sure the best way to explain this to our users

  return ($out);  
}


sub status_table_header {
  my $out = '';

  my (@progress_states) = ReqData::get_all_progress_states();

  foreach $progress (@progress_states) {  
    $out .= "\t<th>$REQ_NAME $progress</th>\n";
  }

  return ($out);
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

  return ;  
}





sub status_table_row {
  my ($self, $row_times, $row_index, $tree, ) = @_;

  my (@outrow) = ();

  # find all the bug_ids which changed at any point in this cell.

  my (%bug_ids) = ();
  
  while (1) {
   my ($time) = $DB_TIMES[$NEXT_DB];

    # find the DB entries which are needed for this cell
    ($time < $row_times->[$row_index]) && last;

    $NEXT_DB++;

    foreach $status (keys %{ $DATABASE{$tree}{$time} }) {

      # do not display bugs whos status_progres is null, these have
      # been deemed uninteresting.

      ($ReqData::STATUS_PROGRESS{$status}) ||
        next;

      my ($query_links) = '';
      foreach $bug_id (sort keys %{ $DATABASE{$tree}{$time}{$status} }) {
	
	my ($table) = '';
	my ($num_rows) = 0;
	my ($max_length) = 0;
        my ($rec) = $DATABASE{$tree}{$time}{$status}{$bug_id};

	# display all the interesting fields
	
	foreach $field (@ReqData::DISPLAY_FIELDS) {

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
          next;

        $table = (
                  "Ticket updated at: ".
                  localtime($time).
                  "<br>\n".
                  $table.
                  "");

        # fix the size so that long summaries do not cause our window
        # to get too large.

	$max_length = 40;

	# a link to the cgibin page which displays the bug
	
	my ($href) = ReqData::bug_id2bug_url($rec);
	my ($window_title) = "$REQ_NAME Info bug_id: $bug_id";

	# we display the list of names in 'teletype font' so that the
	# names do not bunch together. It seems to make a difference if
	# there is a <cr> between each link or not, but it does make a
	# difference if we close the <tt> for each author or only for
	# the group of links.
	my ($query_link) = 
	  HTMLPopUp::Link(
			  "linktxt" => "<tt>$bug_id</tt>",
			  "href" => $href,
			  
			  "windowtxt" => $table,
			  "windowtitle" => $window_title,
			  "windowheight" => ($num_rows * 25) + 100,
			  "windowwidth" => ($max_length * 15) + 100,
			 );

	# put each link on its own line and add good comments so we
	# can debug the HTML.

	$query_link =  "\t\t".$query_link."\n";
	
	$query_links .= (
                         "\t\t<!-- Req: ".("bug_id: $bug_id, ".
                                          "Time: '".localtime($time)."', ".
                                          "Progress: $progress, ".
                                          "Status: $status, ".
                                          "Tree: $tree, ".
                                          "").
                         "  -->\n".
                         "");
	
	$query_links .= $query_link;
      } # foreach $bug_id

      my ($progress) = $ReqData::STATUS_PROGRESS{$status};
      $bug_ids{$progress} .= $query_links;
    } # foreach $status
  } # while (1)

 
  my (@progress_states) = ReqData::get_all_progress_states();

  foreach $progress (@progress_states) {

    if ($bug_ids{$progress}) {
      push @outrow, (
                     "\t<td align=center>\n".
                     $bug_ids{$progress}.
                     "\t</td>\n".
                     "");
    } else {
      
      push @outrow, ("\t<!-- skipping: Req: Progress: $progress tree: $tree -->".
                     "<td align=center>$HTMLPopUp::EMPTY_TABLE_CELL</td>\n");
    }
    
  }

  return @outrow; 
}

1;
