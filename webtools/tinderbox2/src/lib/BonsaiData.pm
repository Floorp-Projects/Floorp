# -*- Mode: perl; indent-tabs-mode: nil -*-

# BonsaiData.pm - An interface to extract data from the Mozilla Bonsai
# system.  Bonsai was not designed as a library so we need to peek at
# private data files and set global variables to pass data.  We
# isolate the details of the Bonsai Interface in this file.  The only
# module which uses this library is: lib/TinderDB/VC_Bonsai.pm


# $Revision: 1.15 $ 
# $Date: 2003/08/17 01:44:06 $ 
# $Author: kestes%walrus.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/lib/BonsaiData.pm,v $ 
# $Name:  $ 


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




package BonsaiData;


# Standard perl libraries

use File::Basename;


# Tinderbox Specific Libraries
use lib '#tinder_libdir#';

use TreeData;


# we really want to 'use lib BONSAI_DIR' unfortunatly BONSAI_DIR is
# set AFTER the libararies are loaded.  We take a quick and dirty
# approach.

sub load_bonsai_libs {
    my ($bonsai_tree) = @_;
  
  ($LIBS_LOADED) && 
    return 1;
  
  $LIBS_LOADED = 1;
  
  $::CVS_ROOT = $TreeData::VC_TREE{$bonsai_tree}{'root'};

  $BONSAI_DIR = ($TinderConfig::BONSAI_DIR ||
                 "/home/httpd/cgi-bin/bonsai");

  # bonsai must be both loaded and run from the bonsai dir.

  chdir ($BONSAI_DIR)	||
    die("Could not cd to $BONSAI_DIR. $!\n");

  # Hide the use libs from the perl compiler.  This is neccessary or
  # it will be evaluated before BONSAI_DIR is defined.

  eval (

        "use lib '$BONSAI_DIR'; ".
        "require '$BONSAI_DIR/cvsquery.pl'; ".

        # provide the functions AdminOpenTree/AdminCloseTree
        "require '$BONSAI_DIR/adminfuncs.pl'; ".

        "");

    if ($@) {
        die(
            "Error in Tinderbox2 package: BonsaiData".
            "Could not load Bonsai file: \n".
            "\t'$BONSAI_DIR/cvsquery.pl'\n ".
            "or\n".
            "\t'$BONSAI_DIR/adminfuncs.pl'\n".
            "Check that CVS repository is readable at: '$::CVS_ROOT'\n".
            "\n".
            $@
            );
    }

  # turn the warnings off.  Bonsai turns them on but it still is not
  # clean.  Also I get warnings in ./lib/HTMLPopUp.pm inside the code
  # which comes from cgi.pm.  I do not wish to tamper with that code.

  $^W = 0;

  # we should not depend on any directory being mounted, we are a daemon.
  chdir ("/")	||
    die("Could not cd to /. $!\n");
  
  return 1;
}




# This modules must not use Tinderbox Specific Libraries. 
# This library is only an interface abstraction layer.

# Read the bonsai data file and return the value of interest.  For
# security reasons we avoid loading the data via an eval.

sub get_file_variable {
    my ($file, $variable) = @_;

    open(DATA, "<$file") ||
	die("Can't open Bonsai Data file: $file\n");
    
    my ($value) = undef;

    while (<DATA>) { 
	if (/^\$\:\:$variable = '?(\d+)'?;/) {
	    $value = $1;
	    last;
	}
    }
    
    close(DATA)	||
	die("Can't close Bonsai Data file: $file\n");

    defined($value) || 
	die("Bonsai Data file: $file ".
	    "does not define value '$value'\n");
    
    return $value;
}


# Bonsai has no function to return if the tree is open or closed.
# This data handled internally via a global boolean variable
# $::TreeOpen; We will peek at the current batch file to get the
# data.

