
#!d:/nstools/bin/perl5

use Cwd;

$build_depend=1;      #depend or clobber
$build_tree = '';
$build_tag = '';
$build_name = '';
$build_continue = 0;
$build_sleep=10;
$no32 = 0;
$original_path = $ENV{'PATH'};
$early_exit = 1;
$doawt11 = 0;

$do_clobber = '';
$client_param = '';

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
$start_dir =~ s/\//\\/ ;
$last_time = 0;

print "starting dir is: $start_dir\n";

while( $early_exit ){
    chdir("$start_dir");
    if( time - $last_time < (60 * $build_sleep) ){
        $sleep_time = (60 * $build_sleep) - (time - $last_time);
        print "\n\nSleeping $sleep_time seconds ...\n";
        sleep( $sleep_time );
    }
    $last_time = time;

    $start_time = time;
    $start_time = adjust_start_time($start_time);
    $start_time_str = `unix_date +"%m/%d/%Y %H:%M"`;
    chop($start_time_str);
# call setup_env here in the loop, to update MOZ_DATE with each pass.
# setup_env uses start_time_str for MOZ_DATE.
    &setup_env;

    $cur_dir = cwd;
    $cur_dir =~ s/\//\\/ ;

    if( $cur_dir ne $start_dir ){
        print "startdir: $start_dir, curdir $cur_dir\n";
        die "curdir != startdir";
    }

    if ($build_test) {
        $early_exit = 0;        # stops this while loop after one pass.
    }

# strip_conf fails to strip any variables from the real environemnt
#    &strip_config;
    &do_build(1,$do_clobber);
#    &restore_config;

}

sub setup32 {
    local ($awt) = @_;

    $ENV{"MOZ_BITS"}  = '32';
    $ENV{"WINOS"}  = 'WINNT';
    $ENV{"STANDALONE_IMAGE_LIB"} = '1';
    $ENV{"MODULAR_NETLIB"} = '1';
    $ENV{"OS_TARGET"} = 'WIN95';
    $ENV{"PATH"}      = $original_path;
    $ENV{"_MSC_VER"}      = '1200';
    $moz_src          = $ENV{'MOZ_SRC'} = $start_dir;
    $build_name       = 'Win32 VC6.0' . ($build_depend?'Depend':'Clobber');
    $do_clobber       = $clobber_str;
    $logname          = "win32.log";
}


