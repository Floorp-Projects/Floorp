# -*- Mode: perl; indent-tabs-mode: nil -*-
#

# TinderDB::VC_CVS - methods to query the (raw) CVS Version Control
# system and find the users who have checked changes into the tree
# recently and renders this information into HTML for the status page.
# This module depends on TreeData.  The TinderHeader::TreeState is
# queried so that users will be displayed in the color which
# represents the state of the tree at the time they checked their code
# in.  If you are using bonsai you should not use this module, use
# TinderDB::VC_Bonsai instead.


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

# complete rewrite by Ken Estes, Mail.com (kestes@staff.mail.com).
# Contributor(s): 

package TinderDB::VC;

# the raw CVS implemenation of the Version Control DB for Tinderbox.
# This column of the status table will report who has changed files in
# the CVS repository and what files they have changed.

# Remember CVS does all its work in GMT (UCT), Tinderbox does all its work in
# local time.  Thus this file needs to perform the conversions.

# The 'cvs history' command does not give correct info about
# modifications to the repository (types= 'ARM') when passed a module
# name, it was only designed to work on individual files.  If you use
# the -m option you will not find any info about checkins. We leave
# off the module name and take the info for the whole repository
# instead. If this becomes a bother we can reject information about
# updates which do not match a pattern.  In this respect the pattern
# becomes a proxy for the module.  The pattern is stored in
# $TreeData::VC_TREE{$tree}{'dir_pattern'}.  

# There is nothing I can think of to get information about which
# branch the changes were checked in on.  The history command also has
# no notion of branches, but that is a common problem with the way CVS
# does branching anyway.

# What we really want it the checkin comments but history does not
# give us that information and it would be too expensive to run cvs
# again for each file.  The file name is good enough but other VC
# implementations should use the checkin comments if availible.

#   We store the hash of all names who modified the tree at a
#   particular time as follows:

#   $DATABASE{$tree}{$time}{'author'}{$author}{$file} = 1;

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


# Most of the real time for the tinder.cgi process is waiting for CVS
# to tell us about the history.  

# dprofpp says that:
#	 %64.8 of real time which is 66.25 seconds out of 102.15 Seconds
#	 was spent in 3 calls to TinderDB::VC::apply_db_updates()

# It may be a good idea to separate this out into a different process.
# We may want to have a process which gets the history data and passes
# it to tinder.cgi via the same type of updates as the build accepts.
# Then both the tinder.cgi and this new cvs process could be run out
# of cron periodically and the webpages would not be delayed if CVS
# takes a long time to respond this might improve the response time of
# the web updates.  Also the OS process scheduler could do a better
# job of scheduling if there were two different processes one which
# was IO bound and one which was CPU bound.  A separate CVS process
# may improve the quality of the output the cvs info page says that
# there is an option `-w'

#     Show only the records for modifications done from the same
#     working directory where `history' is executing.

# I sugest making the separation a configurable option so some users
# can have VC_CVS run in push mode and others in pull mode.

# It would even be better if bonsai could be convinced set up a
# subscription service for trees.  Then Bonsai could mail tinderbox
# the updates which apply to the branches of interest and tinderbox
# would not have to pool and it would get the branch information which
# it needs.


# Load standard perl libraries
use File::Basename;
use Time::Local;

# Load Tinderbox libraries
use TinderDB::BasicTxtDB;
use Utils;
use HTMLPopUp;
use TreeData;
use VCDisplay;


$VERSION = ( qw $Revision: 1.5 $ )[1];

@ISA = qw(TinderDB::BasicTxtDB);

# Add an empty object, of this DB subclass, to end of the set of all
# HTML columns.  This registers the subclass with TinderDB and defines
# the order of the HTML columns.

push @TinderDB::HTML_COLUMNS, TinderDB::VC->new();



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


# get the recent data from CVS and the treestate file.

