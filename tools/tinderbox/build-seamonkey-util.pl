#!/usr/bin/perl
# -*- Mode: perl; indent-tabs-mode: nil -*-
# vim:sw=4:ts=8:et:ai:
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

require 5.003;

use Sys::Hostname;
use strict;
use POSIX qw(sys_wait_h strftime);
use Cwd;
use File::Basename; # for basename();
use File::Path;     # for rmtree();
use Config;         # for $Config{sig_name} and $Config{sig_num}
use File::Find ();
use File::Copy;

$::UtilsVersion = '$Revision: 1.273 $ ';

package TinderUtils;

#
# Optional, external, post-mozilla build
#
require "post-mozilla.pl" if -e "post-mozilla.pl";

#
# Test for Time::HiRes, for ms resolution from gettimeofday().
#
require "gettime.pl";

#
# For performance tests, we need the following perl modules installed:
# (MacOSX, Linux, Win2k):
#
# Time::HiRes      for higher timer resolution
# Bundle::LWP      for http (to report data to graph)
#
# The "CPAN" way of installing this is to start here:
#   % sudo perl -MCPAN -e shell
#   <take defaults..>
#   cpan> install Time::HiRes
#   [...]
#   cpan> install Bundle::LWP
#

my $co_time_str = 0;  # Global, let tests send cvs co time to graph server.


sub Setup {
    InitVars();
    my $args = ParseArgs();
    LoadConfig();
    ApplyArgs($args); # Apply command-line arguments after the config file.
    GetSystemInfo();
    SetupEnv();
    SetupPath();
    ValidateSettings(); # Perform some basic validation on settings
}


sub Build {
    #my () = @_;
    BuildIt();
}


sub PrintUsage {
    die <<END_USAGE
    usage: $0 [options]
Options:
  --depend               Build depend (must have this option or clobber).
  --clobber              Build clobber.
  --example-config       Print an example 'tinder-config.pl'.
  --display DISPLAY      Set DISPLAY for tests.
  --once                 Do not loop.
  --noreport             Do not report status to tinderbox server.
  --nofinalreport        Do not report final status, only start status.
  --notest               Do not run smoke tests.
  --testonly             Only run the smoke tests (do not pull or build).
  --notimestamp          Do not pull by date.
   -tag TREETAG          Pull by tag (-r TREETAG).
   -t TREENAME           The name of the tree
  --mozconfig FILENAME   Provide a mozconfig file for $Settings::moz_client_mk
  --version              Print the version number (same as cvs revision).
  --help
More details:
  To get started, run '$0 --example-config'.
END_USAGE
}


sub ParseArgs {
    PrintUsage() if $#ARGV == -1;

    my $args = {};
    my $arg;
    while ($arg = shift @ARGV) {
        $args->{BuildDepend} = 0, next if $arg eq '--clobber';
        $args->{BuildDepend} = 1, next if $arg eq '--depend';
        TinderUtils::PrintExampleConfig(), exit if $arg eq '--example-config';
        PrintUsage(), exit if $arg eq '--help' or $arg eq '-h';
        $args->{ReportStatus} = 0, next if $arg eq '--noreport';
        $args->{ReportFinalStatus} = 0, next if $arg eq '--nofinalreport';
        $args->{RunMozillaTests} = 0, next if $arg eq '--notest';
        $args->{TestOnly} = 1, next if $arg eq '--testonly';
        $args->{BuildOnce} = 1, next if $arg eq '--once';
        $args->{UseTimeStamp} = 0, next if $arg eq '--notimestamp';

        # debug post-mozilla.pl
        $args->{SkipMozilla} = 1, next if $arg eq '--skip-mozilla'; 

        my %args_with_options = qw(
            --display DisplayServer
            -tag BuildTag
            -t BuildTree
            --mozconfig MozConfigFileName
        );
        if (defined $args_with_options{$arg}) {
            my $arg_arg = shift @ARGV;
            PrintUsage() if $arg_arg eq '' or $arg_arg =~ /^-/;
            $args->{$args_with_options{$arg}} = $arg_arg;
        } elsif ($arg eq '--version' or $arg eq '-v') {
            die "$0: version" . substr($::Version,9,6) . "\n";
        } else {
            warn "Error: Unknown option: $arg\n";
            PrintUsage();
        }
    }

    return $args;
}

sub ApplyArgs {
    my ($args) = @_;

    my ($variable_name, $value);
    while (($variable_name, $value) = each %{$args}) {
        eval "\$Settings::$variable_name = \"$value\";";
    }
}

sub ValidateSettings {
    # Lowercase the LogCompression and LogEncoding variables for convenience.
    $Settings::LogCompression = lc $Settings::LogCompression;
    $Settings::LogEncoding = lc $Settings::LogEncoding;

    # Make sure LogCompression and LogEncoding are set to valid values.
    if ($Settings::LogCompression !~ /^(bzip2|gzip)?$/) {
        warn "Invalid value for LogCompression: $Settings::LogCompression.\n";
        exit;
    }
    if ($Settings::LogEncoding !~ /^(base64|uuencode)?$/) {
        warn "Invalid value for LogEncoding: $Settings::LogEncoding.\n";
        exit;
    }

    # If LogEncoding is set to 'base64', ensure we have the MIME::Base64
    # module before we go through the entire build.
    if ($Settings::LogEncoding eq 'base64') {
        eval "use MIME::Base64 ();";
        if ($@) {
            warn "LogEncoding set to base64 but the MIME::Base64 module could not be loaded.\n";
            warn "The error message was:\n\n";
            warn $@;
            exit;
        }
    }

    # If LogCompression is set, make sure LogEncoding is set or else the log
    # will not be transferred properly.
    if ($Settings::LogCompression ne '' && $Settings::LogEncoding eq '') {
        warn "LogEncoding must be set if LogCompression is set.\n";
        exit;
    }
}


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


sub GetSystemInfo {
    $Settings::OS = `uname -s`;
    my $os_ver = `uname -r`;
    $Settings::CPU = `uname -m`;
    #$Settings::ObjDir = '';
    my $build_type = $Settings::BuildDepend ? 'Depend' : 'Clobber';
    my $host = ::hostname();
    $host = $1 if $host =~ /(.*?)\./;
    chomp($Settings::OS, $os_ver, $Settings::CPU, $host);

    # Redirecting stderr to stdout works on *nix, winnt, but not on win98.
    $Settings::TieStderr = '2>&1';

    if ($Settings::OS eq 'AIX') {
        my $osAltVer = `uname -v`;
        chomp($osAltVer);
        $os_ver = "$osAltVer.$os_ver";
    }

    $Settings::OS = 'BSD_OS' if $Settings::OS eq 'BSD/OS';
    $Settings::OS = 'IRIX'   if $Settings::OS eq 'IRIX64';

    if ($Settings::OS eq 'SCO_SV') {
        $Settings::OS = 'SCOOS';
        $os_ver = '5.0';
    }
    if ($Settings::OS eq 'QNX') {
        $os_ver = `uname -v`;
        chomp($os_ver);
        $os_ver =~ s/^([0-9])([0-9]*)$/$1.$2/;
    }

    if ($Settings::OS =~ /^CYGWIN_(.*?)-(.*)$/) {
        # the newer cygwin apparently has different output for 'uname'
        # e.g., CYGWIN_98-4.10 == win98SE, and CYGWIN_NT-5.0 == win2k
        $Settings::OS = 'WIN' . $1;
        $os_ver = $2;
        $host =~ tr/A-Z/a-z/;
    }
    if ($Settings::OS =~ /^WIN/) {
        $host =~ tr/A-Z/a-z/;
        $Settings::TieStderr = "" if $Settings::OS eq 'WIN98';
    }

    $Settings::DirName = "${Settings::OS}_${os_ver}_$build_type";
    $Settings::BuildName = "$Settings::OS ${os_ver} $host $build_type";

    $Settings::DistBin = "dist/bin";

    # Make the build names reflect architecture/OS

    if ($Settings::OS eq 'AIX') {
        # $Settings::BuildName set above.
    }
    if ($Settings::OS eq 'BSD_OS') {
        $Settings::BuildName = "BSD/OS $os_ver $host $build_type";
    }
    if ($Settings::OS eq 'Darwin') {
        $Settings::BuildName = "MacOSX Darwin $os_ver $host $build_type";
    }
    if ($Settings::OS eq 'FreeBSD') {
        $Settings::BuildName = "$Settings::OS/$Settings::CPU $os_ver $host $build_type";
    }
    if ($Settings::OS eq 'HP-UX') {
        $Settings::BuildName = "$Settings::OS $os_ver $host $build_type";
    }
    if ($Settings::OS eq 'IRIX') {
        # $Settings::BuildName set above.
    }
    if ($Settings::OS eq 'Linux') {
        if ($Settings::CPU eq 'alpha' or $Settings::CPU eq 'sparc') {
            $Settings::BuildName = "$Settings::OS/$Settings::CPU $os_ver $host $build_type";
        } elsif ($Settings::CPU eq 'armv4l' or $Settings::CPU eq 'sa110') {
            $Settings::BuildName = "$Settings::OS/arm $os_ver $host $build_type";
        } elsif ($Settings::CPU eq 'ppc') {
            $Settings::BuildName = "$Settings::OS/$Settings::CPU $os_ver $host $build_type";
        } elsif (($Settings::CPU eq 'i686') or ($Settings::CPU eq 'i586')) {
            $Settings::BuildName = "$Settings::OS $host $build_type";
        } else {
            # $Settings::BuildName set above
        }
    }
    if ($Settings::OS eq 'NetBSD') {
        $Settings::BuildName = "$Settings::OS/$Settings::CPU $os_ver $host $build_type";
    }
    if ($Settings::OS eq 'OSF1') {
        # Assumes 4.0D for now.
    }
    if ($Settings::OS eq 'QNX') {
    }
    if ($Settings::OS eq 'SunOS') {
        if ($Settings::CPU eq 'i86pc') {
            $Settings::BuildName = "$Settings::OS/i386 $os_ver $host $build_type";
        } else {
            $Settings::OSVerMajor = substr($os_ver, 0, 1);
            if ($Settings::OSVerMajor ne '4') {
                $Settings::BuildName = "$Settings::OS/sparc $os_ver $host $build_type";
            }
        }
    }
    $Settings::BuildName .= " $Settings::BuildNameExtra";
}

sub LoadConfig {
    if (-r 'tinder-config.pl') {
        no strict 'vars';

        open CONFIG, 'tinder-config.pl' or
            print "can't open tinder-config.pl, $?\n";

        while (<CONFIG>) {
            package Settings;
            #warn "config:$_";
            eval;
        }

        close CONFIG;
    } else {
        warn "Error: Need tinderbox config file, tinder-config.pl\n";
        warn "       To get started, run the following,\n";
        warn "          $0 --example-config > tinder-config.pl\n";
        exit;
    }
}

my $objdir;
sub SetupEnv {
    umask 0;

    # Assume this file lives in the base dir, this will
    # avoid human error from setting this manually.
    $Settings::BaseDir = get_system_cwd();

    my $topsrcdir = "$Settings::BaseDir/$Settings::DirName/mozilla";
    $objdir = "$topsrcdir/${Settings::ObjDir}";

    if ($Settings::ReleaseBuild) {
        $ENV{BUILD_OFFICIAL}   = 1;
        $ENV{MOZILLA_OFFICIAL} = 1;
      if ($Settings::OS =~ /^WIN/) {
          $ENV{MOZ_PROFILE}      = 1;
          $ENV{PDBFILE}      = "NONE";
      }
    }

    if ($Settings::ObjDir ne '') {
        $ENV{LD_LIBRARY_PATH} = "$topsrcdir/${Settings::ObjDir}/$Settings::DistBin:" . "$ENV{LD_LIBRARY_PATH}";
    } else {
        $ENV{LD_LIBRARY_PATH} = "$topsrcdir/$Settings::DistBin:" . ($ENV{LD_LIBRARY_PATH} || "");
    }

    # MacOSX needs this set.
    if ($Settings::OS eq 'Darwin') {
        $ENV{DYLD_LIBRARY_PATH} = "$ENV{LD_LIBRARY_PATH}";
    }

    $ENV{LIBPATH} = "$topsrcdir/${Settings::ObjDir}/$Settings::DistBin:"
        . ($ENV{LIBPATH} || "");
    # BeOS requires that components/ be in the library search path per bug 51655
    $ENV{LIBRARY_PATH} = "$topsrcdir/${Settings::ObjDir}/$Settings::DistBin:"
	. "$topsrcdir/${Settings::ObjDir}/$Settings::DistBin/components:"
        . ($ENV{LIBRARY_PATH} || "");
    $ENV{ADDON_PATH} = "$topsrcdir/${Settings::ObjDir}/$Settings::DistBin:"
        . ($ENV{ADDON_PATH} || "");
    $ENV{MOZILLA_FIVE_HOME} = "$topsrcdir/${Settings::ObjDir}/$Settings::DistBin";
    $ENV{DISPLAY} = $Settings::DisplayServer;
    $ENV{MOZCONFIG} = "$Settings::BaseDir/$Settings::MozConfigFileName"
        if $Settings::MozConfigFileName ne '' and -e $Settings::MozConfigFileName;

    # Mail test needs build-time env set.  -mcafee
    if($Settings::MailBloatTest) {
        $ENV{BUILD_MAIL_SMOKETEST} = "1";
    }

    # Codesighs/codesize test needs this to pull the right stuff.
    if ($Settings::CodesizeTest or $Settings::EmbedCodesizeTest) {
      $ENV{MOZ_MAPINFO} = "1";
    }

}

