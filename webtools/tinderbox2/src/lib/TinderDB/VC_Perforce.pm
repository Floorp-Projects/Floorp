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

# The only perforce command this module runs is:
#
#		 'p4 describe -s $num'
#
# This means that tinderbox needs to run as a user which has Perforce
# 'list' privileges.

# Tinderbox works on the notion of "tree" which is a set of
# directories.  Perforce has no easily identifiable analog.  We create
# this notion using the variables
# $TreeData::VC_TREE{$tree}{'dir_pattern'} which define regular
# expressions of file names.  There must be at least one file in the
# change set which matches the dir pattern for us to assume that this
# change set applies to this tree.  If not we ignore the change set.



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





package TinderDB::VC_Perforce;


# The Perforce implemenation of the Version Control DB for Tinderbox.
# This column of the status table will report who has changed files in
# the Perforce Depot and what files they have changed.



#  We store the hash of all names who modified the tree at a
#  particular time as follows:

#  $DATABASE{$tree}{$time}{'author'}{$author} = 
#            [\@affected_files, \@jobs_fixed, $change_num];

# Where each element of @affected_files looks like
#	 [$filename, $revision, $action, $comment];
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

use lib '/tmp/tinderbox2/bin/local_conf',
   '/tmp/tinderbox2/bin/default_conf',
   '/tmp/tinderbox2/bin/lib',
   '/tmp/t/tinderbox2/./build/local_conf',
   '/tmp/t/tinderbox2/./build/default_conf',
   '/tmp/t/tinderbox2/./build/lib';


use TinderDB::BasicTxtDB;
use Utils;
use HTMLPopUp;
use TreeData;
use VCDisplay;


$VERSION = ( qw $Revision: 1.4 $ )[1];

@ISA = qw(TinderDB::BasicTxtDB);

# Add an empty object, of this DB subclass, to end of the set of all
# HTML columns.  This registers the subclass with TinderDB and defines
# the order of the HTML columns.

push @TinderDB::HTML_COLUMNS, TinderDB::VC_Perforce->new();

$ENV{'P4PORT'} = $TinderConfig::PERFORCE_PORT || 1666;



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
    my $description = "Tree State: $state";
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

  my ($affected_files, $jobs_fixed);
  
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

        my($affected_files_ref, $jobs_fixed_ref, $change_num) = 
            @{ $DATABASE{$tree}{$time}{'author'}{$author} };

        $affected_files->{$author}{$time}{$change_num} = 
            $affected_files_ref;

        $jobs_fixed->{$author}{$time}{$change_num} = 
            $jobs_fixed_ref;

    }
  } # while (1)

  # If there is no treestate, then the tree state has not changed
  # since an early time.  The earliest time was assigned a state in
  # apply_db_updates().  It is possible that there are no treestates at
  # all this should not prevent the VC column from being rendered.

  my ($cell_color) = TreeData::TreeState2color($LAST_TREESTATE);
  my ($char) = TreeData::TreeState2char($LAST_TREESTATE);

  my $cell_options;
  my $text_browser_color_string;
  if ( ($LAST_TREESTATE) && ($cell_color) ) {
       $cell_options = "bgcolor=$cell_color ";

       $text_browser_color_string = 
         HTMLPopUp::text_browser_color_string($cell_color, $char);
  }

  my $query_links = '';
  $query_links.=  "\t\t".$text_browser_color_string."\n";

  if ( scalar(%{$affected_files}) || scalar(%{$jobs_fixed}) ) {
    
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

    # define two tables, to show what was checked in and what jobs
    # were fixed for each author

    my (@authors) = main::uniq ( 
                                 (keys %{$affected_files}), 
                                 (keys %{$jobs_fixed})
                                 );

    foreach $author (@authors) {
      my ($table) = '';
      my ($num_rows) = 0;
      my ($max_length) = 0;
      
      $table .= "Work by <b>$author</b> <br> for $vc_info \n";

      # create two tables showing what has happened in this time period, the
      # first shows checkins the second shows jobs.

      my (@affected_header) = get_affected_files_header();
   
      ($affected_table, $affected_num_rows, $affected_max_length) = 
          struct2table(\@affected_header, $author, $affected_files);

      if ($affected_num_rows > 0) {
          $table .= $affected_table;
          $num_rows += $affected_num_rows;
          $max_length = main::max(
                                  $max_length, 
                                  $affected_max_length,
                                  );
      }

      my (@jobs_header) =  get_jobs_fixed_header();

      ($jobs_table, $jobs_num_rows, $jobs_max_length) = 
          struct2table(\@jobs_header, $author, $jobs_fixed);

      if ($jobs_num_rows > 0) {
          $table .= $jobs_table; 
          $num_rows += $jobs_num_rows;
          $max_length = main::max(
                                  $max_length, 
                                  $jobs_max_length,
                                  );
      }


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

    
    $query_links.=  "\t\t".$text_browser_color_string."\n";

    @outrow = (
               "\t<td align=center $cell_options>\n".
               $query_links.
               "\t</td>\n".
               "");
        
  } else {
    
    @outrow = ("\t<!-- skipping: VC: tree: $tree -->".
               "<td align=center $cell_options>$text_browser_color_string</td>\n");
  }
  
  
  return @outrow; 
}



