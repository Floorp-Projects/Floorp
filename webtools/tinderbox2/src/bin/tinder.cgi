#!#perl# #perlflags# --
# -*- Mode: perl; indent-tabs-mode: nil -*-
#

# $Revision: 1.34 $ 
# $Date: 2003/08/16 18:31:03 $ 
# $Author: kestes%walrus.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/bin/tinder.cgi,v $ 
# $Name:  $ 

# tinder.cgi - the main tinderbox program.  This program make all the 
# 		HTML pages.

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



# Standard perl libraries
use File::Basename;
use Sys::Hostname;
use Time::Local;


# Tinderbox libraries

use lib '#tinder_libdir#';

use TinderConfig;
use Utils;
use TreeData;
use BTData;
use FileStructure;
use HTMLPopUp;
use VCDisplay;
use Summaries;
use TinderDB;
use TinderHeader;

$VERSION = '#tinder_version#';

# the default number of hours shown on the status page

$DEFAULT_DISPLAY_HOURS = $TinderConfig::DEFAULT_DISPLAY_HOURS || (6);

$MAX_DISPLAY_HOURS = 100;

sub usage {


    my $usage =<<EOF;

$0	[--version] [--help]  
$0	--daemon-mode
$0	--tree=str [--start-time=time] 
		[--end-time=time] [--display-hours=hrs] 
		[--table-spacing=min]


Informational Arguments


--version	Print version information for this program

--help		Show this usage page


CGI Mode Arguments

(These argments are desgined to be pased to the cgi script via the webserver.  
 The command line interface is provided for testing.)


--start-time	The time which the table should being at, 
		in time() format.  If not given the current 
		time is assumed.

--end-time	The time which the table should end at, in time() format.
		The start-time is always earlier then then end-time.

--display-hours The number of hours which the table should show starting
		at time --start-time.  If --end-time and --display-hours 
		are not set the default --display-hours is: $DEFAULT_DISPLAY_HOURS.
		This argument is only effective if --end-time is not set.
		Both --end-time and --display-hours are equivalent means
		of stating when the table should end.

--noignore	Show all build columns even if some of them 
		have been set to ignore.


Daemon Mode Arguments


--daemon-mode	Indicate that this execution is in daemon mode.  No
		output is sent to standard out.  The effect of this
	        run should be to update the webpages for every tree.


Synopsis

This program generates the Tinderbox Web pages.  It can be run either
in daemon mode via a cron job or via a webserver as a regular cgi bin
program.

In daemon mode the program will update all the databases with current
data and prepare new summary pages and status pages for all trees.

In CGI mode the program will prepare the status page that the user
asked for, this page is not saved to disk and we update none of the
databases.  This requires the user to specify a single tree which the
page will represent and pass in any additional arguments which the
user wishes to be different from the defaults.

New data is pushed into the system via administrative web forms and
via mail which is delivered to the helper program processmail and has
the specified format.  Additional data is gathered by having the
program query the Version Control Software to find any updates which
have happended recently.

Errors are logged to the logfile: $ERROR_LOG



Examples

tinder.cgi --help

tinder.cgi --daemon-mode

tinder.cgi --tree=SeaMonkey --start-time=956535519 \\
		--display-hours=6 --table-spacing=15 


EOF

    print $usage;
    exit 0;

} # usage




# parse the command line arguments

sub parse_args {

  my (%form) = HTMLPopUp::split_cgi_args(
                                         'cgi_remove_args' => ['daemon-mode'],
                                         );

  my ($tree) = $form{'tree'};
  my ($daemon_mode) = (grep /daemon-mode/, keys %form);
  
  my ($start_time) = $form{'start-time'} || $main::TIME;
  
  my ($end_time) = $form{"end-time"};
  
  if(grep /noignore/, keys %form) {
    $NOIGNORE = 1;
  }

  # take care of the informational arguments
  
  if(grep /version/, keys %form) {
    print "$0: Version: $VERSION\n";
    exit 0;  
  }
  
  if (grep /help/, keys %form) {
    usage();
  }
  
  # check that we are given valid arguments

  if ($tree) {
    (TreeData::tree_exists($tree)) ||
      die("tree: $tree does not exist\n");    

    # tree is safe, untaint it.
    $tree =~ m/(.*)/;
    $tree = $1;
  }

  $start_time = extract_digits($start_time);
  (is_time_valid($start_time)) ||
    die("Can not prepare web page with start_time: $start_time. \n");
  
  if ( !($end_time) ) {
    my ($display_hours) = ( $form{'display-hours'} || 
                            $DEFAULT_DISPLAY_HOURS );
    $display_hours = extract_digits($display_hours);
    $end_time = $start_time - ($display_hours*$main::SECONDS_PER_HOUR);
  }
  
  $end_time = extract_digits($end_time);
  (is_time_valid($end_time)) ||
    die("Can not prepare web page with end_time: $end_time. \n");
    
  {
    my ($display_hours) = int (($start_time - $end_time) / ($main::SECONDS_PER_HOUR));

    ($display_hours > 0) ||
      die("start_time must be greater then end_time.".
          " start_time: $start_time, end_time: $end_time. \n");
    
    ($display_hours <= $MAX_DISPLAY_HOURS) ||
      die("Number of hours to display is too large. \n");
   }

  ( ($daemon_mode) || ($tree) ) ||
    die("If you are not running in daemon mode you must specify a tree\n");
  
  return ($daemon_mode, $start_time, $end_time, $tree,);
} # parse_args