sub SetupPath {
    #print "Path before: $ENV{PATH}\n";
    $ENV{PATH} .= ":$Settings::BaseDir/$Settings::DirName/mozilla/${Settings::ObjDir}/$Settings::DistBin";
    if ($Settings::OS eq 'AIX') {
        $ENV{PATH} = "/builds/local/bin:$ENV{PATH}:/usr/lpp/xlC/bin";
        $Settings::ConfigureArgs   .= '--x-includes=/usr/include/X11 '
            . '--x-libraries=/usr/lib --disable-shared';
        $Settings::ConfigureEnvArgs ||= 'CC=xlC_r CXX=xlC_r';
        $Settings::Compiler ||= 'xlC_r';
        $Settings::NSPRArgs .= 'NS_USE_NATIVE=1 USE_PTHREADS=1';
    }

    if ($Settings::OS eq 'BSD_OS') {
        $ENV{PATH}        = "/usr/contrib/bin:/usr/contrib/gnome/bin:/usr/bin:$ENV{PATH}";
        $ENV{LD_LIBRARY_PATH} = "/usr/contrib/lib:/usr/contrib/gnome/lib:$ENV{LD_LIBRARY_PATH}";
        #$Settings::ConfigureArgs .= '--disable-shared';
        #$Settings::ConfigureEnvArgs ||= 'CC=shlicc2 CXX=shlicc2';
        #$Settings::Compiler ||= 'shlicc2';
        #$Settings::mail ||= '/usr/ucb/mail';
        # Because ld dies if it encounters -include
        #$Settings::MakeOverrides ||= 'CPP_PROG_LINK=0 CCF=shlicc2';
        $Settings::NSPRArgs .= 'NS_USE_GCC=1 NS_USE_NATIVE=';
    }

    if ($Settings::OS eq 'Darwin') {
        $ENV{PATH} = "/bin:/usr/bin:/sw/bin:$ENV{PATH}";
        $Settings::ConfigureEnvArgs = 'CC=cc CXX=c++';
        $Settings::Compiler = 'cc';
        $Settings::mail = '/usr/bin/mail';
        $Settings::Make = '/usr/bin/make';

        # There should be a way of auto-detecting this, for now
        # you have to match BuildDebug and --enable-optimize, 
        # --disable-debug to make things work here.
        if ($Settings::BuildDebug) {
            $Settings::DistBin = "dist/".$Settings::ProductName."Debug.app/Contents/MacOS";
        } else {
            $Settings::DistBin = "dist/".$Settings::ProductName.".app/Contents/MacOS";
        }
    }

    if ($Settings::OS eq 'FreeBSD') {
        $ENV{PATH} = "/bin:/usr/bin:$ENV{PATH}";
        if ($ENV{HOST} eq 'angelus.mcom.com') {
            $Settings::ConfigureEnvArgs = 'CC=egcc CXX=eg++';
            $Settings::Compiler = 'egcc';
        }
        $Settings::mail ||= '/usr/bin/mail';
    }

    if ($Settings::OS eq 'HP-UX') {
        $ENV{PATH} = "/opt/ansic/bin:/opt/aCC/bin:/builds/local/bin:"
            . "$ENV{PATH}";
        $ENV{LPATH} = "/usr/lib:$ENV{LD_LIBRARY_PATH}:/builds/local/lib";
        $ENV{SHLIB_PATH} = $ENV{LPATH};
        $Settings::ConfigureArgs   .= '--x-includes=/usr/include/X11 '
            . '--x-libraries=/usr/lib --disable-gtktest ';
        $Settings::ConfigureEnvArgs ||= 'CC="cc -Ae" CXX="aCC -ext"';
        $Settings::Compiler ||= 'cc/aCC';
        # Use USE_PTHREADS=1 instead of CLASSIC_NSPR if DCE is installed.
        $Settings::NSPRArgs .= 'NS_USE_NATIVE=1 CLASSIC_NSPR=1';
    }

    if ($Settings::OS eq 'IRIX') {
        $ENV{PATH} = "/opt/bin:$ENV{PATH}";
        $ENV{LD_LIBRARY_PATH}   .= ':/opt/lib';
        $ENV{LD_LIBRARYN32_PATH} = $ENV{LD_LIBRARY_PATH};
        $Settings::ConfigureEnvArgs ||= 'CC=cc CXX=CC CFLAGS="-n32 -O" CXXFLAGS="-n32 -O"';
        $Settings::Compiler ||= 'cc/CC';
        $Settings::NSPRArgs .= 'NS_USE_NATIVE=1 USE_PTHREADS=1';
    }

    if ($Settings::OS eq 'NetBSD') {
        $ENV{PATH} = "/bin:/usr/bin:$ENV{PATH}";
        $ENV{LD_LIBRARY_PATH} .= ':/usr/X11R6/lib';
        $Settings::ConfigureEnvArgs ||= 'CC=egcc CXX=eg++';
        $Settings::Compiler ||= 'egcc';
        $Settings::mail ||= '/usr/bin/mail';
    }

    if ($Settings::OS eq 'OSF1') {
        $ENV{PATH} = "/usr/gnu/bin:$ENV{PATH}";
        $ENV{LD_LIBRARY_PATH} .= ':/usr/gnu/lib';
        $Settings::ConfigureEnvArgs ||= 'CC="cc -readonly_strings" CXX="cxx"';
        $Settings::Compiler ||= 'cc/cxx';
        $Settings::MakeOverrides ||= 'SHELL=/usr/bin/ksh';
        $Settings::NSPRArgs .= 'NS_USE_NATIVE=1 USE_PTHREADS=1';
        $Settings::ShellOverride ||= '/usr/bin/ksh';
    }

    if ($Settings::OS eq 'QNX') {
        $ENV{PATH} = "/usr/local/bin:$ENV{PATH}";
        $ENV{LD_LIBRARY_PATH} .= ':/usr/X11/lib';
        $Settings::ConfigureArgs .= '--x-includes=/usr/X11/include '
            . '--x-libraries=/usr/X11/lib --disable-shared ';
        $Settings::ConfigureEnvArgs ||= 'CC="cc -DQNX" CXX="cc -DQNX"';
        $Settings::Compiler ||= 'cc';
        $Settings::mail ||= '/usr/bin/sendmail';
    }

    if ($Settings::OS eq 'SunOS') {
        if ($Settings::OSVerMajor eq '4') {
            $ENV{PATH} = "/usr/gnu/bin:/usr/local/sun4/bin:/usr/bin:$ENV{PATH}";
            $ENV{LD_LIBRARY_PATH} = "/home/motif/usr/lib:$ENV{LD_LIBRARY_PATH}";
            $Settings::ConfigureArgs .= '--x-includes=/home/motif/usr/include/X11 '
                . '--x-libraries=/home/motif/usr/lib';
            $Settings::ConfigureEnvArgs ||= 'CC="egcc -DSUNOS4" CXX="eg++ -DSUNOS4"';
            $Settings::Compiler ||= 'egcc';
        } else {
            $ENV{PATH} = '/usr/ccs/bin:' . $ENV{PATH};
        }
        if ($Settings::CPU eq 'i86pc') {
            $ENV{PATH} = '/opt/gnu/bin:' . $ENV{PATH};
            $ENV{LD_LIBRARY_PATH} = '/opt/gnu/lib:' . $ENV{LD_LIBRARY_PATH};
            if ($Settings::ConfigureEnvArgs eq '') {
                $Settings::ConfigureEnvArgs ||= 'CC=egcc CXX=eg++';
                $Settings::Compiler ||= 'egcc';
            }

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
                $Settings::ConfigureEnvArgs ||= 'CC=cc CXX=CC';
                my $comptmp   = `cc -V 2>&1 | head -1`;
                chomp($comptmp);
                $Settings::Compiler ||= "cc/CC \($comptmp\)";
                $Settings::NSPRArgs .= 'NS_USE_NATIVE=1';
            } else {
                $Settings::NSPRArgs .= 'NS_USE_GCC=1 NS_USE_NATIVE=';
            }
            if ($Settings::OSVerMajor eq '5') {
                $Settings::NSPRArgs .= ' USE_PTHREADS=1';
            }
        }
    }
    if ($Settings::OS =~ /^WIN/) {
        $Settings::use_blat = 1;
        $Settings::Compiler = 'cl';
    }

    $Settings::ConfigureArgs .= '--cache-file=/dev/null';

    # Pass $ObjDir along to the build system.
    if($Settings::ObjDir) {
        my $_objdir .= "MOZ_OBJDIR=$Settings::ObjDir";
        $Settings::MakeOverrides .= $_objdir;
    }

    #print "Path after: $ENV{PATH}\n";
}

sub print_log {
    my ($text) = @_;
    print LOG $text;
    print $text;
}

sub run_shell_command_with_timeout {
    my ($shell_command, $timeout_secs) = @_;
    my $now = localtime();
    local $_;

    chomp($shell_command);
    print_log "Begin: $now\n";
    print_log "$shell_command\n";

    my $pid = fork; # Fork off a child process.

    unless ($pid) { # child
        my $status = 0;
        open CMD, "$shell_command $Settings::TieStderr |" or die "open: $!";
        print_log $_ while <CMD>;
        close CMD or $status = 1;
        exit($status);
    }
    my $result = wait_for_pid($pid, $timeout_secs);

    $now = localtime();
    print_log "End:   $now\n";

    return $result;
}


sub run_shell_command {
    my ($shell_command) = @_;
    local $_;

    my $status = 0;
    chomp($shell_command);
    print_log "$shell_command\n";
    open CMD, "$shell_command $Settings::TieStderr |" or die "open: $!";
    print_log $_ while <CMD>;
    close CMD or $status = 1;
    return $status;
}

sub adjust_start_time {
    # Allows the start time to match up with the update times of a mirror.
    my ($start_time) = @_;

    # Since we are not pulling for cvs-mirror anymore, just round times
    # to 1 minute intervals to make them nice and even.
    my $cycle = 1 * 60;    # Updates every 1 minutes.
    my $begin = 0 * 60;    # Starts 0 minutes after the hour.
    my $lag   = 0 * 60;    # Takes 0 minute to update.
    return int(($start_time - $begin - $lag) / $cycle) * $cycle + $begin;
}


sub mail_build_started_message {
    my ($start_time) = @_;
    my $msg_log = "build_start_msg.tmp";
    open LOG, ">$msg_log";

    PrintUsage() if $Settings::BuildTree =~ /^\s+$/i;

    my $platform = $Settings::OS =~ /^WIN/ ? 'windows' : 'unix';

    print_log "\n";
    print_log "tinderbox: tree: $Settings::BuildTree\n";
    print_log "tinderbox: builddate: $start_time\n";
    print_log "tinderbox: status: building\n";
    print_log "tinderbox: build: $Settings::BuildName\n";
    print_log "tinderbox: errorparser: $platform\n";
    print_log "tinderbox: buildfamily: $platform\n";
    print_log "tinderbox: version: $::Version\n";
    print_log "tinderbox: END\n";
    print_log "\n";

    close LOG;

    if ($Settings::blat ne "" && $Settings::use_blat) {
        system("$Settings::blat $msg_log -t $Settings::Tinderbox_server");
    } else {
        system "$Settings::mail $Settings::Tinderbox_server "
            ." < $msg_log";
    }
    unlink "$msg_log";
}

sub encode_log {
    my $input_file = shift;
    my $output_file = shift;
    my $buf;
    if($Settings::LogEncoding eq 'base64') {
        eval "use MIME::Base64 ();";
        while(read($input_file, $buf, 60*57)) {
            print $output_file MIME::Base64::encode($buf);
        }
    }
    elsif($Settings::LogEncoding eq 'uuencode') {
        while(read($input_file, $buf, 45)) {
            print $output_file pack("u*", $buf);
        }
    }
    else {
        # Make sendmail happy.
        # Split lines longer than 1000 charaters into 1000 character lines.
        # If any line is a dot on a line by itself, replace it with a blank
        # line. This prevents cases where a <cr>.<cr> occurs in the log file.
        # Sendmail interprets that as the end of the mail, and truncates the
        # log before it gets to Tinderbox.  (terry weismann, chris yeh)
        
        while (<$input_file>) {
            my $length = length($_);
            my $offset;
            for ($offset = 0; $offset < $length ; $offset += 1000) {
                my $chars_left = $length - $offset;
                my $output_length = $chars_left < 1000 ? $chars_left : 1000;
                my $output = substr $_, $offset, $output_length;
                $output =~ s/^\.$//g;
                $output =~ s/\n//g;
                print $output_file "$output\n";
            }
        }
    }
}

