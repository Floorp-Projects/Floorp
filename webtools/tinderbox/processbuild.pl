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

use Time::Local;
require 'tbglobals.pl'; # for $gzip

umask 0;

if ($ARGV[0] eq '--check-mail') {
  $only_check_mail = 1;
  shift @ARGV;
}
$mail_file = $ARGV[0];

%MAIL_HEADER = ();
%tinderbox = ();

# Scan the logfile once to get mail header and build variables
#
open LOG, "<$mail_file" or die "Can't open $!";

parse_mail_header(*LOG, \%MAIL_HEADER);
parse_log_variables(*LOG, \%tinderbox);

close LOG;

# Make sure variables are defined correctly
#
check_required_variables(\%tinderbox, \%MAIL_HEADER);

die "Mail variables passed the test\n" if $only_check_mail;

# Write data to "build.dat"
#
$tinderbox{logfile} = "$tinderbox{builddate}.$$.gz";
write_build_data(\%tinderbox);

# Compress the build log and put it in the tree
#
compress_log_file(\%tinderbox, $mail_file)
  unless $tinderbox{status} =~ /building/;

unlink $mail_file;


# Who data
#
system "./buildwho.pl $tinderbox{tree}";


# Warnings
#   Compare the name with $warning_buildnames_pat which is defined in
#   $tinderbox{tree}/treedata.pl if at all.
require "$tinderbox{tree}/treedata.pl" if -r "$tinderbox{tree}/treedata.pl";
if (defined $warning_buildnames_pat
    and $tinderbox{build} =~ /^$warning_buildnames_pat$/
    and $tinderbox{status} ne 'failed') {
  system "./warnings.pl $tinderbox{tree}/$tinderbox{logfile}";
}

# Bloat data
#   Compare the name with $bloat_buildnames_pat which is defined in
#   $tinderbox{tree}/treedata.pl if at all.
if (defined $bloat_buildnames_pat
    and $tinderbox{build} =~ /^$bloat_buildnames_pat$/
    and $tinderbox{status} eq 'success') {
  system "./bloat.pl $tinderbox{tree} $tinderbox{logfile}";
}

# Pageloader data
#   Compare the name with $pageloader_buildnames_pat which is defined in
#   $tinderbox{tree}/treedata.pl if at all.
if (defined $pageloader_buildnames_pat
    and $tinderbox{build} =~ /^$pageloader_buildnames_pat$/
    and $tinderbox{status} eq 'success') {
  system "./pageloader.pl $tinderbox{tree} $tinderbox{logfile}";
}

# Startup data
#   Compare the name with $startup_buildnames_pat which is defined in
#   $tinderbox{tree}/treedata.pl if at all.
if (defined $startup_buildnames_pat
    and $tinderbox{build} =~ /^$startup_buildnames_pat$/
    and $tinderbox{status} eq 'success') {
  system "./startup.pl $tinderbox{tree} $tinderbox{logfile}";
}

# Scrape data
#   Compare the name with $scrape_buildnames_pat which is defined in
#   $tinderbox{tree}/treedata.pl if at all.
if (defined $scrape_buildnames_pat
    and $tinderbox{build} =~ /^$scrape_buildnames_pat$/
    and $tinderbox{status} eq 'success') {
  system "./scrape.pl $tinderbox{tree} $tinderbox{logfile}";
}

# Static pages
#   For Sidebar flash and tinderbox panels.
$ENV{QUERY_STRING}="tree=$tinderbox{tree}&static=1";
system './showbuilds.cgi';

# end of main
######################################################################


# This routine will scan through log looking for 'tinderbox:' variables
#
sub parse_log_variables {
  my ($fh, $tbx) = @_;
  local $_;

  while (<$fh>) {
    chomp;
    if (/^tinderbox:.*:/) {
      last if /^tinderbox: END/;
      my ($key, $value) = (split /:\s*/, $_, 3)[1..2];
      $value =~ s/\s*$//;
      $tbx->{$key} = $value;
    }
  }
}