# generate the status page for the requested tree.

sub HTML_status_page {

  my ($start_time, $end_time, $tree, ) = @_;

  my ($status_page_url) = (FileStructure::get_filename($tree, 'tree_URL').
                           "/status.html");

  my ($out) = '';

  # load the headers these all have well known namespaces
  
  my ($motd) = TinderHeader::gettree_header('MOTD', $tree);
  my ($tree_state) = TinderHeader::gettree_header('TreeState', $tree);
  my ($break_times) = TinderHeader::gettree_header('Build', $tree);
  my ($ignore_builds) = TinderHeader::gettree_header('IgnoreBuilds', $tree);

  my ($html_tree_state) = '';
  my ($html_ignore_builds) = '';

  ($tree_state) &&
    ($html_tree_state .= (
                          "<a NAME=\"status\">".
                          "The tree is currently: ".
                          "<font size=+2>".
                          $tree_state.
                          "</font></a><br>\n"));
  ($ignore_builds) &&
    ($html_ignore_builds .= (
                             "Unmonitored Builds: ".
                             join(", ", split (/,/, 
                                               $ignore_builds
                                        )).
                             "<br>\n"));

  my (@legend) = TinderDB::status_table_legend($tree);
  my (@header) = TinderDB::status_table_header($tree);
  my (@body) = TinderDB::status_table_body($start_time, $end_time, $tree);

  # create the footer links
  
  my ($display_time) = $start_time - $end_time;

  # round the division to the nearest integer.

  my ($display_hours) =  sprintf '%d', ($display_time/$main::SECONDS_PER_HOUR);

  my ($display_2hours) = min($display_hours*2, $MAX_DISPLAY_HOURS);
  my ($display_4hours) = min($display_hours*4, $MAX_DISPLAY_HOURS);
  my ($display_8hours) = min($display_hours*8, $MAX_DISPLAY_HOURS);
  my ($previous_links) = 
    HTMLPopUp::Link(
                    "linktxt"=>"Show previous $display_hours hours", 
                    "href"=>("$FileStructure::URLS{'tinderd'}".
                             "\?".
                             "tree=$tree\&".
                             "start-time=$end_time\&".
                             "display-hours=$display_hours"),
                   ).
   "<br>\n".
    HTMLPopUp::Link(
                    "linktxt"=>"Show previous $display_2hours hours", 
                    "href"=>("$FileStructure::URLS{'tinderd'}".
                             "\?".
                             "tree=$tree\&".
                             "start-time=$end_time\&".
                             "display-hours=$display_2hours"),
                   ).
   "<br>\n".
    HTMLPopUp::Link(
                    "linktxt"=>"Show previous $display_4hours hours", 
                    "href"=>("$FileStructure::URLS{'tinderd'}".
                             "\?".
                             "tree=$tree\&".
                             "start-time=$end_time\&".
                             "display-hours=$display_4hours"),
                   ).
   "<br>\n".
    HTMLPopUp::Link(
                    "linktxt"=>"Show previous $display_8hours hours", 
                    "href"=>("$FileStructure::URLS{'tinderd'}".
                             "\?".
                             "tree=$tree\&".
                             "start-time=$end_time\&".
                             "display-hours=$display_8hours"),
                   ).
   "<br>\n".
    HTMLPopUp::Link(
                    "linktxt"=>"Show current status", 
                    "href"=>$status_page_url,
                   ).
  "<br><p>\n\n".
  HTMLPopUp::Link(
                  "linktxt"=>"Regnerate HTML Pages",
                  "href"=>("$FileStructure::URLS{'regenerate'}".
                           "\?".
                           "tree=$tree"),
                  ).
  "<br>\n".
  HTMLPopUp::Link(
                  "linktxt"=>"Administrate this tree ($tree)",
                  "href"=>("$FileStructure::URLS{'admintree'}".
                           "\?".
                           "tree=$tree"),
                  ).
  "<br>\n";
  my ($links) = 
    HTMLPopUp::Link(
                    "linktxt"=>"Add to Notice Board",
                    "href"=>("$FileStructure::URLS{'addnote'}".
                             "\?".
                             "tree=$tree"),
                   ).
 "<br>\n";

  $out .= HTMLPopUp::page_header('title'=>"Tinderbox Status Page tree: $tree", 
                                 'refresh'=>$REFRESH_TIME);
  $out .= "\n\n";
  $out .= "<!-- /Build Page Headers -->\n\n\n";
  $out .= "$links\n";
  $out .= "<br>\n";
  $out .= HTMLPopUp::Link("name"=>"MOTD",)."\n";
  $out .= "<!-- Message of the Day -->\n";
  $out .=  $motd;
  $out .= "<p>\n<!-- /Message of the Day -->\n";
  $out .= "\n\n";
  $out .= HTMLPopUp::Link("name"=>"TreeState",)."\n";
  $out .= "<!-- Tree State -->\n";
  $out .= "$html_tree_state";
  $out .= "$html_ignore_builds";
  $out .= "<!-- Break Times -->\n";
  $out .= "$break_times<br>\n";
  $out .= "\n\n";
  $out .= HTMLPopUp::Link("name"=>"Table",)."\n";
  $out .= "<!-- Table Header -->\n";
  $out .= "<table border=1 bgcolor='#FFFFFF' cellspacing=1 cellpadding=1>\n";
  $out .= "<tr>\n";
  $out .= "@header";
  $out .= "</tr>\n\n";
  $out .= "<!-- Table Contents -->\n\n";
  $out .= "@body";
  $out .= "<!-- /Table Contents -->\n\n";
  $out .= "</table>\n\n";
  $out .= "<!-- Page Footer --><p>\n";
  $out .= $links;
  $out .= "<br>\n";
  $out .= $previous_links;
  $out .= "<br>\n";
  $out .= HTMLPopUp::Link("name"=>"Legend",)."\n";
  $out .= "<!-- Table Legend -->\n";
  $out .= "<FONT SIZE=\"+3\"><B><NOBR>Table Legend</NOBR></B></FONT>\n";
  $out .= "<table width=\"100%\" border=1 cellpadding=0 cellspacing=0>\n";
  $out .= "	@legend\n\n";
  $out .= "</table>\n\n";
  my (@structures) = HTMLPopUp::define_structures();
  $out .= "@structures";
  $out .= "<!-- /Page Footer --><p>\n\n";
  $out .= "</HTML>\n\n";

  return $out;
}