sub apply_db_updates {
  my ($self, $tree,) = @_;
  
  # VC_Bonsai provides a TreeState implementation but most VC
  # implementations will not

  my ($tree_state) = TinderHeader::gettree_header('TreeState', $tree);
 
  # sort numerically descending
  @times = sort {$b <=> $a} keys %{ $DATABASE{$tree} };

  my $last_data = 0;
  foreach $time (@times) {

    # Purge duplicate 'treestate' entries to keep the DB size down.
    
    ($DATABASE{$tree}{$time}{'treestate'}) &&
    ($DATABASE{$tree}{$time}{'treestate'} eq $tree_state) &&
      delete $DATABASE{$tree}{$time}{'treestate'};
    
    # find the last time we got CVS data
    ($DATABASE{$tree}{$time}{'author'}) || next;
    $last_data = $time;
    last;
 } 

  $DATABASE{$tree}{$main::TIME}{'treestate'} = $tree_state;

  ($last_data) ||
   ($last_data = $main::TIME - $TinderDB::TRIM_SECONDS );
  

  my $cvs_date_str = "";
  {
    # convert $last_data to cvs input format
    
    my ($sec,$min,$hour,$mday,$mon,
        $year,$wday,$yday,$isdst) =
          gmtime($last_data);
    $mon++;
    $year += 1900;
    $cvs_date_str = sprintf("%02u/%02u/%04u %02u:%02u:%02u GMT",
                            $mon, $mday, $year, $hour, $min, $sec)
  }   

  my $num_updates = 0;
  my $sec_per_day = 60*60*24;
  my $sec_per_month = $sec_per_day*30;
  my $sec_per_year  = $sec_per_day*365;

  # Can you believe that CVS gives no information about the YEAR?
  # We do not even  get a two digit year.
  # Luckally for us we are only interested in history in our recent
  # past, within the last year.

  my $year = 1900 + (gmtime(time()))[5];
  my $sec = 0;

  # see comments about CVS history at the top of this file for details
  # about this commmand and limitations in CVS.

  # for testing here is a good command at my site
  #  cvs -d /devel/java_repository history -c -a -D '400000 seconds ago' 

  my @cmd = ('cvs', 
             '-d', $TreeData::VC_TREE{$tree}{'root'}, 
             'history',
             '-c', '-a', '-D', $cvs_date_str,);

  my ($pid) = open(CVS, "-|");
  
  # did we fork a new process?

  defined ($pid) || 
    die("Could not fork for cmd: '@cmd': $!\n");

  # If we are the child exec. 
  # Remember the exec function returns only if there is an error.

  ($pid) ||
    exec(@cmd) || 
      die("Could not exec: '@cmd': $!\n");

  my @cvs_output = <CVS>;
  
  close(CVS) || 
    die("Could not close exec: '@cmd': $!\n");

  ($?) &&
    die("Could not cmd: '@cmd' exited with error: $?\n");
  
  if ($cvs_output[0] !~  m/^No records selected.\n/) {

    foreach $line (@cvs_output) {

      my ($rectype, $date, $time, $tzone, $author, 
          $revision, $file, $repository_dir, $eqeq, $dest_dir)
        = split(/\s+/, $line);
      
      # Ignore directories which are not in our module.  Since we are not
      # CVS we can only guess what these might be based on patterns.
      
      # This is to correct a bug (lack of feature) in CVS history command.

      ($TreeData::VC_TREE{$tree}{'dir_pattern'}) && 
        (!($repository_dir =~ 
           m!$TreeData::VC_TREE{$tree}{'dir_pattern'}!)) &&
             next;

      # We can not ignore changes which are not on our branch without
      # using bonsai CVS does not really support any analysis of
      # branches.  This is what bonsai was designed for.

      
      # We are only interested in records which indicate that the
      # database has been modified.  We leave out tagging as this is a
      # build function and our build ids should not appear on the
      # developer pages.

      ( $rectype =~ m/^([AMR])$/ ) || next;
      
      { 
        # convert cvs times into unix times.
        
        my ($mon, $mday) = split(/\//, $date);
        my ($hours, $min,) = split(/:/, $time);
        
        # The perl conventions for these variables is 0 origin while the
        # "display" convention for these variables is 1 origin.
        
        $mon--;
        
        # Can you believe that CVS gives no information about the YEAR?
        # We set $sec to zero.
        $time = timegm($sec,$min,$hours,$mday,$mon,$year);    

        # This fix needs to be corrected every year on Jan 1,
        if ( ($main::TIME - $time) > $sec_per_month) {
          $time = timegm($sec,$min,$hours,$mday,$mon,$year - 1);    
        }
      }
      {
        
        # double check that we understand the format, and untaint our
        # variables.
        
        if ( $eqeq ne '==' ) {
          die("Can not parse CVS history output, ".
              "'==' not in right place,: '$line'\n")
        }
        $eqeq = '==';
        if ( $tzone ne '+0000' ) {
          die("Can not parse CVS history output, ".
              "expected TZ: '+0000': '$line'\n")
        }
        $tzone = '+0000';
        if ( $rectype !~ m/^([MACFROGWUT])$/ ) {
          die("Can not parse CVS history output ".
              "rectype: $rectype unknown: '$line'\n")
        }
        $rectype = $1;
        
        if ( (($main::TIME - $sec_per_year) > $time) || 
             (($main::TIME + $sec_per_year) < $time) ) {
          die("CVS reported time: $time ".gmtime($time).
              " which is more then a year away from now.\n")
        }

        # At mozilla.org authors are email addresses with the "\@"
        # replaced by "\%" they have one user with a + in his name

        $author =~ m/([a-zA-Z0-9\.\%\+]+)/;
        $author = $1;      
      }
      
      $DATABASE{$tree}{$time}{'author'}{$author}{"$repository_dir/$file"}=1;
      $num_updates ++;

    } # foreach $line
  } # any updates

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
    my $color = TreeData::TreeState2color($state);
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

  my @outrow = ();

  # we assume that tree states only change rarely so there are very
  # few cells which have more then one state associated with them.
  # It does not matter what we do with those cells.
  
  # find all the authors who changed code at any point in this cell
  # find the tree state for this cell.

  my %authors = ();
  
  while (1) {
    $time = $DB_TIMES[$NEXT_DB];

    # find the DB entries which are needed for this cell
    ($time < $row_times->[$row_index]) && last;

    $NEXT_DB++;

    if ($DATABASE{$tree}{$time}{'treestate'}) {
      $LAST_TREESTATE = $DATABASE{$tree}{$time}{'treestate'};
    }

    foreach $author (keys %{ $DATABASE{$tree}{$time}{'author'} }) {
      foreach $file (keys %{ $DATABASE{$tree}{$time}{'author'}{$author} }) {
        $authors{$author}{$time}{$file} = 1;
      }
    }
  }

  # If there is no treestate, then the tree state has not changed
  # since an early time.  The earliest time was assigned a state in
  # apply_db_updates().  It is possible that there are no treestates at
  # all this should not prevent the VC column from being rendered.

  my $color = TreeData::TreeState2color($LAST_TREESTATE);

  ($LAST_TREESTATE) && ($color) &&
    ($color = "bgcolor=$color");
  
  my $query_links = '';
  if ( scalar(%authors) ) {
    
    # find the times which bound the cell so that we can set up a
    # VC query.
    
    $maxdate = $row_times->[$row_index];
    if ($row_index > 0){
      $mindate = $row_times->[$row_index - 1];
    } else {
      $mindate = $main::TIME;
    }
    
    foreach $author (sort keys %authors) {
      my $table = '';
      my $num_rows = 0;
      my $max_length = 0;
      
      # define a table, to show what was checked in
      $table .= (
                 "Checkins by <b>$author</b>\n".
                 "<table border cellspacing=2>\n".
                 "");
      
      # add table headers
      $table .= (
                 "\t<tr>".
                 "<th>Time</th>".
                 "<th>File</th>".
                 "</tr>\n".
                 "");
      
      # sort numerically descending
      foreach $time ( sort {$b <=> $a} keys %{ $authors{$author} }) {
        foreach $file (keys %{ $authors{$author}{$time}}) {
          $num_rows++;
          $max_length = main::max($max_length , length($file));
          $table .= (
                     "\t<tr>".
                     "<td>".HTMLPopUp::timeHTML($time)."</td>".
                     "<td>".$file."</td>".
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
                          "windowtitle" => "VC Info",
                          "windowheight" => ($num_rows * 50) + 100,
                          "windowwidth" => ($max_length * 10) + 100,
                         );

      # If you have a VCDisplay implementation you should make the
      # link point to its query method otherwise you want a 'mailto:'
      # link

      my $query_link = "";
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

      # put each link on its own line so we can debug the HTML
      $query_link = "\t\t".$query_link."\n";

      $query_links .= $query_link;
    }
    
    @outrow = (
               "\t<td align=center $color>\n".
               $query_links.
               "\t</td>\n".
               "");
    
  } else {
    
    # If there are only spaces or there is nothing in the cell then
    # Netscape 4.5 prints nothing, not even a color.  It prints a
    # color if you use '<br>' as the cell contents. However a ' '
    # inside <pre> should be more portable.
    
    @outrow = ("\t<!-- skipping: VC: tree: $tree -->".
               "<td align=center $color>$HTMLPopUp::EMPTY_TABLE_CELL</td>\n");
  }
  
  
  return @outrow; 
}



1;

