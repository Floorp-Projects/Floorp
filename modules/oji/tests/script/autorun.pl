# 
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Sun Microsystem
# Inc. Portions created by Sun are
# Copyright (C) 1999 Sun Microsystems, Inc. All
# Rights Reserved.
#
# Contributor(s): 
# !/bin/perl
#
#
# Created By:     Raju Pallath
# Creation Date:  Aug 2nd 1999
#
# Ported by:	  Konstantin S. Ermakov
# Ported date:	  Aug 24th 1999
#
# This script is used to  invoke all test case for OJI API
# through apprunner and to recored their results
#
#

# Attach Perl Libraries
###################################################################
use POSIX ":sys_wait_h"; #used by waitpid
use Cwd;
use File::Copy;
#use Win32::Process;
#use IO::Handle;
###################################################################


########################
#                      #
#     User's settings  #
#                      #
########################


#sometimes we need to specify additional parameters for mozilla
$ADDITIONAL_PARAMETERS="-P mozProfile";

# time in seconds after which the apprunner has to be killed.
# by default the apprunner will be up for so much time regardless of
# whether over or not. User can either decrease it or increase it.
#
$DELAY_FACTOR = 30;


#Mozilla's executable
$MOZILLA_EXECUTABLE="mozilla-bin";
if ($^O =~ /Win32/i) {
  $MOZILLA_EXECUTABLE="mozilla.exe";
}

$FULL_TEST_LIST="OJITestsList.lst";

$TEST_RESULTS="OJITestResults.txt";

$TEST_DESCRIPTIONS="../OJIAPITestsDescr.html";
$TARGET_WINDOW="Test Descriptions";

$DEFAULT_TEST_URL="http://shiva:10001/oji";

# time period in seconds of periodically checking: is the apprunner still alive
$DELAY_OF_CYCLE = 1;


##############################################################
##############################################################
##############################################################


$DOCROOT = $ENV{"TEST_URL"} ? $ENV{"TEST_URL"} : $DEFAULT_TEST_URL;

# delimiter for logfile
$delimiter="###################################################\n";


##################################################################
# Usage
##################################################################
sub usage() {

    print "\n";
    print "##################################################################\n";
    print "   perl autorun.pl [ -t <test case> ]\n";
    print "\n";
    print " where <test case> is one of the test cases\n";
    print " name ex: JVMManager_CreateProxyJNI_1\n";
    print "\n";
    print "##################################################################\n";
    print "\n";
   
}

##################################################################
# Title display
##################################################################
sub title() {
	
    print "\n";
    print "################################################\n";
    print "   Automated Execution of OJI API TestSuite\n";
    print "################################################\n";
    print "\n";
    print "NOTE: You need to copy files test.html into \n";
    print "      some document directory of HTTP server. TEST_URL environment \n";
    print "      variable should contain the URL of this directory.\n";
    print "\n";
    print "\n";

}

##################################################################
#
# check which tests to run. XML/HTML or both
#
##################################################################
sub checkRun() {
    $runtype = "0";
    while( true ) {
	print "Run 1) HTML Test suite.\n";
	print "    2) XML Test suite.\n";
	print "    3) BOTH HTML and XML.\n";
	print "Enter choice (1-3) :\n";
	$runtype = getc;
	  
	if(( $runtype ne "1" ) &&
	   ( $runtype ne "2" ) &&
	   ( $runtype ne "3" ) ) 
	{
	    print "Invaid choice. Range is from 1-3...\n";
	    print "\n";
	    next;
	} else {
	    last;
	}
    }
}


#########################################################################
#
# Append table entries to Output HTML File 
#
#########################################################################
sub appendEntries() {
   print LOGHTML "<tr><td></td><td></td></tr><td></td>\n";
   print LOGHTML "<tr><td></td><td></td></tr><td></td>\n";
   print LOGHTML "<tr bgcolor=\"#FF6666\"><td>Test Status (XML)</td><td>Result</td><td>Comment</td></tr>\n";

}

