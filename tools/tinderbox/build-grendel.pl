#!/usr/bin/perl

require 5.000;

use Sys::Hostname;
use POSIX "sys_wait_h";
use Cwd;

$Version = "0.001";

sub InitVars {

    # PLEASE FILL THIS IN WITH YOUR PROPER EMAIL ADDRESS
    $BuildAdministrator = "$ENV{'USER'}\@$ENV{'HOST'}";

    #Default values of cmdline opts
    $BuildDepend = 0;	#depend or clobber (clobber by default)
    $ReportStatus = 1;  # Send results to server or not
    $BuildOnce = 0;     # Build once, don't send results to server
    $BuildClassic = 0;  # Build classic source

    #relative path to binary
    $BinaryName{'grendel'} = 'Main.class';

    # Set these to what makes sense for your system
    $cpus = 1;
    $Make = 'gmake'; # Must be gnu make
    $mail = '/bin/mail';
    $Autoconf = 'autoconf -l build/autoconf';
    $CVS = 'cvs -z3';
    $CVSCO = 'co -P';

    # Set these proper values for your tinderbox server
    $Tinderbox_server = 'tinderbox-daemon\@cvs-mirror.mozilla.org';
    #$Tinderbox_server = 'external-tinderbox-incoming\@tinderbox.seawood.org';

    # These shouldn't really need to be changed
    $BuildSleep = 1; # Minimum wait period from start of build to start
                      # of next build in minutes (grendel depend builds 

    $BuildTree = 'MozillaTest'; # until you get the script working.
                                # when it works, change to the tree you're
                                # actually building
    $BuildTag = '';
    $BuildName = '';
    $TopLevel = '.';
    $Topsrcdir = 'mozilla/grendel';
    $BuildObjName = '';
    $BuildConfigDir = 'mozilla/grendel/config';
    $ClobberStr = 'realclean';
    # Yeah, i know, hardcoding is bad. Set this to what you want
    $ConfigGuess = './build/autoconf/config.guess';
    $Logfile = '${BuildDir}.log';
  } #EndSub-InitVars

 
sub SetupEnv {
    umask(0);
    $ENV{"CVSROOT"} = ':pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot';
    # this needs to be set to run smoketests (to a display the box can open)
    #   $ENV{"DISPLAY"} = 'crucible.mcom.com:0.0';
} #EndSub-SetupEnv

sub SetupPath { 
  # this is a noop for now (can't think of path specifics that grendel needs)
    my($Path);
    $Path = $ENV{PATH};
    print "Path before: $Path\n";
    
    # this is where you'd add path stuff (probably conditional to platform)

    $Path = $ENV{PATH};
    print "Path After: $Path\n";
} #EndSub-SetupPath

##########################################################################
# NO USER CONFIGURABLE PIECES BEYOND THIS POINT                          #
##########################################################################

sub GetSystemInfo {

    $OS = `uname -s`;
    $OSVer = `uname -r`;
    
    chop($OS, $OSVer);
    
    if ( $OS eq 'AIX' ) {
	$OSVer = `uname -v`;
	chop($OSVer);
	$OSVer = $OSVer . "." . `uname -r`;
	chop($OSVer);
    }
        
    if ( $OS eq 'IRIX64' ) {
	$OS = 'IRIX';
    }
    
    if ( $OS eq 'SCO_SV' ) {
	$OS = 'SCOOS';
	$OSVer = '5.0';
    }
    
    my $host, $myhost = hostname;
    chomp($myhost);
    ($host, $junk) = split(/\./, $myhost);
	
    $BuildName = ""; 
	
    if ( "$host" ne "" ) {
	$BuildName = $host . ' ';
    }
    $BuildName .= $OS . ' ' . $OSVer . ' ' . ($BuildDepend?'Depend':'Clobber');
    $DirName = $OS . '_' . $OSVer . '_' . ($BuildDepend?'depend':'clobber');
    
    $RealOS = $OS;
    $RealOSVer = $OSVer;
    
    if ( $OS eq 'HP-UX' ) {
	$RealOSVer = substr($OSVer,0,4);
    }
    
    if ( $OS eq 'Linux' ) {
	$RealOSVer = substr($OSVer,0,3);
    }

    $logfile = "${DirName}.log";
  
} #EndSub-GetSystemInfo

