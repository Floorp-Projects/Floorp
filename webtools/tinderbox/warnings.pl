#! /usr/bin/perl
# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Mozilla Public License
# Version 1.1 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
# License for the specific language governing rights and limitations
# under the License.
#
# The Original Code is Tinderbox
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are Copyright (C) 1999
# Netscape Communications Corporation. All Rights Reserved.
#
# Contributor(s): Stephen Lamm <slamm@mozilla.org>

#
# Usage:
#
# Run this script on lounge.mozilla.org.  To run it there,
# cd to /opt/webtools/tinderbox and run it without
# any arguments:  
#
#   ./warnings.pl [--debug]
#

use strict;
use FileHandle;
use Compress::Zlib;
use lib "@TINDERBOX_DIR@";

require 'tbglobals.pl';

# A few global variables are used in the program.
#
# %warnings - Main warnings data structure
# $warnings{$file}{$line} => { 
#   first_seen_line =>, # First place in build log where warning is seen.
#   count           =>, # Number of warnings attributed to this line.
#   ignorecount     =>, # Number of ignored warnings for this line.
#   list => [ {         # Array of hash tables for each warning.
#     log_file     =>,  # Then name of the log file from which the warning came.
#     warning_text =>,  # The actual text of the warning
#     ignore       =>,  # Is this warning ignored? (values are 0 or 1)
#   }, {...}, ... ]
# }
#
# %warnings_by_who - Like %warnings, but adds indexing by who, and blame fields.
# $warnings_by_who{$who}{$file}{$line} => {
#   first_seen_line =>,
#   count           =>,
#   ignorecount     =>,
#   list => [ {
#     log_file     =>,
#     warning_text =>,
#     ignore       =>,
#   }, {...}, ... ]
#   line_rev        =>, # CVS revision number of line
#   source          =>, # Five lines of source code centered on the line
# }
#
# %who_count - Number of warnings attributed to each email address
# $who_count{$who} = $count
#
# @ignore - Array of regexp's of warnings to ignore
# @ignore_dir - Array of hashes of warnings matched with directories
# @ignore_dir = (
#  { warning=>'<warning pattern>', dir=>'<source patter>' },
#  {...}, ...
# )
# @ignore_match - Array of hashes of warnings matched with source to ignore
# @ignore_match = (
#  { warning=>'<warning pattern>', source=>'<source patter>' },
#  {...}, ...
# )

sub usage {
  warn "usage: warnings.pl <tree/logfile>\n";
}

$ENV{PATH} = "@SETUID_PATH@";

my $debug = 0;
$debug = 1, shift @ARGV if $ARGV[0] eq '--debug';

&usage, die "Error: Not enough arguments\n" if $#ARGV == -1;

# Load tinderbox build data.
#   (So we can find the last successful build for the tree of intestest.)
my $log_file = shift @ARGV;
# tinderbox/tbglobals.pl uses many shameful globals

&usage, die "Logfile does not exist, $log_file\n" unless -e $log_file;

my $tree;
($tree, $log_file) = split '/', $log_file;
my %form;

$tree = &require_only_one_tree($tree);
$form{tree} = $tree;
&tb_load_treedata($tree);
my $cvs_module = $::global_treedata->{$tree}->{cvs_module};
my $cvs_root = $::global_treedata->{$tree}->{cvs_root};


my $source_root = 'mozilla';
my ($exclude_pat, $tag, %bases, %fullpath, %modules, %module_files);
my (%seen, %unblamed, @who_list, @cvs_modules);

# ===================================================================
# Warnings to ignore

my @mac_ignore = (
  'function has no prototype',
  'inline function call .* not inlined',
);

my @ignore = ( 
  'location of the previous definition',
  '\' was hidden',
  #'declaration of \`index\' shadows global',
  'declaration of \`ws\' shadows global', # From istream
  'declaration of \`free\' shadows global', # From strstream.h
  'declaration of \`(?:y0|y1|j1|remainder)\' shadows global', #From mathcalls.h
  'is not \(any longer\) pertinent', # cvs warning we can safely ignore
  'ANSI C forbids long long integer constants', # js uses long long constants
);

