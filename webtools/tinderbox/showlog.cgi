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

require 'tbglobals.pl';
require 'header.pl';

# Process the form arguments
%form = ();
&split_cgi_args();

#############################################################
# Global variables

$LINES_AFTER_ERROR = 5;
$LINES_BEFORE_ERROR = 30;

my $last_modified_time = 0;
my $expires_time = time() + 3600;

# These variables are set by the error parser functions:
#    has_error(), has_warning(), and has_errorline().
$error_file = '';
$error_file_ref = '';
$error_line = 0;
$error_guess = 0;

#############################################################
# CGI inputs

if (defined($args = $form{log}) or defined($args = $form{exerpt})) {

  ($full_logfile, $linenum) = split /:/,  $args;
  ($tree, $logfile) = split /\//, $full_logfile;

  my $br = tb_find_build_record($tree, $logfile);
  $errorparser = $br->{errorparser};
  $buildname   = $br->{buildname};
  $buildtime   = $br->{buildtime};

  $numlines = 50;
  $numlines = $form{numlines} if exists $form{numlines};
} else {
  $tree        = $form{tree};
  $errorparser = $form{errorparser};
  $logfile     = $form{logfile};
  $buildname   = $form{buildname};
  $buildtime   = $form{buildtime};
}
$fulltext    = $form{fulltext};

$enc_buildname = url_encode($buildname);

die "the \"tree\" parameter must be provided\n" unless $tree;
require "$tree/treedata.pl";

$time_str = print_time($buildtime);

$|=1;

my @stat_logfile = stat("$tree/$logfile");
my @stat_notes = stat("$tree/notes.txt");
if ($stat_logfile[9] > $stat_notes[9]) {
    $last_modified_time = $stat_logfile[9];
} else {
    $last_modified_time = $stat_notes[9];
}

if ($linenum) {

  print_fragment();

  exit;
}

print_header();
print_notes();

# Dynamically load the error parser
#
die "the \"errorparser\" parameter must be provided\n" unless $errorparser;
require "ep_${errorparser}.pl";

if ($fulltext)
{
  my $errors = print_summary();
  print_log($errors);
}
else
{
  $brief_filename = $logfile;
  $brief_filename =~ s/.gz$/.brief.html/;
  if (-T "$tree/$brief_filename" and -M _ > -M $tree/$logfile) 
  {
    open(BRIEFFILE, "<$tree/$brief_filename");
    print while (<BRIEFFILE>)
  }
  else
  {
    open(BRIEFFILE, ">$tree/$brief_filename");

    my $errors = print_summary();
    print_log($errors);
  }
}

# end of main
############################################################

sub print_fragment {
  print "Content-type: text/html\n";
  print "Last-Modified: " . gmtime($last_modified_time) . "\n";
  print "Expires: " . gmtime($expires_time) . "\n";
  print "\n";

  my $heading = "Build Log (Fragment)";
  my $subheading = "$buildname on $time_str";
  my $title = "$heading - $subheading";

  EmitHtmlTitleAndHeader($title, $heading, $subheading);

  print "<a href='showlog.cgi?tree=$tree&errorparser=$errorparser&logfile=$logfile&buildtime=$buildtime&buildname=$enc_buildname&fulltext=1'>Show Full Build Log</a>";

  open(BUILD_IN, "$gzip -d -c $tree/$logfile|");

  my $first_line = $linenum - ($numlines/2);
  my $last_line  = $linenum + ($numlines/2);

  print "<pre><b>.<br>.<br>.<br></b>";
  while (<BUILD_IN>) {
    next if $. < $first_line;
    last if $. > $last_line;
    print "<b><font color='red'>" if $. == $linenum;
    print;
    print "</font></b>" if $. == $linenum;
  }
  print "<b>.<br>.<br>.<br></b></pre>";

}

sub print_header {
  print "Content-type: text/html\n";
  print "Last-Modified: " . gmtime($last_modified_time) . "\n";
  print "Expires: " . gmtime($expires_time) . "\n";
  print "\n";

  if ($fulltext) {
    $s = 'Show <b>Brief</b> Log';
    $s1 = '';
    $s2 = 'Full';
  }
  else {
    $s = 'Show <b>Full</b> Log';
    $s1 = 1;
    $s2 = 'Brief';
  }
  
  my $heading = "Build Log ($s2)";
  my $subheading = "$buildname on $time_str";
  my $title = "$heading - $subheading";

  EmitHtmlTitleAndHeader($title, $heading, $subheading);

  print "
<font size=+1>
<dt><a href='showlog.cgi?tree=$tree&errorparser=$errorparser&logfile=$logfile&buildtime=$buildtime&buildname=$enc_buildname&fulltext=$s1'>$s</a>
<dt><a href=\"showbuilds.cgi?tree=$tree\">Return to the Build Page</a>
<dt><a href=\"addnote.cgi?tree=$tree\&buildname=$enc_buildname\&buildtime=$buildtime\&logfile=$logfile\&errorparser=$errorparser\">
Add a Comment to the Log</a>
</font>
";
}

sub print_notes {
  #
  # Print notes
  #
  $found_note = 0;
  open(NOTES,"<$tree/notes.txt") 
    or print "<h2>warning: Couldn't open $tree/notes.txt </h2>\n";
  print "$buildtime, $buildname<br>\n";
  while (<NOTES>) {
    chop;
    ($nbuildtime,$nbuildname,$nwho,$nnow,$nenc_note) = split(/\|/);
    #print "$_<br>\n";
    if ($nbuildtime == $buildtime and $nbuildname eq $buildname) {
      if (not $found_note) {
	print "<H2>Build Comments</H2>\n";
	$found_note = 1;
      }
      $now_str = print_time($nnow);
      $note = url_decode($nenc_note);
      print "<pre>\n[<b><a href=mailto:$nwho>$nwho</a> - $now_str</b>]\n$note\n</pre>";
    }
  }
  close(NOTES);
}

