# -*- Mode: perl; indent-tabs-mode: nil -*-

# Utils.pm - General purpose utility functions.  Every project needs a
# kludge bucket for common access.

# $Revision: 1.38 $ 
# $Date: 2003/08/16 18:31:07 $ 
# $Author: kestes%walrus.com $ 
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

# complete rewrite by Ken Estes:
#	 kestes@staff.mail.com Old work.
#	 kestes@reefedge.com New work.
#	 kestes@walrus.com Home.
# Contributor(s): 





package main;



# Standard perl libraries

use Sys::Hostname;
use File::Basename;
use Time::Local;


# Tinderbox libraries



# check that the data directories exists and meet the security policy.

sub security_check_data_dir {
  my ($dir) = (@_);

  ( -l $dir ) &&
    die("Security Error. dir: $dir is a symbolic link.\n");
  
  mkdir_R($dir);

  # Must use 'CORE::stat' as some versions of perl have a 'stat' which
  # gives an object and others return a list.
  # (stat fix for perl 5.6 from "John Turner" <jdturner@nc.rr.com>)

  ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
   $atime,$mtime,$ctime,$blksize,$blocks) =
     CORE::stat($dir);

  my $tinderbox_uid = $>;

  ( $uid == $tinderbox_uid ) ||
    die("Security Error. dir: $dir, owner: $uid is not owned by ".
        "the tinderbox id: $tinderbox_uid.\n");

  ( $mode & 02) &&
    die("Security Error. dir: $dir is writable by other.\n");

  return 1;
}


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
  $SECONDS_PER_HOUR = (60*$SECONDS_PER_MINUTE);
  $SECONDS_PER_DAY = (24*$SECONDS_PER_HOUR);
  $SECONDS_PER_WEEK = (7*$SECONDS_PER_DAY);
  $SECONDS_PER_MONTH = (30*$SECONDS_PER_DAY);
  $SECONDS_PER_YEAR = (365*$SECONDS_PER_DAY);

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

  $SECONDS_AGO_ACCEPTABLE = $TinderConfig::SECONDS_AGO_ACCEPTABLE ||
      ($SECONDS_PER_HOUR*10);

  $SECONDS_FROM_NOW_ACCEPTABLE = $TinderConfig::SECONDS_FROM_NOW_ACCEPTABLE ||
      ($SECONDS_PER_MINUTE*10);

  @ORIG_ARGV = @ARGV;

  # taint perl requires we clean up these bad environmental variables.
  
  delete @ENV{'IFS', 'CDPATH', 'ENV', 'BASH_ENV', 'LD_PRELOAD'};

  # sudo deletes these variables as well

  delete @ENV{'KRB_CONF', 'KRB5_CONFIG'};
  delete @ENV{'LOCALDOMAIN', 'RES_OPTIONS', 'HOSTALIASES'};




  # How do we run various commands?  The TinderConfig file contains
  # full path names AND arguments.  We use the list form of the
  # variable to ensure that when these executables are run we are
  # always taint safe.

  @GZIP = @TinderConfig::GZIP;
  @GUNZIP = @TinderConfig::GUNZIP;
  @UUDECODE= @TinderConfig::UUDECODE;
  
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

  umask 0022; 
  $| = 1;
  $PROGRAM = File::Basename::basename($0);
  $TIME = time();
  $PID = $$; 
  $LOCALTIME = localtime($main::TIME);
  $START_TIME = $TIME;

  $HOSTNAME = Sys::Hostname::hostname();

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


sub chk_security {

  # Look at several potential security problems and die if they
  # could cause us problems.

  # Check effective uid of the process to see if we have
  # been configured to run with too many privileges.

  # Ideally we do not want the tinderbox application running with the
  # privileges of a restricted user id (like: root, daemon, bin,
  # mail, adm) this should not happen because users configure it
  # (dangerous) or because of some security accident.

  $tinderbox_uid = $TinderConfig::TINDERBOX_UID;
  $tinderbox_gid = $TinderConfig::TINDERBOX_GID;

  ( $> == $tinderbox_uid ) ||
    die("Security Error. ".
        "Must not run this program using an effective user id ".
        "which is different than the tinderbox user id. ".
        "id: $> id must be $tinderbox_uid\n");

  ( $) == $tinderbox_gid) ||
    die("Security Error. ".
        "Must not run this program using effective group id ".
        "different than the tinderbox group id.".
        "id: $) must be $tinderbox_gid\n");


  my ($logdir) = File::Basename::dirname($ERROR_LOG);
  mkdir_R($logdir);
  security_check_data_dir($logdir);

  my ($lockdir) = File::Basename::dirname($LOCK_FILE);
  mkdir_R($lockdir);
  security_check_data_dir($lockdir);

  my (@trees) = TreeData::get_all_trees();
  foreach $tree (@trees) {

      for my $dir_key (qw(TinderDB_Dir TinderHeader_Dir full-log brief-log)) {
          my $dir = FileStructure::get_filename($tree, $dir_key);
          security_check_data_dir($dir);
      }

  }

  return ;  
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

