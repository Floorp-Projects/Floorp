#!/usr/local/bin/perl

# generate random build data for testing purposes.  This program will
# generate several hours worth of 'randomized builds' starting at the
# current time.  The brief_log link will be generated with a phony log
# URL.


# $Revision: 1.1 $ 
# $Date: 2000/06/22 04:13:29 $ 
# $Author: mcafee%netscape.com $ 
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

use Utils;
use HTMLPopUp;


# since this is a test we do not want to use TinderConfig.pm to get
# information about our configuration.  Our test needs to be self
# sufficent, we hardcode the test data here at the top of the file to
# make it easy to change.

$TINDERBOX_DIR="/tmp/tinderbox";

$TINDERBOX_DIR="/web/htdocs/gci/iname-raven/build-group/tinderbox";



@TREES = ('Project_A', 'Project_B', 'Project_C');

@BUILD_NAMES = (
		'Build_Packages (Solaris)', 'Build_Packages (Linux)',
		'Coverage_Tests', 'Performance_Tests', 'Failover_Tests', 
		'Lint_Tests', 
		'Next_Milestone',
	       );






foreach $i (0 .. 45) {

  foreach $tree (@TREES) {

  foreach $build (@BUILD_NAMES) {
      
      $random_status = rand 6;
      $random_status =~ s/\..*//;
      if ( $random_status > 2 ) {
	$status = 'success'; 
      } else {
	$status = ( 'success', 'test_failed', 'busted', ) [$random_status];
      }
      $runtime = (rand 25) + 10;
      # convert minutes to seconds, and remove fractions.
      $runtime *= 60;
      $runtime =~ s/\..*//;
      
      if (!($starttime{$tree}{$build})) {
	$starttime{$tree}{$build} = time();
      }
      $timenow = $starttime{$tree}{$build};
      $starttime{$tree}{$build} -= $runtime;
      $starttime = $starttime{$tree}{$build};
      $local_starttime = localtime($starttime);
      $local_endtime = localtime($timenow);
      
$out = <<EOF;

\$record = {
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
 
      mkdir_R("$TINDERBOX_DIR/$tree/db", 0777);
      mkdir_R("$TINDERBOX_DIR/$tree/h", 0777);
      
      open(FILE, ">$TINDERBOX_DIR/$tree/db/Build.Update.$tree.$build.$timenow");
      
      print FILE $out;
      
      close(FILE);
      
      
    }
    
  }
  
}

