# -*- Mode: perl; indent-tabs-mode: nil -*-

# General purpose utility functions.  Every project needs a kludge
# bucket for common access.

# $Revision: 1.3 $ 
# $Date: 2000/09/10 17:29:17 $ 
# $Author: kestes%staff.mail.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/lib/Utils.pm,v $ 
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

# complete rewrite by Ken Estes, Mail.com (kestes@staff.mail.com).
# Contributor(s): 





package main;



# Standard perl libraries

use Sys::Hostname;
use File::Basename;


# Tinderbox libraries



sub set_static_vars {

# This functions sets all the static variables which are often
# configuration parameters.  Since it only sets variables to static
# quantites it can not fail at run time. Some of these variables are
# adjusted by parse_args() but asside from that none of these
# variables are ever written to. All global variables are defined here
# so we have a list of them and a comment of what they are for.




  # where errors are loged
  
  $ERROR_LOG = ( $TinderConfig::ERROR_LOG ||
                  "/var/log/tinderbox/log");
  
  # where the daemon mode lock (for all trees) is placed
  
  $LOCK_FILE = ( $TinderConfig::LOCK_FILE ||
                 "/usr/apache/cgibin/webtools/tinderbox/tinderd.lock");

  # the time between auto refreshes for all pages in seconds.
  
  $REFRESH_TIME = ( $TinderConfig::REFRESH_TIME || 
                    (60 * 15)
                  );

  @ORIG_ARGV = @ARGV;

  $ENV{'PATH'}= (
                 '/bin'.
                 ':/usr/bin'.
                 ':/usr/local/bin'.
                 
                 ':/opt/gnu/bin'.
                 ':/usr/ucb'.
                 ':/usr/ccs/bin'.
                 '');
  
  # taint perl requires we clean up these bad environmental variables.
  
  delete @ENV{'IFS', 'CDPATH', 'ENV', 'BASH_ENV', 'LD_PRELOAD'};
  
  
  
  # How do we run the unzip command? The full path to gzip is found by
  # configure and substituted into this file.
  
  @GZIP = ("#gzip#",);
  
  @GUNZIP = ("#gzip#", "--uncompress", "--to-stdout",);
  
  
  # The GNU UUDECODE will use these arugments, Solaris uudecode is
  # different. The full path to uudecode is found by configure and
  # substituted into this file.
  
  @UUDECODE = ("#uudecode#", "-o",);
  
  # the version number of this tinderbox release.

  $VERSION = '#tinder_version#';


  return 1;
}



sub get_env {

# this function sets variables similar to set_static variables.  This
# function may fail only if the OS is in a very strange state.  after
# we leave this function we should be all set up to give good error
# handling, should things fail.

# This function should run as early as possible (directly after
# set_static_vars) so that the error environment is setup incase of
# problems.

  $| = 1;
  $PROGRAM = File::Basename::basename($0);
  $TIME = time();
  $PID = $$; 
  $LOCALTIME = localtime($main::TIME);
  $START_TIME = $TIME;

  $HOSTNAME = Sys::Hostname::hostname();

  # check both real and effective uid of the process to see if we have
  # been configured to run with too much privileges.

  ( $< == 0 ) &&
    die("Security Error. Must not run this program as root\n");

  ( $> == 0 ) &&
    die("Security Error. Must not run this program as root\n");

  my ($logdir) = File::Basename::dirname($ERROR_LOG);
  mkdir_R($logdir);

  my ($lockdir) = File::Basename::dirname($LOCK_FILE);
  mkdir_R($lockdir);

  my (@trees) = TreeData::get_all_trees();
  foreach $tree (@trees) {

    my ($dir) = FileStructure::get_filename($tree, 'TinderDB_Dir');
    mkdir_R($dir, 0777);
    
    my ($dir) = FileStructure::get_filename($tree, 'TinderHeader_Dir');
    mkdir_R($dir, 0777);

  }
  
  open (LOG , ">>$ERROR_LOG") ||
    die("Could not open logfile: $ERROR_LOG\n");


  # pick a unique id to append to file names. We do not want to worry
  # about locks, and multiple instances of this program can be active
  # at the same time

  $UID = join('.', $TIME, $$);

  $SIG{'__DIE__'} = \&fatal_error;
  $SIG{'__WARN__'} = \&log_warning;

  return ;
}


