#!#perl# --

# generate random notices for testing purposes.  This program will
# generate several hours worth of 'random notes' staring at the
# current time.


# $Revision: 1.7 $ 
# $Date: 2000/11/09 19:14:00 $ 
# $Author: kestes%staff.mail.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/test/gennotices.tst,v $ 
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

# complete rewrite by Ken Estes, Mail.com (kestes@staff.mail.com).
# Contributor(s): 




# Load the standard perl libraries



# Load the tinderbox specific libraries
use lib '#tinder_libdir#';

use TinderConfig;
use Utils;
use HTMLPopUp;

# since this is a test we do not want to use TinderConfig.pm to get
# information about our configuration.  Our test needs to be self
# sufficent, we hardcode the test data here at the top of the file to
# make it easy to change.



$TINDERBOX_HTML_DIR = ( $TinderConfig::TINDERBOX_HTML_DIR ||
			"/usr/apache/cgibin/webtools/tinderbox");

$TINDERBOX_DATA_DIR = ( $TinderConfig::TINDERBOX_DATA_DIR ||
			"/usr/apache/cgibin/webtools/tinderbox");


@TREES = ('Project_A', 'Project_B', 'Project_C');

@NAMES = qw( bob steve joe alice john jane sue ) ;

@NOTES = (

	  ("I am going to be checking in a ".
	   "<FONT SIZE=\\\"+2\\\">big.</FONT> change now, ".
	   "call me if there are any problems"),

	  ("Who broke the foo-bar module?  ".
	   "I can not check in till you fix it."),

	  ("Can anyone here help me port to the new architecture. ".
	   "I am having trouble."),

	  ("The last code that Magnus checked it is a real mess, ".
	   "did he get it code reviewed?  I am not sure he understands ".
	   "the checkin process."),

	  ("I broke the build,  I forgot to check in one file.  ".
	   "It will work now."),

	  ("I broke the tests. Sorry I forgot to run the ".
	   "<FONT SIZE=\\\"+2\\\">performance tests,</FONT> ".
	   "I was in a rush.  I will back out my changes."),

	 );





foreach $tree (@TREES) {


  mkdir_R("$TINDERBOX_DATA_DIR/$tree/db", 0777);
  mkdir_R("$TINDERBOX_DATA_DIR/$tree/h", 0777);
      
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
    if ($num_notes >= 5) {
      $num_notes = 0;
    }
    $num_notes =~ s/\..*//;
    
    foreach $j (0 .. $num_notes) {
      
      my ($random_user) = rand scalar(@NAMES);
      $random_user =~ s/\..*//;
      my ($random_note) = rand scalar(@NOTES);
      $random_note =~ s/\..*//;
      $user = $NAMES[$random_user];
      $note = $NOTES[$random_note];
      
      my ($mailaddr) = "$user\@mozilla.org";
      my ($localtimenow) = localtime($timenow);

      my ($pretty_time) = HTMLPopUp::timeHTML($timenow);
      my ($rendered_notice) = (
			       "\t\t<p>\n".
			       
			       ("\t\t\t[<b>".
				$mailaddr." - $pretty_time".
				"</b>]\n").
			       
			       "\t\t</p>\n".
			       
			       "\t\t<p>\n".
			       "\t\t\t$note\n".
			       "\t\t</p>\n"
			      );

$out = <<EOF;

\$r = {
		'tree' => '$tree',
		'mailaddr' => '$mailaddr',
		'rendered_notice' => '$rendered_notice', 
		'time' => '$timenow',
		'localtime' => '$localtimenow',
           };
EOF
  ;

      my ($update_file) = ("$TINDERBOX_DATA_DIR/$tree/db/".
			   "Notice.Update.$timenow.$mailaddr");
      
      # first we try and make the file name leagal, then we check by using
      # the definiative legal filename checker.

      $update_file =~ s/\@/\./g;
      $update_file = main::extract_filename_chars($update_file);

      open(FILE, ">$update_file");
      
      print FILE $out;
      
      close(FILE);
      
    } # foreach $j
  } # foreach $i
} # foreach $tree