sub BuildIt {
    my ($fe, @felist, $EarlyExit, $LastTime, $StartTimeStr);

    mkdir("$DirName", 0777);
    chdir("$DirName") || die "Couldn't enter $DirName";
    
    $StartDir = getcwd();
    $LastTime = 0;
    
    print "Starting dir is : $StartDir\n";
    
    $EarlyExit = 0;
    
    while ( ! $EarlyExit ) {
	chdir("$StartDir");
	if ( time - $LastTime < (60 * $BuildSleep) ) {
	    $SleepTime = (60 * $BuildSleep) - (time - $LastTime);
	    print "\n\nSleeping $SleepTime seconds ...\n";
	    sleep($SleepTime);
	}
	
	$LastTime = time;
	
	$StartTime = time - 60 * 10;
	$StartTimeStr = &CVSTime($StartTime);
	
	&StartBuild if ($ReportStatus);
 	$CurrentDir = getcwd();
	if ( $CurrentDir ne $StartDir ) {
	    print "startdir: $StartDir, curdir $CurrentDir\n";
	    die "curdir != startdir";
	}

	$BuildDir = $CurrentDir;
	
	unlink( "$logfile" );
	
	print "opening $logfile\n";
	open( LOG, ">$logfile" ) || print "can't open $?\n";
	print LOG "current dir is -- $hostname:$CurrentDir\n";
	print LOG "Build Administrator is $BuildAdministrator\n";
	&PrintEnv;
	
	$BuildStatus = 0;

	mkdir($TopLevel, 0777);
	chdir($TopLevel) || die "chdir($TopLevel): $!\n";
	      
	print "$CVS $CVSCO $Topsrcdir\n";
	print LOG "$CVS $CVSCO $Topsrcdir\n";
	open (PULL, "$CVS $CVSCO $Topsrcdir 2>&1 |") || die "open: $!\n";
	while (<PULL>) {
	    print $_;
	    print LOG $_;
	}
	close(PULL);
	
	# Move to topsrcdir
	chdir($Topsrcdir) || die "chdir($Topsrcdir): $!\n";
	    
	# if we are building depend, rebuild dependencies
	
	if ($BuildDepend) {
	    print LOG "$Make MAKE='$Make -j $cpus' depend 2>&1 |\n";
	    open ( MAKEDEPEND, "$Make MAKE='$Make -j $cpus' depend 2>&1 |\n");
	    while ( <MAKEDEPEND> ) {
		print $_;
		print LOG $_;
	    }
	    close (MAKEDEPEND);
	    system("rm -rf dist");

	} else {
	    # Building clobber
	    print LOG "$Make MAKE='$Make -j $cpus' $ClobberStr 2>&1 |\n";
	    open( MAKECLOBBER, "$Make MAKE='$Make -j $cpus' $ClobberStr 2>&1 |");	    
	    while ( <MAKECLOBBER> ) {
	    	print $_;
	    	print LOG $_;
	    }
	    close( MAKECLOBBER );
	}

	@felist = split(/,/, $FE);
	    
	foreach $fe ( @felist ) {	    
	    if (&BinaryExists($fe)) {
		print LOG "deleting existing binary\n";
		&DeleteBinary($fe);
	    }
	}

	print LOG "$Make MAKE='$Make -j $cpus' 2>&1 |\n";
	open(BUILD, "$Make MAKE='$Make -j $cpus' 2>&1 |\n");
	while (<BUILD>) {
	  print $_;
	  print LOG $_;
	}
	close(BUILD);
      

	foreach $fe (@felist) {
	  if (&BinaryExists($fe)) {
	    print LOG "export binary exists, build successful. Testing...\n";
	    #return 0 if no problem, else 333 for a runtime error
	    $BuildStatus = &RunSmokeTest($fe);
	  }
	  else {
	    print LOG "export binary missing, build FAILED\n";
	    $BuildStatus = 666;
	  }
	  
	  if ( $BuildStatus == 0 ) {
	    $BuildStatusStr = 'success';
	  }
	  elsif ( $BuildStatus == 333 ) {
	    $BuildStatusStr = 'testfailed';
	  }
	  else {
	    $BuildStatusStr = 'busted';
	  }
	  # replaced by above lines
	  #	    $BuildStatusStr = ( $BuildStatus ? 'busted' : 'success' );
	  
	  print LOG "tinderbox: tree: $BuildTree\n";
	  print LOG "tinderbox: builddate: $StartTime\n";
	  print LOG "tinderbox: status: $BuildStatusStr\n";
	  print LOG "tinderbox: build: $BuildName $fe\n";
	  print LOG "tinderbox: errorparser: unix\n";
	  print LOG "tinderbox: buildfamily: unix\n";
	  print LOG "tinderbox: END\n";	    
	}
	close(LOG);
	chdir("$StartDir");
	
# this fun line added on 2/5/98. do not remove. Translated to english,
# that's "take any line longer than 1000 characters, and split it into less
# than 1000 char lines.  If any of the resulting lines is
# a dot on a line by itself, replace that with a blank line."  
# This is to prevent cases where a <cr>.<cr> occurs in the log file.  Sendmail
# interprets that as the end of the mail, and truncates the log before
# it gets to Tinderbox.  (terry weismann, chris yeh)
#
# This was replaced by a perl 'port' of the above, writen by 
# preed@netscape.com; good things: no need for system() call, and now it's
# all in perl, so we don't have to do OS checking like before.

	open(LOG, "$logfile") || die "Couldn't open logfile: $!\n";
	open(OUTLOG, ">${logfile}.last") || die "Couldn't open logfile: $!\n";
	    
	while (<LOG>) {
	    $q = 0;
	    
	    for (;;) {
		$val = $q * 1000;
		$Output = substr($_, $val, 1000);
		
		last if $Output eq undef;
		
		$Output =~ s/^\.$//g;
		$Output =~ s/\n//g;
		print OUTLOG "$Output\n";
		$q++;
	    } #EndFor
		
	} #EndWhile
	    
	close(LOG);
	close(OUTLOG);

	system( "$mail $Tinderbox_server < ${logfile}.last" )
	    if ($ReportStatus );
	unlink("$logfile");
	
	# if this is a test run, set early_exit to 0. 
	#This mean one loop of execution
	$EarlyExit++ if ($BuildOnce);
    }
    
} #EndSub-BuildIt

