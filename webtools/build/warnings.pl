#! /usr/bin/env perl

use FileHandle;
use FileNameFind;

$tree = 'SeaMonkey';
$cvsroot = '/u/slamm/tt/cvsroot/mozilla';
@ignore = ( 'long long', '__cmsg_data' );
$ignore_pat = "(?:".join('|',@ignore).")";

for $build_record (last_successful_builds($tree)) {
  print "hey\n";
  next unless $build_record->{errorparser} eq 'unix';
  next unless $build_record->{buildname} =~ /\b(Clobber|Clbr)\b/;

  print "log is $tree/$builld_record->{logfile}\n";
  exit;
  $fh = new FileHandle "gunzip $tree/$builld_record->{logfile} |";
  &gcc_parser($fh, $tree);

  last;
}

#&dump_warning_data;
&build_blame;
&print_warnings_as_html;

# end of main
# ===================================================================

sub last_successful_builds {
  my $tree = shift;
  my @build_records = ();
  my $br;

  # tinderbox/globals.pl uses many shameful globals
  $form{tree} = $tree;

  $maxdate = time;
  $mindate = $maxdate - 8*60*60; # Go back 8 hours
  
  print STDERR "Loading build data...";
  use Cwd
  chdir '../tinderbox';
  require 'globals.pl';
  &load_data;
  chdir '../build';
  print STDERR "done\n";
  
  for (my $ii=1; $ii <= $name_count; $ii++) {
    for (my $tt=1; $tt <= $time_count; $tt++) {
      if (defined($br = $build_table->[$tt][$build_index])
          and $br->{buildstatus} eq 'success') {
        push @build_records, $br;
  } } }
  return @build_records;
}

sub gcc_parser {
  my ($fh, $tree) = @_;
  my $dir = '';

  my $find = new FileNameFind;
  $find->open($tree);
  
  while (<$fh>) {
    # Directory
    #
    if (/^gmake\[\d\]: Entering directory \`(.*)\'$/) {
      ($build_dir = $1) =~ s|.*/mozilla/||;
      next;
    }

    # Now only match lines with "warning:"
    next unless /warning:/;
    next if /$ignore_pat/o;

    chomp; # Yum, yum

    my ($filename, $line, undef, $warning_text) = split /:\s*/;
    $filename =~ s/.*\///;
    
    my $dir = find_dir($find, $build_dir, $filename);
    my $file = "$dir/$filename";

    unless (defined($warnings{"$file:$line"})) {
      # Remember where in the build log the warning occured

      $warnings{"$file:$line"} = {
         first_seen_line => $.,
         count           => 0,
         warning_text    => $warning_text,
      };
    }
    $warnings{"$file:$line"}->{count}++;
    push @{$warnings_per_file{$file}}, $line;
    $ii++;
    last if $ii > 20;
  }
}

sub find_dir {
  my ($find, $dir, $filename) = @_;

  my @dirs = $find->lookup($filename, candidate=>$dir);

  if ($#dirs == -1) {
    warn "No match for $filename ($dir).\n";
    return "<no_match>";
  } elsif ($#dirs == 0) {
    return $dirs[0];
  } else {
    warn "Multiple match for $filename ($dir).\n";
    return "<multiple_matches>";
  }
}

sub dump_warning_data {
  while (my ($file_and_line, $record) = each %warnings) {
    print join ':', 
      "$file_and_line",
      $record->{first_seen_line},
      $record->{count},
      $record->{warning_text};
    print "\n";
  }
}

sub build_blame {
  use lib '../bonsai';
  require 'utils.pl';
  require 'cvsblame.pl';

  while (($file, $lines) = each %warnings_per_file) {

    my $rcs_filename = "$cvsroot/$file,v";

    unless (-e $rcs_filename) {
      warn "Unable to find $rcs_filename\n";
      next;
    }

    my $revision = &parse_cvs_file($rcs_filename);
    @text = &extract_revision($revision);
    for $line (@{$lines}) {
      my $line_rev = $revision_map[$line-1];
      my $who = $revision_author{$line_rev};
      my $source_text = join '', @text[$line-3..$line+1];
      chomp $source_text;
      
      my $warn_rec = $warnings{"$file:$line"};
      $warn_rec->{line_rev} = $line_rev;
      $warn_rec->{source}   = $source_text;

      $warnings_by_who{$who}{$file}{$line} = $warn_rec;

      $who_count{$who} += $warn_rec->{count};
    }
  }
}

sub print_warnings_as_html {
  for $who (sort { $who_count{$b} <=> $who_count{$a}
                   || $a cmp $b } keys %who_count) {
    my $count = $who_count{$who};
    my ($name, $email);
    ($name = $who) =~ s/%.*//;
    ($email = $who) =~ s/%/@/;
  
    print "<font size='+1' face='Helvetica,Arial'><b>";
    print "<a name='$name' href='mailto:$email'>$name</a>";
    print " (1 warning)"       if $count == 1;
    print " ($count warnings)" if $count > 1;
    print "</b></font>";

    print "\n<ol>\n";
    for $file (sort keys %{$warnings_by_who{$who}}) {
      for $linenum (sort keys %{$warnings_by_who{$who}{$file}}) {
        my $warn_rec = $warnings_by_who{$who}{$file}{$linenum};
        my $warning = $warn_rec->{warning_text};
        print "<li>";
        # File link
        print "<a target='_other' href='"
              .file_url($file,$linenum)."'>";
        print   "$file:$linenum";
        print "</a> ";
        print "<br>";
        # Warning text
        print "\u$warning";
        # Build log link
        my $log_line = $warn_rec->{first_seen_line};
        print " (<a href='"
          .build_url($tree, $log_line)
            ."' target='_other'>See build log</a>)";
        print "<br>";
        
        # Source code fragment
        #
        my ($keyword) = $warning =~ /\`([^\']*)\'/;
        print "<table cellpadding=4><tr><td bgcolor='#ededed'>";
        print "<pre><font size='-1'>";

        my $source_text = $warn_rec->{source};
        my @source_lines = split /\n/, $source_text;
        my $line_index = $linenum - 2;
        for $line (@source_lines) {
          $line =~ s/&/&amp;/g;
          $line =~ s/</&lt;/g;
          $line =~ s/>/&gt;/g;
          $line =~ s|$keyword|<b>$keyword</b>|g;
          print "<font color='red'>" if $line_index == $linenum;
          print "$line_index $line<BR>";
          print "</font>" if $line_index == $linenum;
          $line_index++;
        }
        print "</font>"; #</pre>";
        print "</td></tr></table>\n";
      }
    }
    print "</ol>\n";
  }
}

sub build_url {
  my ($tree, $linenum) = @_;

  return "http://tinderbox.mozilla.org/showlog.cgi?tree=$tree"
        ."&logfile="
        ."&line=$linenum"
        ."&numlines=50";
}

sub file_url {
  my ($file, $linenum) = @_;

  return "http://cvs-mirror.mozilla.org/webtools/bonsai/cvsblame.cgi"
        ."?file=mozilla/$file&mark=$linenum#".($linenum-10);

}