# send all the rounding functions through here.

sub round {
 my ($number) = @_;

 my $out = sprintf ("%.2f",         # round
                    $number);
 return $out;
}

 
sub clean_times {
    my (@in) = @_;

    # Round all times to nearest minute, so that we do not get two times
    # appearing in the time column which display as the same string.
    # We do however want times which are odd numbers of minutes.
    
    @out = map { ( $_ - ($_%60) ) } @in;
    @out = main::uniq(@out);
    
    # sort numerically descending
    @out = sort {$b <=> $a} @out ;
    
    return @out;
}


# given a time we round down to nearest 5 minutes 

sub round_time {
    my ($time) = @_;

    my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) =
        localtime($time);

    my $remainder = $min % 5;
    $time = $time - ($remainder*$main::SECONDS_PER_MINUTE) - $sec;

    return $time;
}


# make a directory (and all of its parents if need be).

# You can optionally specify the permssions for all the directories
# created.


sub mkdir_R {
  my ($dir, $mode) = @_;

  (-d $dir) && return ;

  my ($default_new_dir_mode) = 0755;

  $mode = $mode || $default_new_dir_mode;

  my @dir = split('/', $dir);

  foreach $i (0..$#dir) {
    my ($dir_prefix) = join('/', @dir[0..$i]);
    ($dir_prefix) || next;

      (-d $dir_prefix) ||
	mkdir ($dir_prefix, $mode) ||
	  die("Could not mkdir: $dir_prefix, for writing: $!\n");
  }
  
  return 1;
}


# Run a system command (list format) and return the list of output.
# If the command has already been run return the value of the previous
# run.

# Due to abstraction issues, sometimes the same command (and
# arguments) is issued at several different parts of the program.
# Since we have just run the command we can use the previous results.
# The cache is not saved to disk so the cache is 'cleared' during each
# execution of the program.

sub cache_cmd {
  my @cmd = @_;

  # good choices for this are '#', '\', '"', '|', ';'

  my ($join_char) = '#';

  ("@cmd" =~ m/$join_char/) &&
    die("cmd '@cmd' can not contain character '$join_char'\n");

  my ($key) = join ($join_char, @cmd);

  # If we already know the results of this command, do not bother to
  # run it again.

  $CMD_CACHE{$key} &&
    return @{ $CMD_CACHE{$key} };

  # We need to run the command

  my ($pid) = open(CMD, "-|");
  
  # did we fork a new process?

  defined ($pid) || 
    die("Could not fork for cmd: '@cmd': $!\n");

  # If we are the child exec. 
  # Remember the exec function returns only if there is an error.

  ($pid) ||
    exec(@cmd) || 
      die("Could not exec: '@cmd': $!\n");

  # If we are the parent read all the childs output. 

  my @cmd_output = <CMD>;
  
  close(CMD) || 
    die("Could not close exec: '@cmd': \$?: $? : \$\!: $!\n");

  ($?) &&
    die("Could not cmd: '@cmd' exited with error: $?\n");

  # save the results of the command in the cache.

  $CMD_CACHE{$key} = [@cmd_output];

  return @cmd_output;
}

# fatal errors need to be valid HTML

sub fatal_error {
# It is important to prevent unprintable characters from getting into
# the log file.  There are attacks against the software which reads
# the logfile using especially crafted escape sequence.

# http://www.securityfocus.com/archive/1/313007
#          or
# http://www.digitaldefense.net/labs/papers/Termulation.txt
#          or
# http://www.digitaldefense.net/labs/papers/Termulation.txt
#    Temulation.txt
#    TERMINAL EMULATOR SECURITY ISSUES Copyright © 2003
#    Digital Defense Incorporated All Rights Reserved

  my  @error = @_;
  foreach $_ (@error) {
    $_ = main::extract_printable_chars($_);
    print LOG "[$LOCALTIME] $_";
  }
  print LOG "\n";

  # Do not check for errors, the lock file may not exist and even if
  # we have trouble removing the file we will be exiting anyway.

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
# It is important to prevent unprintable characters from getting into
# the log file.  There are attacks against the software which reads
# the logfile using especially crafted escape sequence.

# http://www.securityfocus.com/archive/1/313007
#          or
# http://www.digitaldefense.net/labs/papers/Termulation.txt
#    Temulation.txt
#    TERMINAL EMULATOR SECURITY ISSUES Copyright © 2003
#    Digital Defense Incorporated All Rights Reserved

  my  @error = @_;

  foreach $_ (@error) {
    $_ = main::extract_printable_chars($_) ;
    print LOG "[$LOCALTIME] $_";
  }

  return ;
}


