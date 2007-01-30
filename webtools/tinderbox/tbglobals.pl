# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Netscape Public
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
# Contributor(s): 

use strict;
# Reading the log backwards saves time when we only want the tail.
use Backwards;
use Digest::MD5 qw(md5_hex);
use Tie::IxHash;
use FileHandle;
use Fcntl qw(:DEFAULT :flock);

require 'header.pl';

#
# Global variabls and functions for tinderbox
#

#
# Constants
#
$::CI_CHANGE=0;
$::CI_DATE=1;
$::CI_WHO=2;
$::CI_REPOSITORY=3;
$::CI_DIR=4;
$::CI_FILE=5;
$::CI_REV=6;
$::CI_STICKY=7;
$::CI_BRANCH=8;
$::CI_LINES_ADDED=9;
$::CI_LINES_REMOVED=10;
$::CI_LOG=11;

#
# Global variables
#

# Variables set from Makefile
$::bonsai_dir = "@BONSAI_DIR@";
$::bonsai_url = "@BONSAI_URL@";
$::default_cvsroot = "@CVSROOT@";
$::registry_url = "@REGISTRY_URL@";
$::data_dir='data';

@::global_tree_list = ();
undef @::global_tree_list;

%::global_treedata = undef;

# Other globals that are set elsewhere
# $::nowdate
# $::mindate
# $::maxdate
# $::hours
# $::default_hours
# $::rel_path
# $::CVS_ROOT
# $::CVS_REPOS_SUFFIX
# $::CHECKIN_DATA_FILE
# $::CHECKIN_INDEX_FILE
# $::query_module
# $::query_branch
# $::query_branchtype
# $::query_branch_head
# $::query_date_min
# $::query_date_max
# $::query_logexpr
# $::have_mod_map
# $::begin_tag
# $::end_tag
# $::lines_added
# $::lines_removed
# $::versioninfo

# Set this to show real end times for builds instead of just using
# the start of the next build as the end time.
my $display_accurate_build_end_times = 1;

# Format version of treedata.pl
# Use Tie::IxHash to keep order of treedata variables
tie %::default_treedata => 'Tie::IxHash',
    treedata_version => 1,
    who_days => 14,
    use_bonsai => 1,
    use_viewvc => 0,
    cvs_module => '',
    cvs_branch => '',
    cvs_root => '',
    bonsai_tree => '',
    viewvc_url => '',
    viewvc_repository => '',
    viewvc_dbdriver => '',
    viewvc_dbhost => '',
    viewvc_dbport => '',
    viewvc_dbname => '',
    viewvc_dbuser => '',
    viewvc_dbpasswd => '';

1;

sub trick_taint{
    my $in = shift;
    return undef if !defined($in);
    $in =~ /(.*)/;
    return $1;
}

sub make_tree_list {
    return @::global_tree_list if defined(@::global_tree_list);
    while(<*>) {
        if( -d $_ && $_ ne "$::data_dir" && $_ ne 'CVS' && -f "$_/treedata.pl") {
            push @::global_tree_list, $_;
        }
    }
    return @::global_tree_list;
}

sub validate_tree($) {
    my ($tree) = (@_);
    my @treelist = &make_tree_list();
    $tree = undef if (!grep {$tree eq $_} @treelist);
    return $tree;
}

sub require_only_one_tree {
    my ($tree) = (@_);
    $tree = &validate_tree($tree);
    &show_tree_selector, exit if !defined($tree);
    return $tree;
}

sub show_tree_selector {

    print "Content-type: text/html\n\n";

    &EmitHtmlHeader("tinderbox");

    print "<P><TABLE WIDTH=\"100%\">";
    print "<TR><TD ALIGN=CENTER>Select one of the following trees:</TD></TR>";
    print "<TR><TD ALIGN=CENTER>\n";
    print " <TABLE><TR><TD><UL>\n";

    my @list = &make_tree_list();

    foreach (@list) {
        print "<LI><a href=showbuilds.cgi?tree=$_>$_</a>\n";
    }
    print "</UL></TD></TR></TABLE></TD></TR></TABLE>";

    print "<P><TABLE WIDTH=\"100%\">";
    print "<TR><TD ALIGN=CENTER><a href=admintree.cgi>";
    print "Create a new tree</a> or administer one of the following trees:</TD></TR>";
    print "<TR><TD ALIGN=CENTER>\n";
    print " <TABLE><TR><TD><UL>\n";

    foreach (@list) {
        print "<LI><a href=admintree.cgi?tree=$_>$_</a>\n";
    }
    print "</UL></TD></TR></TABLE></TD></TR></TABLE>";
}