#########################################################################
#
# Construct Output HTML file Header
#
#########################################################################
sub constructHTMLHeader() {
   print LOGHTML "<html><head><title>\n";
   print LOGHTML "OJI API Test Status\n";
   print LOGHTML "</title></head><body bgcolor=\"white\">\n";
   print LOGHTML "<center><h1>\n";
   print LOGHTML "OJI API Automated TestRun Results\n";
   $date = localtime;
   print LOGHTML "</h1><h2>", $date;
   print LOGHTML "</h2></center>\n";
   print LOGHTML "<hr noshade>";
   print LOGHTML "<table bgcolor=\"lightgreen\" border=1 cellpadding=10>\n";
   print LOGHTML "<tr bgcolor=\"lightblue\">\n";
   print LOGHTML "<td>Test Case</td>\n";
   print LOGHTML "<td>Result</td>\n";
   print LOGHTML "<td>Comment</td></tr>\n";
}

#########################################################################
#
# Construct Output HTML file indicating status of each run
#
#########################################################################
sub constructHTMLBody() {
    open( MYLOG, $LOGTXT ) || print("WARNING: can't open log file $LOGTXT: $!\n");
    @all_lines = <MYLOG>;
    close( MYLOG );
    
    my $line;
    my $f_cnt = 0;
    my $p_cnt = 0;
    foreach $line ( sort @all_lines ) {
        # avoid linebreaks
	chop $line;
	if ($line eq "") {
	    next;
	}   
	$comment = "---";
	# assuming that all lines are kind'of 'aaa=bbb'
	($class, $status) = split /\: /, $line;	
	if ($status =~ /(.*?) \((.*?)\)$/) {
		$status = $1;
		$comment = $2 ? $2 : "---";
		$p_cnt++;
	}
	if ($status =~ /FAIL/) {
		$status = "<font color=\"red\">".$status;
		$status = $status."<\/font>";
		$f_cnt++;
	}
	print LOGHTML "<tr><td><a target=\"$TARGET_WINDOW\" href=\"$TEST_DESCRIPTIONS#$class\">",$class,"</a></td><td>",$status,"</td><td>",$comment,"</td></tr>\n";
    } 
    my $pp = sprintf "%.2f", $p_cnt/($p_cnt+$f_cnt)*100;  
    my $pf = sprintf "%.2f", $f_cnt/($p_cnt+$f_cnt)*100;
    print LOGHTML "<tr><td colspan=3>Total: $p_cnt($pp\%) tests passed and $f_cnt($pf\%) tests failed.</td></tr>\n";
}
		
#########################################################################
#
# Construct Output HTML file Footer
#
#########################################################################
sub constructHTMLFooter() {
    print LOGHTML "</table></body></html>\n";
}

#########################################################################
#
# Construct Output HTML file indicating status of each run
#
#########################################################################
sub constructHTML() {
	constructHTMLHeader();
	constructHTMLBody();
	constructHTMLFooter();
}

#########################################################################
#
# Construct LogFile Header. The Log file is always appended with entries
#
#########################################################################
sub constructLogHeader() {
    print LOGFILE "\n";
    print LOGFILE "\n";    
    print LOGFILE $delimiter;
    $date = localtime;
    print LOGFILE "Logging Test Run on $date ...\n";
    print LOGFILE $delimiter;
    print LOGFILE "\n";    	

    print "All Log Entries are maintained in LogFile $LOGFILE\n";
    print "\n";
}

#########################################################################
#
# Construct LogFile Footer. 
#
#########################################################################
sub constructLogFooter() {
    print "\n";
    print LOGFILE $delimiter;
    $date = localtime;
    print LOGFILE "End of Logging Test $date ...\n";
    print LOGFILE $delimiter;
    print "\n"; 
}

########################################################################
#
# Construct Log String
#
########################################################################
sub constructLogString {
    my $logstring = shift(@_);
    print LOGFILE "$logstring\n";
    print "$logstring\n";

}

########################################################################
#
# Safely append to file : open, append, close.
#
########################################################################
sub safeAppend {
    my $file = shift(@_);
    my $line = shift(@_);
    open (FILE, ">>$file") or die ("Cann't open $file");
    print FILE $line;
    close FILE;
}


#######################################################################
#
# Running Test case under Win32
#
#######################################################################

