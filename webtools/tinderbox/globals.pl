# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
# License for the specific language governing rights and limitations
# under the License.
#
# The Original Code is the Tinderbox build tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are Copyright (C) 1998
# Netscape Communications Corporation. All Rights Reserved.

#
# Global variabls and functions for tinderbox
#

#
# Global variables
#

$td1 = {};
$td2 = {};

$build_list = [];           # array of all build records
$build_name_index = {};
$ignore_builds = {};
$build_name_names = [];
$name_count = 0;

$build_time_index = {};
$build_time_times = [];
$time_count = 0;
$mindate_time_count = 0;  # time_count that corresponds to the mindate

$build_table = [];
$who_list = [];
$who_list2 = [];
@note_array = ();


#$body_tag = "<BODY TEXT=#000000 BGCOLOR=#8080C0 LINK=#FFFFFF VLINK=#800080 ALINK=#FFFF00>";
#$body_tag = "<BODY TEXT=#000000 BGCOLOR=#FFFFC0 LINK=#0000FF VLINK=#800080 ALINK=#FF00FF>";
if( $ENV{'USERNAME'} eq 'ltabb' ){
    $gzip = 'gzip';
}
else {
    $gzip = '/usr/local/bin/gzip';
}

$data_dir='data';

$lock_count = 0;

1;

sub lock{
    #if( $lock_count == 0 ){
    #    print "locking $tree/LOCKFILE.lck\n";
    #    open( LOCKFILE_LOCK, ">$tree/LOCKFILE.lck" );
    #    flock( LOCKFILE_LOCK, 2 );
    #}
    #$lock_count++;

}