sub lock_datafile {
    my ($file) = @_;

    my $lock_fh = new FileHandle ">>$file"
      or die "Couldn't open semaphore file, $file: $!";

    # Get an exclusive lock with a non-blocking request
    unless (flock($lock_fh, LOCK_EX|LOCK_NB)) {
        die "Lock unavailable: $!";
    }
    return $lock_fh;
}

sub unlock_datafile {
    my ($lock_fh) = @_;

    flock $lock_fh, LOCK_UN;  # Free the lock
    close $lock_fh;
}

sub print_time {
  my ($t) = @_;
  my ($sec,$minute,$hour,$mday,$mon,$year);
  ($sec,$minute,$hour,$mday,$mon,$year,undef) = localtime($t);
  sprintf("%d/%02d/%02d&nbsp;%02d:%02d:%02d",$year+1900,$mon+1,$mday,$hour,$minute,$sec);
}

#------------------
# is_today( $time )
#
# Takes a Unix time_t and returns true if it represents a time that has
# the same 'day of year' and 'year' as today.  Returns true if no arguments
# were given.
#
sub is_today {
  my ($tt) = @_;

  my $today = 1;

  if ( $tt ) {
    my $tt_year = (localtime($tt))[5];
    my $tt_yday = (localtime($tt))[7];

    my $now_year = (localtime())[5];
    my $now_yday = (localtime())[7];

    if ( ($tt_year ne $now_year) or ($tt_yday ne $now_yday) ) {
      $today = 0;
    }
  }

  return $today;
}

#---------------------------------
# both_are_today( $time1, $time2 )
#
# Tests both $time1 and $time2 to see if either are not today.  Returns true
# in that case and false otherwise.
#
sub both_are_today {
  my ($t11, $t22) = @_;

  return 0 if ! is_today($t11);
  return 0 if ! is_today($t22);
  return 1;
}

#---------------------------------
# get_local_hms( $time, $qualify )
#
# Converts a Unix time_t value into a date using format HH:MM.  If $qualify
# is true, it qualifies the date by prepending mm/DD.  Returns the resulting
# string.
#
sub get_local_hms {
  my ($t, $need_to_qualify) = @_;

  my (undef,$minute,$hour,$mday,$mon,$year,undef) = localtime($t);
  my $formatted_date = sprintf("%02d:%02d",$hour,$minute);

  if ($need_to_qualify) {
    $formatted_date = sprintf("%d/%02d/%02d",$year+1900,$mon+1,$mday) . " " . $formatted_date;
  }

  return $formatted_date;
}

#--------------------------------------
# get_time_difference( $time1, $time2 )
#
# Calculates a human-readable difference in time between two Unix time_t
# values.  Returns the resulting string.
#
sub get_time_difference {
  my ($t11, $t22) = @_;

  $t11 = 0 if !$t11;
  $t22 = 0 if !$t22;

  # Flip $t11 and $t22 if $t22 isn't later than $t11.
  if ( $t11 gt $t22 ) {
    my $temp = $t11;
    $t11 = $t22;
    $t22 = $temp;
  }

  my $time_diff = $t22 - $t11;

  # @time_slots is an array that we will keep our time difference in.
  #   0 => seconds
  #   1 => minutes
  #   2 => hours
  #   3 => days
  my @time_slots;

  # Zero out the array.
  for my $ii ( 0 .. 3 ) {
    $time_slots[$ii] = 0;
  }
  
  # What's in $time_diff is the difference in seconds.  Modulo 60 to get a
  # number of seconds within a minute.
  $time_slots[0] = $time_diff % 60;
  $time_diff = $time_diff / 60;

  # Now what's left in $time_diff is the difference in minutes.  Modulo 60 to
  # get a number of minutes within an hour.
  $time_slots[1] = $time_diff % 60;
  $time_diff = $time_diff / 60;

  # Now what's left in $time_diff is the difference in hours.  Modulo 24 to get
  # a number of hours within a day.
  $time_slots[2] = $time_diff % 24;
  $time_diff = $time_diff / 24;

  # Now what's left in $time_diff are the number of days.  Floor it to get rid
  # of any fractional left from previous maths.
  $time_slots[3] = floor($time_diff);

  format_time_difference(@time_slots);
}