sub CVSTime {
    my($StartTimeArg) = @_;
    my($RetTime, $StartTimeArg, $sec, $minute, $hour, $mday, $mon, $year);
    
    ($sec,$minute,$hour,$mday,$mon,$year) = localtime($StartTimeArg);
    $mon++; # month is 0 based.
    
    sprintf("%02d/%02d/%02d %02d:%02d:00", $mon,$mday,$year,$hour,$minute );
}

sub StartBuild {
    
    my($fe, @felist);

    @felist = split(/,/, $FE);

#    die "SERVER: " . $Tinderbox_server . "\n";
    open( LOG, "|$mail $Tinderbox_server" );
    foreach $fe ( @felist ) {
	print LOG "\n";
	print LOG "tinderbox: tree: $BuildTree\n";
	print LOG "tinderbox: builddate: $StartTime\n";
	print LOG "tinderbox: status: building\n";
	print LOG "tinderbox: build: $BuildName $fe\n";
	print LOG "tinderbox: errorparser: unix\n";
	print LOG "tinderbox: buildfamily: unix\n";
	print LOG "tinderbox: END\n";
	print LOG "\n";
    }
    close( LOG );
}

# check for the existence of the binary
sub BinaryExists {
    my($fe) = @_;
    my($Binname);
    $fe = 'x' if (!defined($fe)); 

    $BinName = $BuildDir . '/' . $TopLevel . '/' . $Topsrcdir . '/' . $BuildObjName . $BinaryName{"$fe"};
    
    print LOG $BinName . "\n"; 
    if ((-e $BinName) && (-x $BinName) && (-s $BinName)) {
	1;
    }
    else {
	0;
    }
}