# convert perforce string time into unix time.
# currently I see times of the form 
# (this function will change if perforce changes how it reports the time)
#          /year/month/mday hours:min:seconds

sub parse_perforce_time { 

  my ($date, $time) = @_;

  # we do not bother to untaint the variables since we will check the
  # result to see if it is resonable.

  my ($year, $mon, $mday) = split(/\//, $date);

  my ($hours, $min, $sec) = split(/:/, $time);
  
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

  (defined($METADATA{$tree}{'next_change_num'})) ||
      ($METADATA{$tree}{'next_change_num'} = 1);

  while (1) {

        my $change_num = $METADATA{$tree}{'next_change_num'};
        $ENV{'PATH'}=$ENV{'PATH'}.':/usr/src/perforce';
        my (@cmd) = ('p4', 'describe', '-s', $change_num);

        store_cmd_output(@cmd);        

        if (does_cmd_nextline_match("^Change ")) {

      # Ignore directories which are not in our module.  Since we are not
      # CVS we can only guess what these might be based on patterns.
      
      # This is to correct a bug (lack of feature) in CVS history command.

#      if ($TreeData::VC_TREE{$tree}{'dir_pattern'}) {
#
#
#        # There must be at least one file in the change set which
#        # matches the dir pattern for us to assume that this change
#        # set applies to this tree.  If not we ignore the change set.
#
#        my $pattern = $TreeData::VC_TREE{$tree}{'dir_pattern'};
#        my $count = ( 
#                   grep {/$pattern/} 
#                   map { @{ $_ }[0] } 
#                   @affected_files
#                   );
#       ($count) ||   
#            next;
#      }

            $METADATA{$tree}{'next_change_num'}++;
            $num_updates++;

            # process the data in this change set

            my ($change_num, $author, $time) = parse_update_header_line();
            my ($comment) = parse_update_comment();
            my (@jobs_fixed) = parse_update_jobs_fixed();
            my (@affected_files) = parse_update_affected_files($comment);

            $DATABASE{$tree}{$time}{'author'}{$author} = 
            [\@affected_files, \@jobs_fixed, $change_num];

        } else {
            last;
        }

    }

    $METADATA{$tree}{'updates_since_trim'} += $num_updates;

    if ( ($METADATA{$tree}{'updates_since_trim'} >
          $TinderDB::MAX_UPDATES_SINCE_TRIM)
         ) {
        $METADATA{$tree}{'updates_since_trim'}=0;
        trim_db_history(@_);
    }
    
    $self->savetree_db($tree);
    
    return $num_updates;
} # apply_db_updates


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
        
        $time = parse_perforce_time($date, $time);
        
        if ( !(main::is_time_valid($time)) ) {
            die("Perforce reported time: $time ".scalar(localtime($time)).
                " : '$date $time' ".
                " is not valid. \n");
        }
    }
    return ($change_num, $author, $time,);
} # parse_update_header_line


sub parse_update_jobheader_line {

   my ($line) = get_cmd_nextline();

    # the line we are parsing looks like this.
    #
    # 		job000001 on 2001/05/27 by ken *closed*
    # 
    # Remember: the jobname is any sequence of non-whitespace characters

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

        # Ignore the date it is not very accurate and thus not
        # interesting.
    }

    return($jobname,$author,$status);
} # parse_update_jobheader_line



sub parse_update_jobs_fixed {
            
    # before we begin parsing ensure we have the right section of output.

    (does_cmd_nextline_match('^Jobs fixed ')) ||
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
    # this is the header to use when parsing the jobs_fixed datastructure.
    my (@header) = qw(JobName Author Status Comment);
    return @header;
}




sub parse_update_affected_files {
    my ($comment) = @_;

    # before we begin parsing ensure we have the right section of output.

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
        
        my ($elipsis, $filename_and_revision, $action,) = 
            split(/\s+/, $line);

        my ($filename, $revision,) = split(/\#/, $filename_and_revision);
        
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

        push @affected_files, [$filename, $revision, $action, $comment];
        $line = get_cmd_nextline();
    }
    
    return @affected_files;
} # parse_update_affected_files


sub get_affected_files_header {
    # this is the header to use when parsing the affect_files datastructure.
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
