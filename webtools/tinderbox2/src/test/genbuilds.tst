#!#perl# --
# -*- Mode: perl; indent-tabs-mode: nil -*-
#

# generate random build data for testing purposes.  This program will
# generate several hours worth of 'randomized builds' starting at the
# current time.  The brief_log link will be generated with a phony log
# URL.


# $Revision: 1.17 $ 
# $Date: 2003/08/17 00:57:35 $ 
# $Author: kestes%walrus.com $ 
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

@BUILD_NAMES = (
		'Build_Packages_(Solaris)', 'Build_Packages_(Linux)',
		'Coverage_Tests', 'Performance_Tests', 'Failover_Tests', 
		'Lint_Tests', 
		'Next_Milestone',
	       );

# status for the first build in a column 
@INITIAL_STATUS_LIST = ( 'success', 'test_failed', 'build_failed',  
			 'building', 'not_running');

# status for all other builds
@STATUS_LIST         = ( 'success', 'test_failed', 'build_failed', );


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

  $random_status =~ s/\..*//;
  if ( $random_status > $#STATUS_LIST ) {
    $status = 'success'; 
  } else {
    $status = @STATUS_LIST[$random_status];
  }

  return $status;
}


sub rand_num_errors {

  my ($num) = rand 300;
  $num =~ s/\..*//;

  return $num;
}




# This is just like rand_status except that the first status in any
# column could be 'building' or 'not_running'

sub rand_initial_status {

  # all ( $random_status > 2 ) will be converted to success everything
  # else has equal weight.

  my ($random_status) = rand 6;
  my $status; 

  $random_status =~ s/\..*//;
  if ( $random_status > $#INITIAL_STATUS_LIST ) {
    $status = 'success'; 
  } else {
    $status = @INITIAL_STATUS_LIST[$random_status];
  }

  return $status;
}


# generate a random build time.  We are generating builds backwards
# from most recent to oldest time.  We need to generate random gaps as
# well as random size builds.

sub gen_rnd_build {
  my ($starttime) = @_;

  my ($end) = $starttime;
  $end -= rand_gap();
  
  my ($runtime) = rand_runtime();
  
  # If the run was less then six minutes increase the gap between
  # start times.
  
  my ($run_gap) = (6*60) - $runtime;
  if ($run_gap < 0){
    $run_gap = 0;
  }
  
  my ($begin) = $end - ($runtime + $run_gap) ;
  $begin =~ s/\..*//;
  $end =~ s/\..*//;

  return ($end, $begin);
}


      

sub write_update_record {
  my ($tree, $build, $status, $num_errors, $begin, $end,) = @_;

      # put the localtimes in the update file to ease debugging.
      
      my ($local_starttime) = localtime($begin);
      my ($local_endtime) = localtime($end);
      
  my (%data) = (
	   
              'tree' => $tree,
              'buildname' => $build,
              'status' => $status,
              'starttime' => $begin,
#  starttime: '$local_starttime', endtime: '$local_endtime', buildname: '$build',
              'timenow' => $end,
              'errors' => $num_errors,

# this link does not point to real log files, it is only to help give
# an idea of what the real link will look like and acts as a comment to make debugging easier.

	      'brieflog' => "http://www.mozilla.org/tree=$tree/buildname=$build/starttime=$starttime/status=$status",
	      'fulllog' => "http://www.mozilla.org/tree=$tree/buildname=$build/starttime=$starttime/status=$status",
              'errorparser' => "unix",
	  );

    
    
  my ($update_file) = ("$TINDERBOX_DATA_DIR/$tree/db/".
		       "Build.Update.$tree.$build.$begin");

  $update_file =~ s/([^0-9a-zA-Z\.\-\_\/\:]+)/\./g;

  $update_file = main::extract_filename_chars($update_file);

  mkdir_R("$TINDERBOX_DATA_DIR/$tree/db");
  mkdir_R("$TINDERBOX_DATA_DIR/$tree/h");

  Persistence::save_structure( 
                              \%data,
                              $update_file,
                             );
  return 1;
}




foreach $tree (@TREES) {
  
  foreach $build (@BUILD_NAMES) {
    
    my ($starttime) = time();

    my ($end, $begin) = gen_rnd_build($starttime);
    $starttime = $begin;
    
    my ($status) = rand_initial_status(); 
    my ($num_errors) = rand_num_errors();

    ($status eq 'not_running') ||
      write_update_record($tree, $build, $status, $num_errors, $begin, $end,);

    foreach $i (0 .. 45) {
      
      my ($end, $begin) = gen_rnd_build($starttime);
      $starttime = $begin;

      my ($status) = rand_status(); 
      write_update_record($tree, $build, $status, $num_errors, $begin, $end,);
    } # $i
    
  } # $build
  
} # $tree

