#!/tools/ns/bin/perl5

use Sys::Hostname;

#initialize variables
$build_depend=1;      #depend or clobber
$build_tree = '';
$build_tag = '';
$build_name = '';
$build_continue = 0;
$build_dir = '';
$build_objname = '';
$build_lite = 0;
$build_sleep = 10;
$early_exit = 1;

&parse_args;

$os = `uname -s`;
chop($os);

$osver = `uname -r`;
chop($osver);

$dirname = $os . '_' . $osver . '_' . ($build_depend?'depend':'clobber');
$build_name = $os . ' ' . $osver . ' ' . ($build_depend?'Depend':'Clobber');

&setup_env;

$logfile = "${dirname}.log";

if( !$build_depend ){
    $clobber_str = 'clobber_all';
}

$ENV{"CVSROOT"} = ':pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot';

mkdir("$dirname", 0777);
chdir("$dirname") || die "couldn't cd to $dirname";

$start_dir = `pwd`;
$last_time = 0;
chop($start_dir);

print "starting dir is :$start_dir\n";

while( $early_exit ){
    chdir("$start_dir");
    if( time - $last_time < (60 * $build_sleep) ) {
        $sleep_time = (60 * $build_sleep) - (time - $last_time);
        print "\n\nSleeping $sleep_time seconds ...\n";
        sleep( $sleep_time );
    }
    $last_time = time;

    $start_time = time-60*10;
    $start_time_str = &cvs_time( $start_time );

    &start_build;

    $cur_dir = `pwd`;
    chop($cur_dir);
    if( $cur_dir ne $start_dir ){
        print "startdir: $start_dir, curdir $cur_dir\n";
        die "curdir != startdir";
    }

    unlink( "${dirname}.log" );

    print "opening ${dirname}.log\n";
    open( LOG, ">${dirname}.log" ) || print "can't open $?\n";
    $hostname = hostname();
    print LOG "current dir is -- $hostname:$cur_dir\n";
    &print_env;
    $build_dir = $cur_dir;

    $build_status = 0;
if (!$build_test) {
	open( PULL, "cvs co MozillaSourceUnix 2>&1 |");

	while( <PULL> ){
		print $_;
		print LOG $_;
	}
	close( PULL );
}
    chdir("mozilla/config") || die "couldn't chdir to 'mozilla/config'";
    print LOG "gmake show_objname 2>&1 |\n";
    open ( GETOBJ, "gmake show_objname 2>&1 |\n");
    while ( <GETOBJ> ) {
        print $_;
	print LOG $_;
	$build_objname = $_;
	chop($build_objname);
    }
    close ( <GETOBJ> ); 

    print "objname is " . $build_objname . " \n";

# if we are building depend, rebuild dependencies

    if ($build_depend) {
        print LOG "gmake depend 2>&1 |\n";
        open ( MAKEDEPEND, "gmake depend 2>&1 |\n");
        while ( <MAKEDEPEND> ) {
             print $_;
             print LOG $_;
        }
        close ( <MAKEDEPEND> );
    }

    chdir("..") || die "couldn't chdir to 'ns'";
    print LOG "gmake export libs install 2>&1 |\n";

# preflight build by deleting any existing binary

	if (&binaryexists) {
		print LOG "deleting netscape-export\n";
		&deletebinary;
       	}

# now build damn you

        open( BUILD, "gmake $clobber_str export libs install 2>&1 |");

	while( <BUILD> ){
		print $_;
		print LOG $_;
	}
	close( BUILD );

# if we have a netscape-export after building, build worked
    if (&binaryexists) {
       print LOG "netscape-export exists, build SUCCESSFUL!\n";
       $build_status = 0;
    }
    else {
       print LOG "netscape-export missing, build FAILED\n";
       $build_status = 666;
    }

    print LOG "\nBuild Status = $build_status\n";

    $build_status_str = ( $build_status ? 'busted' : 'success' );

    print LOG "tinderbox: tree: $build_tree\n";
    print LOG "tinderbox: builddate: $start_time\n";
    print LOG "tinderbox: status: $build_status_str\n";
    print LOG "tinderbox: build: $build_name\n";
    print LOG "tinderbox: errorparser: unix\n";
    print LOG "tinderbox: buildfamily: unix\n";

    close( LOG );
    chdir("$start_dir");

    unlink( "${dirname}.log.last" );

# this fun line added on 2/5/98. do not remove. Translated to english,
# that's "take any line longer than 1000 characters, and split it into less
# than 1000 char lines.  If any of the resulting lines is
# a dot on a line by itself, replace that with a blank line."  
# This is to prevent cases where a <cr>.<cr> occurs in the log file.  Sendmail
# interprets that as the end of the mail, and truncates the log before
# it gets to Tinderbox.  (terry weismann, chris yeh)
    system("fold -1000 < ${dirname}.log | sed -e 's/^\.\$//' > ${dirname}.log.last");
    system( "/bin/mail tinderbox-daemon\@warp < ${dirname}.log.last" );


# if this is a test run, set early_exit to 0. This mean one loop of execution
    if ($build_test) {
	$early_exit = 0;
    }
}

