#!c:/nstools/bin/perl5

use Cwd;

$build_depend=1;      #depend or clobber
$build_tree = '';
$build_tag = '';
$build_name = '';
$build_continue = 0;
$build_sleep=10;
$no32 = 0;
$no16 = 0;
$original_path = $ENV{'PATH'};
$early_exit = 1;
$doawt11 = 0;

$do_clobber = '';
$client_param = 'pull_and_build_all';

&parse_args;

if( $build_test ){
    $build_sleep=1;
}

$dirname = ($build_depend?'dep':'clob');


$logfile = "${dirname}.log";

if( $build_depend ){
    $clobber_str = 'depend';
}     
else {
    $clobber_str = 'clobber_all';
}


mkdir("$dirname", 0777);
chdir("$dirname") || die "couldn't cd to $dirname";

$start_dir = cwd;
$last_time = 0;

print "starting dir is :$start_dir\n";


while( $early_exit ){
    chdir("$start_dir");
    if( time - $last_time < (60 * $build_sleep) ){
        $sleep_time = (60 * $build_sleep) - (time - $last_time);
        print "\n\nSleeping $sleep_time seconds ...\n";
        sleep( $sleep_time );
    }
    $last_time = time;

    $start_time = time-60*10;
    $start_time_str = &cvs_time( $start_time );
# call setup_env here in the loop, to update MOZ_DATE with each pass.
# setup_env uses start_time_str for MOZ_DATE.
    &setup_env;


    $cur_dir = cwd;

    if( $cur_dir ne $start_dir ){
        print "startdir: $start_dir, curdir $cur_dir\n";
        die "curdir != startdir";
    }

# build 32-bit with AWT_11=1
    &setup32("1");
    if( !$no32 ){
    	if( !$noawt11 ){
	    &do_build(1,$do_clobber);
    	}
    }

    if ($build_test) {
	$early_exit = 0;	# stops this while loop after one pass.
    }
    if( !$no16 ){

# build 32-bit with AWT_11=0
# necessary before building 16-bit because 16-bit cannot use AWT 1.1 classes
	&setup32("0");
	if( !$no32 ){
	    if( !$noawt11 ){
		&do_build(0,'');
	    } else {
		&do_build(1,$do_clobber);
	    }
	}

        &setup16;

# strip_conf fails to strip any variables from the real environemnt
#	&strip_config;
        &do_build(0,'');
#	&restore_config;
    }
}

sub copy_win16_dist {

    system 'xcopy w:\ y:\ns\dist /S /E /F'; 
    print "COPYCOPYCOPY\n";

}

sub build_NSPR20_Win16 {

    &start_build;
    unlink( "${logname}.last" );
    rename( "${logname}","${logname}.last");

    print "opening ${logname}\n";
    open( LOG, ">${logname}" ) || print "can't open $?\n";
    print LOG "current dir is :$cur_dir\n";

    &print_env;

    chdir("$moz_src/ns/nspr20") || die "couldn't chdir to '$moz_src/ns/nspr20'";
    print LOG   "gmake |\n";
    open( BUILDNSPR, "gmake 2>&1 |") || print "couldn't execute gmake\n";;

    while( <BUILDNSPR> ) {
        print $_;
        print LOG $_;
    }
    close ( BUILDNSPR );

    close( LOG );
}

sub setup32 {
    local ($awt) = @_;

    $ENV{"MOZ_BITS"}  = '32';
    $ENV{"AWT_11"}    = $awt;
    $doawt11 	      = $awt;

    $ENV{"INCLUDE"}   = "$msdev\\include;$msdev\\mfc\\include";
    $ENV{"LIB"}       = "$msdev\\lib;$msdev\\mfc\\lib";
    $ENV{"PATH"}      = $original_path . ";$msdev\\bin";
    $ENV{"OS_TARGET"} = 'WIN95';
    $moz_src          = $ENV{'MOZ_SRC'} = $start_dir;
    $build_name       = 'Win32 ' . ($build_depend?'Depend':'Clobber');
    $do_clobber       = $clobber_str;
    $logname          = "win32.log";
}

