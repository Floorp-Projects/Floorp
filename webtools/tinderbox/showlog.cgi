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

use lib "../bonsai";

require 'globals.pl';
require 'lloydcgi.pl';
require 'header.pl';

#use Benchmark;
#$t0 = new Benchmark;

$|=1;

$LINES_AFTER_ERROR = 5;
$LINES_BEFORE_ERROR = 30;

$error_file = '';
$error_file_ref = '';
$error_line = 0;
$error_guess = 0;

print "Content-type: text/html\n\n";

$error_count = 0;

$next_err = 0;

@log_errors = ();
$log_ln = 0;

if (1) {
    $tree = $form{'tree'};
    $errorparser = $form{'errorparser'};
    $logfile = $form{'logfile'};
    $summary = $form{'summary'};
    $buildname = $form{'buildname'};
    $buildtime = $form{'buildtime'};
    $enc_buildname = &url_encode($buildname);
    $frames = $form{'frames'};
    $fulltext = $form{'fulltext'};
    $mainframe = $form{'mainframe'};
    #print "$buildname \n $buildtime \n $errorparser \n $logfile \n  $tree \n $enc_buildname \n";
}
else {
    $tree = 'FreeSource';
    $errorparser = 'windows';
    $logfile = '19692.gz';
}

die "the \"tree\" parameter must be provided\n" unless $tree;
require "$tree/treedata.pl";

if( $mainframe ){
    print "
<HTML>
<FRAMESET name = main ROWS='70%, 30%'>
    <FRAME NAME='log' SCROLL=AUTO SRC='showlog.cgi?tree=$tree&errorparser=$errorparser&logfile=$logfile&summary=$summary&buildtime=$buildtime&buildname=$enc_buildname&fulltext=$fulltext&frames=1'>
    <FRAME NAME='SOURCEFRAME' SCROLL='AUTO' SRC='Empty.html'>
</FRAMESET>
</HTML>
";
    exit 0;
}

if( $frames ){
    $source_target = "target=SOURCEFRAME";
}


#
# Dynamically load the error parser
#
die "the \"errorparser\" parameter must be provided\n" unless $errorparser;
require "ep_${errorparser}.pl";

$time_str = print_time( $form{'buildtime'} );

if( $fulltext ){
    $s = 'Show <b>Brief</b> Log';
    $s1 = '';
    $s2 = 'Full';
}
else {
    $s = 'Show <b>Full</b> Log';
    $s1 = 1;
    $s2 = 'Brief';
}

print "<META HTTP-EQUIV=\"EXPIRES\" CONTENT=\"1\">\n";

my $heading = "Build Log ($s2)";
my $subheading = "$form{'buildname'} on $time_str";
my $title = "$heading - $subheading";

EmitHtmlTitleAndHeader($title, $heading, $subheading);

print "
<font size=+1>
<dt><a href='showlog.cgi?tree=$tree&errorparser=$errorparser&logfile=$logfile&summary=$summary&buildtime=$buildtime&buildname=$enc_buildname&fulltext=$s1&frames=1'>$s</a>
<dt><a href=\"showbuilds.cgi?tree=$form{'tree'}\">Return to the Build Page</a>
<dt><a href=\"addnote.cgi?tree=$tree\&buildname=$enc_buildname\&buildtime=$buildtime\&logfile=$logfile\&errorparser=$errorparser\">
Add a Comment to the Log</a>
</font>
";

#
# Print notes
#
$found_note = 0;
open(NOTES,"<$tree/notes.txt") || print "<h2>warning: Couldn't open $tree/notes.txt </h2>\n";
while(<NOTES>){
    chop;
    ($nbuildtime,$nbuildname,$nwho,$nnow,$nenc_note) = split(/\|/);
    #print "$_<br>\n";
    if( $nbuildtime == $buildtime && $nbuildname eq $buildname ){
        if( !$found_note ){
            print "<H2>Build Comments</H2>\n";
            $found_note = 1;
        }
        $now_str = &print_time($nnow);
        $note = &url_decode($nenc_note);
        print "<pre>\n[<b><a href=mailto:$nwho>$nwho</a> - $now_str</b>]\n$note\n</pre>";
    }
}
close(NOTES);

#
# Print the summery fisrt
#
$log_ln = 0;
$next_err = 0;
@summary_output = ();
@log_output = ();
open( BUILD_IN, "$gzip -d -c $tree/$logfile|" ) or die "Unable to open $tree/$logfile\n";
while( $line = <BUILD_IN> ){
    &parse_line( $line );
}
close( BUILD_IN );
push @log_errors, 9999999;        


print "
<H2>Build Error Summary</H2>
<p> Click error to take you to the error in the log.
<PRE>
";
print foreach (@summary_output);
print "</PRE>\n";

print "<H2>Build Error Log</H2>\n<pre>";
print foreach (@log_output);
print
"</PRE>

<p>
<font size=+1><a name=\"err$next_err\">No More Errors</a></font>
<br>
<br>
<br>
";

#$t1 = new Benchmark;
#print "Total time ".timestr(timediff($t1,$t0))."\n";


sub parse_line {
    my( $line ) = @_;
    my( $has_error, $dur, $dur_min,$dur_sec, $dur_str, $logline );

    $has_error = &has_error;
    $has_warning = &has_warning;

    $line =~ s/&/&amp;/g;
    $line =~ s/</&lt;/g;

    $logline = '';

    if( ($has_error || $has_warning) && &has_errorline( $line ) ) {
        $q = quotemeta( $error_file );
        #$goto_line = ($error_line ? 10 > $error_line - 10 : 1 );
        $goto_line = ($error_line > 10 ? $error_line - 10 : 1 );
        $cvsblame = ($error_guess ? "cvsguess.cgi" : "cvsblame.cgi"); 
        $line =~ s@$q@<a href=../bonsai/$cvsblame?file=$error_file_ref&rev=$cvs_branch&mark=$error_line#$goto_line $source_target>$error_file</a>@
    }

    if( $has_error ){
        #
        # Make Summary line
        # 
        push @log_errors, $log_ln + $LINES_AFTER_ERROR;        
        if( ! $last_was_error ) {
            push @summary_output, "<a href=\"#err$next_err\">$line</a>";
        }

        #
        # Make log line
        # 

        if( ! $last_was_error ) {
            $logline .= "<a name=\"err$next_err\"></a>";
            $next_err++;
            $logline .= "<a href=\"#err$next_err\">NEXT</a> ";
        }
        else {
            $logline .= "     ";
        }

        $logline .= "<font color=\"000080\">$line</font>";
        
        $last_was_error = 1;
    }
    elsif( $has_warning ){
        $logline .= "     ";
        $logline .= "<font color=000080>$line</font>";
    }
    else {
        $logline .= "     $line";
        $last_was_error = 0;
    }
    $log_ln++;

    &push_log_line( $logline );
}


sub push_log_line {
    my( $ln ) = @_;
    if( $fulltext ){
        push @log_output, $ln;
        return;
    }

    if( $log_ln > $log_errors[$cur_error] ){
        $cur_error++;
    }
    
    if( $log_ln >= $log_errors[$cur_error] - $LINES_BEFORE_ERROR ){
        if( $log_skip != 0 ){
            push @log_output,
	      "\n<i><font size=+1> Skipping $log_skip Lines...</i></font>\n\n";
            $log_skip = 0;
        }
        push @log_output, $ln;
    }
    else {
        $log_skip++;
    }
    $log_ln++;
}