sub do_build {
    local ($pull, $do_clobber) = @_;
    
    print "pull = $pull, do_clobber = $do_clobber\n";
    &start_build;

    print "opening log file ${logname}\n";
    open( LOG, ">${logname}" ) || print "can't open $logname\n";
    print LOG "current dir is: $cur_dir\n";

    &print_env;
    $build_status = 0;
    if( $pull ){
        if ( $build_tag eq '' ){
#           print LOG   "cvs -d %MOZ_CVSROOT% co -A mozilla/config 2>&1 |\n";
#           open( PULL, "cvs -d %MOZ_CVSROOT% co -A mozilla/config 2>&1 |") || print "couldn't execute cvs\n";;
            print LOG   "cvs -d $moz_cvsroot co -A mozilla/client.mak 2>&1 |\n";
            open( PULL, "cvs -d $moz_cvsroot co -A mozilla/client.mak 2>&1 |") || print "couldn't execute cvs\n";;
        } else{
            print LOG   "cvs -d $moz_cvsroot co -A -r $build_tag mozilla/config 2>&1 |\n";
            open( PULL, "cvs -d $moz_cvsroot co -A -r $build_tag mozilla/config 2>&1 |") || print "couldn't execute cvs\n";;
            print LOG   "cvs -d $moz_cvsroot co -A -r $build_tag mozilla/client.mak 2>&1 |\n";
            open( PULL, "cvs -d $moz_cvsroot co -A -r $build_tag mozilla/client.mak 2>&1 |") || print "couldn't execute cvs\n";;
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

#    if( $do_clobber ne '' ){
#        print LOG   "nmake -f client.mak $do_clobber |\n";
#        print "nmake -f client.mak $do_clobber |\n";
#        open( CLOBBER, "nmake -f client.mak $do_clobber 2>&1 |") || print "couldn't execute nmake\n";;

        # tee the output
#        while( <CLOBBER> ){
#            print $_;
#            print LOG $_;
#        }
#        close( CLOBBER );
#    }


    if (!$pull) {
      $client_param = $do_clobber . ' build_all';
    }
    else {
      $client_param = 'pull_all ' . $do_clobber . ' build_all';
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

    print LOG "\n";
    print LOG "tinderbox: tree: $build_tree\n";
    print LOG "tinderbox: builddate: $start_time\n";
    print LOG "tinderbox: status: $build_status_str\n";
    print LOG "tinderbox: build: $build_name\n";
    print LOG "tinderbox: errorparser: windows\n";
    print LOG "tinderbox: buildfamily: windows\n";

    close( LOG );
    chdir("$start_dir");
    system( "$nstools\\bin\\blat ${logname} -t tinderbox-daemon\@cvs-mirror.mozilla.org" );

 # we should save at least one log back
    system("copy ${logname} last-${logname}");
}

sub cvs_time {
    local( $ret_time );

    ($sec,$minute,$hour,$mday,$mon,$year) = localtime( $_[0] );
    $mon++; # month is 0 based.

    sprintf("%02d/%02d/%04d %02d:%02d:00",
        $mon,$mday,$year,$hour,$minute );
}


sub start_build {
    open( LOG, ">logfile" );
    print LOG "\n";
    print LOG "tinderbox: tree: $build_tree\n";
    print LOG "tinderbox: builddate: $start_time\n";
    print LOG "tinderbox: status: building\n";
    print LOG "tinderbox: build: $build_name\n";
    print LOG "tinderbox: errorparser: windows\n";
    print LOG "tinderbox: buildfamily: windows\n";
    print LOG "\n";
    close( LOG );
    system("$nstools\\bin\\blat logfile -t tinderbox-daemon\@cvs-mirror.mozilla.org" );
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
    die "usage: buildit.pl [--depend | --clobber] [--continue] [--test] [-tag TAGNAME] -t TREENAME\n";
}

sub setup_env {
    local($p);

#    $ENV{"MOZ_DEBUG"} = '1';
    $ENV{"MOZ_MFC"} = '1';
    $ENV{"CVSROOT"} = ':pserver:cltbld%netscape.com@cvs.mozilla.org:/cvsroot';
    $moz_cvsroot = $ENV{"CVSROOT"};
#    $ENV{"MOZ_EDITOR"} = '1';
    $ENV{"VERBOSE"} = '1';
    $ENV{"NGLAYOUT_PLUGINS"} = '1';
    $nstools = $ENV{"MOZ_TOOLS"};
    if( $nstools eq '' ){
        print "warning: environment variable MOZ_TOOLS not set\n";
        $ENV{"MOZ_TOOLS"} = 'C:\nstools';
        $nstools = $ENV{"MOZ_TOOLS"};
        print "using MOZ_TOOLS = $nstools \n";

    }
#    $msdev = $ENV{"MOZ_MSDEV"} = 'c:\msdev';
#    if( $msdev eq '' ){
#        die "error: environment variable MOZ_MSDEV not set\n";
#    }
#    $msvc = $ENV{"MOZ_MSVC"} = 'c:\msvc';
#    if( $msvc eq '' ){
#        die "error: environment variable MOZ_VC not set\n";
#    }
    if ( $build_tag ne '' ) {
        $ENV{"MOZ_BRANCH"} = $build_tag;
    }
    $moz_src = $ENV{"MOZ_SRC"} = $start_dir;
#    $ENV{"INCLUDE"}='c:\devstudio\vc\lib;C:\DevStudio\VC\INCLUDE;C:\DevStudio\VC\MFC\INCLUDE;C:\DevStudio\VC\lib;C:\DevStudio\VC\ATL\INCLUDE';
#    $ENV{"LIB"}='C:\DevStudio\VC\MFC\LIB;C:\DevStudio\VC\lib';
#    $ENV{"PATH"}='C:\DevStudio\SharedIDE\BIN;C:\DevStudio\VC\bin;C:\DevStudio\VC\lib;C:\DevStudio\VC\BIN\WINNT;.;%MOZ_TOOLS%\perl5\bin;C:\WINNT\system32;C:\WINNT;c:\usr\local\bin;%MOZ_TOOLS%\bin';
##
## We should pull by date to avoid mid-checkin disasters
##
    $ENV{"MOZ_DATE"} = $start_time_str;
    
    &setup32
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
}

sub restore_config {
        $ENV{"COMPUTERNAME"}=$save_compname;
        $ENV{"USERDOMAIN"}=$save_userdomain;
        $ENV{"USERNAME"}=$save_username;
        $ENV{"MSDevDir"}=$save_userprofile;
        &print_env;
}

sub adjust_start_time {
    # Allows the start time to match up with the update times of a mirror.
    my ($start_time) = @_;

    # Since we are not pulling for cvs-mirror anymore, just round times
    # to 1 minute intervals to make them nice and even.
    my $cycle = 1 * 60; # Updates every 1 minutes.
    my $begin = 0 * 60; # Starts 0 minutes after the hour.
    my $lag = 0 * 60; # Takes 0 minute to update.
    return int(($start_time - $begin - $lag) / $cycle) * $cycle + $begin;
}