# a dummy status handler
sub null {
  return ;
}

# the functions max, min, median all return undefined if they are
# passed the empty list.


sub median {

  # sort numerically ascending
  my (@sorted) = sort {$a <=> $b} @_;
  my ($middle) = int(($#sorted +1)/ 2);
  return $sorted[$middle];
}

sub min {

  # sort numerically ascending
  my (@sorted) = sort {$a <=> $b} @_;
  return $sorted[0];
}


sub max {

  # sort numerically decending
  my (@sorted) = sort {$b <=> $a} @_;
  return $sorted[0];
}



# make a directory (and all of its parents if need be).

# You can optionally specify the permssions for all the directories
# created.


sub mkdir_R {
  my ($dir, $mode) = @_;

  (-d $dir) && return ;

  $mode = $mode || 0755;

  my @dir = split('/', $dir);

  foreach $i (0..$#dir) {
    my ($dir) = join('/', @dir[0..$i]);
    ($dir) || next;

      (-d $dir) ||
	mkdir ($dir, $mode) ||
	  die("Could not mkdir: $dir, for writing: $!\n");
  }
  
  return ;
}



# fatal errors need to be valid HTML

sub fatal_error {
  my  @error = @_;
  foreach $_ (@error) {
    print LOG "[$LOCALTIME] $_";
  }
  print LOG "\n";

  # do not check for errors, the lock file may not exits and even if
  # we have trouble removing the file we we will be exiting anyway.

  unlink ($LOCK_FILE);


  print "Content-type: text/html\n\n";
  print "<html>\n";

  foreach $_ (@error) {
    print  "[$LOCALTIME] $PROGRAM: $_";
  }

  print "</html>";
  print "\n";

  die("\n");
}


sub log_warning {
  my  @error = @_;

  foreach $_ (@error) {
    print LOG "[$LOCALTIME] $_";
  }

  return ;
}


sub overwrite_file {
  my ($filename, @out) = @_;

  my ($outfile) = $filename;
  my ($tmpfile) = "$outfile.$UID";

  my ($dir) = File::Basename::dirname($outfile);
  
  mkdir_R($dir);
  
  open(FILE, ">$tmpfile") ||
    die("Could not open: $tmpfile. $!\n");
  
  print FILE @out;
  
  close(FILE) ||
    die("Could not close: $tmpfile. $!\n");

  (-f $outfile) && 
    (!(unlink($outfile))) &&
      die("Could not unlink: $outfile. $!\n");
    
  rename($tmpfile, $outfile) ||
    die("Could not rename: '$tmpfile to $outfile. $!\n");

  return ;
}


# append data to the end of a file

sub append_file {
  my ($filename, @out);

  open(FILE, ">>$filename") ||
    die("Could not open: $filename. $!\n");
  
  print FILE @out;
  
  close(FILE) ||
    die("Could not close: $filename. $!\n");

  return ;
}

# returns the unique elements of a list in sorted order.

sub uniq {
  my (@in) = @_;
  my (@out, %seen);

  foreach $_ (@in) {
    $seen{$_} = 1;
  }

  @out = sort (keys %seen);

  return @out;
}

# load a list of modules

sub require_modules {
  my @impls = @_;

  foreach $impl (@impls) {
    
    # '$impl' is not a bare word so we must preform this data
    # transformation which require normally does
    
    $impl =~ s!::!/!g;
    $impl .= ".pm";
    
    require $impl;
  }

  return 1;
}

1;
