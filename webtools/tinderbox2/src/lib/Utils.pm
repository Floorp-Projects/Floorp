# -*- Mode: perl; indent-tabs-mode: nil -*-

# General purpose utility functions.  Every project needs a kludge
# bucket for common access.

# $Revision: 1.5 $ 
# $Date: 2000/09/22 15:01:32 $ 
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


  # localtime(2000 * 1000 * 1000) = 'Tue May 17 23:33:20 2033'
  $LARGEST_VALID_TIME = (2000 * 1000 * 1000);
  
  # localtime(2000 * 1000 * 400) = 'Tue May  9 02:13:20 1995'
  $SMALLEST_VALID_TIME = (2000 * 1000 * 400);

  # It is easier to understand algorithms if you use named constants,
  # rather then some mysterious constants hard coded into the code.

  $SECONDS_PER_MINUTE = (60);
  $SECONDS_PER_HOUR = (60*60);
  $SECONDS_PER_DAY = (60*60*24);

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
  # about locks when writing files, and multiple instances of this
  # program can be active at the same time

  $UID = join('.', $TIME, $$);

  $SIG{'__DIE__'} = \&fatal_error;
  $SIG{'__WARN__'} = \&log_warning;

  return 1;
}


# a dummy status handler
sub null {
  return 1;
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
  
  return 1;
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


sub atomic_rename_file {
  my ($oldfile, $outfile) = @_;

  (-f $outfile) && 
    (!(unlink($outfile))) &&
      die("Could not unlink: $outfile. $!\n");
  
  rename($oldfile, $outfile) ||
    die("Could not rename: '$oldfile to $outfile. $!\n");
  
  return 1;
}


sub overwrite_file {
  my ($outfile, @outdata) = @_;

  my ($dirname) = File::Basename::dirname($outfile);
  my ($basename) = File::Basename::basename($outfile);

  # It is important that the tmp files have a Unique prefix so that #
  # when the TinderDB globs for update files it does not find the half
  # written updates.

  my ($tmpfile) = "$dirname/Tmp.$basename.$main::UID";

  mkdir_R($dirname);
  
  open(FILE, ">$tmpfile") ||
    die("Could not open: $tmpfile. $!\n");
  
  print FILE @outdata;
  
  close(FILE) ||
    die("Could not close: $tmpfile. $!\n");

  atomic_rename_file($tmpfile, $outfile);

  return 1;
}


# append data to the end of a file

sub append_file {
  my ($filename, @out);

  open(FILE, ">>$filename") ||
    die("Could not open: $filename. $!\n");
  
  print FILE @out;
  
  close(FILE) ||
    die("Could not close: $filename. $!\n");

  return 1;
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



# return true iff the argument is a valid time in time() format.

sub is_time_valid {
  my ($time) = @_;
  
  $valid = (
            ($time < $LARGEST_VALID_TIME) &&
            ($time > $SMALLEST_VALID_TIME) 
           );

  return $valid;
}



# ---------- 

# The 'extract' functions will untaint their input data and return
# only data which meets the extraction critireion.

# ----------


# remove unprintable characters from a string

# this is for untainting data which could be almost anything but
# should not be 'binary' data.

sub extract_printable_chars {
  my ($str) = @_;

  $str =~ s![^a-zA-Z0-9\ \t\n\`\"\'\;\:\,\?\.\-\_\+\=\\\|\/\~\!\@\#\$\%\^\&\*\(\)\{\}\[\]\<\>]+!!g;

  $str =~ m!(.*)!s;
  $str = $1;

  return $str;
}


# remove characters are not digits from a string
sub extract_digits {
  my ($str) = @_;

  $str =~ m/([0-9]+)/;
  $str = $1;

  return $str;
}


# remove characters which do not belong in a filename/static URL from a string
sub extract_filename_chars {
  my ($str) = @_;

  $str =~ m/([0-9a-zA-Z\.\-\_\/\:]+)/;
  $str = $1;

  return $str;
}


# remove unprintable characters and return a "safe" html string.


# we must filter user input to prevent this:

# http://www.ciac.org/ciac/bulletins/k-021.shtml
# http://www.cert.org/advisories/CA-2000-02.html

#   When a victim with scripts enabled in their browser reads this
#   message, the malicious code may be executed
#   unexpectedly. Scripting tags that can be embedded in this way
#   include <SCRIPT>, <OBJECT>, <APPLET>, and <EMBED>.

# note that since we want some tags to be allowed (href) but not
# others.  This requirement breaks the taint perl mechanisms for
# checking as we can not escape every '<>'.

# If there are bad tags this could really mangle the html, however
# the result is always 'safe html'.

sub extract_html_chars {
  my ($str) = @_;

  $str = extract_printable_chars($str);

  # Remove any known "bad" tags.  Since any user can post notices we
  # have to prevent bad scripts from being posted.  It is just too
  # hard to follow proper security procedures ('tainting') and escape
  # everything then put back the good tags.

  $str =~ s!	<
		[^>]*			# ignore modifiers which do not end tag
		(?:(?:SCRIPT)|(?:OBJECT)|(?:APPLET)|(?:EMBED)) # these are bad
		[^>]*			# ignore modifiers which do not end tag
		>
	!!ixg;

  return $str;
}




# remove characters which do not belong in a mail addresses, or CVS
# authors or Unix log int ids from a string


sub extract_user {
  my ($user) = @_;

  # If the data looks like this: 
  # 	' "leaf (Daniel Nunes)" <leaf@mozilla.org> ' 
  # extract the mail address.

  if ( $user =~ m/<([^>]+)>/ ) {
    $user = $1;
  }

  # At mozilla.org authors are email addresses with the "\@"
  # replaced by "\%" they have one user with a + in his name

  $user =~ m/([a-zA-Z0-9\_\-\.\%\+\@]+)/;
  $user = $1;

  return $user;
}


1;