sub mail_build_finished_message {
    my ($start_time, $build_status, $binary_url, $logfile) = @_;

    # Rewrite LOG to OUTLOG, shortening lines.
    open OUTLOG, ">$logfile.last" or die "Unable to open logfile, $logfile: $!";

    my $platform = $Settings::OS =~ /^WIN/ ? 'windows' : 'unix';

    # Put the status at the top of the log, so the server will not
    # have to search through the entire log to find it.
    print OUTLOG "\n";
    print OUTLOG "tinderbox: tree: $Settings::BuildTree\n";
    print OUTLOG "tinderbox: builddate: $start_time\n";
    print OUTLOG "tinderbox: status: $build_status\n";
    print OUTLOG "tinderbox: binaryurl: $binary_url\n" if ($binary_url ne "");
    print OUTLOG "tinderbox: build: $Settings::BuildName\n";
    print OUTLOG "tinderbox: errorparser: $platform\n";
    print OUTLOG "tinderbox: buildfamily: $platform\n";
    print OUTLOG "tinderbox: version: $::Version\n";
    print OUTLOG "tinderbox: utilsversion: $::UtilsVersion\n";
    print OUTLOG "tinderbox: logcompression: $Settings::LogCompression\n";
    print OUTLOG "tinderbox: logencoding: $Settings::LogEncoding\n";
    print OUTLOG "tinderbox: END\n";

    if ($Settings::LogCompression eq 'gzip') {
        open GZIPLOG, "gzip -c $logfile |" or die "Couldn't open gzip'd logfile: $!\n";
        encode_log(\*GZIPLOG, \*OUTLOG);
        close GZIPLOG;
    }
    elsif ($Settings::LogCompression eq 'bzip2') {
        open BZ2LOG, "bzip2 -c $logfile |" or die "Couldn't open bzip2'd logfile: $!\n";
        encode_log(\*BZ2LOG, \*OUTLOG);
        close BZ2LOG;
    }
    else {
        open LOG, "$logfile" or die "Couldn't open logfile, $logfile: $!";
        encode_log(\*LOG, \*OUTLOG);
        close LOG;
    }    
    close OUTLOG;
    unlink($logfile);

    # If on Windows, make sure the log mail has unix lineendings, or
    # we'll confuse the log scraper.
    if ($platform eq 'windows') {
        open(IN,"$logfile.last") || die ("$logfile.last: $!\n");
        open(OUT,">$logfile.new") || die ("$logfile.new: $!\n");
        while (<IN>) {
            s/\r\n$/\n/;
	    print OUT "$_";
        } 
        close(IN);
        close(OUT);
        File::Copy::move("$logfile.new", "$logfile.last") or die("move: $!\n");
    }

    if ($Settings::ReportStatus and $Settings::ReportFinalStatus) {
        if ($Settings::blat ne "" && $Settings::use_blat) {
            system("$Settings::blat $logfile.last -t $Settings::Tinderbox_server");
        } else {
            system "$Settings::mail $Settings::Tinderbox_server "
                ." < $logfile.last";
        }
    }
}

sub BuildIt {
    # $Settings::DirName is set in build-seamonkey-utils.pl
    mkdir $Settings::DirName, 0777;
    chdir $Settings::DirName or die "Couldn't enter $Settings::DirName";

    my $build_dir = get_system_cwd();

    if ($Settings::OS =~ /^WIN/ && $build_dir !~ m/^.:\//) {
        chomp($build_dir = `cygpath -w $build_dir`);
        $build_dir =~ s/\\/\//g;
    }

    my $binary_basename = "$Settings::BinaryName";

    my $binary_dir;
    $binary_dir = "$build_dir/$Settings::Topsrcdir/${Settings::ObjDir}/$Settings::DistBin";

    my $dist_dir = "$build_dir/$Settings::Topsrcdir/${Settings::ObjDir}/dist";
    my $full_binary_name = "$binary_dir/$binary_basename";

    my $embed_binary_basename = "$Settings::EmbedBinaryName";
    my $embed_binary_dir = "$build_dir/$Settings::Topsrcdir/${Settings::ObjDir}/${Settings::EmbedDistDir}";
    my $full_embed_binary_name = "$embed_binary_dir/$embed_binary_basename";

    my $exit_early = 0;
    my $start_time = 0;

    my $build_failure_count = 0;  # Keep count of build failures.

    # Bypass profile manager at startup.
    $ENV{MOZ_BYPASS_PROFILE_AT_STARTUP} = 1;

    # Avoid debug assertion dialogs (win32)
    $ENV{XPCOM_DEBUG_BREAK} = "warn";

    # Set up tag stuff.
    # Only one tag per file, so -r will override any -D settings.
    $Settings::CVSCO .= " -r $Settings::BuildTag"
        unless not defined($Settings::BuildTag) or $Settings::BuildTag eq '';

    print "Starting dir is : $build_dir\n";

    while (not $exit_early) {
        # $BuildSleep is the minimum amount of time a build is allowed to take.
        # It prevents sending too many messages to the tinderbox server when
        # something is broken.
        my $sleep_time = ($Settings::BuildSleep * 60) - (time - $start_time);
        if (not $Settings::TestOnly and $sleep_time > 0) {
            print "\n\nSleeping $sleep_time seconds ...\n";
            sleep $sleep_time;
        }
        $start_time = time();

        # Set this each time, since post-mozilla.pl can reset this.
        $ENV{MOZILLA_FIVE_HOME} = "$binary_dir";

        my $cvsco = '';

        # Note: Pull-by-date works on a branch, but cvs stat won't show
        # you this.  Thanks to cls for figuring this out.
        if ($Settings::UseTimeStamp) {
            $start_time = adjust_start_time($start_time);
            my $time_str = POSIX::strftime("%m/%d/%Y %H:%M", localtime($start_time));

            # Global, sorry.  Tests need this, it's everywhere.
            # Switch to format the graph server uses, to be consistant.
            $co_time_str = POSIX::strftime("%Y:%m:%d:%H:%M:%S", localtime($start_time));

            # Figure out the timezone.  Some platform issues here:
            #
            # Win32/Activate returns the long form, e.g. "Pacific Daylight Time"
            # which chokes win32 cvs.  Win32/cygwin perl returns "".
            #
            # Jaguar/MacOSX 10.2 posix call crashes.
            #

            my $timezone;

            if ($Settings::Timezone) {
                # If this is set in tinder-config.pl, use it.
                $timezone = $Settings::Timezone;
            } elsif (not ($Settings::OS =~ /^(Darwin|WIN)/)) {
                # Try posix call to find timezone.
                # Don't do this for Darwin, Win32.
                $timezone = POSIX::strftime("%Z", localtime($start_time));
            } else {
                # Fallback to what `date` says.
                chomp($timezone = `date "+%Z"`);
            }

            print "timezone = $timezone\n";

            $time_str .= " $timezone";
            $ENV{MOZ_CO_DATE} = "$time_str";
            # command.com/win9x loathes single quotes in command line
            $cvsco = "$Settings::CVSCO -D \"$time_str\"";
        } else {
            $cvsco = "$Settings::CVSCO -A";
        }

        mail_build_started_message($start_time) if $Settings::ReportStatus;

        chdir $build_dir;
        my $logfile = "$Settings::DirName.log";
        print "Opening $logfile\n";
        open LOG, ">$logfile"
            or die "Cannot open logfile, $logfile: $?\n";
        print_log "current dir is -- " . ::hostname() . ":$build_dir\n";
        print_log "Build Administrator is $Settings::BuildAdministrator\n";

        # Print user comment if there is one.
        if ($Settings::UserComment) {
            print_log "$Settings::UserComment\n";
        }

        # System id
        print_log "uname -a = " . `uname -a`;

        # Print out redhat version if we have it.
        if (-e "/etc/redhat-release") {
            print_log `cat /etc/redhat-release`;
        }

        PrintEnv();

        # Print out failure count
        if($build_failure_count > 0) {
            print_log "Previous consecutive build failures: $build_failure_count\n";
        }


        my $build_status = 'none';
        my $binary_url   = '';

        my $external_build = "$Settings::BaseDir/post-mozilla.pl";

        # Allow skipping of mozilla phase.
        unless ($Settings::SkipMozilla) {
          
          if (-e $external_build) {
              PostMozilla::PreBuild();
          }
          # Make sure we have $Settings::moz_client_mk
          unless (-e "$TreeSpecific::name/$Settings::moz_client_mk") {
            
            # Set CVSROOT here.  We should only need to checkout a new
            # version of $Settings::moz_client_mk once; we might have 
            # more than one cvs tree so set CVSROOT here to avoid confusion.
            $ENV{CVSROOT} = $Settings::moz_cvsroot;
            
            run_shell_command("$Settings::CVS $cvsco $TreeSpecific::name/$Settings::moz_client_mk $TreeSpecific::extrafiles");
          }
          
          # Create toplevel source directory.
          chdir $Settings::Topsrcdir or die "chdir $Settings::Topsrcdir: $!\n";
          
          # Build it
          unless ($Settings::TestOnly) { # Do not build if testing smoke tests.
            if ($Settings::OS =~ /^WIN/) {
              DeleteBinaryDir($binary_dir);
            } else {
              # Delete binary so we can test for it to determine success after building.
              DeleteBinary($full_binary_name);
              if ($Settings::EmbedTest or $Settings::BuildEmbed) {
                DeleteBinary($full_embed_binary_name);
              }

              # Delete dist directory to avoid accumulating cruft there, some commercial
              # build processes also need to do this.
              if (-e $dist_dir) {
                print_log "Deleting $dist_dir\n";
                File::Path::rmtree($dist_dir, 0, 0);
                if (-e "$dist_dir") {
                  print_log "Error: rmtree('$dist_dir', 0, 0) failed.\n";
                }
              }
            }

            my $status = 0;

            # Pull using separate step so that we can timeout if necessary
            my $make_co = "$Settings::Make -f $Settings::moz_client_mk " .
                "$TreeSpecific::checkout_target";
            if ($Settings::FastUpdate) {
                $make_co = "$Settings::Make -f $Settings::moz_client_mk fast-update";
            }

            # Run the checkout command.
            if ($build_status ne 'busted') {
                $status = run_shell_command_with_timeout("$make_co", 
                                         $Settings::CVSCheckoutTimeout);
                if ($status->{exit_value} != 0) {
                    $build_status = 'busted';
                    if ($status->{timed_out}) {
                        print_log "Error: CVS checkout timed out.\n";
                        # Need to figure out how to kill rogue cvs processes
                        my $_cvs_pid=`ps -u $ENV{USER} | grep cvs`;
                        $_cvs_pid =~ s/[a-zA-Z]*\s*(\d+).*/$1/;
                        chomp($_cvs_pid);
                        if ("$_cvs_pid" eq "" ) {
                            print_log "Cannot find cvs process to kill.\n";
                        } else {
                            print "cvs pid $_cvs_pid\n";
                            kill_process($_cvs_pid);
                        }
                    } else {
                        print_log "Error: CVS checkout failed.\n";
                    }
                } else {
                    $build_status = 'success';
                }
            }

            # Build up initial make command.
            my $make = "$Settings::Make -f $Settings::moz_client_mk $Settings::MakeOverrides CONFIGURE_ENV_ARGS='$Settings::ConfigureEnvArgs'";
            if ($Settings::FastUpdate) {
              $make = "$Settings::Make -f $Settings::moz_client_mk fast-update && $Settings::Make -f $Settings::moz_client_mk $Settings::MakeOverrides CONFIGURE_ENV_ARGS='$Settings::ConfigureEnvArgs' build";
            }

            # Make sure we have an ObjDir if we need one.
            mkdir $Settings::ObjDir, 0777 if ($Settings::ObjDir && ! -e $Settings::ObjDir);

            # Run the clobber target.
            if (!$Settings::BuildDepend && $build_status ne 'busted') {
                $status = run_shell_command "$make $TreeSpecific::clobber_target";
                if ($status != 0) {
                    $build_status = 'busted';
                }
            }

            # Run the build target.
            if ($build_status ne 'busted') {
                $status = run_shell_command "$make $TreeSpecific::build_target";
                if ($status != 0) {
                    $build_status = 'busted';
                } elsif (not BinaryExists($full_binary_name)) {
                    print_log "Error: binary not found: $binary_basename\n";
                    $build_status = 'busted';
                } else {
                    $build_status = 'success';
                }
            }

            # TestGtkEmbed is only built by default on certain platforms.
            if ($build_status ne 'busted' and ($Settings::EmbedTest or $Settings::BuildEmbed)) {
              if (not BinaryExists($full_embed_binary_name)) {
                print_log "Error: binary not found: $Settings::EmbedBinaryName\n";
                $build_status = 'busted';
              } else {
                $build_status = 'success';
              }
            }
          }

          if ($build_status ne 'busted' and BinaryExists($full_binary_name)) {
            print_log "$binary_basename binary exists, build successful.\n";

            if ($Settings::RunMozillaTests) {
              $build_status = run_all_tests($full_binary_name,
                                            $full_embed_binary_name,
                                            $build_dir);
            } else {
              print_log "Skipping Mozilla tests.\n";
              $build_status = 'success';
            }
          }

        } # SkipMozilla

        #
        #  Run (optional) external, post-mozilla build here.
        #
        if (((-e $external_build) and ($build_status eq 'success')) || 
            ($Settings::SkipMozilla)) {
            ($build_status, $binary_url) = PostMozilla::main($build_dir);
        }

        # Increment failure count if we failed.
        if ($build_status eq 'busted') {
            $build_failure_count++;
        } else {
            $build_failure_count = 0;
        }

        # win98 just ain't up to the task of continuous builds
        print_log "System going down for a reboot!! " . scalar localtime() . "\n"
            if $Settings::OS eq 'WIN98' && $Settings::RebootSystem;

        close LOG;
        chdir $build_dir;

        mail_build_finished_message($start_time, $build_status, $binary_url, $logfile)
            if $Settings::ReportStatus;

        rebootSystem() if $Settings::OS eq 'WIN98' && $Settings::RebootSystem;

        $exit_early++ if $Settings::TestOnly and ($build_status ne 'success');
        $exit_early++ if $Settings::BuildOnce;
    }
}