sub cvs_time {
    local( $ret_time );

    ($sec,$minute,$hour,$mday,$mon,$year) = localtime( $_[0] );
    $mon++; # month is 0 based.

    sprintf("%02d/%02d/%02d %02d:%02d:00",
        $mon,$mday,$year,$hour,$minute );
}

sub start_build {
    #open( LOG, ">>logfile" );
    open( LOG, "|/bin/mail tinderbox-daemon\@warp" );
    print LOG "\n";
    print LOG "tinderbox: tree: $build_tree\n";
    print LOG "tinderbox: builddate: $start_time\n";
    print LOG "tinderbox: status: building\n";
    print LOG "tinderbox: build: $build_name\n";
    print LOG "tinderbox: errorparser: unix\n";
    print LOG "tinderbox: buildfamily: unix\n";
    print LOG "\n";
    close( LOG );
}

sub parse_args {
    local($i);

    if( @ARGV == 0 ){
        &usage;
    }
    $i = 0;
    while( $i < @ARGV ){
	if ($ARGV[$i] eq '--lite') {
		$build_lite = 1;
	}
        elsif( $ARGV[$i] eq '--depend' ){
            $build_depend = 1;
        }
        elsif ( $ARGV[$i] eq '--clobber' ){
            $build_depend = 0;
        }
        elsif ( $ARGV[$i] eq '--continue' ){
            $build_continue = 1;
        }
        elsif ( $ARGV[$i] eq '--test' ){
            $build_test = 1;
        }
        elsif ( $ARGV[$i] eq '-tag' ){
            $i++;
            $build_tag = $ARGV[$i];
            if( $build_tag eq '' || $build_tag eq '-t'){
                &usage;
            }
        }
        elsif ( $ARGV[$i] eq '-t' ){
            $i++;
            $build_tree = $ARGV[$i];
            if( $build_tree eq '' ){
                &usage;
            }
        }
        $i++;
    }
    if( $build_tree eq '' ){
        &usage;
    }
}

sub usage {
    die "usage: buildit.pl [--depend | --clobber] --continue --test -tag TREETAG -t TREENAME\n";
}

sub setup_env {
    local($p);
    $p = $ENV{PATH};
    print "Path before: $p\n";
    umask(0);
    $ENV{'MOZILLA_CLIENT'} = 1;
    $ENV{'NETSCAPE_HIERARCHY'} = 1;
    $ENV{'BUILD_OFFICIAL'} = 1;
    $ENV{'NSPR20'} = 1;
    $ENV{'AWT_11'} = 1;
    $ENV{'NO_SECURITY'} = 1;
    if ($build_lite) {
#	$ENV{'MOZ_LITE'} = 1;
	$ENV{'MOZ_MEDIUM'} = 1;
    }
    $ENV{'PATH'} = '/usr/local/bin:/tools/ns/bin:/usr/local/bin:/tools/contrib/bin:/usr/local/bin:/usr/sbin:/usr/bsd:/sbin:/usr/bin:/bin:/usr/bin/X11:/usr/etc:/usr/hosts:/usr/ucb:';

    if( $os eq 'SunOS' ){
        $ENV{'PATH'} = '/usr/ccs/bin:/tools/ns/soft/gcc-2.6.3/run/default/sparc_sun_solaris2.4/bin:'
                . $ENV{'PATH'};
        $ENV{'NO_MDUPDATE'} = 1;
    }

    if( $os eq 'AIX' ){
        $ENV{'PATH'} =  $ENV{'PATH'} . '/usr/lpp/xlC/bin:/usr/local-aix/bin:'
                ;
        #$ENV{'NO_MDUPDATE'} = 1;
    }

    $p = $ENV{PATH};
    print "Path After: $p\n";
}

sub print_env {
foreach $key (keys %ENV) {
	print LOG "$key = $ENV{$key}\n";
	print "$key = $ENV{$key}\n";
}
}

# check for the existance of netscape-export
sub binaryexists {
   $binname = $build_dir . '/mozilla/cmd/xfe/' . $build_objname . '/mozilla-export';
   print LOG $binname . "\n"; 
   if ((-e $binname) && (-x $binname) && (-s $binname)) {
     1;
   }
   else {
     0;
   }
}

sub deletebinary {

$binname = $build_dir . '/mozilla/cmd/xfe/' . $build_objname . '/mozilla-export';
print LOG "unlinking $binname\n";
unlink ($binname) || print LOG "unlinking $binname failed\n";

}
