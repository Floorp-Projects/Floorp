#!/usr/bin/perl

require 5.000;

use Sys::Hostname;
use POSIX "sys_wait_h";
use Cwd;

$Version = '$Revision: 1.2 $';

sub InitVars {
    # PLEASE FILL THIS IN WITH YOUR PROPER EMAIL ADDRESS
    $BuildAdministrator = "$ENV{'USER'}\@$ENV{'HOST'}";

    # Default values of cmdline opts
    $BuildDepend = 1;	# Depend or Clobber
    $ReportStatus = 1;	# Send results to server, or not
    $BuildOnce = 0;	# Build once, don't send results to server
    $RunTest = 1;	# Run the smoke test on successful build, or not
    $UseTimeStamp = 0;	# Use the CVS 'pull-by-timestamp' option, or not

    # Relative path to binary
    $BinaryName{'apprunner'} = '/dist/bin/apprunner';

    # Set these to what makes sense for your system
    $cpus = 1;
    $Make = 'gmake'; # Must be GNU make
    $MakeOverrides = '';
    $mail = '/bin/mail';
    $CVS = 'cvs -q';
    $CVSCO = 'checkout -P';

    # Set these proper values for your tinderbox server
    $Tinderbox_server = 'tinderbox-daemon\@cvs-mirror.mozilla.org';

    # You'll need to change these to suit your machine's needs
    $BaseDir = '/builds/tinderbox/SeaMonkey';
    $DisplayServer = 'crucible.mcom.com:0.0';

    # These shouldn't really need to be changed
    $BuildSleep = 10; # Minimum wait period from start of build to start
                      # of next build in minutes
    $BuildTree = 'MozillaTest'; # until you get the script working.
                                # when it works, change to the tree you're
                                # actually building
    $BuildName = '';
    $BuildTag = '';
    $BuildObjName = '';
    $BuildConfigDir = 'mozilla/config';
    $BuildStart = '';
    $TopLevel = '.';
    $Topsrcdir = 'mozilla';
    $ClobberStr = 'realclean';
    $ConfigureEnvArgs = '';
    $ConfigureArgs = ' --cache-file=/dev/null ';
    $ConfigGuess = './build/autoconf/config.guess';
    $Logfile = '${BuildDir}.log';
    $Compiler = 'gcc';
    $ShellOverride = ''; # Only used if the default shell is too stupid
}

sub ConditionalArgs {
  $FE = 'apprunner'; 
  $BuildModule = 'SeaMonkeyAll';
  $CVSCO .= " -r $BuildTag" if ( $BuildTag ne '' );
}

sub SetupEnv {
    umask(0);
    $ENV{"CVSROOT"} = ':pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot';
    $ENV{"LD_LIBRARY_PATH"} = $NSPRDir . '/lib:' . $BaseDir . '/' . $DirName . '/mozilla/' . $ObjDir . 'dist/bin:/usr/lib/png:/usr/local/lib:' . $BaseDir . '/' . $DirName . '/mozilla/dist/bin';
    $ENV{"DISPLAY"} = $DisplayServer;
    $ENV{"MOZCONFIG"} = $BaseDir . '/' . $ConfigFileName;
}

