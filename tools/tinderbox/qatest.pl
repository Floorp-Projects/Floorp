#!/usr/bin/perl -w
# -*- Mode: perl; indent-tabs-mode: nil -*-
#
###################################################################
# Report Summary Generator for ARMS v2.0 by Vladimir Ermakov	
###################################################################
# This file parses through a results file and generates a
# test summary giving the following info:
#	  # of tcs passed
#	  # of tcs failed
#	  # of tcs died
#	  % of pass and fail
#	  List of testcases that failed.
#	  Separates the results into suites with a convinient quick link
#		 at the top of the page
###################################################################
# Questions Coments go to vladimire@netscape.com
###################################################################

sub PrintUsage {
  die <<END_USAGE
  usage: $0 <filename> ["express"]
END_USAGE
}

if ($#ARGV < 0) {
  PrintUsage();
}

my $DATAFILE = $ARGV[0];
my $MODE;     

if($#ARGV > 0) {
  $MODE = $ARGV[1];
} else {
  $MODE = "";
}

my $express  = 0;

if($MODE eq "express") {
  $express = 1;
}

my $ngdir	= "http://geckoqa.mcom.com/ngdriver/"; #$input->url(-base=>1) . "/ngdriver/";
my $ngsuites = $ngdir . "suites/";
my $conffile = "ngdriver.conf";
my $HTMLreport = "";

unless ($express) {
  print "Content-type: text/html\n\n";
}

&generateResults;

$File{'SuiteList'} = \@suiteArray;

&generateHTML;

$File{'Project'} = "Buffy";

print $HTMLreport;