sub rebootSystem {
    # assumption is that system has been configured to automatically 
    # startup tinderbox again on the other side of the reboot
    if ($Settings::OS eq 'WIN98') {
        # http://support.microsoft.com/directory/article.asp?ID=KB;EN-US;Q234216
        #   6 == RESTART | FORCE; applies to win98 and winME
        print "System going down for a reboot!! ", scalar localtime, "\n";
        system("rundll32.exe shell32.dll,SHExitWindowsEx 6") == 0 ||
            warn "Failed to $! $@ $?";
    } else {
        print "rebootSystem() called on non-Win9x system. wtf?\n";
    }
}


# Create a profile named $Settings::MozProfileName in the normal $build_dir place.
sub create_profile {
    my ($build_dir, $binary_dir, $binary) = @_;
    my $profile_log = "$build_dir/create-profile.log";
    my $result = run_cmd($build_dir, $binary_dir,
                         [$binary, "-CreateProfile", $Settings::MozProfileName],
                         $profile_log, $Settings::CreateProfileTimeout);
    print_logfile($profile_log, "Profile Creation");
    return $result;
}


# Find mozilla profile.
sub get_profile_dir {
    my $build_dir = shift;
    my $profile_dir;
    if ($Settings::OS =~ /^WIN/) {
        if ($Settings::OS =~ /^WIN9/) { # 98 [but what does uname say on Me?]
            $profile_dir = $ENV{winbootdir} || $ENV{windir} || "C:\\WINDOWS";
            $profile_dir .= "\\Application Data";
        } elsif ($Settings::OS =~ /^WINNT/) { # NT 4, 2K, XP(?)
            # afaict, %USERPROFILE% should always be there, NT 4.0 and up
            if ($ENV{USERPROFILE}) {
                $profile_dir = $ENV{USERPROFILE} . "\\Application Data";
            } else { # use %APPDATA% as a fallback (or prepare to fail)
                $profile_dir = $ENV{APPDATA} || "C:\\_UNKNOWN_";
            }
        }
        if ($Settings::VendorName) {
          $profile_dir .= "\\$Settings::VendorName\\$Settings::ProductName\\Profiles\\$Settings::MozProfileName";
        }
        else {
          $profile_dir .= "\\$Settings::ProductName\\Profiles\\$Settings::MozProfileName";
        }
        $profile_dir =~ s|\\|/|g;
        if ($Settings::VendorName) {
          ($profile_dir) = <"$profile_dir*">;
        }
    } elsif ($Settings::OS eq "BeOS") {
        $profile_dir = "/boot/home/config/settings/Mozilla/$Settings::MozProfileName";
    } elsif ($Settings::OS eq "Darwin") {
        # This is ifdef'd in nsXREDirProvider.cpp
        if ($Settings::ProductName eq 'Thunderbird') {
            $profile_dir = "$ENV{HOME}/Library/Thunderbird/Profiles";
            ($profile_dir) = <$profile_dir/*.$Settings::MozProfileName>;
        } else {
            $profile_dir = "$ENV{HOME}/Library/Application Support/$Settings::ProductName/Profiles/$Settings::MozProfileName";
            if ($Settings::VendorName) {
                ($profile_dir) = <"$profile_dir*">;
            }
        }
    } else {
        # *nix
        if ($Settings::VendorName) {
          $profile_dir = "$build_dir/.".lc($Settings::VendorName)."/".lc($Settings::ProductName)."/$Settings::MozProfileName";
          ($profile_dir) = <$profile_dir*>;
        }
        else {
          $profile_dir = "$build_dir/.".lc($Settings::ProductName)."/";
          ($profile_dir) = <$profile_dir . "*" . $Settings::MozProfileName . "*">;
        }
    }

    return $profile_dir;
}

#
# Given profile directory, find pref file hidden in salt directory.
# profile $Settings::MozProfileName must exist before calling this sub.
#
sub find_pref_file {
    my $profile_dir = shift;

    # default to *nix
    my $pref_file = "prefs.js";

    unless (-e $profile_dir) {
        print_log "ERROR: profile $profile_dir does not exist\n";
        #XXX should make 'run_all_tests' throw a 'testfailed' exception
        # and just skip all the continual checking for $test_result
        return; # empty list
    }

    my $found = undef;
    my $sub = sub {$pref_file = $File::Find::name, $found++ if $pref_file eq $_};
    File::Find::find($sub, $profile_dir);
    unless ($found) {
        print_log "ERROR: couldn't find prefs.js in $profile_dir\n";
        return; # empty list
    }

    # Find full profile_dir while we're at it.
    $profile_dir = File::Basename::dirname($pref_file);

    print_log "profile dir = $profile_dir\n";
    print_log "prefs.js    = $pref_file\n";

    return ($pref_file, $profile_dir);
}



sub BinaryExists {
    my ($binary) = @_;
    my ($binary_basename) = File::Basename::basename($binary);

    if (not -e $binary) {
        print_log "$binary does not exist.\n";
        0;
    } elsif (not -s _) {
        print_log "$binary is zero-size.\n";
        0;
    } elsif (not -x _) {
        print_log "$binary is not executable.\n";
        0;
    } else {
        print_log "$binary_basename exists, is nonzero, and executable.\n";
        1;
    }
}

sub min {
    my $m = $_[0];
    my $i;
    foreach $i (@_) {
        $m = $i if ($m > $i);
    }
    return $m;
}


sub DeleteBinary {
    my ($binary) = @_;
    my ($binary_basename) = File::Basename::basename($binary);

    if (BinaryExists($binary)) {
        print_log "Deleting binary: $binary_basename\n";
        print_log "unlinking $binary\n";
        unlink $binary or print_log "Error: Unlinking $binary failed\n";
    } else {
        print_log "No binary detected; none deleted.\n";
    }
}

sub DeleteBinaryDir {
    my ($binarydir) = @_;
    if (-e $binarydir) {
        print_log "Deleting $binarydir\n";
        my $count = File::Path::rmtree($binarydir, 0, 0);
        if (-e "$binarydir") {
            print_log "Error: rmtree('$binarydir', 0, 0) failed.\n";
        }
    } else {
        print_log "No binarydir detected; none deleted.\n";
    }
}

sub PrintEnv {
    local $_;

    # Print out environment settings.
    my $key;
    foreach $key (sort keys %ENV) {
        print_log "$key=$ENV{$key}\n";
    }

    # Print out mozconfig if found.
    if (defined $ENV{MOZCONFIG} and -e $ENV{MOZCONFIG}) {
        print_log "-->mozconfig<----------------------------------------\n";
        open CONFIG, "$ENV{MOZCONFIG}";
        print_log $_ while <CONFIG>;
        close CONFIG;
        print_log "-->end mozconfig<----------------------------------------\n";
    }

    # Say if we found post-mozilla.pl
    if(-e "$Settings::BaseDir/post-mozilla.pl") {
        print_log "Found post-mozilla.pl\n";
    } else {
        print_log "Didn't find $Settings::BaseDir/post-mozilla.pl\n";
    }

    # Print compiler setting
    if ($Settings::Compiler ne '') {
        print_log "===============================\n";
        if ($Settings::Compiler eq 'gcc' or $Settings::Compiler eq 'egcc') {
            my $comptmp = `$Settings::Compiler --version`;
            chomp($comptmp);
            print_log "Compiler is -- $Settings::Compiler ($comptmp)\n";
        } else {
            print_log "Compiler is -- $Settings::Compiler\n";
        }
        print_log "===============================\n";
    }
}

# Parse a file for $token, given a file handle.
sub file_has_token {
    my ($filename, $token) = @_;
    local $_;
    my $has_token = 0;
    open TESTLOG, "<$filename" or die "Cannot open file, $filename: $!";
    while (<TESTLOG>) {
        if (/$token/) {
            $has_token = 1;
            last;
        }
    }
    close TESTLOG;
    return $has_token;
}

# Parse a file for $token, return the token.
# Look for the line "<token><delimiter><return-value>", e.g.
# for "__startuptime,5501"
#   token        = "__startuptime"
#   delimiter    = ","
#   return-value = "5501";
#
sub extract_token_from_file {
    my ($filename, $token, $delimiter) = @_;
    local $_;
    my $token_value = 0;
    open TESTLOG, "<$filename" or die "Cannot open file, $filename: $!";
    while (<TESTLOG>) {
        if (/$token/) {
            # pull the token out of $_
            $token_value = substr($_, index($_, $delimiter) + 1);
            chomp($token_value);
            last;
        }
    }
    close TESTLOG;
    return $token_value;
}



sub kill_process {
    my ($target_pid) = @_;
    my $start_time = time;

    # Try to kill and wait 10 seconds, then try a kill -9
    my $sig;
    for $sig ('TERM', 'KILL') {
        print "kill $sig $target_pid\n";
        kill $sig => $target_pid;
        my $interval_start = time;
        while (time - $interval_start < 10) {
            # the following will work with 'cygwin' perl on win32, but not 
            # with 'MSWin32' (ActiveState) perl
            my $pid = waitpid($target_pid, POSIX::WNOHANG());
            if (($pid == $target_pid and POSIX::WIFEXITED($?)) or $pid == -1) {
                my $secs = time - $start_time;
                $secs = $secs == 1 ? '1 second' : "$secs seconds";
                print_log "Process killed. Took $secs to die.\n";
                return;
            }
            sleep 1;
        }
    }
    die "Unable to kill process: $target_pid";
}

BEGIN {
    my %sig_num = ();
    my @sig_name = ();

    sub signal_name {
        # Find the name of a signal number
        my ($number) = @_;

        unless (@sig_name) {
            unless($Config::Config{sig_name} && $Config::Config{sig_num}) {
                die "No sigs?";
            } else {
                my @names = split ' ', $Config::Config{sig_name};
                @sig_num{@names} = split ' ', $Config::Config{sig_num};
                foreach (@names) {
                    $sig_name[$sig_num{$_}] ||= $_;
                }
            }
        }
        return $sig_name[$number];
    }
}

sub fork_and_log {
    # Fork a sub process and log the output.
    my ($home, $dir, $args, $logfile) = @_;

    my $pid = fork; # Fork off a child process.

    unless ($pid) { # child

        # Chimera doesn't want to reset home dir.
        if ($Settings::ResetHomeDirForTests) {
            $ENV{HOME} = $home if ($Settings::OS ne "BeOS");
        }
            
        # Explicitly set cwd to home dir.
        chdir $home or die "chdir($home): $!\n";

        # Now cd to dir where binary is..
        chdir $dir or die "chdir($dir): $!\n";

        open STDOUT, ">$logfile";
        open STDERR, ">&STDOUT";
        select STDOUT; $| = 1;  # make STDOUT unbuffered
        select STDERR; $| = 1;  # make STDERR unbuffered
        exec { $args->[0] } @$args;
        die "Could not exec()";
    }
    return $pid;
}

# Stripped down version of fork_and_log().
sub system_fork_and_log {
    # Fork a sub process and log the output.
    my ($cmd) = @_;

    my $pid = fork; # Fork off a child process.

    unless ($pid) { # child
        exec $cmd;
        die "Could not exec()";
    }
    return $pid;
}


sub wait_for_pid {
    # Wait for a process to exit or kill it if it takes too long.
    my ($pid, $timeout_secs) = @_;
    my ($exit_value, $signal_num, $dumped_core, $timed_out) = (0,0,0,0);
    my $sig_name;
    my $loop_count;

    die ("Invalid timeout value passed to wait_for_pid()\n")
        if ($timeout_secs <= 0);

    eval {
        $loop_count = 0;
        while (++$loop_count < $timeout_secs) {
            my $wait_pid = waitpid($pid, POSIX::WNOHANG());
            # the following will work with 'cygwin' perl on win32, but not 
            # with 'MSWin32' (ActiveState) perl
            last if ($wait_pid == $pid and POSIX::WIFEXITED($?)) or $wait_pid == -1;
            sleep 1;
        }

        $exit_value = $? >> 8;
        $signal_num = $? >> 127;
        $dumped_core = $? & 128;
        if ($loop_count >= $timeout_secs) {
            die "timeout";
        }
        return "done";
    };

    if ($@) {
        if ($@ =~ /timeout/) {
            kill_process($pid);
            $timed_out = 1;
        } else { # Died for some other reason.
            die; # Propagate the error up.
        }
    }
    $sig_name = $signal_num ? signal_name($signal_num) : '';

    return { timed_out=>$timed_out,
             exit_value=>$exit_value,
             sig_name=>$sig_name,
             dumped_core=>$dumped_core };
}

#
# Note that fork_and_log() sets the HOME env variable to do
# the command, this allows us to have a local profile in the
# shared cltbld user account.
#
sub run_cmd {
    my ($home_dir, $binary_dir, $args, $logfile, $timeout_secs) = @_;
    my $now = localtime();

    print_log "Begin: $now\n";
    print_log "cmd = " . join(' ', @{$args}) . "\n";

    my $pid = fork_and_log($home_dir, $binary_dir, $args, $logfile);
    my $result = wait_for_pid($pid, $timeout_secs);

    $now = localtime();
    print_log "End:   $now\n";

    return $result;
}

# System version of run_cmd().
sub run_system_cmd {
    my ($cmd, $timeout_secs) = @_;

    print_log "cmd = $cmd\n";
    my $pid = system_fork_and_log($cmd);
    my $result = wait_for_pid($pid, $timeout_secs);

    return $result;
}

sub get_system_cwd {
    my $a = Cwd::getcwd()||`pwd`;
    chomp($a);
    return $a;
}

sub print_test_errors {
    my ($result, $name) = @_;

    if (not $result->{timed_out} and $result->{exit_value} != 0) {
        if ($result->{sig_name} ne '') {
            print_log "Error: $name: received SIG$result->{sig_name}\n";
        }
        print_log "Error: $name: exited with status $result->{exit_value}\n";
        if ($result->{dumped_core}) {
            print_log "Error: $name: dumped core.\n";
        }
    }
}


# Report test results back to a server.
# Netscape-internal now, will push to mozilla.org, ask
# mcafee or jrgm for details.
#
# Needs the following perl stubs, installed for rh7.1:
# perl-Digest-MD5-2.13-1.i386.rpm
# perl-MIME-Base64-2.12-6.i386.rpm
# perl-libnet-1.0703-6.noarch.rpm
# perl-HTML-Tagset-3.03-3.i386.rpm
# perl-HTML-Parser-3.25-2.i386.rpm
# perl-URI-1.12-5.noarch.rpm
# perl-libwww-perl-5.53-3.noarch.rpm
#
sub send_results_to_server {
    my ($value, $raw_data, $testname, $tbox) = @_;

    # Prepend raw data with cvs checkout date, performance
    # Use MOZ_CO_DATE, but with same graph/collect.cgi format. (server)
    my $data_plus_co_time = "MOZ_CO_DATE=$co_time_str\t$raw_data";

    my $tmpurl = "http://$Settings::results_server/graph/collect.cgi";
    $tmpurl .= "?value=$value&data=$data_plus_co_time&testname=$testname&tbox=$tbox";

    print_log "send_results_to_server(): \n";
    print_log "tmpurl = $tmpurl\n";

    # libwww-perl has process control problems on windows,
    # spawn wget instead.
    if ($Settings::OS =~ /^WIN/) {
        system ("wget", "-O", "/dev/null", $tmpurl);
        print_log "send_results_to_server() succeeded.\n";
    } else {
        my $res = eval q{
            use LWP::UserAgent;
            use HTTP::Request;
            my $ua  = LWP::UserAgent->new;
            $ua->timeout(10); # seconds
            my $req = HTTP::Request->new(GET => $tmpurl);
            my $res = $ua->request($req);
            return $res;
        };
        if ($@) {
            warn "Failed to submit startup results: $@";
            print_log "send_results_to_server() failed.\n";
        } else {
            print "Results submitted to server: \n",
            $res->status_line, "\n", $res->content, "\n";
            print_log "send_results_to_server() succeeded.\n";
        }
    }
}


sub print_logfile {
    my ($logfile, $test_name) = @_;
    print_log "----------- Output from $test_name ------------- \n";
    open READRUNLOG, "$logfile";
    print_log "  $_" while <READRUNLOG>;
    close READRUNLOG;
    print_log "----------- End Output from $test_name --------- \n";
}


##################################################
#                                                #
#  Test definitions start here.                  #
#                                                #
##################################################

# Run all tests.  Had to pass in both binary and embed_binary.
#
sub run_all_tests {
    my ($binary, $embed_binary, $build_dir) = @_;

    my $binary_basename       = File::Basename::basename($binary);
    my $binary_dir            = File::Basename::dirname($binary);
    my $embed_binary_basename = File::Basename::basename($embed_binary);
    my $embed_binary_dir      = File::Basename::dirname($embed_binary);

    my $test_result = 'success';

    # Windows needs this for file: urls.
    my $win32_build_dir = $build_dir;
    if ($Settings::OS =~ /^WIN/ && $win32_build_dir !~ m/^.:\//) {
        chomp($win32_build_dir = `cygpath -w $win32_build_dir`);
        $win32_build_dir =~ s/\\/\//g;
    }

    #
    # Before running tests, run regxpcom so that we don't crash when
    # people change contractids on us (since we don't autoreg opt builds)
    #
    unlink("$binary_dir/components/compreg.dat") or warn "$binary_dir/components/compreg.dat not removed\n";
    if($Settings::RegxpcomTest) {
#        AliveTest("regxpcom", $build_dir, ["$binary_dir/regxpcom"],
        AliveTest("regxpcom", $build_dir, [$binary, "-register"],
                  $Settings::RegxpcomTestTimeout);
    }


    my ($pref_file, $profile_dir);

    if ($Settings::UseMozillaProfile) {
      # Profile directory.  This lives in way-different places
      # depending on the OS.
      #
      my $profiledir = get_profile_dir($build_dir);

      #
      # Make sure we have a profile to run tests.  This is assumed to be called
      # $Settings::MozProfileName and will live in $build_dir/.mozilla.
      # Also assuming only one profile here.
      #
      my $cp_result = 0;

      unless (-d "$profiledir") {
        print_log "No profile found, creating profile.\n";
        $cp_result = create_profile($build_dir, $binary_dir, $binary);
      } else {
        print_log "Found profile.\n";

        # Recreate profile if we have $Settings::CleanProfile set.
        if ($Settings::CleanProfile) {
          my $deletedir = $profiledir;

          print_log "Creating clean profile ...\n";
          print_log "Deleting $deletedir ...\n";
          File::Path::rmtree([$deletedir], 0, 0);
          if (-e "$deletedir") {
            print_log "Error: rmtree([$deletedir], 0, 0) failed.\n";
          }
          $cp_result = create_profile($build_dir, $binary_dir, $binary);
        }
      }

      # Set status, in case create profile failed.
      if ($cp_result) {
        # We should check $cp_result->{exit_value} too, except
        # semi-single-profile landing made 0 the success value (which is
        # good), so we now have inconsistent expected results.
        if (not $cp_result->{timed_out}) {
          $test_result = "success";
        } else {
          $test_result = "testfailed";
        }
      }

      # Call get_profile_dir again, so it can find the extension-salted
      # profile directory under the profile root.

      $profiledir = get_profile_dir($build_dir);

      #
      # Find the prefs file, remember we have that random string now
      # e.g. <build-dir>/.mozilla/default/uldx6pyb.slt/prefs.js
      # so File::Path::find will find the prefs.js file.
      ##
      ($pref_file, $profile_dir) = find_pref_file($profiledir);

      #XXX this is ugly and hacky 
      $test_result = 'testfailed' unless $pref_file;;

    } elsif($Settings::BinaryName eq "TestGtkEmbed") {
      print_log "Using TestGtkEmbed profile\n";
      
      $pref_file   = "$build_dir/.TestGtkEmbed/TestGtkEmbed/prefs.js";
      $profile_dir = "$build_dir";

      # Create empty prefs file if needed
      #unless (-e $pref_file) {
      #  system("mkdir -p $build_dir/.TestGtkEmbed/TestGtkEmbed");
      #  system("touch $pref_file");
      #}

      # Run TestGtkEmbed to generate proper pref file.
      # This should only need to be run the first time for a given tree.
      unless (-e $pref_file) {
        $test_result = AliveTest("EmbedAliveTest_profile", $build_dir,
                                 ["$embed_binary_dir/$embed_binary_basename"],
                                 $Settings::EmbedTestTimeout);
      }
    }


    #
    # Set prefs to run tests properly.
    #
    if ($pref_file && $test_result eq 'success') { #XXX lame
        if($Settings::LayoutPerformanceTest  or
           $Settings::DHTMLPerformanceTest   or
           $Settings::XULWindowOpenTest      or
           $Settings::StartupPerformanceTest or
           $Settings::MailBloatTest          or
           $Settings::QATest                 or
           $Settings::BloatTest2             or
           $Settings::BloatTest) {
            
            # Chances are we will be timing these tests.  Bring gettime() into memory
            # by calling it once, before any tests run.
            Time::PossiblyHiRes::getTime();
            
            # Some tests need browser.dom.window.dump.enabled set to true, so
            # that JS dump() will work in optimized builds.
            set_pref($pref_file, 'browser.dom.window.dump.enabled', 'true');
            
            # Set security prefs to allow us to close our own window,
            # pageloader test (and possibly other tests) needs this on.
            set_pref($pref_file, 'dom.allow_scripts_to_close_windows', 'true');

            # Suppress firefox's popup blocking
            if ($Settings::BinaryName =~ /^firefox/) {
                set_pref($pref_file, 'privacy.popups.firstTime', 'false');
                set_pref($pref_file, 'dom.disable_open_during_load', 'false');

                # Suppress default browser dialog
                set_pref($pref_file, 'browser.shell.checkDefaultBrowser', 'false');
            }

            # Suppress security warnings for QA test.
            if ($Settings::QATest) {
                set_pref($pref_file, 'security.warn_submit_insecure', 'true');
            }
            
        }
    }

    #
    # Assume that we want to test modern skin for all tests.
    #
    if ($pref_file && $test_result eq 'success' and $Settings::UseMozillaProfile) { #XXX lame
        if (system("\\grep -s general.skins.selectedSkin $pref_file > /dev/null")) {
            print_log "Setting general.skins.selectedSkin to modern/1.0\n";
            open PREFS, ">>$pref_file" or die "can't open $pref_file ($?)\n";
            print PREFS "user_pref(\"general.skins.selectedSkin\", \"modern/1.0\");\n";
            close PREFS;
        } else {
            print_log "Modern skin already set.\n";
        }
    }

    # Mozilla alive test
    #
    # Note: Bloat & other tests depend this on working.
    # Only disable this test if you know it passes and are
    # debugging another part of the test sequence.  -mcafee
    #
    if ($Settings::AliveTest and $test_result eq 'success') {
        $test_result = AliveTest("MozillaAliveTest", $build_dir,
                                 [$binary, "-P", $Settings::MozProfileName],
                                 $Settings::AliveTestTimeout);
    }

    # Mozilla java test
    if ($Settings::JavaTest and $test_result eq 'success') {

        # Workaround for rh7.1 & jvm < 1.3.0:
        $ENV{LD_ASSUME_KERNEL} = "2.2.5";

        $test_result = AliveTest("MozillaJavaTest", $build_dir,
                                 [$binary, "http://java.sun.com"],
                                 $Settings::JavaTestTimeout);
    }


    # Viewer alive test
    if ($Settings::ViewerTest and $test_result eq 'success') {
        $test_result = AliveTest("ViewerAliveTest", $build_dir,
                                 ["$binary_dir/viewer"],
                                 $Settings::ViewerTestTimeout);
    }

    # Embed test.  Test the embedded app.
    if ($Settings::EmbedTest and $test_result eq 'success') {
        $test_result = AliveTest("EmbedAliveTest", $build_dir,
                                 ["$embed_binary_dir/$embed_binary_basename"],
                                 $Settings::EmbedTestTimeout);
    }

    # Bloat test (based on nsTraceRefcnt)
    if ($Settings::BloatTest and $test_result eq 'success') {
      my @app_args;
      if($Settings::BinaryName eq "TestGtkEmbed" ||
         $Settings::BinaryName =~ /^firefox/) {
        @app_args = ["resource:///res/bloatcycle.html"];
      } else {
        @app_args = ["-f", "bloaturls.txt"];
      }      
      $test_result = BloatTest($binary, $build_dir,
                               @app_args, "",
                               $Settings::BloatTestTimeout);
    }

    # New and improved bloat/leak test (based on trace-malloc)
    if ($Settings::BloatTest2 and $test_result eq 'success') {
        $test_result = BloatTest2($binary, $build_dir, $Settings::BloatTestTimeout);
    }

    # Mail bloat/leak test.
    # Needs:
    #   BUILD_MAIL_SMOKETEST=1 set in environment
    #   $Settings::CleanProfile = 0
    #
    # Manual steps for this test:
    # 1) Create pop account qatest03/Ne!sc-pe
    # 2) Login to this mail account, type in password, and select
    #    "remember password with password manager".
    # 3) Add first recipient of new Inbox to AB, select "receives plaintext"
    # 4) If mail send fails, sometimes nsmail-2 flakes, may need
    #    an occasional machine reboot.
    #
    if ($Settings::MailBloatTest and $test_result eq 'success'
       and $Settings::UseMozillaProfile) {

        print_log "______________MailBloatTest______________\n";

        my $inbox_dir = "Mail/nsmail-2";

        chdir("$profile_dir/$inbox_dir");

        # Download new, test Inbox on top of existing one.
        # wget will not re-download, using -N to check timestamps.
        system("wget -N -T 60 http://www.mozilla.org/mailnews/bloat_Inbox");

        # Replace the Inbox file.
        unlink("Inbox");
        system("cp bloat_Inbox Inbox");

        # Remove the Inbox.msf file.
        # unlink("Inbox.msf");

        $test_result = BloatTest($binary, $build_dir, ["-mail"], "mail",
                                 $Settings::MailBloatTestTimeout);

        # back to build_dir
        chdir($build_dir);
    }


    # DomToTextConversion test
    if (($Settings::EditorTest or $Settings::DomToTextConversionTest)
        and $test_result eq 'success') {
        $test_result =
            FileBasedTest("DomToTextConversionTest", $build_dir, $binary_dir,
                          ["perl", "TestOutSinks.pl"], $Settings::DomTestTimeout,
                          "FAILED", 0, 0);  # Timeout means failure.
    }

    # XpcomGlue test.  Do not run this on MacOSX.
    if ($Settings::XpcomGlueTest and $test_result eq 'success') {
      $test_result =
        FileBasedTest("XpcomGlueTest", $build_dir, $binary_dir,
                      ["nsTestSample"], $Settings::DomTestTimeout,
                      "Test passed", 1, 0);  # Timeout means failure.

      if ($test_result eq 'testfailed') {
        print_log "XpcomGlueTest: If this fails ask dougt\@netscape.com for help.\n";
      }
    }


    # SeaMonkey Codesize test.
    #
    if ($Settings::CodesizeTest and $test_result eq 'success') {
      CodesizeTest("SeaMonkeyCodesizeTest", $build_dir, 0);
    }


    # Embed Codesize test.
    if ($Settings::EmbedCodesizeTest and $test_result eq 'success') {
      CodesizeTest("EmbedCodesizeTest", $build_dir, 1);
    }


    # Layout performance test.
    if ($Settings::LayoutPerformanceTest and $test_result eq 'success') {
      my @app_args;
      if($Settings::BinaryName eq "TestGtkEmbed" ||
         $Settings::BinaryName =~ /^firefox/) {
        @app_args = [$binary];        
      } else {
        @app_args = [$binary, "-P", $Settings::MozProfileName];
      }

      $test_result = LayoutPerformanceTest("LayoutPerformanceTest",
                                           $build_dir,
                                           @app_args);
    }

    # DHTML performance test.
    if ($Settings::DHTMLPerformanceTest and $test_result eq 'success') {
      my @app_args;
      if($Settings::BinaryName eq "TestGtkEmbed" ||
         $Settings::BinaryName =~ /^firefox/) {
        @app_args = [$binary];        
      } else {
        @app_args = [$binary, "-P", $Settings::MozProfileName];
      }

      $test_result = DHTMLPerformanceTest("DHTMLPerformanceTest",
                                          $build_dir,
                                          @app_args);
    }

    # QA test: Client-side JS, DOM/HTML/Views, form submission.
    if ($Settings::QATest and $test_result eq 'success') {
        $test_result = QATest("QATest",
                              $build_dir,
                              $binary_dir,
                              [$binary, "-P", $Settings::MozProfileName]);
    }


    # xul window open test.
    #
    if ($Settings::XULWindowOpenTest and $test_result eq 'success') {
        my $open_time;
        my $test_name = "XULWindowOpenTest";
        my $binary_log = "$build_dir/$test_name.log";

        # Settle OS.
        run_system_cmd("sync; sleep 5", 35);

        my @urlargs = (-chrome,"file:$build_dir/mozilla/xpfe/test/winopen.xul");
        if($test_result eq 'success') {
            $open_time = AliveTestReturnToken($test_name,
                                              $build_dir,
                                              [$binary, "-P", $Settings::MozProfileName, @urlargs],
                                              $Settings::XULWindowOpenTestTimeout,
                                              "__xulWinOpenTime",
                                              ":");
            chomp($open_time);
        }
        if($open_time) {
            $test_result = 'success';
            
            my $time = POSIX::strftime "%Y:%m:%d:%H:%M:%S", localtime;

            print_log "TinderboxPrint:" .
                "<a title=\"Best nav open time of 9 runs\" href=\"http://$Settings::results_server/graph/query.cgi?testname=xulwinopen&tbox=" .
                    ::hostname() . "&autoscale=1&days=7&avg=1&showpoint=$time,$open_time\">Txul:$open_time" . "ms</a>\n";

            # Pull out samples data from log.
            my $raw_data = extract_token_from_file($binary_log, "openingTimes", "=");
            chomp($raw_data);

            if($Settings::TestsPhoneHome) {
                send_results_to_server($open_time, $raw_data,
                                       "xulwinopen", ::hostname());
            }
        } else {
            $test_result = 'testfailed';
        }
    }

    if ($Settings::StartupPerformanceTest and $test_result eq 'success') {

      # Win32 needs to do some url magic for file: urls.
      my $startup_build_dir = $build_dir;
      if ($Settings::OS =~ /^WIN/) {
        $startup_build_dir = $win32_build_dir;
      }

      my @app_args;
      if($Settings::BinaryName eq "TestGtkEmbed") {
        @app_args = [];        
      } else {
        @app_args = ["-P", $Settings::MozProfileName];
      }

      $test_result = StartupPerformanceTest("StartupPerformanceTest",
                                            $binary,
                                            $startup_build_dir,
                                            @app_args,
                                            "file:$startup_build_dir/../startup-test.html");
    }
    return $test_result;
}

sub set_pref {
    my ($pref_file, $pref, $value) = @_;
    if (system("\\grep -s $pref $pref_file > /dev/null")) {
        print_log "Setting $pref to $value\n";
        open PREFS, ">>$pref_file" or die "can't open $pref_file ($?)\n";
        print PREFS "user_pref(\"$pref\", $value);\n";
        close PREFS;
    } else {
        print_log "Already set $pref\n";
    }
}


# Start up Mozilla, test passes if Mozilla is still alive
# after $timeout_secs (seconds).
#
sub AliveTest {
    my ($test_name, $build_dir, $args, $timeout_secs) = @_;
    my $binary = @$args[0];
    my $binary_basename = File::Basename::basename($binary);
    my $binary_dir = File::Basename::dirname($binary);
    my $binary_log = "$build_dir/$test_name.log";
    local $_;

    # Print out testname
    print_log "\n\nRunning $test_name test ...\n";

    # Debug
    #print_log "\n\nbuild_dir = $build_dir ...\n";
    #print_log "\n\nbinary_dir = $binary_dir ...\n";
    #print_log "\n\nbinary = $binary ...\n";

    # Print out timeout.
    print_log "Timeout = $timeout_secs seconds.\n";

    my $result = run_cmd($build_dir, $binary_dir, $args,
                         $binary_log, $timeout_secs);

    print_logfile($binary_log, "$test_name");

    if ($result->{timed_out}) {
        print_log "$test_name: $binary_basename successfully stayed up"
            ." for $timeout_secs seconds.\n";
        return 'success';
    } else {
        print_test_errors($result, $binary_basename);
        return 'testfailed';
    }
}

# Same as AliveTest, but look for a token in the log and return
# the value.  (used for startup iteration test).
sub AliveTestReturnToken {
    my ($test_name, $build_dir, $args, $timeout_secs, $token, $delimiter) = @_;
    my $status;
    my $rv = 0;

    # Same as in AliveTest, needs to match in order to find the log file.
    my $binary = @$args[0];
    my $binary_basename = File::Basename::basename($binary);
    my $binary_dir = File::Basename::dirname($binary);
    my $binary_log = "$build_dir/$test_name.log";

    $status = AliveTest($test_name, $build_dir, $args, $timeout_secs);

    # Look for and return token
    if ($status) {
        $rv = extract_token_from_file($binary_log, $token, $delimiter);
        chomp($rv);
        chop($rv) if ($rv =~ /\r$/); # cygwin perl doesn't chomp dos-newlinesproperly so use chop.
        if ($rv) {
            print "AliveTestReturnToken: token value = $rv\n";
        }
    }

    return $rv;
}


# Run a generic test that writes output to stdout, save that output to a
# file, parse the file looking for failure token and report status based
# on that.  A hack, but should be useful for many tests.
#
# test_name = Name of test we're gonna run, in $Settings::DistBin.
# testExecString = How to run the test
# testTimeoutSec = Timeout for hung tests, minimum test time.
# statusToken = What string to look for in test output to
#     determine test status.
# statusTokenMeansPass = Default use of status token is to look for
#     failure string.  If this is set to 1, then invert logic to look for
#     success string.
#
# timeout_is_ok = Don't report test failure if test times out.
#
# Note: I tried to merge this function with AliveTest(),
#       the process flow control got too confusing :(  -mcafee
#
sub FileBasedTest {
    my ($test_name, $build_dir, $binary_dir, $test_args, $timeout_secs,
        $status_token, $status_token_means_pass, $timeout_is_ok) = @_;
    local $_;

    # Assume the app is the first argument in the array.
    my ($binary_basename) = @$test_args[0];
    my $binary_log = "$build_dir/$test_name.log";

    # Print out test name
    print_log "\n\nRunning $test_name ...\n";

    my $result = run_cmd($build_dir, $binary_dir, $test_args,
                         $binary_log, $timeout_secs);

    print_logfile($binary_log, $test_name);

    if (($result->{timed_out}) and (!$timeout_is_ok)) {
      print_log "Error: $test_name timed out after $timeout_secs seconds.\n";
      return 'testfailed';
    } elsif ($result->{exit_value} != 0) {
      print_log "Error: $test_name exited with status $result->{exit_value}\n";
      print_test_errors($result, $test_name);
      return 'testfailed';
    } else {
      print_log "$test_name exited normally\n";
    }

    my $found_token = file_has_token($binary_log, $status_token);
    if ($found_token) {
        print_log "Found status token in log file: $status_token\n";
    } else {
        print_log "Status token, $status_token, not found\n";
    }

    if (($status_token_means_pass and $found_token) or
        (not $status_token_means_pass and not $found_token)) {
        return 'success';
    } else {
        print_log "Error: $test_name has failed.\n";
        return 'testfailed';
    }
} # FileBasedTest


sub LayoutPerformanceTest {
    my ($test_name, $build_dir, $args) = @_;
    my $layout_test_result;
    my $layout_time;
    my $layout_time_details;
    my $binary_log = "$build_dir/$test_name.log";
    my $url = "http://$Settings::pageload_server/page-loader/loader.pl?delay=1000&nocache=0&maxcyc=4&timeout=$Settings::LayoutPerformanceTestPageTimeout&auto=1";
    
    # Settle OS.
    run_system_cmd("sync; sleep 5", 35);
    
    $layout_time = AliveTestReturnToken($test_name,
                                        $build_dir,
                                        [@$args, $url],
                                        $Settings::LayoutPerformanceTestTimeout,
                                        "_x_x_mozilla_page_load",
                                        ",");
    
    if($layout_time) {
      chomp($layout_time);
      my @times = split(',', $layout_time);
      $layout_time = $times[0];  # Set layout time to first number. 
    } else {
      print_log "TinderboxPrint:Tp:[CRASH]\n";
      
      # Run the test a second time.  Getting intermittent crashes, these
      # are expensive to wait, a 2nd run that is successful is still useful.
      # Sorry for the cut & paste. -mcafee
      
      $layout_time = AliveTestReturnToken($test_name,
                                          $build_dir,
                                          [@$args, $url],
                                          $Settings::LayoutPerformanceTestTimeout,
                                          "_x_x_mozilla_page_load",
                                          ",");
      
      # Print failure message if we fail 2nd time.
      unless($layout_time) {
        print_log "TinderboxPrint:Tp:[CRASH]\n";
      } else {
        chomp($layout_time);
        my @times = split(',', $layout_time);
        $layout_time = $times[0];  # Set layout time to first number.
      }
    }
    
    # Set test status.
    if($layout_time) {
      $layout_test_result = 'success';
    } else {
      $layout_test_result = 'testfailed';
    }
    
    if($layout_test_result eq 'success') {
      my $time = POSIX::strftime "%Y:%m:%d:%H:%M:%S", localtime;

      my $tp_prefix = "";
      if($Settings::BinaryName eq "TestGtkEmbed") {
        $tp_prefix = "m";
      }
      
      print_log "TinderboxPrint:" .
        "<a title=\"Avg of the median per url pageload time\" href=\"http://$Settings::results_server/graph/query.cgi?testname=pageload&tbox=" .
          ::hostname() . "&autoscale=1&days=7&avg=1&showpoint=$time,$layout_time\">" . $tp_prefix . "Tp:$layout_time" . "ms</a>\n";
      
      # Pull out detail data from log.
      my $raw_data = extract_token_from_file($binary_log, "_x_x_mozilla_page_load_details", ",");
      chomp($raw_data);
      
      if($Settings::TestsPhoneHome) {
        send_results_to_server($layout_time, $raw_data, "pageload", ::hostname());
      }
    }

    return $layout_test_result;
}

sub DHTMLPerformanceTest {
    my ($test_name, $build_dir, $args) = @_;
    my $dhtml_test_result;
    my $dhtml_time;
    my $dhtml_time_details;
    my $binary_log = "$build_dir/$test_name.log";
    my $url = "http://www.mozilla.org/performance/test-cases/dhtml/runTests.html";
    
    # Settle OS.
    run_system_cmd("sync; sleep 5", 35);
    
    $dhtml_time = AliveTestReturnToken($test_name,
                                       $build_dir,
                                       [@$args, $url],
                                       $Settings::LayoutPerformanceTestTimeout,
                                       "_x_x_mozilla_dhtml",
                                       ",");

    if($dhtml_time) {
      $dhtml_test_result = 'success';
    } else {
      $dhtml_test_result = 'testfailed';
    }
    
    if($dhtml_test_result eq 'success') {
      my $time = POSIX::strftime "%Y:%m:%d:%H:%M:%S", localtime;

      if ($Settings::TestsPhoneHome) {
        print_log "TinderboxPrint:" .
          "<a title=\"DHTML time\" href=\"http://$Settings::results_server/graph/query.cgi?testname=dhtml&tbox=" .
          ::hostname() . "&autoscale=1&days=7&avg=1&showpoint=$time,$dhtml_time\">Tdhtml:" . $dhtml_time . "ms</a>\n";
        send_results_to_server($dhtml_time, "--", "dhtml", ::hostname());
      } else {
        print_log "TinderboxPrint:" .
          "<abbr title=\"DHTML time\">Tdhtml:" . $dhtml_time . "ms</abbr>\n";
      }      
    }

    return $dhtml_test_result;
}


#
# Codesize test.  Needs:  cvs checkout mozilla/tools/codesighs
#
# This test can be run in two modes.  One for the whole SeaMonkey
# tree, the other for just the embedding stuff.
#
sub CodesizeTest {
  my ($test_name, $build_dir, $isEmbedTest) = @_;

  my $topsrcdir = "$build_dir/mozilla";

  # test needs this set
  $ENV{MOZ_MAPINFO} = "1";
  $ENV{TINDERBOX_OUTPUT} = "1";
  
  #chdir(".."); # up one level.
  
  my $cwd = get_system_cwd();
  print_log "cwd = $cwd\n";
  
  my $type; # "auto" or "base"
  my $zee;  # Letter that shows up on tbox.
  my $testNameString;
  my $graphName;

  if($isEmbedTest) {
    $testNameString = "Embed"; 
    $type = "base";     # Embed test.
    $zee = "mZ";
    $graphName = "codesize_embed";
  } else {
    $testNameString = "SeaMonkey"; 
    $type = "auto";  # SeaMonkey test.
    $zee = "Z";
    $graphName = "codesize";
  }

  my $new_log   = "Codesize-" . $type . "-new.log";
  my $old_log   = "Codesize-" . $type . "-old.log";
  my $diff_log  = "Codesize-" . $type . "-diff.log";
  my $test_log  = "$test_name.log";
  
  print_log "\$build_dir = $build_dir\n";

  # Clear the logs from the last run, so we can properly test for success.
  unlink("$build_dir/$new_log");
  unlink("$build_dir/$diff_log");
  unlink("$build_dir/$test_log");
  
  my $bash_cmd = "$topsrcdir/tools/codesighs/";
  if ($Settings::OS =~ /^WIN/ && $Settings::Compiler ne "gcc") {
    $bash_cmd .= $type . "summary.win.bash";
  } else {
    # Assume Linux for non-windows for now.
    $bash_cmd .= $type . "summary.unix.bash";
  }
 
  my $cmd = ["bash", $bash_cmd];
  push(@{$cmd}, "-o", "$objdir") if ($Settings::ObjDir ne "");
  push(@{$cmd}, $new_log, $old_log, $diff_log);
 
  my $test_result =
    FileBasedTest($test_name, 
                  "$build_dir", 
                  "$build_dir",  # run top of tree, not in dist.
                  $cmd,
                  $Settings::CodesizeTestTimeout,
                  "FAILED", # Fake out failure mode, test file instead.
                  0, 0);    # Timeout means failure.
  
  # Set status based on file creation.
  if (-e "$build_dir/$new_log") {
    print_log "found $build_dir/$new_log\n";
    $test_result = 'success';
    
    # Print diff data to tbox log.
    if (-e "$build_dir/$diff_log") {
      print_logfile("$build_dir/$diff_log", "codesize diff log");
    }
    
    #
    # Extract data.
    #
    my $z_data = extract_token_from_file("$build_dir/$test_log", "__codesize", ":");
    chomp($z_data);
    my $time = POSIX::strftime "%Y:%m:%d:%H:%M:%S", localtime;
    my $z_data_string = PrintSize($z_data,4);
    print_log "TinderboxPrint:" .
      "<a title=\"$testNameString: Code + data size of all shared libs & executables\" href=\"http://$Settings::results_server/graph/query.cgi?testname=$graphName&tbox=" .
        ::hostname() . "&autoscale=1&units=bytes&days=7&avg=0&showpoint=$time,$z_data\">$zee:$z_data_string" . "B</a>\n";
    
    if($Settings::TestsPhoneHome) {
      send_results_to_server($z_data, "--", $graphName, ::hostname());
    }
    
    my $zdiff_data = extract_token_from_file("$build_dir/$test_log", "__codesizeDiff", ":");
    chomp($zdiff_data);
    
    # Print out Zdiff if not zero.  Testing "zero" by looking for "+0 ".
    my $zdiff_sample = substr($zdiff_data,0,3);
    if (not ($zdiff_sample eq "+0 ")) {
      print_log "<a title=\"Change from last $zee value (+added/-subtracted)\" TinderboxPrint:" . $zee . "diff:$zdiff_data</a>\n";
    }
    
    # Testing only!  Moves the old log to some unique name for testing.
    # my $time = POSIX::strftime "%d%H%M%S", localtime;
    # rename("$build_dir/$old_log", "$build_dir/$old_log.$time");
    
    # Get ready for next cycle.
    rename("$build_dir/$new_log", "$build_dir/$old_log");
    
        
  } else {
    print_log "Error: $build_dir/$new_log not found.\n";
    $test_result = 'buildfailed';
  }
}



# Client-side JavaScript, DOM Core/HTML/Views, and Form Submission tests.
# Currently only available inside netscape firewall.
sub QATest {
    my ($test_name, $build_dir, $binary_dir, $args) = @_;
    my $binary_log = "$build_dir/$test_name.log";
    my $url = "http://geckoqa.mcom.com/ngdriver/cgi-bin/ngdriver.cgi?findsuites=suites&tbox=1";
    
    # Settle OS.
    run_system_cmd("sync; sleep 5", 35);
    
    my $rv;

    $rv = AliveTest("QATest_raw", $build_dir,
                    [@$args, $url],
                    $Settings::QATestTimeout);
    
    # XXXX testing.  -mcafee
    $rv = 'success';


    # Post-process log of test output.
    my $mode = "express"; 
    open QATEST, "perl $build_dir/../qatest.pl $build_dir/QATest_raw.log $mode |"
      or die "Unable to run qatest.pl";
    my $qatest_html = "";
    while (<QATEST>) {
      chomp;
      #$_ =~ s/\"/&quot;/g;  #### it doesn't like this line
      # $_ =~ s/\012//g;
      ### $_ =~ s/\s+\S/ /g;  # compress whitespace.
      $qatest_html .= $_;
    }
    close QATEST;
    print_log "\n";

    # This works.
    #$qatest_html = "<table border=2 cellspacing=0><tr%20valign=top><td>&nbsp;</td><td>Passed</td><td>Failed</td><td>Total</td><td>Died</td><td>%&nbsp;Passed</td><td>%&nbsp;Failed</td></tr><tr%20valign=top><td>DHTML</a></td><td>9</td><td>0</td><td>9</td><td>0</td><td>100</td><td>0</td></tr><tr%20valign=top><td>DOM&nbsp;VIEWS</a></td><td>2</td><td>0</td><td>2</td><td>0</td><td>100</td><td>0</td></tr><tr%20valign=top><td>Total:</td><td>11</td><td>0</td><td>11</td><td>0</td><td>100</td><td>0</td></tr></table>";

    # Testing output
    open TEST_OUTPUT, ">qatest_out.log";
    print TEST_OUTPUT $qatest_html;
    close TEST_OUTPUT;

    print_log "TinderboxPrint:<a href=\"javascript:var&nbsp;newwin;newwin=window.open(&quot;&quot;,&quot;&quot;,&quot;menubar=no,resizable=yes,height=150,width=500&quot;);var&nbsp;newdoc=newwin.document;newdoc.write('$qatest_html');newdoc.close();\">QA</a>\n";

    return $rv;  # Hard-coded for now.
}


# Startup performance test.  Time how fast it takes the browser
# to start up.  Some help from John Morrison to get this going.
#
# Needs user_pref("browser.dom.window.dump.enabled", 1);
# (or CPPFLAGS=-DMOZ_ENABLE_JS_DUMP in mozconfig since we
# don't have profiles for tbox right now.)
#
# $startup_url needs ?begin=<time> dynamically inserted.
#
sub StartupPerformanceTest {
  my ($test_name, $binary, $build_dir, $startup_test_args, $startup_url) = @_;
  
  my $i;
  my $startuptime;         # Startup time in ms.
  my $agg_startuptime = 0; # Aggregate startup time.
  my $startup_count   = 0; # Number of successful runs.
  my $avg_startuptime = 0; # Average startup time.
  my @times;
  my $startup_test_result = 'success';
  
  for($i=0; $i<10; $i++) {
    # Settle OS.
    run_system_cmd("sync; sleep 5", 35);
    
    # Generate URL of form file:///<path>/startup-test.html?begin=986869495000
    # Where begin value is current time.
    my ($time, $url, $cwd);
    
    #
    # Test for Time::HiRes and report the time.
    $time = Time::PossiblyHiRes::getTime();
    
    $cwd = get_system_cwd();
    print "cwd = $cwd\n";
    $url  = "$startup_url?begin=$time";
    
    print "url = $url\n";
    
    # Then load startup-test.html, which will pull off the begin argument
    # and compare it to the current time to compute startup time.
    # Since we are iterating here, save off logs as StartupPerformanceTest-0,1,2...
    #
    # -P $Settings::MozProfileName added 3% to startup time, assume one profile
    # and get the 3% back. (http://bugzilla.mozilla.org/show_bug.cgi?id=112767)
    #
    if($startup_test_result eq 'success') {
      $startuptime =
        AliveTestReturnToken("StartupPerformanceTest-$i",
                             $build_dir,
                             [$binary, @$startup_test_args, $url],
                             $Settings::StartupPerformanceTestTimeout,
                             "__startuptime",
                             ",");
    } else {
      print "Startup test failed.\n";
    }
    
    if($startuptime) {
      $startup_test_result = 'success';
      
      # Add our test to the total.
      $startup_count++;
      $agg_startuptime += $startuptime;
      
      # Keep track of the results in an array.
      push(@times, $startuptime);
    } else {
      $startup_test_result = 'testfailed';
    }
    
  } # for loop
  
  if($startup_test_result eq 'success') {
    print_log "\nSummary for startup test:\n";
    
    # Print startup times.
    chomp(@times);
    my $times_string = join(" ", @times);
    print_log "times = [$times_string]\n";
    
    # Figure out the average startup time.
    $avg_startuptime = $agg_startuptime / $startup_count;
    print_log "Average startup time: $avg_startuptime\n";
    
    my $min_startuptime = min(@times);
    print_log "Minimum startup time: $min_startuptime\n";
    
    my $ts_prefix = "";
    if($Settings::BinaryName eq "TestGtkEmbed") {
      $ts_prefix = "m";
    }

    my $time = POSIX::strftime "%Y:%m:%d:%H:%M:%S", localtime;
    my $print_string = "\n\nTinderboxPrint:<a title=\"Best startup time out of 10 startups\"href=\"http://$Settings::results_server/graph/query.cgi?testname=startup&tbox="
      . ::hostname() . "&autoscale=1&days=7&avg=1&showpoint=$time,$min_startuptime\">" . $ts_prefix . "Ts:" . $min_startuptime . "ms</a>\n\n";
    print_log "$print_string";
    
    # Report data back to server
    if($Settings::TestsPhoneHome) {
      print_log "phonehome = 1\n";
      send_results_to_server($min_startuptime, $times_string,
                             "startup", ::hostname());
    } 
  }

  return $startup_test_result;
}


# Page loader/cycling mechanism (mozilla -f option):
# If you are building optimized, you need to add
#   --enable-logrefcnt
# to turn the pageloader code on.  These are on by default for debug.
#
sub BloatTest {
    my ($binary, $build_dir, $bloat_args, $bloatdiff_label, $timeout_secs) = @_;
    my $binary_basename = File::Basename::basename($binary);
    my $binary_dir = File::Basename::dirname($binary);
    my $binary_log = "$build_dir/bloat-cur.log";
    my $old_binary_log = "$build_dir/bloat-prev.log";
    local $_;

    rename($binary_log, $old_binary_log);

    unless (-e "$binary_dir/bloaturls.txt") {
        print_log "Error: bloaturls.txt does not exist.\n";
        return 'testfailed';
    }

    my $platform = $Settings::OS =~ /^WIN/ ? 'windows' : 'unix';
    # If on Windows, make sure the urls file has dos lineendings, or
    # mozilla won't parse the file correctly
    if ($platform eq 'windows') {
	my $bu = "$binary_dir/bloaturls.txt";
        open(IN,"$bu") || die ("$bu: $!\n");
        open(OUT,">$bu.new") || die ("$bu.new: $!\n");
        while (<IN>) {
	    if (! m/\r\n$/) {
                s/\n$/\r\n/;
            }
	    print OUT "$_";
        } 
        close(IN);
        close(OUT);
        File::Copy::move("$bu.new", "$bu") or die("move: $!\n");
    }

    $ENV{XPCOM_MEM_BLOAT_LOG} = 1; # Turn on ref counting to track leaks.

    # Build up binary command, look for profile.
    my @args = ($binary_basename);
    unless (($Settings::MozProfileName eq "") or 
            ($Settings::BinaryName eq "TestGtkEmbed")) {
        @args = (@args, "-P", $Settings::MozProfileName);
    }
    @args = (@args, @$bloat_args);

    my $result = run_cmd($build_dir, $binary_dir, \@args, $binary_log,
                         $timeout_secs);
    $ENV{XPCOM_MEM_BLOAT_LOG} = 0;
    delete $ENV{XPCOM_MEM_BLOAT_LOG};

    print_logfile($binary_log, "$bloatdiff_label bloat test");

    if ($result->{timed_out}) {
        print_log "Error: bloat test timed out after"
            ." $timeout_secs seconds.\n";
        return 'testfailed';
    } elsif ($result->{exit_value}) {
        print_test_errors($result, $binary_basename);
        return 'testfailed';
    }

    # Print out bloatdiff stats, also look for TOTAL line for leak/bloat #s.
    print_log "<a href=#bloat>\n######################## BLOAT STATISTICS\n";
    my $found_total_line = 0;
    my @total_line_array;
    open DIFF, "perl $build_dir/../bloatdiff.pl $build_dir/bloat-prev.log $binary_log $bloatdiff_label|"
        or die "Unable to run bloatdiff.pl";
    while (<DIFF>) {
        print_log $_;

        # Look for fist TOTAL line
        unless ($found_total_line) {
            if (/TOTAL/) {
                @total_line_array = split(" ", $_);
                $found_total_line = 1;
            }
        }
    }
    close DIFF;
    print_log "######################## END BLOAT STATISTICS\n</a>\n";


    # Scrape for leak/bloat totals from TOTAL line
    # TOTAL 23 0% 876224
    my $leaks = $total_line_array[1];
    my $bloat = $total_line_array[3];

    print_log "leaks = $leaks\n";
    print_log "bloat = $bloat\n";

    # Figure out what the label prefix is.
    my $label_prefix = "";
    my $testname_prefix = "";
    unless($bloatdiff_label eq "") {
        $label_prefix = "$bloatdiff_label ";
        $testname_prefix = "$bloatdiff_label" . "_";
    }

    # Figure out testnames to send to server
    my $leaks_testname = "refcnt_leaks";
    my $bloat_testname = "refcnt_bloat";
    unless($bloatdiff_label eq "") {
        $leaks_testname = $testname_prefix . "refcnt_leaks";
        $bloat_testname = $testname_prefix . "refcnt_bloat";
    }

    # Figure out testname labels
    my $leaks_testname_label = "refcnt Leaks";
    my $bloat_testname_label = "refcnt Bloat";
    unless($bloatdiff_label eq "") {
        $leaks_testname_label = $label_prefix . $leaks_testname_label;
        $bloat_testname_label = $label_prefix . $bloat_testname_label;
    }

    my $embed_prefix = "";
    if($Settings::BinaryName eq "TestGtkEmbed") {
      $embed_prefix = "m";
    }

    if($Settings::TestsPhoneHome) {
        # Generate and print tbox output strings for leak, bloat.
        my $leaks_string = "\n\nTinderboxPrint:<a title=\"" . $leaks_testname_label . "\"href=\"http://$Settings::results_server/graph/query.cgi?testname=" . $leaks_testname . "&units=bytes&tbox=" . ::hostname() . "&autoscale=1&days=7&avg=1\">" . $label_prefix . $embed_prefix . "RLk:" . PrintSize($leaks,3) . "B</a>\n\n";
        print_log $leaks_string;

        # Report numbers to server.
        send_results_to_server($leaks, "--", $leaks_testname, ::hostname() );
    } else {
        print_log "TinderboxPrint:" . $label_prefix . $embed_prefix . "RLk:<a title=\"" . $leaks_testname_label . "\">" . PrintSize($leaks,3) . "B</a>\n\n";
    }

    return 'success';
}

# Page loader (-f option):
# If you are building optimized, you need to add
#   --enable-trace-malloc --enable-perf-metrics
# to turn the pageloader code on.  If you are building debug you only
# need
#   --enable-trace-malloc
#

sub ReadLeakstatsLog($) {
    my ($filename) = @_;
    my $leaks = 0;
    my $leaked_allocs = 0;
    my $mhs = 0;
    my $bytes = 0;
    my $allocs = 0;

    open LEAKSTATS, "$filename"
        or die "unable to open $filename";
    while (<LEAKSTATS>) {
        chop;
        my $line = $_;
        if ($line =~ /Leaks: (\d+) bytes, (\d+) allocations/) {
            $leaks = $1;
            $leaked_allocs = $2;
        } elsif ($line =~ /Maximum Heap Size: (\d+) bytes/) {
            $mhs = $1;
        } elsif ($line =~ /(\d+) bytes were allocated in (\d+) allocations./) {
            $bytes = $1;
            $allocs = $2;
        }
    }

    return {
        'leaks' => $leaks,
        'leaked_allocs' => $leaked_allocs,
        'mhs' => $mhs,
        'bytes' => $bytes,
        'allocs' => $allocs
        };
}

sub PercentChange($$) {
    my ($old, $new) = @_;
    if ($old == 0) {
        return 0;
    }
    return ($new - $old) / $old;
}

# Print a value of bytes out in a reasonable
# KB, MB, or GB form.  Sig figs should probably
# be 3, 4, or 5 for most purposes here.  This used
# to default to 3 sig figs, but I wanted 4 so I
# generalized here.  -mcafee
#
# Usage: PrintSize(valueAsInteger, numSigFigs)
# 
sub PrintSize($$) {

    # print a number with 3 significant figures
    sub PrintNum($$) {
        my ($num, $sigs) = @_;
        my $rv;

        # Figure out how many decimal places to show.
        # Only doing a few cases here, for normal range
        # of test numbers.

        # Handle zero case first.
        if ($num == 0) {
          $rv = "0";
        } elsif ($num < 10**($sigs-5)) {
          $rv = sprintf "%.5f", ($num);
        } elsif ($num < 10**($sigs-4)) {
          $rv = sprintf "%.4f", ($num);
        } elsif ($num < 10**($sigs-3)) {
          $rv = sprintf "%.3f", ($num);
        } elsif ($num < 10**($sigs-2)) {
          $rv = sprintf "%.2f", ($num);
        } elsif ($num < 10**($sigs-1)) {
          $rv = sprintf "%.1f", ($num);
        } else {
          $rv = sprintf "%d", ($num);
        }

    }

    my ($size, $sigfigs) = @_;

    # 1K = 1024, previously this was approximated as 1000.
    my $rv;
    if ($size > 1073741824) {  # 1024^3
        $rv = PrintNum($size / 1073741824.0, $sigfigs) . "G";
    } elsif ($size > 1048576) {  # 1024^2
        $rv = PrintNum($size / 1048576.0, $sigfigs) . "M";
    } elsif ($size > 1024) {
        $rv = PrintNum($size / 1024.0, $sigfigs) . "K";
    } else {
        $rv = PrintNum($size, $sigfigs);
    }
}

sub BloatTest2 {
    my ($binary, $build_dir, $timeout_secs) = @_;
    my $binary_basename = File::Basename::basename($binary);
    my $binary_dir = File::Basename::dirname($binary);
    my $PERL = $^X;
    if ($Settings::OS =~ /^WIN/ && $build_dir !~ m/^.:\//) {
        chomp($build_dir = `cygpath -w $build_dir`);
        $build_dir =~ s/\\/\//g;
        $PERL = "perl";
    }
    my $binary_log = "$build_dir/bloattest2.log";
    my $malloc_log = "$build_dir/malloc.log";
    my $sdleak_log = "$build_dir/sdleak.log";
    my $old_sdleak_log = "$build_dir/sdleak.log.old";
    my $leakstats_log = "$build_dir/leakstats.log";
    my $old_leakstats_log = "$build_dir/leakstats.log.old";
    my $sdleak_diff_log = "$build_dir/sdleak.diff.log";
    local $_;

    unless (-e "$binary_dir/bloaturls.txt") {
        print_log "Error: bloaturls.txt does not exist.\n";
        return 'testfailed';
    }

    my $platform = $Settings::OS =~ /^WIN/ ? 'windows' : 'unix';
    # If on Windows, make sure the urls file has dos lineendings, or
    # mozilla won't parse the file correctly
    if ($platform eq 'windows') {
	my $bu = "$binary_dir/bloaturls.txt";
        open(IN,"$bu") || die ("$bu: $!\n");
        open(OUT,">$bu.new") || die ("$bu.new: $!\n");
        while (<IN>) {
	    if (! m/\r\n$/) {
                s/\n$/\r\n/;
            }
	    print OUT "$_";
        } 
        close(IN);
        close(OUT);
        File::Copy::move("$bu.new", "$bu") or die("move: $!\n");
    }

    rename($sdleak_log, $old_sdleak_log);

    my @args;
    if($Settings::BinaryName eq "TestGtkEmbed" ||
       $Settings::BinaryName =~ /^firefox/) {
      @args = ($binary_basename, "-P", $Settings::MozProfileName,
               "resource:///res/bloatcycle.html",
               "--trace-malloc", $malloc_log);
    } else {
      @args = ($binary_basename, "-P", $Settings::MozProfileName,
               "-f", "bloaturls.txt",
               "--trace-malloc", $malloc_log);
    }

    # win32 builds crash on multiple runs when --shutdown-leaks is used
    @args = (@args, "--shutdown-leaks", $sdleak_log) unless $Settings::OS =~ /^WIN/;
    my $result = run_cmd($build_dir, $binary_dir, \@args, $binary_log,
                         $timeout_secs);

    print_logfile($binary_log, "trace-malloc bloat test");

    if ($result->{timed_out}) {
        print_log "Error: bloat test timed out after"
            ." $timeout_secs seconds.\n";
        return 'testfailed';
    } elsif ($result->{exit_value}) {
        print_test_errors($result, $binary_basename);
        return 'testfailed';
    }

    rename($leakstats_log, $old_leakstats_log);

    if ($Settings::OS =~ /^WIN/) {
        @args = ("leakstats", $malloc_log);
    } else {
        @args = ("run-mozilla.sh", "./leakstats", $malloc_log);
    }
    $result = run_cmd($build_dir, $binary_dir, \@args, $leakstats_log,
                      $timeout_secs);
    print_logfile($leakstats_log, "trace-malloc bloat test: leakstats");

    my $newstats = ReadLeakstatsLog($leakstats_log);
    my $oldstats;
    if (-e $old_leakstats_log) {
        $oldstats = ReadLeakstatsLog($old_leakstats_log);
    } else {
        $oldstats = $newstats;
    }
    my $leakchange = PercentChange($oldstats->{'leaks'}, $newstats->{'leaks'});
    my $mhschange = PercentChange($oldstats->{'mhs'}, $newstats->{'mhs'});

    my $leaks_testname_label   = "Leaks: total bytes 'malloc'ed and not 'free'd";
    my $maxheap_testname_label = "Maximum Heap: max (bytes 'malloc'ed - bytes 'free'd) over run";
    my $allocs_testname_label  = "Allocations: number of calls to 'malloc' and friends";

    my $embed_prefix = "";
    if($Settings::BinaryName eq "TestGtkEmbed") {
      $embed_prefix = "m";
    }

    if($Settings::TestsPhoneHome) {
        my $leaks_testname       = "trace_malloc_leaks";
        my $leaks_string = "\n\nTinderboxPrint:<a title=\"" . $leaks_testname_label . "\"href=\"http://$Settings::results_server/graph/query.cgi?testname=" . $leaks_testname . "&units=bytes&tbox=" . ::hostname() . "&autoscale=1&days=7&avg=1\">" . $embed_prefix . "Lk:" . PrintSize($newstats->{'leaks'},3) . "B</a>\n\n";
        print_log $leaks_string;

        my $maxheap_testname       = "trace_malloc_maxheap";
        my $maxheap_string = "\n\nTinderboxPrint:<a title=\"" . $maxheap_testname_label . "\"href=\"http://$Settings::results_server/graph/query.cgi?testname=" . $maxheap_testname . "&units=bytes&tbox=" . ::hostname() . "&autoscale=1&days=7&avg=1\">" . $embed_prefix . "MH:" . PrintSize($newstats->{'mhs'},3) . "B</a>\n\n";
        print_log $maxheap_string;

        my $allocs_testname       = "trace_malloc_allocs";
        my $allocs_string = "\n\nTinderboxPrint:<a title=\"" . $allocs_testname_label . "\"href=\"http://$Settings::results_server/graph/query.cgi?testname=" . $allocs_testname . "&units=bytes&tbox=" . ::hostname() . "&autoscale=1&days=7&avg=1\">" . $embed_prefix . "A:" . PrintSize($newstats->{'allocs'},3) . "</a>\n\n";
        print_log $allocs_string;

        # Send results to server.
        send_results_to_server($newstats->{'leaks'},  "--", $leaks_testname,   ::hostname() );
        send_results_to_server($newstats->{'mhs'},    "--", $maxheap_testname, ::hostname() );
        send_results_to_server($newstats->{'allocs'}, "--", $allocs_testname,  ::hostname() );

    } else {
        print_log "TinderboxPrint:<abbr title=\"$leaks_testname_label\">Lk</abbr>:" . PrintSize($newstats->{'leaks'},3) . "B\n";
        print_log "TinderboxPrint:<abbr title=\"$maxheap_testname_label\">MH</abbr>:" . PrintSize($newstats->{'mhs'},3) . "B\n";
        print_log "TinderboxPrint:<abbr title=\"$allocs_testname_label\">A</abbr>:" . PrintSize($newstats->{'allocs'},3) . "\n";
    }

    if (-e $old_sdleak_log && -e $sdleak_log) {
        print_logfile($old_leakstats_log, "previous run of trace-malloc bloat test leakstats");
        @args = ($PERL, "$build_dir/mozilla/tools/trace-malloc/diffbloatdump.pl", "--depth=15", $old_sdleak_log, $sdleak_log);
        $result = run_cmd($build_dir, $binary_dir, \@args, $sdleak_diff_log,
                          $timeout_secs);
        print_logfile($sdleak_diff_log, "trace-malloc leak stats differences");
    }

    return 'success';
}


# Need to end with a true value, (since we're using "require").
1;
