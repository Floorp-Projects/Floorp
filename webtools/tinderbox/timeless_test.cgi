#!/usr/bonsaitools/bin/perl --
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
# timeless@mac.com, zach

use lib "@BONSAI_DIR@";
require 'tbglobals.pl';
require 'lloydcgi.pl';
require 'imagelog.pl';

$::tree = $form{tree};

# $rel_path is the relative path to webtools/tinderbox used for links.
# It changes to "../" if the page is generated statically, because then
# it is placed in tinderbox/$::tree.
$rel_path = ''; 

# Reading the log backwards saves time when we only want the tail.
use Backwards;

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


  my $have_loaded_treedata = 0;

  sub tb_load_treedata {
    my $tree = shift;

    return if $have_loaded_treedata;
    $have_loaded_treedata = 1;
    require "$tree/treedata.pl" if -r "$tree/treedata.pl";
  }

sub tb_load_data {
  $tree = $form{'tree'};
print "$tree\n";
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

  $td->{bloaty} = "";
  $td->{warnings} = "";

  return $td;
}

sub tb_loadquickparseinfo {
print "sub tb_loadquickparseinfo\n";
print "@_\n";
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
print "bt: $buildtime \t bn: $buildname \t bs: $buildstatus \t iSoB: $includeStatusOfBuilding\n";
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
print 'sub tb_last_status'."\n";
  for (my $tt=0; $tt < $time_count; $tt++) {
    my $br = $build_table->[$tt][$build_index];
    next unless defined $br and $br->{buildstatus};
print "$tt $br->{buildstatus}\n";
    next unless $br->{buildstatus} =~ /^(success|busted|testfailed)$/;
    return $br->{buildstatus};
  }
  return 'building';
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

&do_quickparse;
print "\n----------------------\n";
&do_tinderbox;
sub do_tinderbox{
  my $tinderbox_data = &tb_load_data;
print $tinderbox_data;
  &print_table_header;
}

# end of main
#=====================================================================

sub print_table_header {
  print "box name|last status\n";
  for (my $ii=0; $ii < $name_count; $ii++) {
    my $bn = $build_names->[$ii];
    my $last_status = tb_last_status($ii);
    print "$bn|$last_status\n";
  }
}

  # Check bonsai tree for open/close state

  my $treestate = undef;
  my $checked_state = 0;

  sub _check_tree_state {
    my $tree = shift;

    $checked_state = 1;
    tb_load_treedata($tree); # Loading for the global, $bonsai_tree
    return unless defined $bonsai_tree and $bonsai_tree ne '';

    local $_;
    $::BatchID='';
    eval qq(require "$bonsai_dir/data/$bonsai_tree/batchid.pl");
    if ($::BatchID eq '') {
      warn "No BatchID in $bonsai_dir/data/$bonsai_tree/batchid.pl\n";
      return;
    }
    open(BATCH, "<", "$bonsai_dir/data/$bonsai_tree/batch-$::BatchID.pl")
      or print "can't open batch-$::BatchID.pl<br>";
    while (<BATCH>) { 
      if (/^\$::TreeOpen = '(\d+)';/) {
        $treestate = $1;
        last;
      }
    }
    return;
  }

  sub is_tree_state_available {
    my $tree = shift;
    $tree = $::tree unless defined $tree;
    return 1 if defined $treestate;
    return 0 if $checked_state;
    _check_tree_state($tree);
    return is_tree_state_available();
  }

  sub is_tree_open {
    my $tree = shift;
    $tree = $::tree unless defined $tree;
    _check_tree_state($tree) unless $checked_state;
    return $treestate;
  }

sub do_quickparse {
  print "Content-type: text/plain\n\n";

  my @treelist = split /,/, $::tree;
  foreach my $tt (@treelist) {
    if (is_tree_state_available($tt)) {
      my $state = is_tree_open($tt) ? 'open' : 'closed';
      print "State|$tt|$bonsai_tree|$state\n";
    }
    my (%build, %times);
    tb_loadquickparseinfo($tt, \%build, \%times);
    
    foreach my $buildname (sort keys %build) {
      print "Build|$tt|$buildname|$build{$buildname}\n";
    }
  }
}
