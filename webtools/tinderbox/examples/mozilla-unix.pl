#!/tools/ns/bin/perl5

require 5.000;
require "buildit.config";

use Sys::Hostname;
use Cwd;

&InitVars;
&ParseArgs;
&GetSystemInfo;
&SetupEnv;
&SetupPath;
&BuildIt;

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

$DirName = $OS . '_' . $OSVer . '_' . ($BuildDepend?'depend':'clobber');
$BuildName = $OS . ' ' . $OSVer . ' ' . ($BuildDepend?'Depend':'Clobber');

$RealOS = $OS;
$RealOSVer = $OSVer;

if ( $OS eq 'HP-UX' ) {
    $RealOSVer = substr($OSVer,0,4);
}

if ( $OS eq 'Linux' ) {
    $RealOSVer = substr($OSVer,0,3);
}

if ( $ENV{"BUILD_OPT"} eq 1 ) {
    $BuildObjName = $RealOS . $RealOSVer . '_OPT.OBJ';
}
else {
    $BuildObjName = $RealOS . $RealOSVer . '_DBG.OBJ';
}

$logfile = "${DirName}.log";

if ( !$BuildDepend ) {
    $ClobberStr = "realclean";
}

} #EndSub-GetSystemInfo

sub BuildIt {
mkdir("$DirName", 0777);
chdir("$DirName") || die "Couldn't enter $DirName";

$StartDir = getcwd();
$LastTime = 0;

print "Starting dir is : $StartDir\n";

$EarlyExit = 1;

while ( $EarlyExit ) {
    chdir("$StartDir");
    if ( time - $LastTime < (60 * $BuildSleep) ) {
	$SleepTime = (60 * $BuildSleep) - (time - $LastTime);
	print "\n\nSleeping $SleepTime seconds ...\n";
	sleep($SleepTime);
    }

    $LastTime = time;

    $StartTime = time - 60 * 10;
    $StartTimeStr = &CVSTime($StartTime);

    &StartBuild;

    $CurrentDir = getcwd();
    if ( $CurrentDir ne $StartDir ) {
	print "startdir: $StartDir, curdir $CurrentDir\n";
	die "curdir != startdir";
    }

    $BuildDir = $CurrentDir;

    unlink( "${DirName}.log" );

    print "opening ${DirName}.log\n";
    open( LOG, ">${DirName}.log" ) || print "can't open $?\n";
    print LOG "current dir is -- $hostname:$CurrentDir\n";
    &PrintEnv;

    $BuildStatus = 0;
    if (!($BuildTest)) {
# pull tree
	print LOG "cvs co MozillaSourceMotif";

	if ( $BuildTag ne '' ) {
	    open( PULLMAKE, "cvs co -r $BuildTag MozillaSourceMotif 2>&1 |");
	}
	else {
	    open( PULLMAKE, "cvs co MozillaSourceMotif 2>&1 |");
	}

	while ( <PULLMAKE> ) {
	    print $_;
	    print LOG $_;
	}
	close( PULLMAKE );
	
#	print LOG "gmake export libs install";
#
#	if ( $BuildTag ne '' ) {
#	    open( PULL, "cvs co -r $BuildTag client.mk 2>&1 |");
#	}
#	else {
#	    open( PULL, "gmake -f ns/client.mk setup 2>&1 |");
#	}
#
#	while ( <PULL> ) {
#	    print $_;
#	    print LOG $_;
#	}
#	close( PULL );
    }
    chdir($BuildConfigDir) || die "couldn't chdir to $BuildConfigDir";
    print LOG "gmake show_objname 2>&1 |\n";
    open ( GETOBJ, "gmake show_objname 2>&1 |\n");
    while ( <GETOBJ> ) {
	print "$_";
	print LOG "$_";
	$BuildObjName = $_;
	chop($BuildObjName);
    }
    close ( <GETOBJ> ); 

    print "objname is " . $BuildObjName . " \n";

# if we are building depend, rebuild dependencies

    if ($BuildDepend) {
	print LOG "gmake depend 2>&1 |\n";
	open ( MAKEDEPEND, "gmake depend 2>&1 |\n");
	while ( <MAKEDEPEND> ) {
	    print $_;
	    print LOG $_;
	}
	close ( <MAKEDEPEND> );
    }

    chdir("../..");

    chdir($BuildStartDir) || die "couldn't chdir to $BuildStartDir";
    print LOG "gmake -e export libs install 2>&1 |\n";

# preflight build by deleting any existing binary

    if (&BinaryExists) {
	print LOG "deleting existing binary\n";
	&DeleteBinary;
    }

# now build damn you

    open( BUILD, "gmake -e $ClobberStr export libs install 2>&1 |");

    while ( <BUILD> ) {
	print $_;
	print LOG $_;
    }
    close( BUILD );

# if we have a binary after building, build worked

    if (&BinaryExists) {
	print LOG "export binary exists, build SUCCESSFUL!\n";
	$BuildStatus = 0;
	if ($BuildUnixClasses) {
	    &PackageClasses;
	}
    }
    else {
	print LOG "export binary missing, build FAILED\n";
	$BuildStatus = 666;
    }

    print LOG "\nBuild Status = $BuildStatus\n";

    $BuildStatusStr = ( $BuildStatus ? 'busted' : 'success' );

    print LOG "tinderbox: tree: $BuildTree\n";
    print LOG "tinderbox: builddate: $StartTime\n";
    print LOG "tinderbox: status: $BuildStatusStr\n";
    print LOG "tinderbox: build: $BuildName\n";
    print LOG "tinderbox: errorparser: unix\n";
    print LOG "tinderbox: buildfamily: unix\n";

    close( LOG );
    chdir("$StartDir");

    unlink( "${DirName}.log.last" );

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

open(LOG, "${DirName}.log") || die "Couldn't open logfile: $!\n";
open(OUTLOG, ">${DirName}.log.last") || die "Couldn't open logfile: $!\n";

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

    system( "/bin/mail external-tinderbox-incoming\@mozilla.org < ${DirName}.log.last" );

# if this is a test run, set early_exit to 0. This mean one loop of execution
    if ($BuildTest) {
	$EarlyExit = 0;
    }
}

} #EndSub-BuildIt