sub atomic_rename_file {
  my ($oldfile, $outfile) = @_;

  # This may be the output of a glob, make it taint safe.
  $outfile = main::extract_safe_filename($outfile);

  (-f $outfile) && 
    (!(unlink($outfile))) &&
      die("Could not unlink: $outfile. $!\n");
  
  rename($oldfile, $outfile) ||
    die("Could not rename: '$oldfile to $outfile. $!\n");
  
  return 1;
}


sub overwrite_file {
  my ($outfile, @outdata) = @_;

  # This may be the output of a glob, make it taint safe.
  $outfile = main::extract_safe_filename($outfile);

  my ($dirname) = File::Basename::dirname($outfile);
  my ($basename) = File::Basename::basename($outfile);

  # It is important that the tmp files have a Unique prefix so that
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

  # This may be the output of a glob, make it taint safe.
  $filename = main::extract_filename_chars($filename);

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

# given a hash return a string suitable for printing.

sub hash2string {
    my (%hash) = @_;
    my (@keys) = sort keys %hash;
    my $str;

    foreach $key (@keys) {
        $str .= "\t$key=$hash{$key}\n";
    }

    return $str
}


# load a list of modules

sub require_modules {
  my @impls = @_;

  foreach $impl (@impls) {
    
    # '$impl' is not a bare word so we must perform this data
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


# return true iff the argument is a time which is a reasonable time to
# recive data about (it should not be in the future, or too far in the
# past).

sub is_time_in_sync {
  my ($time) = @_;
  
  $valid = (

            ( ($TIME - $time) <= ($SECONDS_AGO_ACCEPTABLE) ) &&
            ( ($TIME + $time) >= ($SECONDS_FROM_NOW_ACCEPTABLE) )

           );

  return $valid;
}


# Convert dates in form "MM/DD/YY HH:MM:SS" to unix date, there
# are better modules in CPAN to do this kind of conversion but we
# do not wish to create additional dependencies if they are not
# needed.

sub fix_time_format {
  my ($date) = @_;
  my ($new_date)=0;

  chomp $date;
  
  if( $date =~ /^([0-9]+)$/ ){

    # if its in date format leave it alone
    $new_date = $1;
    
  } elsif ( $date =~ 
    /([0-9]+)\/([0-9]+)\/([0-9]+)\s+([0-9]+)\:([0-9]+)\:([0-9]+)/ ){
    
    # if its in a known format convert to date format
    $new_date = timelocal($6,$5,$4,$2,$1-1,$3);
    
  } 

  return $new_date;
}


# ---------- 

# The 'extract' functions will untaint their input data and return
# only data which meets the extraction criterion.

# ----------


# remove unprintable characters from a string

# this is for untainting data which could be almost anything but
# should not be 'binary' data.

sub extract_printable_chars {
  my ($str) = @_;

  # a complete list of printable characters.

  $str =~ s![^a-zA-Z0-9\ \t\n\`\"\'\;\:\,\?\.\-\_\+\=\\\|\/\~\!\@\#\$\%\^\&\*\(\)\{\}\[\]\<\>]+!!g;

  if ( $str =~ m!(.*)!s ) {
      $out = $1;
  } else {
      $out = '';
  }

  return $out;
}


# remove characters are not digits from a string
sub extract_digits {
  my ($str) = @_;

  if ( $str =~ m/([0-9]+)/ ) {
      $out = $1;
  } else {
      $out = '';
  }

  return $out;
}


# remove characters which do not belong in a filename/static URL from
# a string

sub extract_filename_chars {
  my ($str) = @_;

  my $out;

  # This may be the output of a glob, make it taint safe.  We
  # should have full control over our own filenames, we do put all
  # printable characters in filenames.

  if ( $str =~ m/([0-9a-zA-Z\.\-\_\/\:]+)/ ) {
      $out = $1;
  } else {
      $out = '';
  }

  return $out;
}



# ensure that filenames are only coming from directories we are
# allowed to write to or read data from.

sub extract_safe_filename {
  my ($str) = @_;

  $str = extract_filename_chars($str);

  # Restrict possible directories for added security
  my ($prefix1) = $FileStructure::TINDERBOX_DATA_DIR;
  my ($prefix2) = $FileStructure::TINDERBOX_HTML_DIR;

  my $out;
  if ( $str =~ m/^((($prefix1)|($prefix2)).*)/ ) {
      $out = $1;
  } else {
      $out = '';
  }

  return $out;
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

  my $out;
  if ( $user =~ m/([a-zA-Z0-9\_\-\.\%\+\@]+)/ ) {
      $out = $1;
  } else {
      $out = '';
  }

  return $out;
}


1;
