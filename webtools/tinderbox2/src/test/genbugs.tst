#!#perl# --
# -*- Mode: perl; indent-tabs-mode: nil -*-
#

# generate random bug tickets for testing purposes.  This program will
# generate several hours worth of 'random bugs' staring at the
# current time.


# $Revision: 1.11 $ 
# $Date: 2003/08/17 00:57:35 $ 
# $Author: kestes%walrus.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/test/genbugs.tst,v $ 
# $Name:  $ 
#


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




# Load the standard perl libraries



# Load the tinderbox specific libraries
use lib '#tinder_libdir#';

use TinderConfig;
use Utils;
use HTMLPopUp;
use Persistence;

# since this is a test we do not want to use TinderConfig.pm to get
# information about our configuration.  Our test needs to be self
# sufficent, we hardcode the test data here at the top of the file to
# make it easy to change.



$TINDERBOX_HTML_DIR = ( $TinderConfig::TINDERBOX_HTML_DIR ||
			"/usr/apache/cgibin/webtools/tinderbox");

$TINDERBOX_DATA_DIR = ( $TinderConfig::TINDERBOX_DATA_DIR ||
			"/usr/apache/cgibin/webtools/tinderbox");


@TREES = ('Project_A', 'Project_B', 'Project_C');

@STATUS = ( 	
	   'UNCONFIRMED',
	   'NEW',
	   'ASSIGNED',
	   'REOPENED',
	   'VERIFIED',
	   'FAILED',
	   'CLOSED',
	  );

@SUMMARIES = (

	      'news articles not downloaded after subscribing to newsgroup',
	      'PERFORMANCE problems building the subscribe datasource',
	      'assertions caused my multi-part news messages', 

	      'Time shown ought to be UTC',
	      'Make distclean failed: too many args on command line',
	      'Connection fails; no error message',

	      ('Clicking on &quot;Twisty&quot; in &lt;Subscribe dialog&gt;'.
	       ' crashes.'),


	      # Single quotes seem to always break the popup window,
	      # even if they are escaped.  I wonder if this is a bug
	      # in netscape or if the javascript requires the
	      # expansion of metacharacters before interpretation.

	      #	'can&#039;t open attachments in &#039;news&#039; messages',
	     );





foreach $tree (@TREES) {


  mkdir_R("$TINDERBOX_DATA_DIR/$tree/db");
  mkdir_R("$TINDERBOX_DATA_DIR/$tree/h");
      
  my ($timenow) = time();

  foreach $i (0 .. 30) {
    
    my ($nexttime) = (rand 100) + 10;
    # convert minutes to seconds, and remove fractions.
    $nexttime *= 60;
    $nexttime =~ s/\..*//;
    
    $timenow -= $nexttime;

    # most notes are 'singular' though occasionally there are clusters
    # of a few notes appearing at the same time.

    $num_notes = rand 10;
    $num_notes =~ s/\..*//;
    if ($num_notes >= 5) {
      $num_notes = 0;
    }
    
    foreach $j (0 .. $num_notes) {
      
      my ($bug_id) = (rand 10000) + 10;
      $bug_id =~ s/\..*//;

      my ($random_status) = rand scalar(@STATUS);
      $random_status =~ s/\..*//;
      $status = $STATUS[$random_status];

      my ($random_summary) = rand scalar(@SUMMARIES);
      $random_summary =~ s/\..*//;
      $summary = $SUMMARIES[$random_summary];

      my ($localtimenow) = localtime($timenow);

      my ($pretty_time) = HTMLPopUp::timeHTML($timenow);

      my (%data) = (
		    'ReportedBy' => "kestes\@staff.mail.com",
		    'Bug#' => $bug_id,
		    'Product' => $tree,
		    'Priority' => "low",
		    'Status' => $status,
		    'Platform' => "All",
		    'Version' => "1.0",
		    'Summary' => $summary,
		    'Component' => "rhcn",
		    'Severity' => "low",
		    'QAContact' => "matty\@box.net.au",
		    
		    'tinderbox_timenow' => $timenow,
		    'tinderbox_localtime_timenow' => $localtimenow,
		    'tinderbox_status' => $status,
		    'tinderbox_bug_id' => $bug_id,
		    'tinderbox_bug_url' => "http://bugzilla.mozilla.org/show_bug.cgi?id=$bug_id",
		    'tinderbox_tree' => "$tree",
		   );
 
      my ($update_file) = ("$TINDERBOX_DATA_DIR/$tree/db/".
			   "BT_Generic.Update.$timenow.$bug_id");
      
      # first we try and make the file name leagal, then we check by using
      # the definiative legal filename checker.

      $update_file =~ s/\@/\./g;
      $update_file = main::extract_filename_chars($update_file);

      
      Persistence::save_structure( 
				  \%data,
				  $update_file,
				 );

      
    } # foreach $j
  } # foreach $i
} # foreach $tree