sub setup16 {
    $moz_src = $ENV{'MOZ_SRC'} = $start_dir;
    $ENV{"MOZ_BITS"} = '16';
# perl 5 is fucked up.  you MUST set AWT_11=0. deleting the environment
# variable doesn't work.  it's removed from the environment entry, but
# is still defined as true for a build.
    $ENV{"AWT_11"} = '0';
    $moz_src = $ENV{'MOZ_SRC'} = "$src_16_drive";
    $ENV{"OS_TARGET"} = 'WIN16';

    $msvc_inc = "$moz_src\\ns\\msvc15\\include;$moz_src\\ns\\msvc15\\mfc\\include";
    $msvc_lib = "$msvc\\lib;$msvc\\mfc\\lib";
    $msvcpath = "$msvc\\bin;c:\\nstools\\bin;c:\\WINNT40;c:\\WINNT40\\system32;c:\\utils";
    $ENV{"MSVC_INC"} = $msvc_inc;
    $ENV{"MSVC_LIB"} = $msvc_lib;
    $ENV{"MSVCPATH"} = $msvcpath;

    $ENV{"INCLUDE"}  = $msvc_inc;
    $ENV{"LIB"}      = $msvc_lib;
    $ENV{"PATH"}     = $msvcpath;

    $watcom = $ENV{"WATCOM"} = "C:\\WATCOM";
    $ENV{"EDPATH"}   = "$watcom\\EDDAT";
    $ENV{"WATC_INC"} = "$watcom\\h;$watcom\\h\win;$msvc_inc";
    $ENV{"WATC_LIB"} = $msvc_lib;
    $ENV{"WATCPATH"} = "$watcom\\BINNT;$watcom\\BINW;c:\\nstools\\bin";

    $build_name = 'Win16 ' . ($build_depend?'Depend':'Clobber');
    $do_clobber = $clobber_str;
    $logname = "win16.log";

    system "subst l: /d";
    system "subst r: /d";
    system "subst $src_16_drive /d";

    system "subst $src_16_drive $start_dir";
    system "subst r: $src_16_drive\\ns\\netsite\\ldap\\libraries\\msdos\\winsock";
    system "subst l: $src_16_drive\\ns\\netsite";
}

sub do_build {
    local ($pull, $do_clobber) = @_;

    &start_build;

    print "opening ${logname}\n";
    open( LOG, ">${logname}" ) || print "can't open $?\n";
    print LOG "current dir is :$cur_dir\n";

    &print_env;
    $build_status = 0;
    if( $pull ){
	if ( $build_tag eq '' ){
	    print LOG   "cvs co -D\"$start_time_str\" mozilla/client.mak 2>&1 |\n";
	    open( PULL, "cvs co -D\"$start_time_str\" mozilla/client.mak 2>&1 |") || print "couldn't execute cvs\n";;
	} else{
	    print LOG   "cvs co -r $build_tag mozilla/client.mak 2>&1 |\n";
	    open( PULL, "cvs co -r $build_tag mozilla/client.mak 2>&1 |") || print "couldn't execute cvs\n";;
	}

        # tee the output
        while( <PULL> ){
            print $_;
            print LOG $_;
        }
        close( PULL );
     
        $build_status = $?;
    }

    chdir("$moz_src/mozilla") || die "couldn't chdir to '$moz_src/mozilla'";

    if( $do_clobber ne '' ){
        print LOG   "nmake -f client.mak $do_clobber |\n";
        print "nmake -f client.mak $do_clobber |\n";
        open( PULL, "nmake -f client.mak $do_clobber 2>&1 |") || print "couldn't execute nmake\n";;

        # tee the output
        while( <PULL> ){
            print $_;
            print LOG $_;
        }
        close( PULL );
    }


    if (!$pull) {
      $client_param = 'build_all';
    }
    else {
      $client_param = 'pull_and_build_all';
    }
    if (!$doawt11) {
      $client_param = 'build_dist';
    }
    
    print LOG "nmake  -f client.mak $client_param 2>&1 |\n";
    open( BUILD, "nmake -f client.mak $client_param 2>&1 |");

    # tee the output
    while( <BUILD> ){
        print $_;
        print LOG $_;
    }
    close( BUILD );
    $build_status |= $?;

    $build_status_str = ( $build_status ? 'busted' : 'success' );

    print LOG "tinderbox: tree: $build_tree\n";
    print LOG "tinderbox: builddate: $start_time\n";
    print LOG "tinderbox: status: $build_status_str\n";
    print LOG "tinderbox: build: $build_name\n";
    print LOG "tinderbox: errorparser: windows\n";
    print LOG "tinderbox: buildfamily: windows\n";

    close( LOG );
    chdir("$start_dir");
    system( "$nstools\\bin\\blat ${logname} -t tinderbox-daemon\@warp" );
}

