#!/usr/bin/perl -w -I.
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Tinderbox 3.
#
# The Initial Developer of the Original Code is
# John Keiser (john@johnkeiser.com).
# Portions created by the Initial Developer are Copyright (C) 2004
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# ***** END LICENSE BLOCK *****

use strict;

use Getopt::Long;
use Date::Format;

our $VERSION = "1.0";
my %args;
GetOptions(\%args, "url:s", "quota:i", "start:i", "end:i");
if (!defined($args{quota}) || !defined($args{start}) || !defined($args{end}) ||
    @ARGV != 1) {
  print <<EOM;

Usage: tbox_build_quota.pl --quota=<megabytes quota> --start=<hours ago> --end=<hours ago> build_dirs_file

This script deletes builds selectively.  It attempts to keep an evenly spaced
number of builds sitting around for a given period that still fits within the
criteria.  It will try to order the builds such that the number of builds per
directory for a particular period of time is even.  (This is meant to ensure
that, for example, we try and keep at least one build per platform per day.)

--url: the toplevel url to the tinderbox server (the dir containing
       showbuilds.pl)
--quota: max number of megabytes (Mb) to use for the time period.
--start: the start of the range in hours (for example, 72).  0 for forever.
--end: the end of the range in hours (for example, 24).  0 for current.
build_dirs_file: the series of build directories to clean up.  Format:

<pattern> <url>
<pattern> <url>
...

  <pattern>: a glob pattern to match a set of files (~/public_html/builds/*)
  <url>: the url the build will point to.  %s will be replaced with the build's
         filename and the url will be sent to the server to notify it that the
         build has been removed.  (http://jkeiser.no-ip.com/~jkeiser/builds/%s)

EOM
  exit(0);
}

my %build_dir_urls;
my @build_dirs;
open CONFIG, $ARGV[0] or die "Could not open $ARGV[0]!";
while (<CONFIG>) {
  my ($build_dir, $build_url) = split /\s+/;
  push @build_dirs, $build_dir;
  $build_dir_urls{$build_dir} = $build_url;
}
close CONFIG;

# Round time up to nearest hour so we have less variability in this process
my $current_time = time;
if ($current_time % (60*60)) {
  $current_time += 60*60 - ($current_time % (60*60));
}

my $start_seconds = $current_time - $args{start}*60*60;
my $end_seconds = $current_time - $args{end}*60*60;

# Gather the list of builds
my $total_size = 0;
my %builds;
foreach my $build_dir (@build_dirs) {
  my @unsorted_build_list;
  foreach my $build (glob($build_dir)) {
    # Make sure this build is in the range we're dealing with
    my @build_stat = stat($build);
    die "No build stat for $build ($build_dir)" if !@build_stat;
    if (($args{start} == 0 || $build_stat[9] >= $start_seconds) &&
        $build_stat[9] <= $end_seconds) {
      push @unsorted_build_list, { build => $build, size => $build_stat[7], mtime => $build_stat[9] };
      $total_size += $build_stat[7];
    }
  }

  @{$builds{$build_dir}} = sort { $a->{mtime} <=> $b->{mtime} } @unsorted_build_list;
}

# Start deleting builds until we reach quota
while ($total_size > ($args{quota}*1024*1024)) {
  my $build_dir_to_delete;
  my $build_to_delete;
  # First try and find the most useless build: the one that is closest to its
  # two siblings.
  my $min_space = -1;
  foreach my $build_dir (keys %builds) {
    next if @{$builds{$build_dir}} < 3;
    for (my $i = 1; $i < @{$builds{$build_dir}} - 1; $i++) {
      my $space = $builds{$build_dir}[$i+1]{mtime} - $builds{$build_dir}[$i-1]{mtime};
      if ($min_space == -1 || $space < $min_space) {
        $min_space = $space;
        $build_dir_to_delete = $build_dir;
        $build_to_delete = $i;
      }
    }
  }
  # If that failed, all build_dirs must have 2 or less builds.  Find one that
  # has 2 builds and lop off the last one.
  if (!defined($build_dir_to_delete)) {
    foreach my $build_dir (keys %builds) {
      if (@{$builds{$build_dir}} == 2) {
        $build_dir_to_delete = $build_dir;
        $build_to_delete = 1;
        last;
      }
    }
  }
  # If that failed, all build_dirs must have 1 build.  Lop off the build on the
  # first such one we find.
  if (!defined($build_dir_to_delete)) {
    foreach my $build_dir (keys %builds) {
      if (@{$builds{$build_dir}} == 1) {
        $build_dir_to_delete = $build_dir;
        $build_to_delete = 0;
        last;
      }
    }
  }
  if (!defined($build_dir_to_delete)) {
    die "Eek!  No more builds left to delete!  You just can't be satisfied, can you?  $total_size left out of " . ($args{quota}*1024*1024);
  }

  $total_size -= $builds{$build_dir_to_delete}[$build_to_delete]{size};
  delete_build(\%builds, $args{url}, $build_dir_urls{$build_dir_to_delete}, $build_dir_to_delete, $build_to_delete);
}

use LWP::UserAgent;
use HTTP::Request::Common;

sub delete_build {
  my ($builds, $tbox_url, $build_url, $build_dir, $build_num) = @_;
  $builds->{$build_dir}[$build_num]{build} =~ /([^\/]*)$/;
  my $build = $1;
  $build_url =~ s/\%s/$build/g;
  print "Deleting ", $builds->{$build_dir}[$build_num]{build}, "\n";
  if ($tbox_url) {
    my $ua = new LWP::UserAgent;
    $ua->agent("TinderboxBuildQuota/" . $::VERSION);
    my $res = $ua->request(POST "$tbox_url/xml/build_deleted.pl", [ url => $build_url ]);
    die "Could not delete $tbox_url due to connection failure\n" if !$res->is_success();
  }
  splice @{$builds->{$build_dir}}, $build_num, 1;
  system("rm", "-f", $builds->{$build_dir}[$build_num]{build});
}