#--------------------------------------
# format_time_difference( @time_slots )
#
# Called by get_time_difference(), takes its @time_slots arrary and parses it
# into human-readable text.  Returns the resulting string.
#
sub format_time_difference {
  my @time_slots = @_;

  # As above, @time_slots is an array that holds our time difference.
  #   0 => seconds
  #   1 => minutes
  #   2 => hours
  #   3 => days

  my @formatted;

  if ( $time_slots[3] eq 1 ) {
    push(@formatted, "1 day");
  } elsif ( $time_slots[3] gt 1 ) {
    push(@formatted, $time_slots[3] . " days");
  }

  if ( $time_slots[2] eq 1 ) {
    push(@formatted, "1 hour");
  } elsif ( $time_slots[2] gt 1 ) {
    push(@formatted, $time_slots[2] . " hours");
  }

  if ( $time_slots[1] eq 1 ) {
    push(@formatted, "1 minute");
  } elsif ( $time_slots[1] gt 1 ) {
    push(@formatted, $time_slots[1] . " minutes");
  }

  # I love that we can track seconds and let the world know, but the unit is
  # too small to be of utility right now.  Sticking this code in an always-
  # false conditional in case we want to turn this on in the future.
  if ( 0 ) {
    if ( $time_slots[0] eq 1 ) {
      push(@formatted, "1 second");
    } elsif ( $time_slots[0] gt 1 ) {
      push(@formatted, $time_slots[0] . " seconds");
    }
  }

  my $formatted_diff;
  if ( scalar(@formatted) gt 0 ) {
    $formatted_diff = join(', ', @formatted);
  } else {
    $formatted_diff = "no time";
  }

  return $formatted_diff;
}

# This should really adhere to:
# http://www.blooberry.com/indexdot/html/topics/urlencoding.htm
sub url_encode {
  my ($s) = @_;

  # First change all percent signs since later encodings use them as escapes.
  $s =~ s/\%/\%25/g;

  $s =~ s/\=/\%3d/g;
  $s =~ s/\?/\%3f/g;
  $s =~ s/ /\%20/g;
  $s =~ s/\n/\%0a/g;
  $s =~ s/\r//g;
  $s =~ s/\"/\%22/g;
  $s =~ s/\'/\%27/g;
  $s =~ s/\|/\%7c/g;
  $s =~ s/\&/\%26/g;
  $s =~ s/\+/\%2b/g;
  return $s;
}

sub url_decode {
  my ($value) = @_;
  $value =~ tr/+/ /;
  $value =~ s/%([a-fA-F0-9][a-fA-F0-9])/pack("C", hex($1))/eg;
  return $value;
}

sub value_encode {
  my ($s) = @_;
  $s =~ s@&@&amp;@g;
  $s =~ s@<@&lt;@g;
  $s =~ s@>@&gt;@g;
  $s =~ s@\"@&quot;@g;
  return $s;
}