# Patterns that need to match warning text and source directory
#
my @ignore_dir = (
  # mailnews is stuck with this
  { warning=>'aggregate has a partly bracketed initializer', 
    dir=>'mailnews/mime/' }
);

# Patterns that need to match warning text and source code text
#
my @ignore_match = (
  { warning=>'statement with no effect', source=>'(?:JS_|PR_)?ASSERT'},
);


# ===================================================================
# beginning of main


# Build a hash table of all the files in the tree.
#   We need this because the warnings do not always give the full
#   paths to files.
#
print STDERR "Building hash of file names..." if ($debug);
my ($file_bases, $file_fullpaths) = build_file_hash($cvs_root, @cvs_modules)
    if ($cvs_module ne '');
print STDERR "done.\n" if ($debug);

# Find the build we want and generate warnings for it
#
my $br = find_build_record($tree,$log_file);
  
my %warnings = ();
my %warnings_by_who = ();
my %who_count = ();
my $total_warnings_count = 0;
my $total_ignored_count = 0;

  # Parse the build log for warnings
  #
  warn "Parsing $br->{buildname}, $log_file\n" if ($debug);

  my $gz = gzopen("$tree/$log_file", "rb") or 
    die "gzopen($tree/$log_file): $!\n";
  if ($br->{errorparser} eq 'unix') {
    gcc_parser($gz, $cvs_root, $tree, $log_file, $file_bases, $file_fullpaths);
  } elsif ($br->{errorparser} eq 'mac') {
    mac_parser($gz, $cvs_root, $tree, $log_file, $file_bases, $file_fullpaths);
  }
  $gz->gzclose();

  # Attach blame to all the warnings
  #   (Yes, global variables are flying around.)
  &build_blame($::cvs_root, $file_fullpaths) if ($cvs_module ne '');

  # Come up with the temporary filenames for the output
  #
  my $warn_file = "$tree/warn$log_file";
  $warn_file =~ s/\.gz$/.html/;
  my $warn_file_by_file = $warn_file;
  $warn_file_by_file =~ s/\.html$/-by-file.html/;

  # Write the warnings indexed by who
  #
  my $fh = new FileHandle;
  $fh->open($warn_file, ">") or die "Unable to open $warn_file: $!\n";
  my $time_str = print_html_by_who($fh, $br);
  $fh->close;

  # Write the warnings indexed by file
  #
#  $fh->open($warn_file_by_file, ">")
#    or die "Unable to open $warn_file_by_file: $!\n";
#  print_html_by_file($fh, $br);
#  $fh->close;

my $total_unignored_warnings = $total_warnings_count - $total_ignored_count;

if ($total_unignored_warnings > 0) {
  # Add an entry to the warning log
  #
  my $lockfile = "$tree/warnings.sem";
  my $lock = &lock_datafile($lockfile);
  my $warn_log = "$tree/warnings.dat";
  $fh->open($warn_log, ">>") or die "Unable to open $warn_log: $!\n";
  print $fh "$log_file|$total_unignored_warnings\n";
  $fh->close;
  &unlock_datafile($lock);
  unlink($lockfile);
}

# end of main
# ===================================================================

sub build_file_hash {
  my ($cvs_root, @modules) = @_;

  read_cvs_modules_file();

  %bases = (); # Set in find_cvs_files
  %fullpath = (); # Set in find_cvs_files

  for my $module_item (@modules) {
    my $module = $module_item->[0];
    $tag = $module_item->[1]; # Used in find_cvs_files

    my @include_list = ();
    my @exclude_list = ();

    if (-d "$cvs_root/$module") {
      $include_list[0] = $module
    } else {
      expand_cvs_modules($module, \@include_list, \@exclude_list);
    }

    $exclude_pat = join '|', @exclude_list; # Used in find_cvs_files

    use Cwd;
    my $save_dir = cwd;

    use File::Find;
    for my $include (@include_list) {
      $include .= ",v" unless -d "$cvs_root/$include";
      &find(\&find_cvs_files, "$cvs_root/$include"); 
    }

    chdir $save_dir;
  }
  return \%bases, \%fullpath;
}

