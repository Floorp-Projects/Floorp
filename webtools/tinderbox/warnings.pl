#! /usr/bonsaitools/bin/perl
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

use FileHandle;

# Terrible globals:
#   %warnings
#   %warnings_by_who
#   %who_count
#   $ignore_pat
#   @ignore_match_pat
#

# This is for gunzip (should add a configure script to handle this).
$ENV{PATH} .= ":/usr/local/bin";

$debug = 1 if $ARGV[0] eq '--debug';

if ($debug) {
  foreach my $key (sort keys %ENV) {
    warn "debug> $key=$ENV{$key}\n";
  }
}

$tree = 'SeaMonkey';
# tinderbox/tbglobals.pl uses many shameful globals
$form{tree} = $tree;
require 'tbglobals.pl';

$cvsroot = '/cvsroot/mozilla';
$lxr_data_root = '/export2/lxr-data';
$source_root_pat = '^.*/mozilla/';

@ignore = ( 
  '__cmsg_data',
  'location of the previous definition',
  '\' was hidden',
  'declaration of \`index\' shadows global',
  'aggregate has a partly bracketed initializer', # mailnews guys say this has to stay
  'declaration of \`ws\' shadows global', # from istream
  'declaration of \`y0\' shadows global', # from mathcalls.h
  'declaration of \`y1\' shadows global', # from mathcalls.h
  'by \`nsHTML(?:Anchor|[^:]*Element)::(?:Set|Get)Attribute', # kipp says this is bogus
);
@ignore_match = (
  { warning=>'statement with no effect', source=>'(?:JS_|PR_)?ASSERT'},
);
$ignore_pat       = "(?:".join('|',@ignore).")";

print STDERR "Building hash of file names...";
($file_bases, $file_fullpaths) = build_file_hash($cvsroot, $tree);
print STDERR "done.\n";

for $br (last_successful_builds($tree)) {
  next unless $br->{buildname} =~ /shrike.*\b(Clobber|Clbr)\b/;

  my $log_file = "$br->{logfile}";

  warn "Parsing build log, $log_file\n";

  $fh = new FileHandle "gunzip -c $tree/$log_file |" 
    or die "Unable to open $tree/$log_file\n";
  &gcc_parser($fh, $cvsroot, $tree, $log_file, $file_bases, $file_fullpaths);
  $fh->close;

  &build_blame;

  my $warn_file = "$tree/warn$log_file";
  $warn_file =~ s/\.gz$/.html/;
  my $warn_file_by_file = $warn_file;
  $warn_file_by_file =~ s/\.html$/-by-file.html/;

  $fh->open(">$warn_file") or die "Unable to open $warn_file: $!\n";
  my $time_str = print_html_by_who($fh, $br);
  $fh->close;

#  $fh->open(">$warn_file_by_file")
#    or die "Unable to open $warn_file_by_file: $!\n";
#  print_html_by_file($fh, $br);
#  $fh->close;
  
  my $total_unignored_warnings = $total_warnings_count - $total_ignored_count;
  next unless $total_unignored_warnings > 0;

  # Make it live
  use File::Copy 'move';
  move($warn_file, "$tree/warnings.html");

  my $warn_summary = "$tree/warn$log_file";
  $warn_summary =~ s/.gz$/.pl/;

  $fh->open(">$warn_summary") or die "Unable to open $warn_summary: $!\n";
  $total_unignored_warnings = commify($total_unignored_warnings);
  print $fh '$warning_summary=\'<p>Check out the '
      ."<a href=\"http://tinderbox.mozilla.org/$tree/warnings.html\">"
      ."$total_unignored_warnings Build Warnings</a> (updated $time_str). "
      .'-<a href="mailto:slamm@netscape.com?subject=About the Build Warnings">'
      .'slamm</a><p>\';'."\n";
  $fh->close;

  move($warn_summary, "$tree/warn.pl");

  warn "$total_unignored_warnings warnings ($total_ignored_count ignored),"
      ." updated $time_str\n";

  last;
}