sub RunTestCaseWin {
# win32 specific values 
# STILL_ALIVE is a constant defined in winbase.h and indicates what
# process still alive
    $STILL_ALIVE = 0x103;
    do 'Win32/Process.pm';

    open(SAVEOUT, ">&STDOUT" );
    open(SAVEERR, ">&STDERR" );
    open(STDOUT, ">$testlog" ) || die "Can't redirect stdout";
    open(STDERR, ">&STDOUT" ) || die "Can't dup STDOUT";
    select STDERR; $|=1; select STDOUT; $|=1; 
    Win32::Process::Create($ProcessObj,
			   "$mozhome/$MOZILLA_EXECUTABLE",
			   "$mozhome/$MOZILLA_EXECUTABLE $ADDITIONAL_PARAMETERS $DOCFILE",
			   1,
			   NORMAL_PRIORITY_CLASS,
			   "$mozhome" ) || die "cann't start apprunner";
    close(STDOUT);
    close(STDERR);
    open(STDOUT, ">&SAVEOUT");
    open(STDERR, ">&SAVEERR");
    
    $crashed = 0;
    $cnt = 0;
    while (true) {
	sleep($DELAY_OF_CYCLE);
	system("$curdir\\killer.exe");
	$ProcessObj->GetExitCode($exit_code);
	if ($exit_code != $STILL_ALIVE ) {
	    $crashed = 1;
	    $logstr = "Test FAILED...";
	    constructLogString "$logstr";
	    $logstr = "Mozilla terminated with exit code $exit_code.";
	    constructLogString "$logstr";
	    $logstr = "Check ErrorLog File $testlog ";
	    constructLogString "$logstr";
	    constructLogString "========================================\n";	
	    safeAppend $LOGTXT, "$testcase: FAILED (Mozilla crashed with exitcode $exit_code)\n";
	    last;
	}	    
	$cnt += $DELAY_OF_CYCLE;
	if ( $cnt >= $DELAY_FACTOR ) {
	    $ProcessObj->Kill(0);
	    last;
	}
    } # while with sleep
    $crashed;
}


#######################################################################
#
# Running Test case under Unix
#
#######################################################################

sub RunTestCaseUnix {
    $exit_code = 0;
    $pid = 0;
    open(SAVEOUT, ">&STDOUT" );
    open(SAVEERR, ">&STDERR" );
    open(STDOUT, ">$testlog" ) || die "Can't redirect stdout";
    open(STDERR, ">&STDOUT" ) || die "Can't dup STDOUT";
    select STDERR; $|=1; select STDOUT; $|=1; 
    if (!($pid = fork())) {
	#this is a child process - we should start mozilla in it
        # the second exec to overide any shell parsing
        # If argument list had any metacharacter (like ~) then it is pass
        # to /bin/sh -c as a separate process. To avoid that we have the
        # second exec.
	exec(" exec $mozhome/$MOZILLA_EXECUTABLE $ADDITIONAL_PARAMETERS $DOCFILE"); 
	close(STDOUT);
	close(STDERR);
	open(STDOUT, ">&SAVEOUT");
	open(STDERR, ">&SAVEERR");
	die "ERROR: Can't start mozilla: $!\n";	
    }
    #this is a parent process - we should control mozilla execution from it
    close(STDOUT);
    close(STDERR);
    open(STDOUT, ">&SAVEOUT");
    open(STDERR, ">&SAVEERR");
    $cnt = 0;
    $crashed = 0;
    print("Mozilla's pid: $pid\n");
    while (true) {
	sleep($DELAY_OF_CYCLE);
	#print "Try call waitpid ...\n";
	if (waitpid($pid,&WNOHANG)) { 
	    $crashed = 1;
	    $logstr = "Test FAILED...";
	    constructLogString "$logstr";
	    $logstr = "Mozilla terminated by signal ".($? & 127)." with exitcode ".($? >> 8);
	    constructLogString "$logstr";		
	    $logstr = "Check ErrorLog File $testlog ";
	    constructLogString "$logstr";
	    constructLogString "========================================\n";
	    safeAppend $LOGTXT, "$testcase: FAILED (Mozilla terminated by signal ".($? & 127)." with exitcode ".($? >> 8).")\n";
	    last;
	}		
	$cnt += $DELAY_OF_CYCLE;
	if ( $cnt >= $DELAY_FACTOR ) {	
	    kill(9, $pid);
            #now we should take exitcode to avoid zombie appearance
            sleep(1);
            waitpid($pid,&WNOHANG);
	    last;
	}
    } # while with sleep
    $crashed;
}


##################################################################
# main
##################################################################

title;

