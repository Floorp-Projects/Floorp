#!#perl# --

# generate random build data for testing purposes.  This program will
# generate several hours worth of 'randomized builds' starting at the
# current time.  The brief_log link will be generated with a phony log
# URL.


# $Revision: 1.9 $ 
# $Date: 2000/11/09 19:14:20 $ 
# $Author: kestes%staff.mail.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/test/genbuilds.tst,v $ 
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

@BUILD_NAMES = (
		'Build_Packages_(Solaris)', 'Build_Packages_(Linux)',
		'Coverage_Tests', 'Performance_Tests', 'Failover_Tests', 
		'Lint_Tests', 
		'Next_Milestone',
	       );


# generate random build times to simulate real builds and stress the
# boundary cases of the html rendering algorithm.

sub rand_runtime {
  my $runtime;
  

  if ( (rand 2) > 1 ) {
    # mostly at the grid size boarder
    # but there will be plenty smaller then gridsize
    $runtime = (rand 5) + (rand 5);
  } else {
    # always bigger then grid size. 
    $runtime = (rand 15) + 15;
  }
  

  # convert minutes to seconds, remove fractions, then make sure that
  # the numbers are not all round.

  $runtime *= 60;
  $runtime =~ s/\..*//;
  $runtime += (rand 15);

  return $runtime;
}


# Ocassionally we generate gaps between builds

sub rand_gap {
  my $gap;
  
  ( (rand 5) > 1 ) &&
    return 0;


  if ( (rand 2) > 1 ) {
    # mostly at the grid size boarder
    # but there will be plenty smaller then gridsize
    $gap = (rand 5) + (rand 5);
  } else {
    # always bigger then grid size. 
    $gap = (rand 15) + 15;
  }
  

  # convert minutes to seconds, remove fractions, then make sure that
  # the numbers are not all round.

  $gap *= 60;
  $gap =~ s/\..*//;
  $gap += (rand 15);

  return $gap;
}


sub rand_status {

  # all ( $random_status > 2 ) will be converted to success everything
  # else has equal weight.

  my ($random_status) = rand 6;
  my $status; 
  @status_list = ( 'success', 'test_failed', 'build_failed', );

  $random_status =~ s/\..*//;
  if ( $random_status > 2 ) {
    $status = 'success'; 
  } else {
    $status = @status_list[$random_status];
  }

  return $status;
}




foreach $tree (@TREES) {

foreach $build (@BUILD_NAMES) {
    
    $starttime = time();
    foreach $i (0 .. 45) {


    $starttime -= rand_gap();
    $timenow = $starttime;
    $runtime = rand_runtime();
    
    # If the run was less then six minutes increase the gap between
    # start times.
    
    $gap = (6*60) - $runtime;
    if ($gap < 0){
      $gap = 0;
    }
    
    $starttime -= ($runtime + gap) ;
    
    $status = rand_status();
    
    # put the localtimes in the update file to ease debugging.
    
    $local_starttime = localtime($starttime);
    $local_endtime = localtime($timenow);
    
$out = <<EOF;

\$r = {
              'tree' => '$tree',
              'buildname' => '$build',
              'buildfamily' => 'unix',
              'status' => '$status',
              'starttime' => '$starttime',
#  starttime: '$local_starttime', endtime: '$local_endtime', buildname: '$build',
              'timenow' => '$timenow',
	      'brieflog' => 'http://www.mozilla.org/tree=$tree/buildname=$build/starttime=$starttime/status=$status',
              'errorparser' => 'unix'
           };
EOF
  ;
 
       mkdir_R("$TINDERBOX_DATA_DIR/$tree/db", 0777);
       mkdir_R("$TINDERBOX_DATA_DIR/$tree/h", 0777);
    
       my ($update_file) = ("$TINDERBOX_DATA_DIR/$tree/db/".
			   "Build.Update.$tree.$build.$timenow");
    
      $update_file =~ s/([^0-9a-zA-Z\.\-\_\/\:]+)/\./g;

      $update_file = main::extract_filename_chars($update_file);

      open(FILE, ">$update_file");
      
      print FILE $out;
      
      close(FILE);
      
      
    }
    
  }
  
}