sub commify {
    my $text = reverse $_[0];
    $text =~ s/(\d\d\d)(?=\d)(?!\d*\.)/$1,/g;
    return scalar reverse $text;
}

# end of main
# ===================================================================

sub build_file_hash {
  my ($cvsroot, $tree) = @_;

  $lxr_data_root = "/export2/lxr-data/\L$tree";

  $lxr_file_list = "\L$lxr_data_root/.glimpse_filenames";
  open(LXR_FILENAMES, "<$lxr_file_list")
    or die "Unable to open $lxr_file_list: $!\n";

  use File::Basename;
  
  my $file_count = 0;

  while (<LXR_FILENAMES>) {
    my ($base, $dir, $ext) = fileparse($_,'\.[^/]*');

    next unless $ext =~ /^\.(cpp|h|C|s|c|mk|in)$/;

    $base = "$base$ext";
    $dir =~ s|$lxr_data_root/mozilla/||;
    $dir =~ s|/$||;

    $fullpath{"$dir/$base"}=1;

    unless (exists $bases{$base}) {
      $bases{$base} = $dir;
    } else {
      $bases{$base} = '[multiple]';
    }
    $file_count++;
  }
  warn "debug> $file_count files indexed\n" if $debug;

  return \%bases, \%fullpath;
}

sub last_successful_builds {
  my $tree = shift;
  my @build_records = ();
  my $br;

  $maxdate = time;
  $mindate = $maxdate - 5*60*60; # Go back 5 hours
  
  print STDERR "Loading build data...";
  &load_data;
  print STDERR "done\n";
  
  for (my $ii=1; $ii <= $name_count; $ii++) {
    for (my $tt=1; $tt <= $time_count; $tt++) {
      if (defined($br = $build_table->[$tt][$ii])
          and $br->{buildstatus} eq 'success') {
        push @build_records, $br;
        last;
  } } }
  return @build_records;
}