sub get_tree_state {
    my ($bonsai_tree) = @_;
    
    load_bonsai_libs($bonsai_tree);

    # The default file is in the data directory, other files are in
    # directories with their own name.  If there is no directory the
    # tree is assumed open.

    my $is_bonsai_default = $TreeData::VC_TREE{$bonsai_tree}{'is_bonsai_default'};

    my ($dir);

    if ($is_bonsai_default) {
        $dir = "$BONSAI_DIR/data";
    } else {
        $dir = "$BONSAI_DIR/data/$bonsai_tree";
    }

    # new branches may not have directories
    (-d $dir) ||
        return 'Open';

    my ($file);

    # find the current batch file
    $file = "$dir/batchid.pl";
    (-f $file) ||
        return 'Open';

    # find the current batch file

    my $current_batchid = get_file_variable($file, 'BatchID');

    # get the tree state
    $file = "$dir/batch-${current_batchid}.pl";
    (-f $file) ||
        return 'Open';

    my $is_tree_open = get_file_variable($file, 'TreeOpen');

    my ($tree_state) = ($is_tree_open ? 'Open' : 'Closed');
    return $tree_state;
}




# We must set global variables to call query_checkins();

# This function is used to prevent other modules from using this
# global data, which would cause the code to become more nonlocal then
# it already is.

sub undef_query_vars {
    
    undef $::cvs_module;
    undef $::cvs_branch;
    undef $::cvs_root;

    undef $::bonsai_tree;
    undef $::TreeID;

    undef @::query_dirs;
    undef $::query_module;
    undef $::query_branch;
    undef $::query_branch_head;
    undef $::query_begin_tag;
    undef $::query_end_tag;
    undef $::query_date_min;
    undef $::query_date_max;
    undef $::query_file;
    undef $::query_who;
    undef $::query_whotype;
    undef $::query_logexpr;
    undef $::query_debug;
    
    return 1;
}



# Find all the checkin data which has occured since $date_min

# The data is returned from bonsai as a list of lists.  Each row is one list.
# We want to handle the data as little as possible in this module,
# our only goals is to isolate the bonsai interface.  So we will
# pass back the data directly to the caller with no intermediate
# passes over it.

# We will provide a hash table to make indexing the check-ins
# easier and more self documenting.

sub get_checkin_data {
    my ($bonsai_tree, $cvs_module, $cvs_branch, $date_min) = @_;

    load_bonsai_libs($bonsai_tree);

    undef_query_vars();

    # no module is synonymous with all modules.
    ($cvs_module) ||
        ($cvs_module = 'all');

    # Set global variables to pass data to query_checkins.

    $::CVS_ROOT = $TreeData::VC_TREE{$bonsai_tree}{'root'};

    $::query_module = $cvs_module;
    $::query_branch = $cvs_branch;
    $::query_date_min = $date_min;

    # some bonsai commands use relative paths from cwd

    chdir($BONSAI_DIR) ||
	die("Could not cd to $BONSAI_DIR. $!\n");

    my ($result) = &query_checkins();

    # we should not depend on any directory being mounted, we are a
    # daemon.

    chdir ("/")	||
	die("Could not cd to /. $!\n");


    undef_query_vars();

    my ($index) = {
	'type' => 0,
	'time' => 1,
	'author' => 2,
	'repository' => 3,
	'dir' => 4,
	'file' => 5,
	'rev' => 6,
	'sticky' => 7,
	'branch' => 8,
	'lines_added' => 9,
	'lines_removed' => 10,
	'log' => 11,
    };

    return ($result,  $index);
}


sub save_tree_state {
  my ($tree, $value) = @_;

  my $time_now = time();

  # bonsai must be both loaded and run from the bonsai dir.

  chdir ($BONSAI_DIR)	||
    die("Could not cd to /. $!\n");

  # Make the code look like what is in  adminmail.pl
  # we use functions in  adminfuncs.pl and globals.pl

  LoadCheckins();

  if ($value eq 'Open') {
      $clear_list_of_checkins=1;
      AdminOpenTree($time_now, $clear_list_of_checkins);
  } elsif ($value eq 'Closed') {
      AdminCloseTree($time_now);
  } else {
      die("Bonsai does not implement TreeState: $value\n");
  }

  WriteCheckins();

  return ;
}



1;
