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

# Reading the log backwards saves time when we only want the tail.
use Backwards;

#
# Global variabls and functions for tinderbox
#

#
# Global variables
#

# From load_data()
$ignore_builds = {};

# From get_build_name_index()
$build_name_index = {};     
$build_names = [];
$name_count = 0;

# Frome get_build_time_index()
$build_time_index = {};
$build_time_times = [];
$mindate_time_count = 0;  # time_count that corresponds to the mindate
$time_count = 0;

$build_table = [];
$who_list = [];
@note_array = ();

$gzip = '/usr/local/bin/gzip';

$data_dir='data';

1;

sub lock{
}

sub unlock{
}

sub print_time {
  my ($t) = @_;
  my ($minute,$hour,$mday,$mon);
  (undef,$minute,$hour,$mday,$mon,undef) = localtime($t);
  sprintf("%02d/%02d&nbsp;%02d:%02d",$mon+1,$mday,$hour,$minute);
}

sub url_encode {
  my ($s) = @_;

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


BEGIN {
  my $have_loaded_treedata = 0;

  sub tb_load_treedata {
    my $tree = shift;

    return if $have_loaded_treedata;
    $have_loaded_treedata = 1;
    require "$tree/treedata.pl" if -r "$tree/treedata.pl";
  }
}

sub tb_load_data {
  $tree = $form{'tree'};

  return undef unless $tree;
  
  tb_load_treedata($tree);
        
  $ignore_builds = {};

  require "$tree/ignorebuilds.pl" if -r "$tree/ignorebuilds.pl";
        
  $td = {};
  $td->{name} = $tree;
  $td->{num} = 0;
  $td->{cvs_module} = $cvs_module;
  $td->{cvs_branch} = $cvs_branch;
  $td->{ignore_builds} = $ignore_builds;
  $cvs_root = '/m/src' if $cvs_root eq '';
  $td->{cvs_root} = $cvs_root;

  $build_list = load_buildlog($td);
  
  get_build_name_index($build_list);
  get_build_time_index($build_list);
  
  load_who($td, $who_list);

  make_build_table($td, $build_list);

  $td->{bloaty} = load_bloaty($td);
  $td->{warnings} = load_warnings($td);

  return $td;
}

sub tb_loadquickparseinfo {
  my ($tree, $build, $times, $includeStatusOfBuilding) = (@_);
  local $_;

  $maxdate = time;
  require "$tree/ignorebuilds.pl" if -r "$tree/ignorebuilds.pl";
    
  my $bw = Backwards->new("$tree/build.dat") or die;
    
  my $latest_time = 0;
  my $tooearly = 0;
  while( $_ = $bw->readline ) {
    chop;
    my ($buildtime, $buildname, $buildstatus) = (split /\|/)[1,2,4];
    
    if ($includeStatusOfBuilding or
        $buildstatus =~ /^success|busted|testfailed$/) {

      # Ignore stuff in the future.
      next if $buildtime > $maxdate;

      $latest_time = $buildtime if $buildtime > $latest_time;

      # Ignore stuff more than 12 hours old
      if ($buildtime < $latest_time - 12*60*60) {
        # Hack: A build may give a bogus time. To compensate, we will
        # not stop until we hit 20 consecutive lines that are too early.

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

sub tb_last_status {
  my ($build_index) = @_;

  for (my $tt=0; $tt < $time_count; $tt++) {
    my $br = $build_table->[$tt][$build_index];
    next unless defined $br and $br->{buildstatus};
    next unless $br->{buildstatus} =~ /^(success|busted|testfailed)$/;
    return $br->{buildstatus};
  }
  return 'building';
}

sub tb_check_password {
  if ($form{password} eq '' and defined $cookie_jar{tinderbox_password}) {
    $form{password} = $cookie_jar{tinderbox_password};
  }
  my $correct = '';
  if (open(REAL, '<data/passwd')) {
    $correct = <REAL>;
    close REAL;
    $correct =~ s/\s+$//;   # Strip trailing whitespace.
  }
  $form{password} =~ s/\s+$//;      # Strip trailing whitespace.
  if ($form{password} ne '') {
    open(TRAPDOOR, "../bonsai/data/trapdoor $form{'password'} |") 
      or die "Can't run trapdoor func!";
    my $encoded = <TRAPDOOR>;
    close TRAPDOOR;
    $encoded =~ s/\s+$//;   # Strip trailing whitespace.
    if ($encoded eq $correct) {
      if ($form{rememberpassword} ne '') {
        print "Set-Cookie: tinderbox_password=$form{'password'} ;"
             ." path=/ ; expires = Sun, 1-Mar-2020 00:00:00 GMT\n";
      }
      return;
    }
  }

  require 'header.pl';

  print "Content-type: text/html\n";
  print "Set-Cookie: tinderbox_password= ; path=/ ; "
       ." expires = Sun, 1-Mar-2020 00:00:00 GMT\n";
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

    my $enc = value_encode($value);
    print "<INPUT TYPE=HIDDEN NAME=$key VALUE=\"$enc\">\n";
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
  my ($mailtime, $buildtime, $buildname, $errorparser,
      $buildstatus, $binaryurl) = (split /\|/, $log_entry)[0..4,6];

  $buildrec = {    
    mailtime    => $mailtime,
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

sub tb_build_static {
  # Build tinderbox static pages
  $ENV{QUERY_STRING}="tree=$tree&static=1";
  $ENV{REQUEST_METHOD}="GET";
  system './showbuilds.cgi >/dev/null&';
}

# end of public functions
#============================================================

sub load_buildlog {
  my ($treedata) = $_[0];

  # In general you always want to make "$_" a local
  # if it is used. That way it is restored upon return.
  local $_;
  my $build_list = [];


  if (not defined $maxdate) {
    $maxdate = time();
  }
  if (not defined $mindate) {
    $mindate = $maxdate - 24*60*60;
  }
  
  my ($bw) = Backwards->new("$treedata->{name}/build.dat") or die;

  my $tooearly = 0;
  while( $_ = $bw->readline ) {
    chomp;
    my ($mailtime, $buildtime, $buildname,
     $errorparser, $buildstatus, $logfile, $binaryurl) = split /\|/;
    
    # Ignore stuff in the future.
    next if $buildtime > $maxdate;
    
    # Ignore stuff in the past (but get a 2 hours of extra data)
    if ($buildtime < $mindate - 2*60*60) {
      # Occasionally, a build might show up with a bogus time.  So,
      # we won't judge ourselves as having hit the end until we
      # hit a full 20 lines in a row that are too early.
      last if $tooearly++ > 20;
      
      next;
    }
    $tooearly = 0;
    if ($form{noignore} or not $treedata->{ignore_builds}->{$buildname}) {
      my $buildrec = {    
                      mailtime    => $mailtime,
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
  my ($treedata, $who_list) = @_;
  local $_;
  
  open(WHOLOG, "<$treedata->{name}/who.dat");
  while (<WHOLOG>) {
    chomp;
    my ($checkin_time, $email) = split /\|/;

    # Find the time slice where this checkin belongs.
    for (my $ii = $time_count - 1; $ii >= 0; $ii--) {
      if ($checkin_time < $build_time_times->[$ii]) {
        $who_list->[$ii+1]->{$email} = 1;
        last;
      } elsif ($ii == 0) {
        $who_list->[0]->{$email} = 1;
      }
    }
  }

  # Ignore the last one
  #
  #if ($time_count > 0) {
  #  $who_list->[$time_count] = {};
  #}
}
    

# Load data about code bloat
#   File format: <build_time>|<build_name>|<leak_delta>|<bloat_delta>
#
sub load_bloaty {
  my $treedata = $_[0];
  local $_;

  my $bloaty = {};
  my ($bloat_baseline,  $leaks_baseline)  = (0,0);

  open(BLOATLOG, "<$treedata->{name}/bloat.dat");
  while (<BLOATLOG>) {
    chomp;
    my ($logfile, $leaks, $bloat) = split /\|/;

    # Allow 1k of noise
    my $leaks_cmp = int(($leaks - $leaks_baseline) / 1000);
    my $bloat_cmp = int(($bloat - $bloat_baseline) / 1000);
    
    # If there was a rise or drop, set a new baseline
    $leaks_baseline = $leaks unless $leaks_cmp == 0;
    $bloat_baseline = $bloat unless $bloat_cmp == 0;

    $bloaty->{$logfile} = [ $leaks, $bloat, $leaks_cmp, $bloat_cmp ];
  }
  return $bloaty;
}

# Load data about build warnings
#   File format: <logfile>|<warning_count>
#
sub load_warnings {
  my $treedata = $_[0];
  local $_;

  my $warnings = {};

  open(WARNINGLOG, "<$treedata->{name}/warnings.dat");
  while (<WARNINGLOG>) {
    chomp;
    my ($logfile, $warning_count) = split /\|/;
    $warnings->{$logfile} = $warning_count;
  }
  return $warnings;
}

sub get_build_name_index {
  my ($build_list) = @_;

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
}

sub get_build_time_index {
  my ($build_list) = @_;

  # Get all the unique build names.
  #
  foreach my $br (@{$build_list}) {
    $build_time_index->{$br->{buildtime}} = 1;
  }

  my $ii = 0;
  foreach my $time (sort {$b <=> $a} keys %{$build_time_index}) {
    $build_time_times->[$ii] = $time;
    $build_time_index->{$time} = $ii;
    $mindate_time_count = $ii if $time >= $mindate;
    $ii++;
  }
  $time_count = $#{$build_time_times} + 1;
}

sub make_build_table {
  my ($treedata, $build_list) = @_;
  my ($ti, $bi, $ti1, $br);

  # Create the build table
  #
  for (my $ii=0; $ii < $time_count; $ii++){
    $build_table->[$ii] = [];
  }

  # Populate the build table with build data
  #
  foreach $br (reverse @{$build_list}) {
    $ti = $build_time_index->{$br->{buildtime}};
    $bi = $build_name_index->{$br->{buildname}};
    $build_table->[$ti][$bi] = $br;
  }

  &load_notes($treedata);

  for ($bi = $name_count - 1; $bi >= 0; $bi--) {
    for ($ti = $time_count - 1; $ti >= 0; $ti--) {
      if (defined($br = $build_table->[$ti][$bi])
          and not defined($br->{rowspan})) {

        # If the cell immediately after us is defined, then we 
        #  can have a previousbuildtime.
        if (defined($br1 = $build_table->[$ti+1][$bi])) {
          $br->{previousbuildtime} = $br1->{buildtime};
        }

        $ti1 = $ti-1;
        while ($ti1 >= 0 and not defined $build_table->[$ti1][$bi]) {
          $build_table->[$ti1][$bi] = -1;
          $ti1--;
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
}

sub load_notes {
  my $treedata = $_[0];

  open(NOTES,"<$treedata->{name}/notes.txt") 
    or print "<h2>warning: Couldn't open $treedata->{name}/notes.txt </h2>\n";
  while (<NOTES>) {
    chop;
    my ($nbuildtime,$nbuildname,$nwho,$nnow,$nenc_note) = split /\|/;
    my $ti = $build_time_index->{$nbuildtime};
    my $bi = $build_name_index->{$nbuildname};

    if (defined $ti and defined $bi) {
      $build_table->[$ti][$bi]->{hasnote} = 1;
      unless (defined $build_table->[$ti][$bi]->{noteid}) {
        $build_table->[$ti][$bi]->{noteid} = $#note_array + 1;
      }
      $noteid = $build_table->[$ti][$bi]->{noteid};
      $now_str = &print_time($nnow);
      $note = &url_decode($nenc_note);
      
      $note_array[$noteid] = '' unless $note_array[$noteid];
      $note_array[$noteid] = "<pre>\n[<b><a href=mailto:$nwho>"
        ."$nwho</a> - $now_str</b>]\n$note\n</pre>"
        .$note_array[$noteid];
    }
  }
  close NOTES;
}