sub gcc_parser {
  my ($fh, $cvsroot, $tree, $log_file, $file_bases, $file_fullnames) = @_;
  my $build_dir = '';

 PARSE_TOP: while (<$fh>) {
    # Directory
    #
    if (/^gmake\[\d\]: Entering directory \`(.*)\'$/) {
      $build_dir = $1;
      $build_dir =~ s|$source_root_pat||o;
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
    $warnings{$file}{$line}->{first_seen_line} = $.
      unless defined $warnings{$file}{$line};

    my $ignore_it = /$ignore_pat/o;
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


sub dump_warning_data {
  while (my ($file, $lines_hash) = each %warnings) {
    while (my ($line, $record) = each %{$lines_hash}) {
      print join ':', 
      $file,$line,
      $record->{first_seen_line},
      $record->{count},
      $record->{warning_text};
      print "\n";
} } }

sub build_blame {
  use lib '../bonsai';
  require 'utils.pl';
  require 'cvsblame.pl';

  while (($file, $lines_hash) = each %warnings) {

    my $rcs_filename = "$cvsroot/$file,v";

    unless (-e $rcs_filename) {
#      warn "Unable to find $rcs_filename\n";
      $unblamed{$file} = 1;
      next;
    }

    my $revision = &parse_cvs_file($rcs_filename);
    @text = &extract_revision($revision);
    LINE: while (my ($line, $line_rec) = each %{$lines_hash}) {
      my $line_rev = $revision_map[$line-1];
      my $who = $revision_author{$line_rev};
      my $source_text = join '', @text[$line-3..$line+1];
      $source_text =~ s/\t/    /g;
      
      $who = "$who%netscape.com" unless $who =~ /[%]/;

      $line_rec->{line_rev} = $line_rev;
      $line_rec->{source}   = $source_text;

      for $ignore_rec (@ignore_match) {
        for my $warn_rec (@{ $line_rec->{list}}) {
          if ($warn_rec->{warning_text} =~ /$ignore_rec->{warning}/
              and $warn_rec->{source} =~ /$ignore_rec->{source}/
             and not $warn_rec->{ignore}) {
            $warn_rec->{ignore} = 1;
            $line_rec->{ignorecount}++;
            warn "ignored $warn_rec->{warning_text}\n";
            next LINE;
          }
        }
      }

      $warnings_by_who{$who}{$file}{$line} = $line_rec;

      $who_count{$who} += $line_rec->{count} - $line_rec->{ignorecount};
    }
  }
}

sub print_html_by_who {
  my ($fh, $br) = @_;
  my ($buildname, $buildtime) = ($br->{buildname}, $br->{buildtime});

  my $time_str = print_time( $buildtime );

  # Change the default destination for print to $fh
  my $old_fh = select($fh);

  my $total_unignored_count = $total_warnings_count - $total_ignored_count;
  print <<"__END_HEADER";
  <html>
    <head>
      <title>Blamed Build Warnings</title>
    </head>
    <body>
      <font size="+2" face="Helvetica,Arial"><b>
        Blamed Build Warnings
      </b></font><br>
      <font size="+1" face="Helvetica,Arial">
        $buildname on $time_str<br>
        $total_unignored_count total warnings
      </font><p>
    
__END_HEADER
  for $who (sort { $who_count{$b} <=> $who_count{$a}
                   || $a cmp $b } keys %who_count) {
    push @who_list, $who;
  }

  # Summary Table (name, count)
  #
  use POSIX;
  print "<table border=0 cellpadding=1 cellspacing=0 bgcolor=#ededed>\n";
  my $num_columns = 6;
  my $num_rows = ceil($#who_list / $num_columns);
  for (my $ii=0; $ii < $num_rows; $ii++) {
    print "<tr>";
    for (my $jj=0; $jj < $num_columns; $jj++) {
      my $index = $ii + $jj * $num_rows;
      next if $index > $#who_list;
      my $name = $who_list[$index];
      my $count = $who_count{$name};
      next if $count == 0;
      $name =~ s/%.*//;
      print "  " x $jj;
      print "<td><a href='#$name'>$name</a>";
      print "</td><td>";
      print "$count";
      print "</td><td>&nbsp;</td>\n";
    }
    print "</tr>\n";
  }
  print "</table><p>\n";

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
  for $who (@who_list, "Unblamed") {
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
    for $file (sort keys %{$warnings_by_who{$who}}) {
      for $linenum (sort keys %{$warnings_by_who{$who}{$file}}) {
        my $line_rec = $warnings_by_who{$who}{$file}{$linenum};
        next if $line_rec->{ignorecount} == $line_rec->{count};
        print_count($count, $line_rec->{count});
        print_warning($tree, $br, $file, $linenum, $line_rec);
        print_source_code($linenum, $line_rec) unless $unblamed{$file};
        $count += $line_rec->{count};
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
  for $who (@who_list, "Unblamed") {
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
    for $file (sort keys %{$warnings_by_who{$who}}) {
      for $linenum (sort keys %{$warnings_by_who{$who}{$file}}) {
        my $line_rec = $warnings_by_who{$who}{$file}{$linenum};
        next if $line_rec->{ignorecount} == $line_rec->{count};
        print_count($count, $line_rec->{count});
        print_warning($tree, $br, $file, $linenum, $line_rec);
        print_source_code($linenum, $line_rec) unless $unblamed{$file};
        $count += $line_rec->{count};
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
  if ($line_rec->{count} == 1) {
    print "See build log excerpt";
  } else {
    print "See 1st of $line_rec->{count} warnings in build log";
  }
  print "</a>)";

  print "</td></tr><tr><td></td><td>";

  for my $warn_rec (@{ $line_rec->{list}}) {
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
    next if $keyword eq '';
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

  return "http://cvs-mirror.mozilla.org/webtools/bonsai/cvsblame.cgi"
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
  s/^(?:$white)?//gm;
  s/^\s+$//gm;
  return $_;
}

