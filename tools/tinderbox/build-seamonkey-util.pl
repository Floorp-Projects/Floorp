#!/usr/bin/perl
#
# Requires: tinder-defaults.pl
#
# Intent: This is becoming a general-purpose tinderbox
#         script, specific uses (mozilla, commercial, etc.) should
#         set variables and then call into this script.
#
# Status: In the process of re-arranging things so a commercial
#         version can re-use this script.
#

require 5.000;

use Sys::Hostname;
use strict;

package TinderUtils;

{

  my $tinder_defaults = "tinder-defaults.pl";


sub InitVars {
    local $_;
    for (@ARGV) {
        # Save DATA section for printing the example.
        return if /^--example-config$/;
    }
    no strict 'vars';

	open DEFAULTS, $tinder_defaults or print "can't open $tinder_defaults, $?\n";
	
    while (<DEFAULTS>) {
      package Settings;
      #warn "config:$_";
      eval;
    }

	close DEFAULTS;

}

sub PrintExampleConfig {
    local $_;
    print "#- tinder-config.pl - Tinderbox configuration file.\n";
    print "#-    Uncomment the variables you need to set.\n";
    print "#-    The default values are the same as the commented variables.\n";
    print "\n";
    
	open DEFAULTS, $tinder_defaults or print "can't open $tinder_defaults, $!\n";
    while (<DEFAULTS>) {
        s/^\$/\#\$/;
        print;
    }
	close DEFAULTS;
}

}

sub GetSystemInfo {
    $Settings::OS = `uname -s`;
    my $os_ver = `uname -r`;
    $Settings::CPU = `uname -m`;
    $Settings::ObjDir = '';
    my $build_type = $Settings::BuildDepend ? 'Depend' : 'Clobber';
    my $host = ::hostname();
    $host =~ s/\..*$//;
    
    chomp($Settings::OS, $os_ver, $Settings::CPU, $host);
    
    if ($Settings::OS eq 'AIX') {
        my $osAltVer = `uname -v`;
        chomp($osAltVer);
        $os_ver = "$osAltVer.$os_ver";
    }
    
    $Settings::OS = 'BSD_OS' if $Settings::OS eq 'BSD/OS';
    $Settings::OS = 'IRIX'   if $Settings::OS eq 'IRIX64';
    
    if ($Settings::OS eq 'QNX') {
        $os_ver = `uname -v`;
        chomp($os_ver);
        $os_ver =~ s/^([0-9])([0-9]*)$/$1.$2/;
    }
    if ($Settings::OS eq 'SCO_SV') {
        $Settings::OS = 'SCOOS';
        $os_ver = '5.0';
    }
    
    $Settings::BuildName = "$host $Settings::OS $build_type";
    $Settings::DirName = "${Settings::OS}_${os_ver}_$build_type";
    
    # Make the build names reflect architecture/OS
    
    if ($Settings::OS eq 'AIX') {
        # $Settings::BuildName set above.
    }
    if ($Settings::OS eq 'BSD_OS') {
        $Settings::BuildName = "$host BSD/OS $os_ver $build_type";
    }
    if ($Settings::OS eq 'FreeBSD') {
        $Settings::BuildName = "$host $Settings::OS/$Settings::CPU $os_ver $build_type";
    }
    if ($Settings::OS eq 'HP-UX') {
        $Settings::BuildName = "$host $Settings::OS $os_ver $build_type";
    }
    if ($Settings::OS eq 'IRIX') {
        # $Settings::BuildName set above.
    }
    if ($Settings::OS eq 'Linux') {
        if ($Settings::CPU eq 'alpha' or $Settings::CPU eq 'sparc') {
            $Settings::BuildName = "$host $Settings::OS/$Settings::CPU $os_ver $build_type";
        } elsif ($Settings::CPU eq 'armv4l' or $Settings::CPU eq 'sa110') {
            $Settings::BuildName = "$host $Settings::OS/arm $os_ver $build_type";
        } elsif ($Settings::CPU eq 'ppc') {
            $Settings::BuildName = "$host $Settings::OS/$Settings::CPU $os_ver $build_type";
        } else {
            # $Settings::BuildName set above
        }
    }
    if ($Settings::OS eq 'NetBSD') {
        $Settings::BuildName = "$host $Settings::OS/$Settings::CPU $os_ver $build_type";
    }
    if ($Settings::OS eq 'OSF1') {
        # Assumes 4.0D for now.
    }
    if ($Settings::OS eq 'QNX') {
    }
    if ($Settings::OS eq 'SunOS') {
        if ($Settings::CPU eq 'i86pc') {
            $Settings::BuildName = "$host $Settings::OS/i386 $os_ver $build_type";
        } else {
            $Settings::OSVerMajor = substr($os_ver, 0, 1);
            if ($Settings::OSVerMajor ne '4') {
                $Settings::BuildName = "$host $Settings::OS/sparc $os_ver $build_type";
            }
        }
    }
}