sub SetupPath {
    my($Path, $comptmp);
    $comptmp = '';
    $Path = $ENV{PATH};
    print "Path before: $Path\n";

    if ( $OS eq 'AIX' ) {
	$ENV{'PATH'} = '/builds/local/bin:' . $ENV{'PATH'} . ':/usr/lpp/xlC/bin';
	$ConfigureArgs .= '--x-includes=/usr/include/X11 --x-libraries=/usr/lib --disable-shared';
	$ConfigureEnvArgs = 'CC=xlC_r CXX=xlC_r';
	$Compiler = 'xlC_r';
	$NSPRArgs .= 'NS_USE_NATIVE=1 USE_PTHREADS=1';
    }

    if ( $OS eq 'BSD_OS' ) {
	$ENV{'PATH'} = '/usr/contrib/bin:/bin:/usr/bin:' . $ENV{'PATH'};
	$ConfigureArgs .= '--disable-shared';
	$ConfigureEnvArgs = 'CC=shlicc2 CXX=shlicc2';
	$Compiler = 'shlicc2';
	$mail = '/usr/ucb/mail';
	$MakeOverrides = 'CPP_PROG_LINK=0 CCF=shlicc2'; # because ld dies if it encounters -include
	$NSPRArgs .= 'NS_USE_GCC=1 NS_USE_NATIVE=';
    }

    if ( $OS eq 'FreeBSD' ) {
	$ENV{'PATH'} = '/bin:/usr/bin:' . $ENV{'PATH'};
	if ( $ENV{'HOST'} eq 'angelus.mcom.com' ) {
	    $ConfigureEnvArgs = 'CC=egcc CXX=eg++';
	    $Compiler = 'egcc';
	}
	$mail = '/usr/bin/mail';
    }

    if ( $OS eq 'HP-UX' ) {
	$ENV{'PATH'} = '/opt/ansic/bin:/opt/aCC/bin:/builds/local/bin:' . $ENV{'PATH'};
	$ENV{'LPATH'} = '/usr/lib:' . $ENV{'LD_LIBRARY_PATH'} . ':/builds/local/lib';
	$ENV{'SHLIB_PATH'} = $ENV{'LPATH'};
	$ConfigureArgs .= '--disable-gtktest --x-includes=/usr/include/X11 --x-libraries=/usr/lib';
	$ConfigureEnvArgs = 'CC="cc -Ae" CXX="aCC -ext"';
	$Compiler = 'cc/aCC';
	# Use USE_PTHREADS=1 instead of CLASSIC_NSPR if you've got DCE installed.
	$NSPRArgs .= 'NS_USE_NATIVE=1 CLASSIC_NSPR=1';
    }

    if ( $OS eq 'IRIX' ) {
	$ENV{'PATH'} = '/opt/bin:' . $ENV{'PATH'};
	$ENV{'LD_LIBRARY_PATH'} .= ':/opt/lib';
	$ENV{'LD_LIBRARYN32_PATH'} = $ENV{'LD_LIBRARY_PATH'};
	$ConfigureEnvArgs = 'CC=cc CXX=CC CFLAGS="-n32 -O" CXXFLAGS="-n32 -O"';
	$Compiler = 'cc/CC';
	$NSPRArgs .= 'NS_USE_NATIVE=1 USE_PTHREADS=1';
    }

    if ( $OS eq 'NetBSD' ) {
	$ENV{'PATH'} = '/bin:/usr/bin:' . $ENV{'PATH'};
	$ENV{'LD_LIBRARY_PATH'} .= ':/usr/X11R6/lib';
	$ConfigureEnvArgs = 'CC=egcc CXX=eg++';
	$Compiler = 'egcc';
	$mail = '/usr/bin/mail';
    }

    if ( $OS eq 'OSF1' ) {
	$ENV{'PATH'} = '/usr/gnu/bin:' . $ENV{'PATH'};
	$ENV{'LD_LIBRARY_PATH'} .= ':/usr/gnu/lib';
	$ConfigureEnvArgs = 'CC="cc -readonly_strings" CXX="cxx"';
	$Compiler = 'cc/cxx';
	$MakeOverrides = 'SHELL=/usr/bin/ksh';
	$NSPRArgs .= 'NS_USE_NATIVE=1 USE_PTHREADS=1';
	$ShellOverride = '/usr/bin/ksh';
    }

    if ( $OS eq 'QNX' ) {
	$ENV{'PATH'} = '/usr/local/bin:' . $ENV{'PATH'};
	$ENV{'LD_LIBRARY_PATH'} .= ':/usr/X11/lib';
	$ConfigureArgs .= '--disable-shared --x-includes=/usr/X11/include --x-libraries=/usr/X11/lib';
	$ConfigureEnvArgs = 'CC="cc -DQNX" CXX="cc -DQNX"';
	$Compiler = 'cc';
	$mail = '/usr/bin/sendmail';
    }

    if ( $OS eq 'SunOS' ) {
	if ( $OSVerMajor eq '4' ) {
	    $ENV{'PATH'} = '/usr/gnu/bin:/usr/local/sun4/bin:/usr/bin:' . $ENV{'PATH'};
	    $ENV{'LD_LIBRARY_PATH'} = '/home/motif/usr/lib:' . $ENV{'LD_LIBRARY_PATH'};
	    $ConfigureArgs .= '--x-includes=/home/motif/usr/include/X11 --x-libraries=/home/motif/usr/lib';
	    $ConfigureEnvArgs = 'CC="egcc -DSUNOS4" CXX="eg++ -DSUNOS4"';
	    $Compiler = 'egcc';
	} else {
	    $ENV{'PATH'} = '/usr/ccs/bin:' . $ENV{'PATH'};
	}
	if ( $CPU eq 'i86pc' ) {
	    $ENV{'PATH'} = '/opt/gnu/bin:' . $ENV{'PATH'};
	    $ENV{'LD_LIBRARY_PATH'} .= ':/opt/gnu/lib';
	    $ConfigureEnvArgs = 'CC=egcc CXX=eg++';
	    $Compiler = 'egcc';
	    # This may just be an NSPR bug, but if USE_PTHREADS is defined, then
	    # _PR_HAVE_ATOMIC_CAS gets defined (erroneously?) and libnspr21 doesn't work.
	    $NSPRArgs .= 'CLASSIC_NSPR=1 NS_USE_GCC=1 NS_USE_NATIVE=';
	} else {
	    # This is utterly lame....
	    if ( $ENV{'HOST'} eq 'fugu' ) {
		$ENV{'PATH'} = '/tools/ns/workshop/bin:/usrlocal/bin:' . $ENV{'PATH'};
		$ENV{'LD_LIBRARY_PATH'} = '/tools/ns/workshop/lib:/usrlocal/lib:' . $ENV{'LD_LIBRARY_PATH'};
		$ConfigureEnvArgs = 'CC=cc CXX=CC';
		$comptmp = `cc -V 2>&1 | head -1`;
		chop($comptmp);
		$Compiler = "cc/CC \($comptmp\)";
		$NSPRArgs .= 'NS_USE_NATIVE=1';
	    } else {
		$NSPRArgs .= 'NS_USE_GCC=1 NS_USE_NATIVE=';
	    }
	    if ( $OSVerMajor eq '5' ) {
		$NSPRArgs .= ' USE_PTHREADS=1';
	    }
	}
    }

    $Path = $ENV{PATH};
    print "Path After: $Path\n";
}