sub DeleteBinary {
    my($fe) = @_;
    my($BinName);
    $fe = 'x' if (!defined($fe)); 

    $BinName = $BuildDir . '/' . $TopLevel . '/' . $Topsrcdir . '/' . $BuildObjName . $BinaryName{"$fe"};
    
    print LOG "unlinking $BinName\n";
    unlink ($BinName) || print LOG "unlinking $BinName failed\n";
}

sub ParseArgs {
    my($i, $manArg);

    if( @ARGV == 0 ) {
	&PrintUsage;
    }
    $i = 0;
    $manArg = 0;
    while( $i < @ARGV ) {
	if ($ARGV[$i] eq '--depend') {
	    $BuildDepend = 1;
 	    $manArg++;
	}
	elsif ($ARGV[$i] eq '--clobber') {
	    $BuildDepend = 0;
	    $manArg++;
	}
	elsif ( $ARGV[$i] eq '--once' ) {
	    $BuildOnce = 1;
	    #$ReportStatus = 0;
	}
	elsif ($ARGV[$i] eq '--noreport') {
	    $ReportStatus = 0;
	}
	elsif ($ARGV[$i] eq '--version' || $ARGV[$i] eq '-v') {
	    die "$0: version $Version\n";
	}
	elsif ( $ARGV[$i] eq '-tag' ) {
	    $i++;
	    $BuildTag = $ARGV[$i];
	    if ( $BuildTag eq '' || $BuildTag eq '-t') {
		&PrintUsage;
	    }
	}
	elsif ( $ARGV[$i] eq '-t' ) {
	    $i++;
	    $BuildTree = $ARGV[$i];
	    if ( $BuildTree eq '' ) {
		&PrintUsage;
	    }
	} else {
	    &PrintUsage;
	}

	$i++;
    } #EndWhile

    if ( $BuildTree =~ /^\s+$/i ) {
	&PrintUsage;
    }

    if ($BuildDepend eq undef) {
	&PrintUsage;
    }

    &PrintUsage if (! $manArg );

} #EndSub-ParseArgs

sub PrintUsage {
    die "usage: $0 [--depend | --clobber] [-v | --version ] [--once --classic --noreport -tag TREETAG -t TREENAME ]\n";
}

sub PrintEnv {
    my ($key);
    foreach $key (keys %ENV) {
	print LOG "$key = $ENV{$key}\n";
	print "$key = $ENV{$key}\n";
    }
} #EndSub-PrintEnv

sub RunSmokeTest {
    my($fe) = @_;
    my($Binary);
    $fe = 'x' if (!defined($fe));

    $Binary = $BuildDir . '/' . $TopLevel . '/' . $Topsrcdir . '/' . $BuildObjName . 'dist/bin/apprunner';
	$BinaryDir = $BuildDir . '/' . $TopLevel . '/' . $Topsrcdir . '/' . $BuildObjName . '/dist/bin';
    print LOG $BinName . "\n";
	
	system("cp -r $BinaryDir $BuildDir/smoketest");

   $waittime = 30;

   $pid = fork;

   exec $Binary if !$pid;

   # parent - wait $waittime seconds then check on child
   sleep $waittime;
   $status = waitpid $pid, WNOHANG();
   if ($status != 0) {
     print LOG "$BinName has crashed or quit. Turn the tree orange now.\n";
     return 333;
   }

   print LOG "Success! $BinName is still running. Killing..\n";
   # try to kill 3 times, then try a kill -9
   for ($i=0 ; $i<3 ; $i++) {
     kill 'TERM',$pid,;
     # give it 3 seconds to actually die
     sleep 3;
     $status = waitpid $pid, WNOHANG();
    last if ($status != 0);
   }
   return 0;
} #EndSub-RunSmokeTest
 
# Main function
&InitVars;
&ParseArgs;
&GetSystemInfo;
&SetupEnv;
&SetupPath;
&BuildIt;

1;