sub CVSTime {
    local($StartTimeArg) = @_;
#    local($RetTime, $StartTimeArg, $sec, $minute, $hour, $mday, $mon, $year);

    ($sec,$minute,$hour,$mday,$mon,$year) = localtime($StartTimeArg);
    $mon++; # month is 0 based.

    sprintf("%02d/%02d/%02d %02d:%02d:00", $mon,$mday,$year,$hour,$minute );
}

sub StartBuild {
    open( LOG, "|/bin/mail external-tinderbox-incoming\@mozilla.org" );
    print LOG "\n";
    print LOG "tinderbox: tree: $BuildTree\n";
    print LOG "tinderbox: builddate: $StartTime\n";
    print LOG "tinderbox: status: building\n";
    print LOG "tinderbox: build: $BuildName\n";
    print LOG "tinderbox: errorparser: unix\n";
    print LOG "tinderbox: buildfamily: unix\n";
    print LOG "\n";
    close( LOG );
}

# check for the existence of the binary
sub BinaryExists {
    my($Binname);
    $BinName = $BuildDir . '/' . $TopLevel . 'cmd/xfe/' . $BuildObjName . $BinaryName;
    print LOG $BinName . "\n"; 
    if ((-e $BinName) && (-x $BinName) && (-s $BinName)) {
	1;
    }
    else {
	0;
    }
}

sub DeleteBinary {
    my($BinName);
    $BinName = $BuildDir . '/' . $TopLevel . 'cmd/xfe/' . $BuildObjName . $BinaryName;
    print LOG "unlinking $BinName\n";
    unlink ($BinName) || print LOG "unlinking $BinName failed\n";
}

sub PackageClasses {
    my($BinName, $TarName);
    $tarname = $BuildDir . '/unix_classes.tar.gz';
    unlink ($TarName) || print LOG "unlinking $TarName failed\n";
    open( BUILD, "gmake -e class_tar 2>&1 |");
    $BinName = $BuildDir . $TopLevel . '/dist/unix_classes.tar.gz';
    link ($BinName,$TarName) || print LOG "linking $BinName to $TarName failed\n";
}

sub ParseArgs {
    my($i);

    if( @ARGV == 0 ) {
	&PrintUsage;
    }
    $i = 0;
    while( $i < @ARGV ) {
	if ($ARGV[$i] eq '--depend') {
	    $BuildDepend = 1;
	}
	elsif ($ARGV[$i] eq '--clobber') {
	    $BuildDepend = 0;
	}
	elsif ( $ARGV[$i] eq '--test' ) {
	    $BuildTest = 1;
	}
	elsif ($ARGV[$i] eq '--saveclasses') {
	    $BuildUnixClasses = 1;
	}
	elsif ($ARGV[$i] eq '--testbuild') {
		$i++;
		if ($ARGV[$i] =~ /^[\w\d\.]+\:\d+\.\d+$/) {
		$RunAcceptanceTests = 1;
		$AcceptanceDisplay = "$ARGV[$i]";
		} else {
		&PrintUsage;
		}
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

} #EndSub-ParseArgs

sub PrintUsage {
    die "usage: buildit.pl [--depend | --clobber] [--test --saveclasses [--testbuild DISPLAY_NAME] ] -tag TREETAG -t TREENAME\n";
}

sub PrintEnv {
    foreach $key (keys %ENV) {
	print LOG "$key = $ENV{$key}\n";
	print "$key = $ENV{$key}\n";
    }
} #EndSub-PrintEnv