sub parse_mail_header {
  my ($fh, $mail_ref) = @_;
  local $_;
  my $name = '';
  
  while(<$fh>) {
    chomp;
    last if $line eq '';
    
    if (/([^ :]*)\:[ \t]+([^\n]*)/) {
      $name = $1;
      $name =~ tr/A-Z/a-z/;
      $mail_ref{$name} = $2;
    }
    elsif ($name ne '') {
      $mail_ref{$name} .= $2;
    }
  }
}

sub check_required_variables {
  my ($tbx, $mail_header) = @_;
  my $err_string = '';

  if ($tbx->{tree} eq '') {
    $err_string .= "Variable 'tinderbox:tree' not set.\n";
  }
  elsif (not -r $tbx->{tree}) {
    $err_string .= "Variable 'tinderbox:tree' not set to a valid tree.\n";
  }
  elsif (($mail_header->{'to'} =~ /external/i or
          $mail_header->{'cc'} =~ /external/i) and
         $tbx->{tree} !~ /external/i) {
    $err_string .= "Data from an external source didn't specify an 'external' tree.";
  }
  if ($tbx->{build} eq '') {
    $err_string .= "Variable 'tinderbox:build' not set.\n";
  }
  if ($tbx->{errorparser} eq '') {
    $err_string .= "Variable 'tinderbox:errorparser' not set.\n";
  }

  # Grab the date in the form of mm/dd/yy hh:mm:ss
  #
  # Or a GMT unix date
  #
  if ($tbx->{builddate} eq '') {
    $err_string .= "Variable 'tinderbox:builddate' not set.\n";
  }
  else {
    if ($tbx->{builddate} =~ 
        /([0-9]*)\/([0-9]*)\/([0-9]*)[ \t]*([0-9]*)\:([0-9]*)\:([0-9]*)/) {
      $tbx->{builddate} = timelocal($6,$5,$4,$2,$1-1,$3);
    }
    elsif ($tbx->{builddate} < 7000000) {
      $err_string .= "Variable 'tinderbox:builddate' not of the form MM/DD/YY HH:MM:SS or unix date\n";
    }
  }

  # Build Status
  #
  if ($tbx->{status} eq '') {
    $err_string .= "Variable 'tinderbox:status' not set.\n";
  }
  elsif (not $tbx->{status} =~ /success|busted|building|testfailed/) {
    $err_string .= "Variable 'tinderbox:status' must be 'success', 'busted', 'testfailed', or 'building'\n";
  }

  # Report errors
  #
  die $err_string unless $err_string eq '';
}

sub write_build_data {
  my $tbx = $_[0];
  $process_time = time;
  open BUILDDATA, ">>$tbx->{tree}/build.dat" 
    or die "can't open $! for writing";
  print BUILDDATA "$process_time|$tbx->{builddate}|$tbx->{build}|$tbx->{errorparser}|$tbx->{status}|$tbx->{logfile}|$tbx->{binaryurl}\n";
  close BUILDDATA;
}

sub compress_log_file {
  my ($tbx, $maillog) = @_;
  local *LOG2;

  open(LOG2, "<$maillog") or die "cant open $!";

  # Skip past the the RFC822.HEADER
  #
  while (<LOG2>) {
    chomp;
    last if /^$/;
  }

  open ZIPLOG, "| $gzip -c > $tbx->{tree}/$tbx->{logfile}"
    or die "can't open $! for writing";
  my $inBinary = 0;
  while (<LOG2>) {
    unless ($inBinary) {
      print ZIPLOG $_;
      if ($hasBinary) {
        $inBinary = (/^begin [0-7][0-7][0-7] /);
      }
    }
    elsif (/^end\n/) {
      $inBinary = 0;
    }
  }
  close ZIPLOG;
  close LOG2;
}