sub print_summary {
  #
  # Print the summary first
  #
  logprint('<H2>Build Error Summary</H2><PRE>');

  @log_errors = ();

  my $line_num = 0;
  my $error_num = 0;
  open(BUILD_IN, "$gzip -d -c $tree/$logfile|");
  while ($line = <BUILD_IN>) {
    $line_has_error = output_summary_line($line, $error_num);
    
    if ($line_has_error) {
      push @log_errors, $line_num;        
      $error_num++;
    }
    $line_num++;
  }
  close(BUILD_IN);

  logprint('</PRE>');

  return \@log_errors;
}

sub print_log_section {
  my ($tree, $logfile, $line_of_interest, $num_lines) = shift;
  local $_;

  my $first_line = $line_of_interest - $num_lines / 2;
  my $last_line = $first_line + $num_lines;

  print "<a href='showlog.cgi?tree=$tree&logfile=$logfile&line="
        .($line_of_interest-$num_lines)."&numlines=$num_lines'>"
        ."Previous $num_lines</a>";
  print "<font size='+1'><b>.<br>.<br>.<br></b></font>";
  print "<pre>";
  my $ii = 0;
  open BUILD_IN, "$gzip -d -c $tree/$logfile|";
  while (<BUILD_IN>) {
    $ii++;
    next if $ii < $first_line;
    last if $ii > $last_line;
    if ($ii == $line_of_intested) {
      print "<b>$_</b>";
    } else {
      print;
    }
  }
  close BUILD_IN;
  print "</pre>";
  print "<font size='+1'><b>.<br>.<br>.<br></b></font>";
  print "<a href='showlog.cgi?tree=$tree&logfile=$logfile&line="
        .($line_of_interest+$num_lines)."&numlines=$num_lines'>"
        ."Next $num_lines</a>";
}

sub print_log {
  my ($errors) = $_[0];

  logprint('<H2>Build Error Log</H2><pre>');

  $line_num = 0;
  open(BUILD_IN, "$gzip -d -c $tree/$logfile|");
  while ($line = <BUILD_IN>) {
    output_log_line($line, $line_num, $errors);
    $line_num++;
  }
  close(BUILD_IN);

  logprint('</PRE><p>'
     ."<font size=+1>No More Errors</a></font>"
     .'<br><br><br>');
}

BEGIN {
  my $last_was_error = 0; 

  sub output_summary_line {
    my ($line, $error_id) = @_;
    
    if (has_error($line)) {
      $line =~ s/&/&amp;/g;
      $line =~ s/</&lt;/g;
      
      if (not $last_was_error) {
        logprint("<a href=\"#err$error_id\">$line</a>");
      } else {
        logprint("$line");
      }
      $last_was_error = 1;
    } else {
      $last_was_error = 0;
    }
    return $last_was_error;
  }
}


BEGIN {
  my $next_error = 0;

  sub output_log_line {
    my ($line, $line_num, $errors) = @_;
    
    my $has_error   = $line_num == $errors->[$next_error];
    my $has_warning = has_warning($line);
    
    $line =~ s/&/&amp;/g;
    $line =~ s/</&lt;/g unless $line =~ /^<a name=[^>]*>(?:<\/a>)?$/i or
                               $line =~ /^<\/a>$/i;

    my $logline = '';
    
    my %out = ();

    if (($has_error or $has_warning) and has_errorline($line, \%out)) {
      $q = quotemeta($out{error_file});
      $goto_line = $out{error_line} > 10 ? $out{error_line} - 10 : 1;
      $cvsblame = $out{error_guess} ? "cvsguess.cgi" : "cvsblame.cgi"; 
      $line =~ s@$q@<a href=../bonsai/$cvsblame?file=$out{error_file_ref}&rev=$cvs_branch&mark=$out{error_line}#$goto_line>$out{error_file}</a>@
    }

    if ($has_error) {
      $next_error++;

      unless ($last_was_error) {
        $logline .= "<a name='err".($next_error - 1)."'></a>";

        # Only print "NEXT ERROR" link if there is another error to jump to
        $have_more_errors = 0;
        my $ii = $next_error;
        while ($ii < $#{$errors} - 1) {
          if ($errors->[$ii] != $errors->[$ii + 1] - 1) {
            $have_more_errors = 1;
            last;
          }
          $ii++;
        }
        if ($have_more_errors) {
          $logline .= "<a href='#err$next_error'>NEXT ERROR</a> ";
        }
      }
      $logline .= "<font color='000080'>$line</font>";
      
      $last_was_error = 1;
    }
    elsif ($has_warning) {
      $logline = "<font color='000080'>$line</font>";
    }
    else {
      $logline = $line;
      $last_was_error = 0;
    }
    
    push_log_line($logline, $errors);
  }
}


sub push_log_line {
    my ($line, $log_errors) = @_;
    if ($fulltext) {
        logprint($line);
        return;
    }

    if ($log_line > $log_errors->[$cur_error] + $LINES_AFTER_ERROR) {
        $cur_error++;
    }
    
    if ($log_line >= $log_errors->[$cur_error] - $LINES_BEFORE_ERROR) {
        if ($log_skip != 0) {
            logprint("\n<i><font size=+1> Skipping $log_skip Lines...</i></font>\n\n");
            $log_skip = 0;
        }
        logprint($line);
    }
    else {
        $log_skip++;
    }
    $log_line++;
}

sub logprint {
  my $line  = $_[0];
  print $line;
  print BRIEFFILE $line if not $fulltext;
}