sub unlock{
    #$lock_count--;
    #if( $lock_count == 0 ){
    #    flock( LOCKFILE_LOCK, 8 );
    #    close( LOCKFILE_LOCK );
    #}
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


sub load_data {
  $tree2 = $form{'tree2'};
  if( $tree2 ne '' ){
    require "$tree2/treedata.pl";
    if( -r "$tree2/ignorebuilds.pl" ){
      require "$tree2/ignorebuilds.pl";
    }

    $td2 = {};
    $td2->{name} = $tree2;
    $td2->{cvs_module} = $cvs_module;
    $td2->{cvs_branch} = $cvs_branch;
    $td2->{num} = 1;
    $td2->{ignore_builds} = $ignore_builds;
    if( $cvs_root eq '' ){
      $cvs_root = '/m/src';
    }
    $td2->{cvs_root} = $cvs_root;
    
    $tree = $form{'tree'};
    require "$tree/treedata.pl";
    if( $cvs_root eq '' ){
      $cvs_root = '/m/src';
    }
  }

  $tree = $form{'tree'};

  return unless $tree;
  #die "the 'tree' parameter must be provided\n" unless $tree;
  
  if ( -r "$tree/treedata.pl" ) {
    require "$tree/treedata.pl";
  }

  $ignore_builds = {};
  if( -r "$tree/ignorebuilds.pl" ){
    require "$tree/ignorebuilds.pl";
  }
        
  $td1 = {};
  $td1->{name} = $tree;
  $td1->{num} = 0;
  $td1->{cvs_module} = $cvs_module;
  $td1->{cvs_branch} = $cvs_branch;
  $td1->{ignore_builds} = $ignore_builds;
  if( $cvs_root eq '' ){
    $cvs_root = '/m/src';
  }
  $td1->{cvs_root} = $cvs_root;

  &lock;
  &load_buildlog;
  &unlock;
  
  &get_build_name_index;
  &get_build_time_index;
  
  &load_who($who_list, $td1);
  if( $tree2 ne '' ){
    &load_who($who_list2, $td2);
  }

  &make_build_table;
}

sub load_buildlog {
  my $mailtime, $buildtime, $buildname, $errorparser;
  my $buildstatus, $logfile,$binaryname;
  my $buildrec, @treelist, $t;

  if (not defined $maxdate) {
    $maxdate = time();
  }
  if (not defined $mindate) {
    $mindate = $maxdate - 24*60*60;
  }
  
  if ($tree2 ne '') { 
    @treelist = ($td1, $td2);
  }
  else {
    @treelist = ($td1);
  }
  
  for $t (@treelist) {
    use Backwards;
    my ($bw) = Backwards->new("$t->{name}/build.dat") or die;

    my $tooearly = 0;
    while( $_ = $bw->readline ) {
      chomp;
      ($mailtime, $buildtime, $buildname,
       $errorparser, $buildstatus, $logfile, $binaryname) = split /\|/;
      
      $buildtime -= $buildtime % 60; # Round to minute
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
      $buildrec = {    
         mailtime    => $mailtime,
         buildtime   => $buildtime,
         buildname   => ($tree2 ne '' ? $t->{name} . ' ' : '' ) . $buildname,
         errorparser => $errorparser,
         buildstatus => $buildstatus,
         logfile     => $logfile,
         binaryname  => $binaryname,
         td          => $t
      };
      if ($form{noignore} or not $t->{ignore_builds}->{$buildname}) {
        push @{$build_list}, $buildrec;
      }
    }
  }
}

sub loadquickparseinfo {
  my ($tree, $build, $times) = (@_);

  do "$tree/ignorebuilds.pl";
    
  use Backwards;

  my ($bw) = Backwards->new("$form{tree}/build.dat") or die;
    
  my $latest_time = 0;
  my $tooearly = 0;
  while( $_ = $bw->readline ) {
    chop;
    my ($buildtime, $buildname, $buildstatus) = (split /\|/)[1,2,4];
    
    if ($buildstatus =~ /^success|busted|testfailed$/) {

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

sub load_who {
  my ($who_list, $td) = @_;
  my $d, $w, $i, $bfound;
  
  open(WHOLOG, "<$td->{name}/who.dat");
  while (<WHOLOG>) {
    $i = $time_count;
    chop;
    ($d,$w) = split /\|/;
    $bfound = 0;
    while ($i > 0 and not $bfound) {
      if ($d <= $build_time_times->[$i]) {
        $who_list->[$i+1]->{$w} = 1;
        $bfound = 1;
      }
      else {
        $i--;
      }
    }
  }

  # Ignore the last one
  #
  if ($time_count > 0) {
    $who_list->[$time_count] = {};
  }
}
    
sub get_build_name_index {
  my $i,$br;

  # Get all the unique build names.
  #
  foreach $br (@{$build_list}) {
    $build_name_index->{$br->{buildname}} = 1;
  }
    
  $i = 1;
  foreach $n (sort keys (%{$build_name_index})) {
    $build_name_names->[$i] = $n;
    $i++;
  }

  $name_count = @{$build_name_names}-1;

  # Update the map so it points to the right index
  #
  for ($i=1; $i < $name_count+1; $i++) {
    $build_name_index->{$build_name_names->[$i]} = $i;
  }
}

sub get_build_time_index {
  my $i,$br;

  # Get all the unique build names.
  #
  foreach $br (@{$build_list}) {
    $build_time_index->{$br->{buildtime}} = 1;
  }

  $i = 1;
  foreach $n (sort {$b <=> $a} keys (%{$build_time_index})) {
    $build_time_times->[$i] = $n;
    $mindate_time_count = $i if $n >= $mindate;
    $i++;
  }

  $time_count = @{$build_time_times}-1;

  # Update the map so it points to the right index
  #
  for ($i=1; $i < $time_count+1; $i++) {
    $build_time_index->{$build_time_times->[$i]} = $i;
  }
  
  #for $i (@{$build_time_times}) {
  #    print $i . "\n";
  #}
  
  #while( ($k,$v) = each(%{$build_time_index})) {
  #    print "$k=$v\n";
  #}
}

sub make_build_table {
  my $i,$ti,$bi,$ti1,$br;

  # Create the build table
  #
  for ($i=1; $i <= $time_count; $i++){
    $build_table->[$i] = [];
  }

  # Populate the build table with build data
  #
  foreach $br (reverse @{$build_list}) {
    $ti = $build_time_index->{$br->{buildtime}};
    $bi = $build_name_index->{$br->{buildname}};
    $build_table->[$ti][$bi] = $br;
  }

  &load_notes;

  for ($bi = $name_count; $bi > 0; $bi--) {
    for ($ti = $time_count; $ti > 0; $ti--) {
      if (defined($br = $build_table->[$ti][$bi])
          and not defined($br->{rowspan})) {

        # If the cell immediately after us is defined, then we 
        #  can have a previousbuildtime.
        if (defined($br1 = $build_table->[$ti+1][$bi])) {
          $br->{previousbuildtime} = $br1->{buildtime};
        }
                                
        $ti1 = $ti-1;
        while ($ti1 > 0 and not defined($build_table->[$ti1][$bi])) {
          $build_table->[$ti1][$bi] = -1;
          $ti1--;
        }
        $br->{rowspan} = $ti - $ti1;
        if ($br->{rowspan} != 1) {
          $build_table->[$ti1+1][$bi] = $br;
          $build_table->[$ti][$bi] = -1;
        }
      }
    }
  }
}

sub load_notes {
  if ($tree2 ne '') { 
    @treelist = ($td1, $td2);
  }
  else {
    @treelist = ($td1);
  }

  foreach $t (@treelist) {
    open(NOTES,"<$t->{name}/notes.txt") 
      or print "<h2>warning: Couldn't open $t->{name}/notes.txt </h2>\n";
    while (<NOTES>) {
      chop;
      ($nbuildtime,$nbuildname,$nwho,$nnow,$nenc_note) = split /\|/;
      $nbuildname = "$t->{name} $nbuildname" if $tree2 ne '';
      $ti = $build_time_index->{$nbuildtime};
      $bi = $build_name_index->{$nbuildname};
      #print "[ti = $ti][bi=$bi][buildname='$nbuildname' $_<br>";
      if ($ti != 0 and $bi != 0) {
        $build_table->[$ti][$bi]->{hasnote} = 1;
        if (not defined($build_table->[$ti][$bi]->{noteid})) {
          $build_table->[$ti][$bi]->{noteid} = (0+@note_array);
        }
        $noteid = $build_table->[$ti][$bi]->{noteid};
        $now_str = &print_time($nnow);
        $note = &url_decode($nenc_note);
        $note_array[$noteid] = "<pre>\n[<b><a href=mailto:$nwho>"
             ."$nwho</a> - $now_str</b>]\n$note\n</pre>"
             .$note_array[$noteid];
      }
    }
    close(NOTES);
  }
}

sub last_success_time {
  my ($row) = @_;

  for (my $tt=1; $tt <= $time_count; $tt++) {
    my $br = $build_table->[$tt][$row];
    next unless defined $br;
    next unless $br->{buildstatus} eq 'success';
    return $build_time_times->[$tt + $br->{rowspan} ];
  }
  return 0;
}

sub last_status {
  my ($row) = @_;

  for (my $tt=1; $tt <= $time_count; $tt++) {
    my $br = $build_table->[$tt][$row];
    next unless defined $br;
    next unless $br->{buildstatus} =~ /^(success|busted|testfailed)$/;
    return $br->{buildstatus};
  }
  return 'building';
}

sub check_password {
  if ($form{password} eq '') {
    if (defined $cookie_jar{tinderbox_password}) {
      $form{password} = $cookie_jar{tinderbox_password};
    }
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
    print "<INPUT TYPE=HIDDEN NAME=$key VALUE='$enc'>\n";
  }
  print "<INPUT TYPE=SUBMIT value=Submit></FORM>\n";
  exit;
}

sub find_build_record {
  my ($tree, $logfile) = @_;

  my $log_entry = `grep $logfile $tree/build.dat`;

  chomp($log_entry);
  my ($mailtime, $buildtime, $buildname, $errorparser,
      $buildstatus, $logfile, $binaryname) = split /\|/, $log_entry;

  $buildrec = {    
    mailtime    => $mailtime,
    buildtime   => $buildtime,
    buildname   => $buildname,
    errorparser => $errorparser,
    buildstatus => $buildstatus,
    logfile     => $logfile,
    binaryname  => $binaryname,
    td          => undef
  };
  return $buildrec;
}