sub LoadConfig {
    if (-r 'tinder-config.pl') {
        { package Settings; do 'tinder-config.pl'; }
    } else {
        warn "Error: Need tinderbox config file, tinder-config.pl\n";
        warn "       To get started, run the following,\n";
        warn "          $0 --example-config > tinder-config.pl\n";
        exit;
    }
}

sub SetupEnv {
    umask 0;
    my $topsrcdir = "$Settings::BaseDir/$Settings::DirName/mozilla";
    $ENV{LD_LIBRARY_PATH} = "$topsrcdir/${Settings::ObjDir}dist/bin"
                          . ":/usr/lib/png:/usr/local/lib";
    $ENV{LD_LIBRARY_PATH} .= ":$topsrcdir/dist/bin"
      if defined $Setting::Objdir and $Settings::Objdir ne '';
    $ENV{MOZILLA_FIVE_HOME} = "$topsrcdir/${Settings::ObjDir}dist/bin";
    $ENV{DISPLAY} = $Settings::DisplayServer;
    $ENV{MOZCONFIG} = "$Settings::BaseDir/$Settings::MozConfigFileName" 
      if $Settings::MozConfigFileName ne '' and -e $Settings::MozConfigFileName;
}

sub SetupPath {
    #print "Path before: $ENV{PATH}\n";

    if ($Settings::OS eq 'AIX') {
        $ENV{PATH} = "/builds/local/bin:$ENV{PATH}:/usr/lpp/xlC/bin";
        $Settings::ConfigureArgs   .= '--x-includes=/usr/include/X11 '
          . '--x-libraries=/usr/lib --disable-shared';
        $Settings::ConfigureEnvArgs = 'CC=xlC_r CXX=xlC_r';
        $Settings::Compiler = 'xlC_r';
        $Settings::NSPRArgs .= 'NS_USE_NATIVE=1 USE_PTHREADS=1';
    }
    
    if ($Settings::OS eq 'BSD_OS') {
        $ENV{PATH}        = "/usr/contrib/bin:/bin:/usr/bin:$ENV{PATH}";
        $Settings::ConfigureArgs .= '--disable-shared';
        $Settings::ConfigureEnvArgs = 'CC=shlicc2 CXX=shlicc2';
        $Settings::Compiler = 'shlicc2';
        $Settings::mail = '/usr/ucb/mail';
        # Because ld dies if it encounters -include
        $Settings::MakeOverrides = 'CPP_PROG_LINK=0 CCF=shlicc2';
        $Settings::NSPRArgs .= 'NS_USE_GCC=1 NS_USE_NATIVE=';
    }
    
    if ($Settings::OS eq 'FreeBSD') {
        $ENV{PATH} = "/bin:/usr/bin:$ENV{PATH}";
        if ($ENV{HOST} eq 'angelus.mcom.com') {
            $Settings::ConfigureEnvArgs = 'CC=egcc CXX=eg++';
            $Settings::Compiler = 'egcc';
        }
        $Settings::mail = '/usr/bin/mail';
    }
    
    if ($Settings::OS eq 'HP-UX') {
        $ENV{PATH} = "/opt/ansic/bin:/opt/aCC/bin:/builds/local/bin:"
          . "$ENV{PATH}";
        $ENV{LPATH} = "/usr/lib:$ENV{LD_LIBRARY_PATH}:/builds/local/lib";
        $ENV{SHLIB_PATH} = $ENV{LPATH};
        $Settings::ConfigureArgs   .= '--x-includes=/usr/include/X11 '
          . '--x-libraries=/usr/lib --disable-gtktest ';
        $Settings::ConfigureEnvArgs = 'CC="cc -Ae" CXX="aCC -ext"';
        $Settings::Compiler = 'cc/aCC';
        # Use USE_PTHREADS=1 instead of CLASSIC_NSPR if DCE is installed.
        $Settings::NSPRArgs .= 'NS_USE_NATIVE=1 CLASSIC_NSPR=1';
    }
    
    if ($Settings::OS eq 'IRIX') {
        $ENV{PATH} = "/opt/bin:$ENV{PATH}";
        $ENV{LD_LIBRARY_PATH}   .= ':/opt/lib';
        $ENV{LD_LIBRARYN32_PATH} = $ENV{LD_LIBRARY_PATH};
        $Settings::ConfigureEnvArgs = 'CC=cc CXX=CC CFLAGS="-n32 -O" CXXFLAGS="-n32 -O"';
        $Settings::Compiler = 'cc/CC';
        $Settings::NSPRArgs .= 'NS_USE_NATIVE=1 USE_PTHREADS=1';
    }
    
    if ($Settings::OS eq 'NetBSD') {
        $ENV{PATH} = "/bin:/usr/bin:$ENV{PATH}";
        $ENV{LD_LIBRARY_PATH} .= ':/usr/X11R6/lib';
        $Settings::ConfigureEnvArgs = 'CC=egcc CXX=eg++';
        $Settings::Compiler = 'egcc';
        $Settings::mail = '/usr/bin/mail';
    }
    
    if ($Settings::OS eq 'OSF1') {
        $ENV{PATH} = "/usr/gnu/bin:$ENV{PATH}";
        $ENV{LD_LIBRARY_PATH} .= ':/usr/gnu/lib';
        $Settings::ConfigureEnvArgs = 'CC="cc -readonly_strings" CXX="cxx"';
        $Settings::Compiler = 'cc/cxx';
        $Settings::MakeOverrides = 'SHELL=/usr/bin/ksh';
        $Settings::NSPRArgs .= 'NS_USE_NATIVE=1 USE_PTHREADS=1';
        $Settings::ShellOverride = '/usr/bin/ksh';
    }
    
    if ($Settings::OS eq 'QNX') {
        $ENV{PATH} = "/usr/local/bin:$ENV{PATH}";
        $ENV{LD_LIBRARY_PATH} .= ':/usr/X11/lib';
        $Settings::ConfigureArgs .= '--x-includes=/usr/X11/include '
          . '--x-libraries=/usr/X11/lib --disable-shared ';
        $Settings::ConfigureEnvArgs = 'CC="cc -DQNX" CXX="cc -DQNX"';
        $Settings::Compiler = 'cc';
        $Settings::mail = '/usr/bin/sendmail';
    }
    
    if ($Settings::OS eq 'SunOS') {
        if ($Settings::OSVerMajor eq '4') {
            $ENV{PATH} = "/usr/gnu/bin:/usr/local/sun4/bin:/usr/bin:$ENV{PATH}";
            $ENV{LD_LIBRARY_PATH} = "/home/motif/usr/lib:$ENV{LD_LIBRARY_PATH}";
            $Settings::ConfigureArgs .= '--x-includes=/home/motif/usr/include/X11 '
              . '--x-libraries=/home/motif/usr/lib';
            $Settings::ConfigureEnvArgs = 'CC="egcc -DSUNOS4" CXX="eg++ -DSUNOS4"';
            $Settings::Compiler = 'egcc';
        } else {
            $ENV{PATH} = '/usr/ccs/bin:' . $ENV{PATH};
        }
        if ($Settings::CPU eq 'i86pc') {
            $ENV{PATH} = '/opt/gnu/bin:' . $ENV{PATH};
            $ENV{LD_LIBRARY_PATH} .= ':/opt/gnu/lib';
            $Settings::ConfigureEnvArgs = 'CC=egcc CXX=eg++';
            $Settings::Compiler = 'egcc';
            
            # Possible NSPR bug... If USE_PTHREADS is defined, then
            #   _PR_HAVE_ATOMIC_CAS gets defined (erroneously?) and
            #   libnspr21 does not work.
            $Settings::NSPRArgs .= 'CLASSIC_NSPR=1 NS_USE_GCC=1 NS_USE_NATIVE=';
        } else {
            # This is utterly lame....
            if ($ENV{HOST} eq 'fugu') {
                $ENV{PATH} = "/tools/ns/workshop/bin:/usrlocal/bin:$ENV{PATH}";
                $ENV{LD_LIBRARY_PATH} = '/tools/ns/workshop/lib:/usrlocal/lib:'
                      . $ENV{LD_LIBRARY_PATH};
                $Settings::ConfigureEnvArgs = 'CC=cc CXX=CC';
                my $comptmp   = `cc -V 2>&1 | head -1`;
                chomp($comptmp);
                $Settings::Compiler = "cc/CC \($comptmp\)";
                $Settings::NSPRArgs .= 'NS_USE_NATIVE=1';
            } else {
                $Settings::NSPRArgs .= 'NS_USE_GCC=1 NS_USE_NATIVE=';
            }
            if ($Settings::OSVerMajor eq '5') {
                $Settings::NSPRArgs .= ' USE_PTHREADS=1';
            }
        }
    }
    #print "Path after: $ENV{PATH}\n";
}

# Need to end with a true value, (since we're using "require").
1;