##########################################################################
# NO USER CONFIGURABLE PIECES BEYOND THIS POINT                          #
##########################################################################

sub GetSystemInfo {
    $OS = `uname -s`;
    $OSVer = `uname -r`;
    $CPU = `uname -m`;
    $BuildName = ""; 

    my($host) = "";
    my($myhost) = hostname;

    chop($OS, $OSVer, $CPU);
    chomp($myhost);

    ($host, $junk) = split(/\./, $myhost);

    if ( $OS eq 'AIX' ) {
	my($osAltVer) = `uname -v`;
	chop($osAltVer);
	$OSVer = $osAltVer . "." . $OSVer;
    }

    if ( $OS eq 'BSD/OS' ) {
	$OS = 'BSD_OS';
    }

    if ( $OS eq 'IRIX64' ) {
	$OS = 'IRIX';
    }

    if ( $OS eq 'QNX' ) {
	$OSVer = `uname -v`;
	chop($OSVer);
	$OSVer =~ s/^([0-9])([0-9]*)$/$1.$2/;
    }

    if ( $OS eq 'SCO_SV' ) {
	$OS = 'SCOOS';
	$OSVer = '5.0';
    }

    if ( "$host" ne "" ) {
	$BuildName = $host . ' ' . $OS . ' ' . $OSVer . ' ' . ($BuildDepend?'Depend':'Clobber');
    }
    $DirName = $OS . '_' . $OSVer . '_' . ($BuildDepend?'depend':'clobber');

    #
    # Make the build names reflect architecture/OS
    #

    if ( $OS eq 'AIX' ) {
      # Assumes 4.2.1 for now.
    }

    if ( $OS eq 'BSD_OS' ) {
      $BuildName = $host . ' BSD/OS ' . $OSVer . ' ' . ($BuildDepend?'Depend':'Clobber');
    }

    if ( $OS eq 'FreeBSD' ) {
      $BuildName = $host . ' ' . $OS . '/' . $CPU . ' ' . $OSVer . ' ' . ($BuildDepend?'Depend':'Clobber');
    }

    if ( $OS eq 'HP-UX' ) {
    }

    if ( $OS eq 'IRIX' ) {
    }
    
    if ( $OS eq 'Linux' ) {
      if ( $CPU eq 'alpha' || $CPU eq 'sparc' ) {
	  $BuildName = $host . ' ' . $OS . '/' . $CPU . ' ' . $OSVer . ' ' . ($BuildDepend?'Depend':'Clobber');
	  } elsif ( $CPU eq 'armv4l' || $CPU eq 'sa110' ) {
	  $BuildName = $host . ' ' . $OS . '/arm ' . $OSVer . ' ' . ($BuildDepend?'Depend':'Clobber');
	  } elsif ( $CPU eq 'ppc' ) {
	  $BuildName = $host . ' ' . $OS . '/' . $CPU . ' ' . $OSVer . ' ' . ($BuildDepend?'Depend':'Clobber');
	  } else {
	  $BuildName = $host . ' ' . $OS . '/i386 ' . $OSVer . ' ' . ($BuildDepend?'Depend':'Clobber');
	    # What's the right way to test for this?
	    if ( $host eq 'truth' ) {
	      $ObjDir .= 'libc1';
	      }
	  }
      }
    
    if ( $OS eq 'NetBSD' ) {
      $BuildName = $host . ' ' . $OS . '/' . $CPU . ' ' . $OSVer . ' ' . ($BuildDepend?'Depend':'Clobber');
      }
    
    if ( $OS eq 'OSF1' ) {
      # Assumes 4.0D for now.
    }

    if ( $OS eq 'QNX' ) {
    }
    
    if ( $OS eq 'SunOS' ) {
      if ( $CPU eq 'i86pc' ) {
	  $BuildName = $host . ' ' . $OS . '/i386 ' . $OSVer . ' ' . ($BuildDepend?'Depend':'Clobber');
	  } else {
	  $OSVerMajor = substr($OSVer, 0, 1);
	    if ( $OSVerMajor eq '4' ) {
	    } else {
	      $BuildName = $host . ' ' . $OS . '/sparc ' . $OSVer . ' ' . ($BuildDepend?'Depend':'Clobber');
	      }
	  }
      }
    
    $logfile = "${DirName}.log";
    
}