sub cvs_time {
    local( $ret_time );

    ($sec,$minute,$hour,$mday,$mon,$year) = localtime( $_[0] );
    $mon++; # month is 0 based.

    sprintf("%02d/%02d/%02d %02d:%02d:00",
        $mon,$mday,$year,$hour,$minute );
}


sub start_build {
    open( LOG, ">>logfile" );
    print LOG "\n";
    print LOG "tinderbox: tree: $build_tree\n";
    print LOG "tinderbox: builddate: $start_time\n";
    print LOG "tinderbox: status: building\n";
    print LOG "tinderbox: build: $build_name\n";
    print LOG "tinderbox: errorparser: windows\n";
    print LOG "tinderbox: buildfamily: windows\n";
    print LOG "\n";
    close( LOG );
    system("$nstools\\bin\\blat logfile -t tinderbox-daemon\@warp" );
}

sub parse_args {
    local($i);

    if( @ARGV == 0 ){
        &usage;
    }
    $i = 0;
    while( $i < @ARGV ){
        if( $ARGV[$i] eq '--depend' ){
            $build_depend = 1;
        }
        elsif ( $ARGV[$i] eq '--clobber' ){
            $build_depend = 0;
        }
        elsif ( $ARGV[$i] eq '--continue' ){
            $build_continue = 1;
        }
        elsif ( $ARGV[$i] eq '--noawt11' ){
            $noawt11 = 1;
        }
        elsif ( $ARGV[$i] eq '--no32' ){
            $no32 = 1;
        }
        elsif ( $ARGV[$i] eq '--no16' ){
            $no16 = 1;
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
    die "usage: buildit.pl [--depend | --clobber] [--no16] [--continue] [--test] [-tag TAGNAME] -t TREENAME\n";
}

sub setup_env {
    local($p);
    $ENV{"MOZ_DEBUG"} = '1';
    $ENV{"MOZ_GOLD"} = '1';
    $ENV{"NO_SECURITY"} = '1';
    $ENV{"MOZ_MEDIUM"} = '1';
    $ENV{"MOZ_CAFE"} = '1';
    $ENV{"NSPR20"} = '1';
    $ENV{"VERBOSE"} = '1';
    $nstools = $ENV{"MOZ_TOOLS"};
    if( $nstools eq '' ){
        die "error: environment variable MOZ_TOOLS not set\n";
    }
    $msdev = $ENV{"MOZ_MSDEV"} = 'c:\msdev';
    if( $msdev eq '' ){
        die "error: environment variable MOZ_MSDEV not set\n";
    }
    $msvc = $ENV{"MOZ_MSVC"} = 'c:\msvc';
    if( $msvc eq '' ){
        die "error: environment variable MOZ_VC not set\n";
    }
    if ( $build_tag ne '' ) {
	$ENV{"MOZ_BRANCH"} = $build_tag;
    }
    $moz_src = $ENV{"MOZ_SRC"} = $start_dir;
    $ENV{"MOZ_DATE"} = $start_time_str;
    $src_16_drive = 'y:';
}

sub print_env {
    local( $k, $v);
    print LOG "\nEnvironment\n";
    print     "\nEnvironment\n";
    for $k (sort keys %ENV){
        $v = $ENV{$k};
        print LOG "   $k=$v\n";
        print     "   $k=$v\n";
    }
    print LOG "\n";
    print     "\n";
    system 'set';
}

sub strip_config {
	$save_compname=$ENV{"COMPUTERNAME"};
	$save_userdomain=$ENV{"USERDOMAIN"};
	$save_username=$ENV{"USERNAME"};
	$save_userprofile=$ENV{"USERPROFILE"};
# most of these deletes have no effect.
	delete($ENV{"COMPUTERNAME"});
	delete($ENV{"USERDOMAIN"});
	delete($ENV{"USERNAME"});
	delete($ENV{"USERPROFILE"});
#	delete($ENV{"WATCOM"});

}

sub restore_config {
	$ENV{"COMPUTERNAME"}=$save_compname;
	$ENV{"USERDOMAIN"}=$save_userdomain;
	$ENV{"USERNAME"}=$save_username;
	$ENV{"MSDevDir"}=$save_userprofile;
	&print_env;
}