sub generateResults
  {
	if (!open(INFILE,$DATAFILE)) {
	  print "DATAFILE = $DATAFILE\n";
	  print "<H3> Cannot open $?, $!</H3>\n";
	  return -1;
	}

	$File{'tcsPass'} = 0;
	$File{'tcsFail'} = 0;
	$File{'tcsDied'} = 0;
	$File{'tcsTotal'} = 0;

	$line = <INFILE>;
	while ($line) {
      my %Suite = ();

      my @diedArray = ();
      $Suite{'DiedList'} = \@diedArray;
      my @testArray = ();
      $Suite{'FailedList'} = \@testArray; 

      # Skip to first anchor.
      while (!($line =~ /<A NAME=".*?">/i) && $line) {
        $line = <INFILE>;       # Go to next line.
      }

      # First anchor.
      $line =~ /<A NAME="(.*?)"><H1>(.*?)<\/H1>/i;


      my $name = $1;
      my $title =$2;

      $Suite{'Name'} = $name; 
      $Suite{'Title'} = $title;

      $Suite{'tcsPass'} = 0;
      $Suite{'tcsFail'} = 0;
      $Suite{'tcsDied'} = 0;
      $Suite{'tcsTotal'} = 0;

      do
		{
          while ($line && !($line =~ /<TC>/i) && !($line =~ /<A name=".*?">/i)) {
            $line = <INFILE>;
          }

          if ($line && ($line =~ /<A name=\"(.*?)\">/i) && ($1 eq $Suite{'Name'})) {
            $line = <INFILE>;  
          }

          # TC = test case.
          if ($line && ($line =~ /<TC>/i)) {
            while ($line && !($line =~ /<ENDTC>/i)) { 
              $line1 = <INFILE>; 
              if ($line1 =~ /<TC>/i) {
                print "<H1>SOMETHING WRONG!</H1><BR>"; next; next;
              }
              $line .= $line1;
            }

            my ($tfName, $tcStat);
            my @lines = split /<->/, $line;
            $tfName = $lines[1];
            $tcStat = $lines[3];
			
            # D=died, P=pass, F=failure
            if ($tcStat eq 'D') {
              $Suite{'tcsDied'} += 1;
              push(@diedArray,$line);
            }
            if ($tcStat eq 'P') {
              $Suite{'tcsPass'} += 1;
              $Suite{'tcsTotal'} += 1;
            }
            if ($tcStat eq 'F') {
              push(@testArray,$line);
              $Suite{'tcsFail'} += 1;
              $Suite{'tcsTotal'} += 1;
            }
            $line = <INFILE>;
          }
		} while ($line && !($line =~ /<A NAME=".*?">/i) && $line);

      $File{'tcsPass'} += $Suite{'tcsPass'};
      $File{'tcsFail'} += $Suite{'tcsFail'};
      $File{'tcsTotal'} += $Suite{'tcsTotal'};
      $File{'tcsDied'} += $Suite{'tcsDied'};
      push(@suiteArray,\%Suite);
	}
	close INFILE;
	1;
  }

sub generateHTML
  {
    my $prjExtension;
    my %extList;

    my $os = `uname -s`;        # Cheap OS id for now

    my %Matrices;
    
    # Hard-coded from ngdriver.conf, I didn't want extra files in tree.
    
    $extList{"mb"}  = "http://cemicrobrowser.web.aol.com/bugReportDetail.php?RID=%";
    $extList{"bs"}  = "http://bugscape.nscp.aoltw.net/show_bug.cgi?id=%";
    $extList{"bz"}  = "http://bugzilla.mozilla.org/show_bug.cgi?id=%";
    $extList{"bzx"} = "http://bugzilla.mozilla.org/show_bug.cgi?id=%";
      
    $prjExtension = "bz";       # Buffy hard-coded here.
      
    $Matricies{"dom-core"} = "http://geckoqa.mcom.com/browser/standards/dom1/tcmatrix/index.html";
    $Matricies{"dom-html"} = "http://geckoqa.mcom.com/browser/standards/dom1/tcmatrix/index.html";
    $Matricies{"domevents"} = "http://geckoqa.mcom.com/browser/standards/dom1/tcmatrix/index.html";
    $Matricies{"javascript"} = "http://geckoqa.mcom.com/browser/standards/javascript/tcmatrix/index.html";
    $Matricies{"forms"} = "http://geckoqa.mcom.com/browser/standards/form_submission/tcmatrix/index.html";
    $Matricies{"formsec"} = "http://geckoqa.mcom.com/browser/standards/form_submission/tcmatrix/index.html";

    unless ($express) {
    $HTMLreport .= <<END_PRINT;
		<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
		<html>
			<head>
			  <title>Test Result Summary on $File{'Platform'}</title>
			  <meta http-equiv="content-type" content="text/html; charset=ISO-8859-1">
			</head>

		<body>
		  <center><u><font color="#990000"><h3>AUTOMATED TEST RESULTS</h3></font></u></center><br>						   
		  <center>			   
		   <table border="0" cols="3" width="90%">
			<tbody>
			 <tr align="Left" valign="CENTER">
			   <td width="250" valign="Middle"><b>OS: </b><font color="#990000">$os</script></font></td>
			 </tr>
			</tbody>			 
		   </table>
		  </center>							
		<hr>		 
 <h3><u>The Following Test Suites Were Run:</u></h3>
<UL>
END_PRINT
    
      for (my $var = 0;$File{'SuiteList'}->[$var]->{'Name'};$var++) {
        if (my $href = $Matrices{$File{'SuiteList'}->[$var]->{'Name'}}) {
          $HTMLreport .= "<li><a href=\"$href\">$File{'SuiteList'}->[$var]->{'Title'}</a></li>\n";
        } else {
          $HTMLreport .= "<li><a href=\"$ngsuites$File{'SuiteList'}->[$var]->{'Name'}\">$File{'SuiteList'}->[$var]->{'Title'}</a></li>\n";
        }
      }
    
    $HTMLreport .= <<END_PRINT;
	</UL>

	<h3><u>Test Result Summary:</u></h3>
END_PRINT
    } # !express


    $HTMLreport .= <<END_PRINT;
<table border=2 cellspacing=0>
<tr valign=top bgcolor=#CCCCCC>
<td bgcolor=#999999>&nbsp;</td>
<td>Passed</td>
<td>Failed</td>
<td>Total</td>
<td>Died</td>
<td>% Passed</td>
<td>% Failed</td>
</tr>
END_PRINT

    for (my $var = 0;$File{'SuiteList'}->[$var]->{'Name'};$var++) {
      $curSuite = $File{'SuiteList'}->[$var];

      $HTMLreport .= "<tr valign=top>\n";
      if($express) {
        $HTMLreport .= "<td>$curSuite->{'Title'}</td>\n";
      } else {
        $HTMLreport .= "<td bgcolor=#cccccc><a href=\"#$curSuite->{'Name'}\">$curSuite->{'Title'}</a></td>\n";
      }
      $HTMLreport .= "<td>$File{'SuiteList'}->[$var]->{'tcsPass'}</td>\n";
      $HTMLreport .= "<td>$File{'SuiteList'}->[$var]->{'tcsFail'}</td>\n";
      $HTMLreport .= "<td>$File{'SuiteList'}->[$var]->{'tcsTotal'}</td>\n";
      $HTMLreport .= "<td>${@{$File{'SuiteList'}}[$var]}{'tcsDied'}</td>\n";

      my $pctPass = $File{'SuiteList'}->[$var]->{'tcsPass'} / $File{'SuiteList'}->[$var]->{'tcsTotal'};
      $pctPass = int($pctPass*10000)/100;
	
      $HTMLreport .= "<td>$pctPass</td>\n";

      my $pctFail = $File{'SuiteList'}->[$var]->{'tcsFail'} / $File{'SuiteList'}->[$var]->{'tcsTotal'};
      $pctFail = int($pctFail*10000)/100;
	
      $HTMLreport .= "<td>$pctFail</td>\n";
      $HTMLreport .= "</tr>\n";
    }

    $HTMLreport .= <<END_PRINT; 
<tr valign=top bgcolor=#FFFFCC>
<td>Total:</td>
<td>$File{'tcsPass'}</td>
<td>$File{'tcsFail'}</td>
<td>$File{'tcsTotal'}</td>
<td>$File{'tcsDied'}</td>
END_PRINT
	my $pctPass = $File{'tcsPass'} / $File{'tcsTotal'};
	$pctPass = int($pctPass*10000)/100;
	my $pctFail = $File{'tcsFail'} / $File{'tcsTotal'};
	$pctFail = int($pctFail*10000)/100;

	$HTMLreport .= <<END_PRINT; 
<td>$pctPass</td>
<td>$pctFail</td>
</tr>
</table>
END_PRINT

    unless ($express) {
    $HTMLreport .= <<END_PRINT;
<h3>Failed Testcases:</h3>
END_PRINT

    my $curSuite;
    for (my $var=0;$File{'SuiteList'}->[$var]->{'Name'};$var++) {
      $curSuite = $File{'SuiteList'}->[$var];

      $HTMLreport .= <<END_PRINT;
	<hr>
	<h2>
	<a NAME="$curSuite->{'Name'}"></a><u><font color="#666600">$curSuite->{'Title'}</font></u></h2>
	<b><u>Died:</u></b>
	<table>
END_PRINT

      for (my $dcnt = 0;$curFile = $File{'SuiteList'}->[$var]->{'DiedList'}->[$dcnt];$dcnt++) {
		($none,$fName) = split(/<->/,$curFile);
		$HTMLreport .= "<tr><td style=\"color: red;\"><A href='$ngsuites$curSuite->{'Name'}/$fName' target=\"new\">$fName</A></td></tr>\n";
      }

      $HTMLreport .= "</table><BR>\n";
      $HTMLreport .= "<b><u>Failures:</u></b><br>";
      $HTMLreport .= "<table BORDER CELLSPACING=0><TR>\n";

      for ($fcnt = 0;$curFile = $File{'SuiteList'}->[$var]->{'FailedList'}->[$fcnt];$fcnt++) {
		($none,$fName,$fDesc,$fStat,$fBug,$fExpected,$fActual) = split(/<->/,$curFile);
		$HTMLreport.= "<TR><TD><A href=\"$ngsuites$curSuite->{'Name'}/$fName\" target=\"_new\">$fName</A></TD><TD>";

		# Quiet perl warnings about unused variables.
		my $tmp; $tmp = $fStat; $tmp = $fActual; $tmp = $fExpected;

		#
		# TEMPRORAY DISABLED.
		#
		my @bugList = split(/[\s+]|,/,$fBug);
		my $bug = "";
		my $index = 0; 

		while ($bugList[$index]) {
          if ($bugList[$index] =~ /(\d+)$prjExtension/i) {
            $bugList[$index] =~ s/(\d+)($prjExtension)/makelink($1,$extList{$2})/ige; 
            $bug .= $bugList[$index];
          }
 
          $index++;
		}
		$HTMLreport.= "$bug</TD><TD>$fDesc</TD></TR>\n";

        #		 $fBug =~ s/(\d+)($prjExtension)/makelink($1,$extList{$2})/ige; 
        #		 $HTMLreport.= "$fBug</TD><TD>$fDesc</TD></TR>\n";

      }
      $HTMLreport .= "</table>\n";
    }
	
    $HTMLreport .= <<END_PRINT;
	</BODY>
	</HTML>
END_PRINT
  } # !express

    1;
  }

sub makelink
  {
	$bugNum = $_[0];
	$bugLnk = $_[1];

	
	$bugLnk =~ s/%/$bugNum/ig;
	$bugLnk = "<A href=\"$bugLnk\">$bugNum</A>";

	return $bugLnk;
  }

1;