sub BuildIt {
    my($fe, @felist, $EarlyExit, $LastTime, $SaveCVSCO, $StartTimeStr, $comptmp, $jflag);
    $comptmp = '';
    $jflag = '';

    mkdir("$DirName", 0777);
    chdir("$DirName") || die "Couldn't enter $DirName";

    $StartDir = getcwd();
    $LastTime = 0;
    $EarlyExit = 0;
    $SaveCVSCO = $CVSCO;

    # With only one it makes no sense to use this.
    $jflag = "-j $cpus" if ( $cpus > 1 );

    print "Starting dir is : $StartDir\n";

    while ( ! $EarlyExit ) {
	chdir("$StartDir");
	if ( time - $LastTime < (60 * $BuildSleep) ) {
	    $SleepTime = (60 * $BuildSleep) - (time - $LastTime);
	    print "\n\nSleeping $SleepTime seconds ...\n";
	    sleep($SleepTime);
	}

	$LastTime = time;

	if ( $UseTimeStamp ) {
	    $CVSCO = $SaveCVSCO;
	} else {
	    $CVSCO = $SaveCVSCO . ' -A';
	}
	$StartTime = time;
	$StartTimeStr = &CVSTime($StartTime);

	&StartBuild if ( $ReportStatus );
 	$CurrentDir = getcwd();
	if ( $CurrentDir ne $StartDir ) {
	    print "startdir: $StartDir, curdir $CurrentDir\n";
	    die "curdir != startdir";
	}

	$BuildDir = $CurrentDir;

	unlink( "$logfile" );

	print "opening $logfile\n";
	open( LOG, ">$logfile" ) || print "can't open $?\n";
	print LOG "current dir is -- " . $ENV{'HOST'} . ":$CurrentDir\n";
	print LOG "Build Administrator is $BuildAdministrator\n";
	&PrintEnv;
	if ( $Compiler ne '' ) {
	    print LOG "===============================\n";
	    if ( $Compiler eq 'gcc' || $Compiler eq 'egcc' ) {
		$comptmp = `$Compiler --version`;
		chop($comptmp);
		print LOG "Compiler is -- $Compiler \($comptmp\)\n";
	    } else {
		print LOG "Compiler is -- $Compiler\n";
	    }
	    print LOG "===============================\n";
	}

	$BuildStatus = 0;

	mkdir($TopLevel, 0777);
	chdir($TopLevel) || die "chdir($TopLevel): $!\n";

	print "$CVS $CVSCO mozilla/client.mk\n";
	print LOG "$CVS $CVSCO mozilla/client.mk\n";
	open (PULL, "$CVS $CVSCO mozilla/client.mk 2>&1 |") || die "open: $!\n";
	while (<PULL>) {
	  print $_;
	  print LOG $_;
	}
	close(PULL);

	chdir($Topsrcdir) || die "chdir($Topsrcdir): $!\n";

	# Let us delete the binaries before rebuilding
	@felist = split(/,/, $FE);
	
	foreach $fe ( @felist ) {	    
	    if ( &BinaryExists($fe) ) {
		print LOG "deleting existing binary\n";
		&DeleteBinary($fe);
	    }
	}
	system("rm -rf dist"); # it's the only way to be sure

	# If we are building depend, don't clobber.
	if ( $BuildDepend ) {
	  print LOG "$Make -f client.mk checkout depend build 2>&1 |\n";
	  open (MAKEDEPEND, "$Make -f client.mk checkout depend build 2>&1 |");
	  while ( <MAKEDEPEND> ) {
	    print $_;
	    print LOG $_;
	  }
	  close (MAKEDEPEND);
	} else {
	  # Building clobber
	  print LOG "$Make -f client.mk checkout clean build 2>&1 |\n";
	  open(MAKECLOBBER, "$Make -f client.mk checkout clean build 2>&1 |");
	  while ( <MAKECLOBBER> ) {
	    print $_;
	    print LOG $_;
	  }
	  close( MAKECLOBBER );
	}
	
	foreach $fe (@felist) {
	    if ( &BinaryExists($fe) ) {
		if ( $RunTest ) {
		    print LOG "export binary exists, build successful.  Testing...\n";
		    $BuildStatus = &RunSmokeTest($fe);
		} else {
		    print LOG "export binary exists, build successful.  Skipping test.\n";
		    $BuildStatus = 0;
		}
	    } else {
		print LOG "export binary missing, build FAILED\n";
		$BuildStatus = 666;
	    }

	    if ( $BuildStatus == 0 ) {
		$BuildStatusStr = 'success';
	    }
	    elsif ( $BuildStatus == 333 ) {
		$BuildStatusStr = 'testfailed';
	    } else {
		$BuildStatusStr = 'busted';
	    }
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

		last if ( $Output eq undef );

		$Output =~ s/^\.$//g;
		$Output =~ s/\n//g;
		print OUTLOG "$Output\n";
		$q++;
	    }
	}

	close(LOG);
	close(OUTLOG);

	system( "$mail $Tinderbox_server < ${logfile}.last" ) if ( $ReportStatus );
	unlink("$logfile");

	# If this is a test run, set early_exit to 0. 
	# This mean one loop of execution
	$EarlyExit++ if ( $BuildOnce );
    }
}