$curdir = cwd();           
$ret=`mkdir -p $curdir/log`;

# Prepare file names
$LOGFILE = "$curdir/log/BWTestRun.log";
$LOGTXT = "$curdir/log/BWTest.txt";
$LOGHTML = "$curdir/log/BWTest.html";

if ($ARGV[0] =~ "genhtml") {
	open( LOGHTML, ">$LOGHTML" ) or die("Can't open HTML file $LOGHTML ($!)...\n");
	print("Generating result HTML page ...\n");
	constructHTML();
	exit(0);
}



# process command-line parameters
# and check for valid usage

$testparam = "";

if ( $#ARGV > 1 ) {
    usage;
    die;
}

if ( $#ARGV > 0 ) {
    if ( $ARGV[0] != "-t" ) {
	usage;
	die;
    } else {
	$testparam = $ARGV[1];
    }
}

$mozhome = $ENV{"MOZILLA_FIVE_HOME"};

if ( $mozhome eq "" ) {
    print "MOZILLA_FIVE_HOME is not set. Please set it and rerun this script....\n";
    die;
}

if ( ! -f "$mozhome/$MOZILLA_EXECUTABLE" ) {
    print "Could not find $MOZILLA_EXECUTABLE in MOZILLA_FIVE_HOME.\n";
    print "Please check your setting...\n";
    die;
}

# Here must come a piece of code, that determinates 
# apprunner instance, removes core, but there's no
# core under win32

# Backup existing .lst file
if ( ! -f "$curdir/$FULL_TEST_LIST" ) {
	print "Can't find list of OJI API tests to be run.";
	die;
}


$id=$$;


# check if output text file of previous run exists.
# if so, then save it as .bak
#
if ( -f "$LOGTXT" ) {
    $newfile="$LOGTXT.bak";
    rename $LOGTXT, $newfile;
}

# check if output html file of previous run exists.
# if so, then save it as .bak
#
if ( -f "$LOGHTML" ) {
    $newfile="$LOGHTML.bak";
    rename $LOGHTML, $newfile;
}

# construct DOCFILE
$DOCFILE = "$DOCROOT/test.html";
$runcnt = 1;
$filename = "$curdir/$FULL_TEST_LIST";

# Prepare log streams

open( LOGHTML, ">$LOGHTML" ) or die("Can't open HTML file...\n");
open( LOGFILE, ">>$LOGFILE" ) or die("Can't open LOG file...\n");
select LOGFILE; $| = 1; select STDOUT; 




constructLogHeader();

$LST_OUT = "$mozhome/OJITests.lst";

$currcnt = 0;
while (true) {
    open( file, $filename ) or die("Can't open $filename...\n");
    while( $line = <file> ) {
	chop $line;
	if ( $testparam ne "" ) {
	    $testcase = $testparam;
	} else {
	    $testcase = $line;
	}

	if ( $testcase eq "" || $testcase =~ /^\s*#/ ) {
	    next;
	} 

	open(LST_OUT, ">$LST_OUT") or die ("Can't open LST_OUT file...\n");
	print LST_OUT $testcase;
	close(LST_OUT);

	chdir( $mozhome );
	#deleting old result ...
	unlink("$mozhome/$TEST_RESULTS");
	$logstr="Running TestCase $testcase....";
	constructLogString "========================================";
	constructLogString "$logstr";

	($nom) = ($testcase =~ /([^\.]*)$/);
	$testlog = "$curdir/log/$nom.$id.log";

	if ($^O =~ /Win32/i) {
	    $crashed = RunTestCaseWin();
	} else {
	    $crashed = RunTestCaseUnix();
	}

	if (!$crashed) {
	    if (!open (TEST_RES, "$mozhome/$TEST_RESULTS") || ($logstr = <TEST_RES>) =~ /^\s*$/) {
		$logstr = "$testcase: FAILED (Mozilla crashed)";
	    }
      	    close TEST_RES;
	    chomp $logstr;
	    constructLogString "$logstr";
	    constructLogString "========================================\n";
	    safeAppend "$LOGTXT", "$logstr\n";
	}

	( $testparam eq "" ) || last;
    
    } # while ( $line
    
    ( ++$currcnt < $runcnt ) || last;
    
} # while(true)
constructLogFooter;

constructHTML();

chdir($curdir);
	
			