# Quotify a string, suitable for invoking a shell process
sub shell_escape {
    my ($file) = @_;
    $file =~ s/\000/_NULL_/g;
    $file =~ s/([ \"\'\`\~\^\?\$\&\|\!<>\(\)\[\]\;\:])/\\$1/g;
    return $file;
}

sub tb_load_treedata($) {
    my ($tree) = @_;

    # Load $tree/treedata.pl into global tree hash
    # The assumption here is that the treedata.pl represents
    # static server-side data that will not change while
    # the scripts are being run.
    if (!defined($::global_treedata->{$tree})) {
        for my $key (keys %::default_treedata) {
            ${$TreeConfig::{$key}} = undef;
        }
        package TreeConfig;
        do "$tree/treedata.pl" if -r "$tree/treedata.pl";
        package main;
        for my $key (keys %::default_treedata) {
            if (defined(${$TreeConfig::{$key}})) {
                $::global_treedata->{$tree}->{$key} = ${$TreeConfig::{$key}};
            }
        }
    }
    if (!defined($::global_treedata->{$tree})) {
        print STDERR "Cannot gather tree configuration for " .
            &shell_escape($tree) . "\n";
        exit(1);
    }
}

sub tb_load_ignorebuilds($) {
    my ($tree) = (@_);
    my $ignore_builds = {};

    undef $TreeConfig::ignore_builds;
    package TreeConfig;
    do "$tree/ignorebuilds.pl" if -r "$tree/ignorebuilds.pl";
    package main;
    $ignore_builds = $TreeConfig::ignore_builds
        if (defined($TreeConfig::ignore_builds));
    undef $TreeConfig::ignore_builds;
    return $ignore_builds;
}

sub tb_load_scrapebuilds($) {
    my ($tree) = (@_);
    my $scrape_builds = {};

    undef $TreeConfig::scrape_builds;
    package TreeConfig;
    do "$tree/scrapebuilds.pl" if -r "$tree/scrapebuilds.pl";
    package main;
    $scrape_builds = $TreeConfig::scrape_builds
        if (defined($TreeConfig::scrape_builds));
    undef $TreeConfig::scrape_builds;
    return $scrape_builds;
}


sub tb_load_warningbuilds($) {
    my ($tree) = (@_);
    my $warning_builds = {};

    undef $TreeConfig::warning_builds;
    package TreeConfig;
    do "$tree/warningbuilds.pl" if -r "$tree/warningbuilds.pl";
    package main;
    $warning_builds = $TreeConfig::warning_builds
        if (defined($TreeConfig::warning_builds));
    undef $TreeConfig::warning_builds;
    return $warning_builds;
}

sub tb_load_rules($) {
    my ($tree) = (@_);
    my $rules = '';

    undef $TreeConfig::rules_message;
    package TreeConfig;
    do "$tree/rules.pl" if -r "$tree/rules.pl";
    package main;
    $rules = $TreeConfig::rules_message
        if (defined($TreeConfig::rules_message));
    undef $TreeConfig::rules_message;
    return $rules;
}

sub tb_load_sheriff($) {
    my ($tree) = (@_);
    my $sheriff = undef;

    undef $TreeConfig::current_sheriff;
    package TreeConfig;
    do "$tree/sheriff.pl" if -r "$tree/sheriff.pl";
    package main;
    $sheriff = $TreeConfig::current_sheriff
        if (defined($TreeConfig::current_sheriff));
    undef $TreeConfig::current_sheriff;
    return $sheriff;
}

sub tb_load_status($) {
    my ($tree) = (@_);
    my $status = undef;

    undef $TreeConfig::status_message;
    package TreeConfig;
    do "$tree/status.pl" if -r "$tree/status.pl";
    package main;
    $status = $TreeConfig::status_message
        if (defined($TreeConfig::status_message));
    undef $TreeConfig::status_message;
    return $status;
}


sub tb_load_data($) {
    my ($form_ref) = @_;
    my $tree = $form_ref->{tree};

    return undef unless $tree;

    &tb_load_treedata($tree);

    my $td = {};
    $td->{name} = $tree;
    $td->{num} = 0;
    $td->{cvs_module} = $::global_treedata->{$tree}->{cvs_module};
    $td->{cvs_branch} = $::global_treedata->{$tree}->{cvs_branch};
    $td->{ignore_builds} = &tb_load_ignorebuilds($tree);
    $td->{scrape_builds} = &tb_load_scrapebuilds($tree);
    $td->{warning_builds} = &tb_load_warningbuilds($tree);
    $td->{cvs_root} = $::global_treedata->{$tree}->{cvs_root};

    my $build_list = &load_buildlog($td, $form_ref);
  
    &get_build_name_index(\$td, $build_list);
    &get_build_time_index(\$td, $build_list);
  
    &load_who(\$td);

    $td->{build_table} = &make_build_table(\$td, $build_list);

    $td->{scrape}     = &load_scrape($td);
    $td->{warnings}   = &load_warnings($td);

    return $td;
}

sub tb_loadquickparseinfo {
  my ($tree, $build, $times, $includeStatusOfBuilding) = (@_);
  local $_;

  return if (! -d "$tree" || ! -r "$tree/build.dat");
  $::maxdate = time if !defined($::maxdate);
  my $ignore_builds = &tb_load_ignorebuilds($tree);
    
  my $bw = Backwards->new("$tree/build.dat") or die;
    
  my $latest_time = 0;
  my $tooearly = 0;
  while( $_ = $bw->readline ) {
    chop;
    my ($buildtime, $buildname, $buildstatus) = (split /\|/)[1,2,4];
    
    if ($includeStatusOfBuilding or
        $buildstatus =~ /^success|busted|testfailed$/) {

      # Ignore stuff in the future.
      next if $buildtime > $::maxdate;

      $latest_time = $buildtime if $buildtime > $latest_time;

      # Ignore stuff more than 12 hours old
      if ($buildtime < $latest_time - 12*60*60) {
        # Hack: A build may give a bogus time. To compensate, we will
        # not stop until we hit 20 consecutive lines that are too early.
        # XXX bug 225735: This is the wrong way of doing things.  When a
        # flood of backed up mail from one machine comes in, it can easily
        # be more than 20.  We should be looking at the mail receipt time
        # to decide when we're done rather than the build time.

        last if $tooearly++ > 20;
        next;
      }
      $tooearly = 0;

      next if exists $ignore_builds->{$buildname};
      next if exists $build->{$buildname}
              and $times->{$buildname} >= $buildtime;
      
      $build->{$buildname} = $buildstatus;
      $times->{$buildname} = $buildtime;
    }
  }
}

sub tb_last_status($$) {
  my ($td, $build_index) = @_;

  for (my $tt=0; $tt < $td->{time_count}; $tt++) {
    my $br = $td->{build_table}->[$tt][$build_index];
    next unless defined $br and $br != -1 and $br->{buildstatus};
    next unless $br->{buildstatus} =~ /^(success|busted|testfailed)$/;
    return $br->{buildstatus};
  }
  return 'building';
}

sub tb_check_password($$) {
    my ($form_ref, $cj_ref) = (@_);
    my %form = %{$form_ref};
    my %cookie_jar = %{$cj_ref};

    if ($form{password} eq '' and defined $cookie_jar{tinderbox_password}) {
        $form{password} = $cookie_jar{tinderbox_password};
    }
    my $correct = '';
    if (open(REAL, "<", "data/passwd")) {
        $correct = <REAL>;
        close REAL;
        $correct =~ s/\s+$//;   # Strip trailing whitespace.
    }
    $form{password} =~ s/\s+$//;      # Strip trailing whitespace.
    if ($form{password} ne '') {
        my $encoded = md5_hex($form{password});
        $encoded =~ s/\s+$//;   # Strip trailing whitespace.
        if ($encoded eq $correct) {
            if ($form{rememberpassword} ne '') {
                print "Set-Cookie: tinderbox_password=$form{'password'} ;"
                    ." path=/ ; expires = Sun, 1-Mar-2020 00:00:00 GMT\n";
            }
            return;
        }
    }

    # Force a return here to test w/o a password.
    # return;

    require 'header.pl';

    print "Content-type: text/html\n";
    print "Set-Cookie: tinderbox_password= ; path=/ ; "
        ." Expires = Sun, 1-Mar-2020 00:00:00 GMT\n";
    print "\n";

    EmitHtmlHeader("What's the magic word?",
                   "You need to know the magic word to use this page.");

    if ($form{password} ne '') {
        print "<B>Invalid password; try again.<BR></B>";
    }
    print q(
            <FORM method=post>
            <B>Password:</B>
            <INPUT NAME=password TYPE=password><BR>
            <INPUT NAME=rememberpassword TYPE=checkbox>
            If correct, remember password as a cookie<BR>
            );
    
    while (my ($key,$value) = each %form) {
        next if $key eq "password" or $key eq "rememberpassword";

        my $enc_key = value_encode($key);
        my $enc_value = value_encode($value);
        print "<INPUT TYPE=HIDDEN NAME=\"$enc_key\" VALUE=\"$enc_value\">\n";
    }
    print "<INPUT TYPE=SUBMIT value=Submit></FORM>\n";
    exit;
}

sub tb_find_build_record {
  my ($tree, $logfile) = @_;
  local $_;

  my $log_entry = '';
  my ($bw) = Backwards->new("$tree/build.dat") or die;
  while( $_ = $bw->readline ) {
    $log_entry = $_ if /$logfile/;
  }

  chomp($log_entry);
  # Skip the logfile in the parse since it is already known.
  my ($endtime, $buildtime, $buildname, $errorparser,
      $buildstatus, $binaryurl) = (split /\|/, $log_entry)[0..4,6];

  my $buildrec = {    
    endtime     => $endtime,
    buildtime   => $buildtime,
    buildname   => $buildname,
    errorparser => $errorparser,
    buildstatus => $buildstatus,
    logfile     => $logfile,
    binaryurl   => $binaryurl,
    td          => undef
  };
  return $buildrec;
}

sub write_treedata() {
    my ($file, $treedata) = @_;

    open( F, ">", "$file") or die ("$file: $!\n");
    for my $var (keys %$treedata) {
        my $value;
        if ("$var" eq "treedata_version" || "$var" eq "who_days" || 
            "$var" eq "use_bonsai" || "$var" eq "use_viewvc") {
            $value = $treedata->{$var};
        } else {
            $value = "\'$treedata->{$var}\'";
        }
        print F "\$${var}=$value;\n";
    }
    print F "1;\n";
    close( F );
}

# end of public functions
#============================================================

sub load_buildlog($$) {
  my ($treedata, $form_ref) = (@_);

  # In general you always want to make "$_" a local
  # if it is used. That way it is restored upon return.
  local $_;
  my $build_list = [];


  if (not defined $::maxdate) {
    $::maxdate = time();
  }
  if (not defined $::mindate) {
    $::mindate = $::maxdate - 24*60*60;
  }
  
  my ($bw) = Backwards->new("$treedata->{name}/build.dat") or die;

  my $tooearly = 0;
  my $internal_build_list;
  LOOP: while( $_ = $bw->readline ) {
    chomp;
    my ($endtime, $buildtime, $buildname,
     $errorparser, $buildstatus, $logfile, $binaryurl) = split /\|/;
    
    # Ignore stuff in the future.
    next if $buildtime > $::maxdate;
    
    # Ignore stuff in the past (but get a 2 hours of extra data)
    if ($buildtime < $::mindate - 2*60*60) {
      # Occasionally, a build might show up with a bogus time.  So,
      # we won't judge ourselves as having hit the end until we
      # hit a full 20 lines in a row that are too early.
      # XXX bug 225735: This is the wrong way of doing things.  When a
      # flood of backed up mail from one machine comes in, it can easily
      # be more than 20.  We should be looking at the mail receipt time
      # to decide when we're done rather than the build time.
      last if $tooearly++ > 20;
      
      next;
    }
    $tooearly = 0;

    if ($form_ref->{noignore} or not $treedata->{ignore_builds}->{$buildname}) {

      # Latest record in build.dat for this (buildtime, buildname) tuple wins.
      if ( $internal_build_list->{$buildtime}->{$buildname} ) {
        next LOOP;
      } else {
        $internal_build_list->{$buildtime}->{$buildname} = 1;
      }

      my $buildrec = {    
                      endtime     => $endtime,
                      buildtime   => $buildtime,
                      buildname   => $buildname,
                      errorparser => $errorparser,
                      buildstatus => $buildstatus,
                      logfile     => $logfile,
                      binaryurl   => $binaryurl,
                      td          => $treedata
                     };
      push @{$build_list}, $buildrec;
    }
  }
  return $build_list;
}

# Load data about who checked in when
#   File format: <build_time>|<email_address>
#
sub load_who {
  my ($td_ref) = @_;
  local $_;

  my $who_list = [];

  open(WHOLOG, "<", "${$td_ref}->{name}/who.dat");
  while (<WHOLOG>) {
    chomp;
    my ($checkin_time, $email) = split /\|/;

    # Find the time slice where this checkin belongs.
    for (my $ii = ${$td_ref}->{time_count} - 1; $ii >= 0; $ii--) {
      if ($checkin_time < ${$td_ref}->{build_time_times}->[$ii]) {
        $who_list->[$ii+1]->{$email} = 1;
        last;
      } elsif ($ii == 0) {
        $who_list->[0]->{$email} = 1;
      }
    }
  }

  # Ignore the last one
  #
  #if (${$td_ref}->{time_count} > 0) {
  #  $who_list->[${$td_ref}->{time_count}] = {};
  #}

  # Return results
  ${$td_ref}->{who_list} = $who_list;
}



# Load data about scrape data.
#   File format: <logfile>|<aaa>|<bbb>|...
#
sub load_scrape {
  my $treedata = $_[0];
  local $_;

  my $scrape = {};
  
  open(SCRAPELOG, "<", "$treedata->{name}/scrape.dat");
  while (<SCRAPELOG>) {
    chomp;
    my @list =  split /\|/;
    my $logfile = @list[0];
    shift(@list);

    $scrape->{$logfile} = [ @list ];
  }
  return $scrape;
}


# Load data about build warnings
#   File format: <logfile>|<warning_count>
#
sub load_warnings {
  my $treedata = $_[0];
  local $_;

  my $warnings = {};

  open(WARNINGLOG, "<", "$treedata->{name}/warnings.dat");
  while (<WARNINGLOG>) {
    chomp;
    my ($logfile, $warning_count) = split /\|/;
    $warnings->{$logfile} = $warning_count;
  }
  return $warnings;
}

sub get_build_name_index {
  my ($td_ref, $build_list) = @_;

  my $build_name_index = {};     
  my $build_names = [];
  my $name_count = 0;

  # Get all the unique build names.
  #
  foreach my $build_record (@{$build_list}) {
    $build_name_index->{$build_record->{buildname}} = 1;
  }
    
  my $ii = 0;
  foreach my $name (sort keys %{$build_name_index}) {
    $build_names->[$ii] = $name;
    $build_name_index->{$name} = $ii;
    $ii++;
  }
  $name_count = $#{$build_names} + 1;

  # Return results
  ${$td_ref}->{build_name_index} = $build_name_index;
  ${$td_ref}->{build_names} = $build_names;
  ${$td_ref}->{name_count} = $name_count;
}

sub get_build_time_index {
  my ($td_ref, $build_list) = @_;

  my $build_time_index = {};
  my $build_time_times = [];
  my $mindate_time_count = 0;  # time_count that corresponds to the mindate
  my $time_count = 0;

  # Get all the unique build names.
  #
  foreach my $br (@{$build_list}) {
    $build_time_index->{$br->{buildtime}} = 1;
    if ($display_accurate_build_end_times) {
      $build_time_index->{$br->{endtime}} = 1;
    }
  }

  my $ii = 0;
  foreach my $time (sort {$b <=> $a} keys %{$build_time_index}) {
    $build_time_times->[$ii] = $time;
    $build_time_index->{$time} = $ii;
    $mindate_time_count = $ii if $time >= $::mindate;
    $ii++;
  }
  $time_count = $#{$build_time_times} + 1;

  # Return results
  ${$td_ref}->{build_time_times} = $build_time_times;
  ${$td_ref}->{build_time_index} = $build_time_index;
  ${$td_ref}->{mindate_time_count} = $mindate_time_count;
  ${$td_ref}->{time_count} = $time_count;
}

sub make_build_table {
    my ($td_ref, $build_list) = @_;
    my ($ti, $bi, $ti1, $br, $br1);

    my $build_table = [];

    # Create the build table
    #
    for (my $ii=0; $ii < ${$td_ref}->{time_count}; $ii++){
        $build_table->[$ii] = [];
    }

    # Populate the build table with build data
    #
    foreach $br (reverse @{$build_list}) {
        $ti = ${$td_ref}->{build_time_index}->{$br->{buildtime}};
        $bi = ${$td_ref}->{build_name_index}->{$br->{buildname}};
        $build_table->[$ti][$bi] = $br;
    }

    # Notes need to be loaded here before the build_table is modified below
    &load_notes($td_ref, \$build_table);

    for ($bi = ${$td_ref}->{name_count} - 1; $bi >= 0; $bi--) {
        for ($ti = ${$td_ref}->{time_count} - 1; $ti >= 0; $ti--) {
            if (defined($br = $build_table->[$ti][$bi])
                and $br != -1
                and not defined($br->{'rowspan'})) {

                # Find the next-defined cell after us.  We may run all the 
                # way to the end of the page and not find a defined cell.
                # That's okay.
                $ti1 = $ti+1;
                while ($ti1 < ${$td_ref}->{time_count} and 
                        not defined $build_table->[$ti1][$bi]) {
                    $ti1++;
                }
                if (defined($br1 = $build_table->[$ti1][$bi])) {
                    $br->{previousbuildtime} = $br1->{buildtime};
                }

                $ti1 = $ti-1;
                
                if ($br->{buildstatus} eq 'building' or 
                    not $display_accurate_build_end_times) {
                    # If the current record represents a system that's 
                    # still building, we'll use the old style and let the 
                    # build window "slide" up to the next defined build record.
                    while ( $ti1 >= 0 and 
                            not defined $build_table->[$ti1][$bi] ) {
                        $build_table->[$ti1][$bi] = -1;
                        $ti1--;
                    }
                } else {
                    # If the current record has a non 'building' status, 
                    # we stop the build window at its "endtime".
                    while ( $ti1 >= 0
                            and not defined $build_table->[$ti1][$bi]
                            and ${$td_ref}->{build_time_times}->[$ti1] < 
                            $br->{endtime} ) {
                        $build_table->[$ti1][$bi] = -1;
                        $ti1--;
                    }
                }

                if ($ti1 > 0 and defined($br1 = $build_table->[$ti1][$bi])) {
                    $br->{nextbuildtime} = $br1->{buildtime};
                }

                $br->{rowspan} = $ti - $ti1;
                unless ($br->{rowspan} == 1) {
                    $build_table->[$ti1+1][$bi] = $br;
                    $build_table->[$ti][$bi] = -1;
                }
            }
        }
    }

  # After we've filled out $build_table with all of the applicable build
  # records, we can do another pass over the table and fill in all the other
  # pieces with null records to pretty up our output.  Ain't that fanciful.
  #
  # When we're not using the optional "show real end times" feature, this
  # section of code is a no-op.  Every cell will be defined, either with a
  # real $br or being set to -1.

    for ($bi = ${$td_ref}->{name_count} - 1; $bi >= 0; $bi--) {
        for ($ti = ${$td_ref}->{time_count} - 1; $ti >= 0; $ti--) {
            if (not defined($build_table->[$ti][$bi])) {
                my $ti1 = $ti;
                while ( $ti1 >= 0 and not defined $build_table->[$ti1][$bi] ) {
                    $build_table->[$ti1][$bi] = -1;
                    $ti1--;
                }

                my $null_record_br = {};
                $null_record_br->{buildstatus} = "null";
                $null_record_br->{rowspan} = $ti - $ti1;
                $build_table->[$ti1+1][$bi] = $null_record_br;
            }
        }
    }

    # Return results
    ${$td_ref}->{build_table} = $build_table;
}

sub load_notes(\$\$) {
    my ($td_ref, $bt_ref) = (@_);
    my @note_array = ();

    open(NOTES, "<", "${$td_ref}->{name}/notes.txt") 
        or print "<h2>warning: Couldn't open ${$td_ref}->{name}/notes.txt </h2>\n";
    while (<NOTES>) {
        chomp;
        my ($nbuildtime,$nbuildname,$nwho,$nnow,$nenc_note) = split /\|/;
        my $ti = ${$td_ref}->{build_time_index}->{$nbuildtime};
        my $bi = ${$td_ref}->{build_name_index}->{$nbuildname};

        if (defined $ti and defined $bi) {
            ${$bt_ref}->[$ti][$bi]->{hasnote} = 1;
            unless (defined ${$bt_ref}->[$ti][$bi]->{noteid}) {
                ${$bt_ref}->[$ti][$bi]->{noteid} = $#note_array + 1;
            }
            my $noteid = ${$bt_ref}->[$ti][$bi]->{noteid};
            my $now_str = &print_time($nnow);
            my $note = &url_decode($nenc_note);

            $note_array[$noteid] = '' unless $note_array[$noteid];
            $note_array[$noteid] = "<pre>\n[<b><a href=mailto:$nwho>"
                ."$nwho</a> - $now_str</b>]\n$note\n</pre>"
                .$note_array[$noteid];
        }
    }
    close NOTES;
    # Return results
    ${$td_ref}->{note_array} = \@note_array;
}

sub split_cgi_args {
    my (@args, $pair, $key, $value, $s, %form);
    
    if ($ENV{"REQUEST_METHOD"} eq 'POST') {
        $s .= $_ while (<>);
    }
    else {
        $s = $ENV{"QUERY_STRING"};
    }
    
    $s =~ tr/+/ /;
    @args= split(/\&/, $s );
    
    for $pair (@args) {
        ($key, $value) = split(/=/, $pair);
        $key   =~ s/%([a-fA-F0-9]{2})/pack("C", hex($1))/eg;
        $value =~ s/%([a-fA-F0-9]{2})/pack("C", hex($1))/eg;
        $form{$key} = $value;
    }
    
    return %form;
}

sub split_cookie_args {
    # extract the cookies from the HTTP_COOKIE environment 
    my %cookie_jar = split('[;=] *',$ENV{'HTTP_COOKIE'});
    return %cookie_jar;
}

sub make_cgi_args {
    my (%form) = (@_);
    my ($k,$v,$ret);
    for $k (sort keys %form){
        $ret .= ($ret eq "" ? '?' : '&');
        $v = $form{$k};
        $ret .= &url_encode($k);
        $ret .= '=';
        $ret .= &url_encode($v);
    }
    return $ret;
}

my @weekdays = ('Sun','Mon','Tue','Wed','Thu','Fri','Sat');
my @months = ('Jan','Feb','Mar','Apr','May','Jun',
           'Jul','Aug','Sep','Oct','Nov','Dec');

sub toGMTString {
    my ($seconds) = $_[0];

    my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst)
        = gmtime($seconds);
    $year += 1900;

    sprintf('%s, %02d-%s-%d %02d:%02d:%02d GMT',
            $weekdays[$wday],$mday,$months[$mon],$year,$hour,$min,$sec);
}