sub CVSTime {
    my($StartTimeArg) = @_;
    my($RetTime, $sec, $minute, $hour, $mday, $mon, $year);

    ($sec,$minute,$hour,$mday,$mon,$year) = localtime($StartTimeArg);
    $mon++; # month is 0 based.

    if ( $UseTimeStamp ) {
	$BuildStart = `date '+%D %H:%M'`;
	chop($BuildStart);
	$CVSCO .= " -D '$BuildStart'";
    }

    sprintf("%02d/%02d/%02d %02d:%02d:00", $mon,$mday,$year,$hour,$minute );
}

sub StartBuild {
    my($fe, @felist);

    @felist = split(/,/, $FE);

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
  my($BinName);
  $fe = 'x' if ( !defined($fe) ); 
  
  $BinName = $BuildDir . '/' . $TopLevel . '/' . $Topsrcdir . $BinaryName{"$fe"};
  
  if ( (-e $BinName) && (-x $BinName) && (-s $BinName) ) {
    print LOG $BinName . " exists, is nonzero, and executable. \n";  
    1;
  }
  else {
    print LOG $BinName . " doesn't exist, is zero-size, or not executable. \n";
    0;
  } 
}

sub DeleteBinary {
    my($fe) = @_;
    my($BinName);
    $fe = 'apprunner' if ( !defined($fe) ); 
    
    $BinName = $BuildDir . '/' . $TopLevel . '/' . $Topsrcdir . $BinaryName{"$fe"};
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
	if ( $ARGV[$i] eq '--clobber' ) {
	    $BuildDepend = 0;
	    $manArg++;
	}
	elsif ( $ARGV[$i] eq '--compress' ) {
	    $CVS = 'cvs -q -z3';
	}
	elsif ( $ARGV[$i] eq '--depend' ) {
	    $BuildDepend = 1;
 	    $manArg++;
	}
	elsif ( $ARGV[$i] eq '--help' || $ARGV[$i] eq '-h' ) {
	    &PrintUsage;
	}
	elsif ( $ARGV[$i] eq '--noreport' ) {
	    $ReportStatus = 0;
	}
	elsif ( $ARGV[$i] eq '--notest' ) {
	    $RunTest = 0;
	}
	elsif ( $ARGV[$i] eq '--once' ) {
	    $BuildOnce = 1;
	}
	elsif ( $ARGV[$i] eq '-tag' ) {
	    $i++;
	    $BuildTag = $ARGV[$i];
	    if ( $BuildTag eq '' || $BuildTag eq '-t' ) {
		&PrintUsage;
	    }
	}
	elsif ( $ARGV[$i] eq '--timestamp' ) {
	    $UseTimeStamp = 1;
	}
	elsif ( $ARGV[$i] eq '-t' ) {
	    $i++;
	    $BuildTree = $ARGV[$i];
	    if ( $BuildTree eq '' ) {
		&PrintUsage;
	    }
	}
	elsif ( $ARGV[$i] eq '--configfile' ) {
	  $i++;
	  $ConfigFileName = $ARGV[$i];
	  if ( $ConfigFileName eq '' ) {
	    &PrintUsage;
	  }
	}
	elsif ( $ARGV[$i] eq '--version' || $ARGV[$i] eq '-v' ) {
	    die "$0: version" . substr($Version,9,6) . "\n";
	} else {
	    &PrintUsage;
	}
	$i++;
    }

    if ( $BuildTree =~ /^\s+$/i ) {
	&PrintUsage;
    }

    if ( $BuildDepend eq undef ) {
	&PrintUsage;
    }

    &PrintUsage if ( $manArg == 0 );
}