sub read_cvs_modules_file
{
  local $_;
  open(MODULES, "<", "$cvs_root/CVSROOT/modules")
    or die "Unable to open modules file: $cvs_root/CVSROOT/modules\n";
  while (<MODULES>) {
    if (/ -a /) {
      while (/\\$/) {
        chomp;
        chop;
        $_ .= <MODULES>;
      }
      chomp;
      my ($module_name, $list) = split /\s+-a\s+/, $_, 2;
      $modules{$module_name} = [ split /\s+/, $list ];
} } }

sub expand_cvs_modules {
  my ($module_name, $include_list, $exclude_list) = @_;
  warn "no module named $module_name\n" unless defined $modules{$module_name};
  for my $member (@{$modules{$module_name}}) {
    next if $member eq '';
    if (defined $modules{$member}) {
      expand_cvs_modules($member, $include_list, $exclude_list);
    } else {
      if ($member =~ /^!/) {
        push @$exclude_list, substr $member, 1;
      } else {
        push @$include_list, $member;
} } } }

sub find_cvs_files {
  $File::Find::prune = 1 if /.OBJ$/ or /^CVS$/ or /^Attic$/;
  if (-d $_) {
    $File::Find::prune = 1 if $seen{$File::Find::name};
    $File::Find::prune = 1 if $exclude_pat ne '' and /$exclude_pat/o;
    $seen{$File::Find::name} = 1;

    return;
  }
  my $dir = $File::Find::dir;
  $dir =~ s|^$cvs_root/$source_root/||o;
  $dir =~ s|/$||;
  my $file = substr $_, 0, -2;

  if (defined $module_files{$file}) {
    $bases{$file} = '[multiple]';
  } else {
    $bases{$file} = $dir;
  }
  $fullpath{"$dir/$file"} = $tag;
}

sub find_build_record {
  my $tree = shift;
  my $log_file = shift;
  my @build_records = ();
  my $br;

  $::maxdate = time;
  $::mindate = $::maxdate - 5*60*60; # Go back 5 hours

  print STDERR "Loading build data..." if ($debug);
  my $td = &tb_load_data(\%form);
  print STDERR "done\n" if ($debug);

  for (my $ii=0; $ii <= $td->{name_count}; $ii++) {
    for (my $tt=0; $tt <= $td->{time_count}; $tt++) {
      if (defined($br = $td->{build_table}->[$tt][$ii])
          and $br != -1
          and $br->{logfile} eq $log_file) {
        return $br;
  } } }
  return undef;
}

