#!/usr/bonsaitools/bin/perl --
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

require 'globals.pl';
require 'timelocal.pl';

umask 0;

%MAIL_HEADER = ();
$building = 0;
$endsection = 0;

open( LOG, "<$ARGV[0]") || die "cant open $!";

&parse_mail_header;

%tbx = ();
&parse_log_variables(\%tbx);

close(LOG);

&check_required_vars(\%tbx);

$tree = $tbx{tree} unless defined $tree;
$logfile = "$tbx{builddate}.$$.gz" unless defined $logfile;
$building=1 if $tbx{status} =~ m/building/;

&write_build_data(\%tbx);

&compress_log_file(\%tbx);
unlink($ARGV[0]);

system "./buildwho.pl $tree";

# Build static pages for Sidebar flash and tinderbox panels.
$ENV{QUERY_STRING}="tree=$tree&static=1";
system './showbuilds.cgi';

# Generate build warnings (only for a successful shrike clobber build)
if ($tbx{build} =~ /shrike.*\b(Clobber|Clbr)\b/ and
    $tbx{status} eq 'success') {
  system './.warnings.pl';
}

# end of main
######################################################################

sub parse_log_variables {
  my $tbx = shift;
  local $_;
  my $key, $value;
  while (<LOG>) {
    chomp;
    if (/^tinderbox:/){
      (undef, $key, $value) = split /:\s*/;
      $tbx->{$key} = $value;
    }
  }
}

sub parse_mail_header {
  my $name = '';
  my $header = '';
  local $_;
  while (<LOG>) {
    chomp;
    
    last if /^$/;
    
#     if (/[^ :]*:\s+.*/) {
#       my ($name, $header) = split /:\s*/, $_, 2;
#       $name =~ tr/A-Z/a-z/;
#       $MAIL_HEADER{$name} = $header;
#     }
  }
}

sub check_required_vars {
  my $tbx = shift;
  my $err = '';

  foreach my $var ('tree','build','errorparser','builddate','status') {
    $err .= "Variable tinderbox:$var not set.\n" if $tbx->{$var} eq '';
  }
  
  if ($tbx->{tree} ne '' and not -r $tbx->{tree}) {
    $err .= "Variable tinderbox:tree not a valid tree.\n";
  }
  
  if ($tbx->{builddate} ne '' and $tbx->{builddate} < 7000000) {
    $err .= "Variable tinderbox:builddate not a valid unix date.\n";
  }

  if ($tbx->{status} ne '' 
      and $tbx->{status} !~ /success|busted|building|testfailed/) {
    $err .= "Variable tinderbox:status must be one of success,"
                  ." busted, testfailed, or building\n";
  }
  
  # Report errors
  die $err if $err ne '';
}

sub write_build_data {
  my $tbx = $_[0];
  my $tt = time();
  open(BUILDDATA, ">>$tbx->{tree}/build.dat" )
    or die "can't open $! for writing";

  # Lock it for exclusive access
  use Fcntl qw(:DEFAULT :flock);
  flock(BUILDATA, LOCK_EX);

  print BUILDDATA "$tt|$tbx->{builddate}|$tbx->{build}|$tbx->{errorparser}"
                 ."|$tbx->{status}|$logfile|$tbx->{binaryname}\n";
  close(BUILDDATA);
}

sub compress_log_file {
  my $tbx = $_[0];
  local $_;

  return if $building;
  
  open(LOG2, "<$ARGV[0]");
  
  # Skip past the the RFC822.HEADER
  #
  while (<LOG2>) {
    chomp;
    last if /^$/;
  }

  open(ZIPLOG, "| $gzip -c > ${tree}/$logfile") 
    or die "can't open $! for writing";

  my $inBinary = 0;
  my $hasBinary = ($tbx->{binaryname} ne '');
  while (<LOG2>) {
    if ($inBinary) {
      $inBinary = 0 if /^end\n/;
    } else {
      $inBinary = (/^begin [0-7][0-7][0-7] /) if $hasBinary;
      print ZIPLOG $_;
    }
  }
  close(ZIPLOG);
  close(LOG2);

  # If a uuencoded binary is part of the build, unpack it.
  #
  if ($hasBinary) {
    my $bin_dir = "$tbx->{tree}/bin/$tbx->{builddate}/$tbx->{build}";
    $bin_dir =~ s/ //g;
    
    system("mkdir -m 0777 -p $bin_dir");
    
    # LTNOTE: I'm not sure this is cross platform.
    system("/tools/ns/bin/uudecode --output-file=$bin_dir/$tbx->{binaryname}"
          ." < $ARGV[0]");
  }
}