sub PrintUsage {
    die "usage: $0 --depend | --clobber [ --once --manual --classic --compress --noreport --notest --timestamp -tag TREETAG -t TREENAME --configfile CONFIGFILENAME --version ]\n";
}

sub PrintEnv {
    my($key);
    foreach $key (sort(keys %ENV)) {
	print LOG $key, '=', $ENV{$key}, "\n";
	print $key, '=', $ENV{$key}, "\n";
    }
}

sub RunSmokeTest {
    my($fe) = @_;
    my($Binary);
    my($status) = 0;
    my($waittime) = 45;
    my($pid) = fork;
    $fe = 'x' if ( !defined($fe) );

    $ENV{"LD_LIBRARY_PATH"} = $BuildDir . '/' . $TopLevel . '/' . $Topsrcdir . '/dist/bin';
    $ENV{"MOZILLA_FIVE_HOME"} = $ENV{"LD_LIBRARY_PATH"};
    $Binary = $BuildDir . '/' . $TopLevel . '/' . $Topsrcdir . $BinaryName{"$fe"};
    
    print LOG $Binary . "\n";
    $BinaryDir = $BuildDir . '/' . $TopLevel . '/' . $Topsrcdir . '/dist/bin';
    $Binary = $BuildDir . '/' . $TopLevel . '/' . $Topsrcdir . '/dist/bin/apprunner';
    $BinaryLog = $BuildDir . '/runlog';

    if ( !$pid ) { # child

      chdir("$BinaryDir");
      unlink("$BinaryLog");
      open(STDOUT,">$BinaryLog");
      select STDOUT; $| = 1; # make STDOUT unbuffered
      open(STDERR,">&STDOUT");
      select STDERR; $| = 1; # make STDERR unbuffered
      exec("$Binary");
      close(STDOUT);
      close(STDERR);
      die "Couldn't exec()";

    }
    
    # parent - wait $waittime seconds then check on child
    sleep $waittime;
    $status = waitpid($pid, WNOHANG());
    if ( $status != 0 ) {
	print LOG "$Binary has crashed or quit.  Turn the tree orange now.\n";
	print LOG "----------- Output from apprunner --------------- \n";
	open(READRUNLOG, "$BinaryLog");
	while (<READRUNLOG>) {
	  print $_;
	  print LOG $_;
	}
	close(READRUNLOG);
	print LOG "--------------- End of Output -------------------- \n";
	return 333;
    }

    print LOG "Success! $Binary is still running.  Killing it, and its children,and its dog...\n";
    # try to kill 3 times, then try a kill -9
    for ($i=0 ; $i<3 ; $i++) {
	kill('TERM',$pid);
      	# give it 3 seconds to actually die
	sleep 3;
	$status = waitpid($pid, WNOHANG());
	last if ( $status != 0 );
    }
    print LOG "----------- Output from apprunner --------------- \n";
    open(READRUNLOG, "$BinaryLog");
    while (<READRUNLOG>) {
      print $_;
      print LOG $_;
    }
    close(READRUNLOG);
    print LOG "--------------- End of Output -------------------- \n";
    return 0;
}

# Main function
&InitVars;
&ParseArgs;
&ConditionalArgs;
&GetSystemInfo;
&SetupEnv;
&SetupPath;
&BuildIt;

1;