# the main loop for daemon mode

# we update all the databases and prepare new summary pages and
# status pages for all trees.

sub daemon_main {
  my ($start_time, $end_time, $tree, ) = @_;

  # If the daemon is still running from last call do not bother
  # running now. This could cause conflicts on the database.
  
  symlink ($UID, $FileStructure::LOCK_FILE) ||
    return ;

  my ($summary_data);

  my (@trees) = TreeData::get_all_trees();

  foreach $tree (@trees) {

    TinderDB::loadtree_db($tree);

    # even if there are no updates do not skip this iteration, we need
    # to show the users that the process is not broken

    $NUM_UPDATES = TinderDB::apply_db_updates($tree);

    my ($outfile) = (FileStructure::get_filename($tree, 'tree_HTML').
                   "/status.html");
    
    my (@out) = HTML_status_page($start_time, $end_time, $tree, );

    overwrite_file($outfile, @out);

    $summary_data = Summaries::summary_pages($tree, $summary_data, $REFRESH_TIME);

    # There are automated bots who need the header data, they extract
    # it from this file.
    
    my ($all_headers) = TinderHeader::get_alltree_headers($tree);
    
    TinderHeader::export_alltree_headers($tree, $all_headers);
    
    # if previous runs have died in the middle of an update, they will
    # leave these files which are useless and need to be cleaned up.
    
    #   (Do not check for errors.  There may not be any files so the
    #    glob may not expand.)
    
    system ("rm -f ".
            FileStructure::get_filename($tree, 'tree_HTML').
            '/*\.html\.*');

  } # foreach tree
  
  Summaries::create_global_index($summary_data);

  unlink ($FileStructure::LOCK_FILE) ||
    die ("Could not remove lockfile: $FileStructure::LOCK_FILE\n");
  
  return ;
}



# the main loop for cgi mode

# prepare the status page that the user asked for, this page is not
# saved to disk and we update none of the databases.

sub cgi_main {
  my ($start_time, $end_time, $tree, ) = @_;
    
  TinderDB::loadtree_db($tree);
  
  print "Content-type: text/html\n\n";

  print HTML_status_page($start_time, $end_time, $tree, );
  
  return ;
}


# write some statistics for performance analysis purposes

sub write_stats {

 my ($end_time) = time();
 my ($run_time) = main::round (
                               ($end_time - $TIME)/
                               $main::SECONDS_PER_MINUTE
                               );

 if ($TinderConfig::LOG_PERFORMANCE) {
   print LOG "run_time: $run_time num_updates: $NUM_UPDATES\n";
 }

 return ;
}


# --------------------main-------------------------
{
  set_static_vars();
  get_env();

  my ($daemon_mode, $start_time, $end_time, $tree, ) =  
    parse_args();
  chk_security();
  
  if ($daemon_mode) {
    daemon_main($start_time, $end_time, $tree, );
  } else {
    cgi_main($start_time, $end_time, $tree,);
  }

  write_stats();
  
  exit 0;
}