sub gcc_parser {
  my ($gz, $cvs_root, $tree, $log_file, $file_bases, $file_fullnames) = @_;
  my $build_dir = '';
  my $ignore_pat = "(?:".join('|',@ignore).")";
  my ($bytesread, $line);

 PARSE_TOP: while (defined($gz) && (($bytesread=$gz->gzreadline($line)) > 0)) {
    $_ = $line ;
    # Directory
    #
    if (/^gmake\[\d\]: Entering directory \`(.*)\'$/) {
      $build_dir = $1;
      $build_dir =~ s|^.*/$source_root/||o;
    }
    
    # Now only match lines with "warning:"
    next unless /warning:/;

    chomp; # Yum, yum

    warn "debug> $_\n" if $debug;

    my ($filename, $line, $warning_text);
    ($filename, $line, undef, $warning_text) = split /:\s*/, $_, 4;
    $filename =~ s/.*\///;
    
    # Special case for Makefiles
    $filename =~ s/Makefile$/Makefile.in/;

    # Look up the file name to determine the directory
    my $dir = '';

    if ($file_fullnames->{"$build_dir/$filename"}) {
      $dir = $build_dir;
    } else {
      unless(defined($dir = $file_bases->{$filename})) {
        $dir = '[no_match]';
      }
    }
    my $file = "$dir/$filename";

    # Special case for "`foo' was hidden\nby `foo2'"
    $warning_text = "...was hidden $warning_text" if $warning_text =~ /^by \`/;

    # Remember line of first occurrence in the build log
    unless (defined $warnings{$file}{$line}) {
      $warnings{$file}{$line}->{first_seen_line} = $.;
      $warnings{$file}{$line}->{ignorecount} = 0;
    }
    my $ignore_it = /$ignore_pat/o;
    unless ($ignore_it) {
      # Now check if the warning should be ignored based on directory
      for my $ignore_rec (@ignore_dir) {
        next unless $dir =~ /^$ignore_rec->{dir}/;
        next unless /$ignore_rec->{warning}/;
        $ignore_it = 1;
        last;
      }
    }
    if ($ignore_it) {
      $warnings{$file}{$line}->{ignorecount}++;
      $total_ignored_count++;
    }

    $warnings{$file}{$line}->{count}++;
    $total_warnings_count++;

    # Do not re-add this warning if it has been seen before
    for my $rec (@{ $warnings{$file}{$line}->{list} }) {
      next PARSE_TOP if $rec->{warning_text} eq $warning_text;
    }

    # Remember where in the build log the warning occured
    push @{ $warnings{$file}{$line}->{list} }, {
         log_file        => $log_file,
         warning_text    => $warning_text,
         ignore          => $ignore_it,
    };
  }
  warn "debug> $. lines read\n" if $debug;
}

sub mac_parser {
  my ($gz, $cvs_root, $tree, $log_file, $file_bases, $file_fullnames) = @_;
  my $build_dir = '';
  my ($bytesread, $line);

  my $ignore_pat = "(?:".join('|',@mac_ignore).")";

 PARSE_TOP: while (defined($gz) && (($bytesread=$gz->gzreadline($line)) > 0)) {
    $_ = $line ;
    # Now only match lines with "warning:"
    next unless /^Warning :/;
#print STDERR "Found a warning: $_";
    chomp; # Yum, yum

    warn "debug> $_\n" if $debug;

    my ($filename, $warning_text);
    (undef, $warning_text) = split /:\s*/, $_, 2;
    $_ = <$fh>;
    while (not /^\S+ line \d+/) {
      next PARSE_TOP if /^\S*$/;
      chomp;
      $warning_text .= " $_";
      $_ = <$fh>;
    }
    ($filename, undef, $line, undef) = split;
    
    # Look up the file name to determine the directory
    my $dir = '';
    unless(defined($dir = $file_bases->{$filename})) {
      $dir = '[no_match]';
    }
    my $file = "$dir/$filename";

    # Remember line of first occurrence in the build log
    unless (defined $warnings{$file}{$line}) {
      $warnings{$file}{$line}->{first_seen_line} = $.;
      $warnings{$file}{$line}->{ignorecount} = 0;
    }
    my $ignore_it = 0;
    $ignore_it = 1 if $warning_text =~ /^$ignore_pat$/o;
    if (0) { # unless ($ignore_it) {
      # Now check if the warning should be ignored based on directory
      for my $ignore_rec (@ignore_dir) {
        next unless $dir =~ /^$ignore_rec->{dir}/;
        next unless /$ignore_rec->{warning}/;
        $ignore_it = 1;
        last;
      }
    }
    if ($ignore_it) {
      $warnings{$file}{$line}->{ignorecount}++;
      $total_ignored_count++;
    }

    $warnings{$file}{$line}->{count}++;
    $total_warnings_count++;

    # Do not re-add this warning if it has been seen before
    for my $rec (@{ $warnings{$file}{$line}->{list} }) {
      next PARSE_TOP if $rec->{warning_text} eq $warning_text;
    }

    # Remember where in the build log the warning occured
    push @{ $warnings{$file}{$line}->{list} }, {
         log_file        => $log_file,
         warning_text    => $warning_text,
         ignore          => $ignore_it,
    };
  }
  warn "debug> $. lines read\n" if $debug;
}

sub build_blame {
  my ($cvs_root, $tags) = @_;
  my ($file, $lines_hash);

  use lib "@BONSAI_DIR@";
  require 'cvsblame.pl';

  while (($file, $lines_hash) = each %warnings) {

    my $rcs_filename = "$cvs_root/$source_root/$file,v";

    unless (-e $rcs_filename) {
      warn "Unable to find $rcs_filename\n";
      $unblamed{$file} = 1;
      next;
    }

    $::opt_rev = $tags->{$file} if defined $tags->{$file};
    my $revision = 0;
    eval {
      $revision = &parse_cvs_file($rcs_filename);
    };
    if ($@) {
      warn "Error parsing $rcs_filename: $@\n";
      $unblamed{$file} = 1;
      next;
    }
    my @text = &extract_revision($revision);
    LINE: while (my ($line, $line_rec) = each %{$lines_hash}) {
      my $line_rev = $::revision_map[$line-1];
      my $who = $::revision_author{$line_rev};
      my $source_text = join '', @text[$line-3..$line+1];
      $source_text =~ s/\t/    /g;
      
      $who = "$who%netscape.com" unless $who =~ /[%]/;

      $line_rec->{line_rev} = $line_rev;
      $line_rec->{source}   = $source_text;

      for my $ignore_rec (@ignore_match) {
        for my $warn_rec (@{ $line_rec->{list}}) {
          if ($warn_rec->{warning_text} =~ /$ignore_rec->{warning}/
              and $source_text =~ /$ignore_rec->{source}/
             and not $warn_rec->{ignore}) {
            $warn_rec->{ignore} = 1;
            $line_rec->{ignorecount}++;
            next LINE;
          }
        }
      }

      $warnings_by_who{$who}{$file}{$line} = $line_rec;

      $who_count{$who} += $line_rec->{count} - $line_rec->{ignorecount};
    }
  }
}

sub print_summary_table
{
  my ($who_list_ref, $who_count_hash_ref) = @_;
  my $num_whos = $#{$who_list_ref} + 1;

  # Summary Table (name, count)
  #
  use POSIX;
  print "<table border=0 cellpadding=1 cellspacing=0 bgcolor=white>\n";
  my $num_columns = 3;
  my $num_rows = ceil($num_whos / $num_columns);
  for (my $ii=0; $ii < $num_rows; $ii++) {
    print "<tr>";
    for (my $jj=0; $jj < $num_columns; $jj++) {
      my $index = $ii + $jj * $num_rows;
      next if $index > $num_whos;
      my $name = $who_list_ref->[$index];
      next unless defined $name;
      my $count = $who_count_hash_ref->{$name};
      next unless $count;
      #warn "$ii\t$jj\t$index\t$name\t$count\n";
      $name =~ s/%.*//;
      print "  " x $jj;
      print "<td><a href='#$name'>$name</a>";
      print "</td><td>";
      print "$count";
      print "</td><td>&nbsp;&nbsp;&nbsp;</td>\n";
    }
    print "</tr>\n";
  }
  print "</table><p>\n";
}

sub print_html_by_who {
  my ($fh, $br) = @_;
  my ($buildname, $buildtime) = ($br->{buildname}, $br->{buildtime});

  my $time_str = print_time( $buildtime );

  # Change the default destination for print to $fh
  my $old_fh = select($fh);

  my $total_unignored_count = $total_warnings_count - $total_ignored_count;
  for my $who (sort { $who_count{$b} <=> $who_count{$a}
                   || $a cmp $b } keys %who_count) {
    next if $who_count{$who} == 0;
    push @who_list, $who;
  }
  print <<"__END_HEADER";
  <html>
    <head>
      <title>Blamed Build Warnings</title>
    </head>
    <body BGCOLOR="#FFFFFF" TEXT="#000000" 
          LINK="#0000EE" VLINK="#551A8B" ALINK="#FF0000">
      <font size="+2" face="Helvetica,Arial"><b>
        Blamed Build Warnings
      </b></font><br>
      <font size="+1" face="Helvetica,Arial">
        $buildname on $time_str<br>
        $total_unignored_count total warnings
      </font><p>
    
      <table cellspacing=0 cellpadding=0 border=0><tr bgcolor="#F0A000"><td>
      <table cellpadding=2 cellspacing=2 border=0><tr bgcolor="#FFFFFF">
        <th colspan=2><font size="+1" face="Helvetica,Arial">
          Summary</font></th></tr>
      <tr bgcolor="#FFFFFF"><td><font face="Helvetica,Arial"><b>
      by count</b></font>
__END_HEADER
  print_summary_table(\@who_list, \%who_count);
  print "</td><td><font face='Helvetica,Arial'><b>";
  print "by name</b></font>";
  print_summary_table([sort @who_list], \%who_count);
  print "</td></tr></table>";
  print "</td></tr></table>";

  # Count Unblamed warnings
  #
  for my $file (keys %unblamed) {
    for my $linenum (keys %{$warnings{$file}}) {
      $who_count{Unblamed} += $warnings{$file}{$linenum}{count};
      $who_count{Unblamed} -= $warnings{$file}{$linenum}{ignorecount};
      $warnings_by_who{Unblamed}{$file}{$linenum} = $warnings{$file}{$linenum};
    }
  }

  # Print all the warnings
  #
  for my $who (@who_list, "Unblamed") {
    my $total_count = $who_count{$who};
    
    next if $total_count == 0;

    my ($name, $email);
    ($name = $who) =~ s/%.*//;
    ($email = $who) =~ s/%/@/;
    
    print "<h2>";
    print "<a name='$name' href='mailto:$email'>" unless $name eq 'Unblamed';
    print "$name";
    print "</a>" unless $name eq 'Unblamed';
    print " (1 warning)"       if $total_count == 1;
    print " ($total_count warnings)" if $total_count > 1;
    print "</h2>";

    print "\n<table>\n";
    my $count = 1;
    for my $file (sort keys %{$warnings_by_who{$who}}) {
      for my $linenum (sort keys %{$warnings_by_who{$who}{$file}}) {
        my $line_rec = $warnings_by_who{$who}{$file}{$linenum};
        my $count_for_line = $line_rec->{count} - $line_rec->{ignorecount};
        next if $count_for_line == 0;
        print_count($count, $count_for_line);
        print_warning($tree, $br, $file, $linenum, $line_rec);
        print_source_code($linenum, $line_rec) unless $unblamed{$file};
        $count += $count_for_line;
      }
    }
    print "</table>\n";
  }

  print <<"__END_FOOTER";
  <p>
    <hr align=left>
    Send questions or comments to 
    &lt;<a href="mailto:slamm\@netscape.com?subject=About the Blamed Build Warnings">slamm\@netcape.com</a>&gt;.
   </body></html>
__END_FOOTER

  # Change default destination back.
  select($old_fh);

  return $time_str;
}

sub print_html_by_file {
  my ($fh, $br) = @_;
  my ($buildname, $buildtime) = ($br->{buildname}, $br->{buildtime});

  my $time_str = print_time( $buildtime );

  # Change the default destination for print to $fh
  my $old_fh = select($fh);

  my $total_unignored_count = $total_warnings_count - $total_ignored_count;
  print <<"__END_HEADER";
  <html>
    <head>
      <title>Build Warnings By File</title>
    </head>
    <body>
      <font size="+2" face="Helvetica,Arial"><b>
        Build Warnings By File
      </b></font><br>
      <font size="+1" face="Helvetica,Arial">
        $buildname on $time_str<br>
        $total_unignored_count total warnings
      </font><p>
    
__END_HEADER

  # Print all the warnings
  #
  for my $who (@who_list, "Unblamed") {
    my $total_count = $who_count{$who};
    my ($name, $email);
    ($name = $who) =~ s/%.*//;
    ($email = $who) =~ s/%/@/;
    
    print "<h2>";
    print "<a name='$name' href='mailto:$email'>" unless $name eq 'Unblamed';
    print "$name";
    print "</a>" unless $name eq 'Unblamed';
    print " (1 warning)"       if $total_count == 1;
    print " ($total_count warnings)" if $total_count > 1;
    print "</h2>";

    print "\n<table>\n";
    my $count = 1;
    for my $file (sort keys %{$warnings_by_who{$who}}) {
      for my $linenum (sort keys %{$warnings_by_who{$who}{$file}}) {
        my $line_rec = $warnings_by_who{$who}{$file}{$linenum};
        my $count_for_line = $line_rec->{count} - $line_rec->{ignorecount};
        next if $count_for_line == 0;
        print_count($count, $count_for_line);
        print_warning($tree, $br, $file, $linenum, $line_rec);
        print_source_code($linenum, $line_rec) unless $unblamed{$file};
        $count += $count_for_line;
      }
    }
    print "</table>\n";
  }

  print <<"__END_FOOTER";
  <p>
    <hr align=left>
    Send questions or comments to 
    &lt;<a href="mailto:slamm\@netscape.com?subject=About the Blamed Build Warnings">slamm\@netcape.com</a>&gt;.
   </body></html>
__END_FOOTER

  # Change default destination back.
  select($old_fh);
}

sub print_count {
  my ($start, $count) = @_;
  print "<tr><td align=right>$start";
  print "-".($start+$count-1) if $count > 1;
  print ".</td>";
}

sub print_warning {
  my ($tree, $br, $file, $linenum, $line_rec) = @_;

  print "<td>";

  # File link
  if ($file =~ /\[multiple\]/) {
    $file =~ s/\[multiple\]\///;
    print   "<a href='http://lxr.mozilla.org/seamonkey/find?string=$file'>";
    print   "$file:$linenum";
    print "</a> (multiple file matches)";
  } elsif ($file =~ /\[no_match\]/) {
    $file =~ s/\[no_match\]\///;
    print   "<b>$file:$linenum</b> (no file match)";
  } else {
    print "<a href='"
      .file_url($file,$linenum)."'>";
    print   "$file:$linenum";
    print "</a> ";
  }

  # Build log link
  my $log_line = $line_rec->{first_seen_line};
  print " (<a href='"
        .build_url($tree, $br, $log_line)
        ."'>";
  my $count = $line_rec->{count} - $line_rec->{ignorecount};
  if ($count == 1) {
    print "See build log excerpt";
  } else {
    print "See 1st of $count warnings in build log";
  }
  print "</a>)";

  print "</td></tr><tr><td></td><td>";

  for my $warn_rec (@{ $line_rec->{list}}) {
    next if $warn_rec->{ignore};
    my $warning  = $warn_rec->{warning_text};

    # Warning text
    print "\u$warning<br>";
  }
  print "</td></tr>";
}

sub print_source_code {
  my ($linenum, $line_rec) = @_;

  # Source code fragment
  #
  print "<tr><td></td><td bgcolor=#ededed>";
  print "<pre>";
  
  my $source_text = trim_common_leading_whitespace($line_rec->{source});
  $source_text =~ s/&/&amp;/gm;
  $source_text =~ s/</&lt;/gm;
  $source_text =~ s/>/&gt;/gm;
  # Highlight the warning's keyword
  for my $warn_rec (@{ $line_rec->{list}}) {
    my $warning = $warn_rec->{warning_text};
    my ($keyword) = $warning =~ /\`([^\']*)\'/;
    next unless defined $keyword and $keyword ne '';
    $source_text =~ s|\b\Q$keyword\E\b|<b>$keyword</b>|gm;
    last;
  }
  my $line_index = $linenum - 2;
  $source_text =~ s/^(.*)$/$line_index++." $1"/gme;
  $source_text =~ s|^($linenum.*)$|<font color='red'>\1</font>|gm;
  chomp $source_text;
  print $source_text;

  #print "</pre>";
  print "</td></tr>\n";
}

sub build_url {
  my ($tree, $br, $linenum) = @_;

  my $name = $br->{buildname};
  $name =~ s/ /%20/g;

  return "../showlog.cgi?log=$tree/$br->{logfile}:$linenum";
}

sub file_url {
  my ($file, $linenum) = @_;

  return "$::bonsai_url/cvsblame.cgi"
        ."?file=mozilla/$file&mark=$linenum#".($linenum-10);

}

sub trim_common_leading_whitespace {
  # Adapted from dequote() in Perl Cookbook by Christiansen and Torkington
  local $_ = shift;
  my $white;  # common whitespace
  if (/(?:(\s*).*\n)(?:(?:\1.*\n)|(?:\s*\n))+$/) {
    $white = $1;
  } else {
    $white = /^(\s+)/;
  }
  s/^(?:$white)?//gm unless $white eq '';
  s/^\s+$//gm;
  return $_;
}

# Print big numbers with commas (e.g. 3,314).
sub commify {
    my $text = reverse $_[0];
    $text =~ s/(\d\d\d)(?=\d)(?!\d*\.)/$1,/g;
    return scalar reverse $text;
}

