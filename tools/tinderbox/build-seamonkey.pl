#!/usr/bin/perl -w

require 5.000;

# This script has split some functions off into a util
# script so they can be re-used by other scripts.
require "build-seamonkey-util.pl";

use strict;
use POSIX qw(sys_wait_h strftime);
use Cwd;
use File::Basename; # for basename();

$::Version = '$Revision: 1.73 $ ';

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
  --mozconfig FILENAME   Provide a mozconfig file for client.mk to use.
  --version              Print the version number (same as cvs revision).
  --help
More details:
  To get started, run '$0 --example-config'.
END_USAGE
}

{
    InitVars();
    my $args = ParseArgs();
    ConditionalArgs();
    TinderboxClientUtils::GetSystemInfo();
    TinderboxClientUtils::LoadConfig();
    ApplyArgs($args); # Apply command-line arguments after the config file.
    TinderboxClientUtils::SetupEnv();
    TinderboxClientUtils::SetupPath();
    BuildIt();
}

# End of main
#======================================================================

sub InitVars {
    local $_;
    for (@ARGV) {
        # Save DATA section for printing the example.
        return if /^--example-config$/;
    }
    while (<DATA>) {
        s/^\s*\$(\S*)/\$Settings::$1/;
        eval;
    }
}

sub PrintExampleConfig {
    local $_;
    print "#- tinder-config.pl - Tinderbox configuration file.\n";
    print "#-    Uncomment the variables you need to set.\n";
    print "#-    The default values are the same as the commented variables.\n";
    print "\n";
    
    while (<DATA>) {
        s/^\$/\#\$/;
        print;
    }
}

sub ParseArgs {
    PrintUsage() if $#ARGV == -1;

    my $args = {};
    while (my $arg = shift @ARGV) {
        $Settings::BuildDepend = 0, next if $arg eq '--clobber';
        $Settings::BuildDepend = 1, next if $arg eq '--depend';
        PrintExampleConfig(), exit if $arg eq '--example-config';
        PrintUsage(), exit if $arg eq '--help' or $arg eq '-h';
        $args->{ReportStatus} = 0, next if $arg eq '--noreport';
        $args->{ReportFinalStatus} = 0, next if $arg eq '--nofinalreport';
        $args->{RunTest} = 0, next if $arg eq '--notest';
        $args->{TestOnly} = 1, next if $arg eq '--testonly';
        $args->{BuildOnce} = 1, next if $arg eq '--once';
        $args->{UseTimeStamp} = 0, next if $arg eq '--notimestamp';
        
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
        }
        elsif ($arg eq '--version' or $arg eq '-v') {
            die "$0: version" . substr($::Version,9,6) . "\n";
        } else {
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

sub ConditionalArgs {
    $ENV{CVSROOT} = ":pserver:$ENV{USER}%netscape.com\@cvs.mozilla.org:/cvsroot";
    $Settings::CVSCO .= " -r $Settings::BuildTag" 
      unless $Settings::BuildTag eq '';
}

sub print_log {
    my ($text) = @_;
    print LOG $text;
    print $text;
}

sub run_shell_command {
    my ($shell_command) = @_;
    local $_;

    chomp($shell_command);
    print_log "$shell_command\n";
    open CMD, "$shell_command 2>&1|" or die "open: $!\n";
    print_log $_ while <CMD>;
    close CMD;
}

sub adjust_start_time {
    # Allows the start time to match up with the update times of a mirror.
    my ($start_time) = @_;

    # Since we are not pulling for cvs-mirror anymore, just round times
    # to 1 minute intervals to make them nice and even.
    my $cycle = 1 * 60;    # Updates every 1 minutes.
    my $begin = 0 * 60;    # Starts 0 minutes after the hour.
    my $lag = 0 * 60;      # Takes 0 minute to update.
    return int(($start_time - $begin - $lag) / $cycle) * $cycle + $begin;
}

sub BuildIt {
    # $Settings::DirName is set in buildl-seamonkey-utils.pl
    mkdir $Settings::DirName, 0777;
    chdir $Settings::DirName or die "Couldn't enter $Settings::DirName";
    
    my $build_dir = getcwd();
    my $binary_basename = "$Settings::BinaryName";
    my $binary_dir = "$build_dir/$Settings::Topsrcdir/dist/bin";
    my $full_binary_name = "$binary_dir/$binary_basename";
    my $exit_early = 0;
    my $start_time = 0;

    # Bypass profile at startup.
    $ENV{MOZ_BYPASS_PROFILE_AT_STARTUP} = 1;
    
    print_log "Starting dir is : $build_dir\n";
    
    while (not $exit_early) {
        # $BuildSleep is the minimum amount of time a build is allowed to take.
        # It prevents sending too many messages to the tinderbox server when
        # something is broken.
        my $sleep_time = ($Settings::BuildSleep * 60) - (time - $start_time);
        if (not $Settings::TestOnly and $sleep_time > 0) {
            print_log "\n\nSleeping $sleep_time seconds ...\n";
            sleep $sleep_time;
        }
        $start_time = time();
        
        my $cvsco = '';
        if ($Settings::UseTimeStamp) {
            $start_time = adjust_start_time($start_time);
            my $time_str = strftime("%m/%d/%Y %H:%M", localtime($start_time));
            $ENV{MOZ_CO_DATE} = "$time_str";
            $cvsco = "$Settings::CVSCO -D '$time_str'";
        } else {
            $cvsco = "$Settings::CVSCO -A";
        }
        
        mail_build_started_message($start_time) if $Settings::ReportStatus;
        
        chdir $build_dir;
        my $logfile = "$Settings::DirName.log";
        print "Opening $logfile\n";
        open LOG, ">$logfile"
          or die "Cannot open logfile, $logfile: $?\n";
        print_log "current dir is -- " . $ENV{HOST} . ":$build_dir\n";
        print_log "Build Administrator is $Settings::BuildAdministrator\n";
        
        PrintEnv();
        
        unless (-e "mozilla/client.mk") {
            run_shell_command "$Settings::CVS $cvsco mozilla/client.mk";
        }
        
        chdir $Settings::Topsrcdir or die "chdir $Settings::Topsrcdir: $!\n";
        
        DeleteBinary($full_binary_name); # Delete the binary before rebuilding

        # Build it
        unless ($Settings::TestOnly) { # Do not build if testing smoke tests.
            my $targets = '';
            $targets = 'checkout realclean build' unless $Settings::BuildDepend;
            run_shell_command "$Settings::Make -f client.mk $targets";
        }
        
        my $build_status;
        if (BinaryExists($full_binary_name)) {
            if ($Settings::RunTest) {
                $build_status = run_tests($full_binary_name, $build_dir);
            } else {
                print_log "$binary_basename binary exists, build successful.\n";
                print_log "Skipping test.\n";
                $build_status = 'success';
            }
        } else {
            print_log "Error: $binary_basename binary missing, build FAILED\n";
            $exit_early++ if $Settings::TestOnly;
            $build_status = 'busted';
        }
        
        close LOG;
        chdir $build_dir;
        
        mail_build_finished_message($start_time, $build_status, $logfile)
          if $Settings::ReportStatus;

        $exit_early++ if $Settings::BuildOnce;
    }
}

sub mail_build_started_message {
    my ($start_time) = @_;
    
    open LOG, "|$Settings::mail $Settings::Tinderbox_server";
    
    PrintUsage() if $Settings::BuildTree =~ /^\s+$/i;

    print_log "\n";
    print_log "tinderbox: tree: $Settings::BuildTree\n";
    print_log "tinderbox: builddate: $start_time\n";
    print_log "tinderbox: status: building\n";
    print_log "tinderbox: build: $Settings::BuildName\n";
    print_log "tinderbox: errorparser: unix\n";
    print_log "tinderbox: buildfamily: unix\n";
    print_log "tinderbox: version: $::Version\n";
    print_log "tinderbox: END\n";
    print_log "\n";
    
    close LOG;
}

sub mail_build_finished_message {
    my ($start_time, $build_status, $logfile) = @_;

    # Rewrite LOG to OUTLOG, shortening lines.
    open OUTLOG, ">$logfile.last" or die "Unable to open logfile, $logfile: $!";
        
    # Put the status at the top of the log, so the server will not
    # have to search through the entire log to find it.
    print OUTLOG "tinderbox: tree: $Settings::BuildTree\n";
    print OUTLOG "tinderbox: builddate: $start_time\n";
    print OUTLOG "tinderbox: status: $build_status\n";
    print OUTLOG "tinderbox: build: $Settings::BuildName\n";
    print OUTLOG "tinderbox: errorparser: unix\n";
    print OUTLOG "tinderbox: buildfamily: unix\n";
    print OUTLOG "tinderbox: version: $::Version\n";
    print OUTLOG "tinderbox: END\n";            
    
    # Make sendmail happy.
    # Split lines longer than 1000 charaters into 1000 character lines.
    # If any line is a dot on a line by itself, replace it with a blank
    # line. This prevents cases where a <cr>.<cr> occurs in the log file. 
    # Sendmail interprets that as the end of the mail, and truncates the
    # log before it gets to Tinderbox.  (terry weismann, chris yeh)
    
    open LOG, "$logfile" or die "Couldn't open logfile, $logfile: $!";
    while (<LOG>) {
        my $length = length($_);
        for (my $offset = 0; $offset < $length ; $offset += 1000) {
            my $chars_left = $length - $offset;
            my $output_length = $chars_left < 1000 ? $chars_left : 1000;
            my $output = substr $_, $offset, $output_length;
            $output =~ s/^\.$//g;
            $output =~ s/\n//g;
            print OUTLOG "$output\n";
        }
    }
    close OUTLOG;
    close LOG;
    unlink($logfile);
    
    if ($Settings::ReportStatus and $Settings::ReportFinalStatus) {
        system "$Settings::mail $Settings::Tinderbox_server "
              ." < $logfile.last";
    } 
}
    
sub run_tests {
    my ($binary, $build_dir) = @_;
    my $binary_basename = basename($binary);
    my $binary_dir = dirname($binary);

    # Mozilla alive test
    print_log "Running AliveTest ...\n";
    my $build_status = RunAliveTest($build_dir, $binary, 60);
    return $build_status if $build_status ne 'success';

    # Viewer alive test
    if ($Settings::ViewerTest) {
        print_log "Running ViewerTest ...\n";
        $build_status = RunAliveTest($build_dir, "$binary_dir/viewer", 60);
        return $build_status if $build_status ne 'success';
    }
    
    # Bloat test
    if ($Settings::BloatStats or $Settings::BloatTest) {
        $build_status = RunBloatTest($binary, $build_dir);
        return $build_status if $build_status ne 'success';
    }
    
    # MailNews test
    # Needs the following security pref set:
    #   user_pref("signed.applets.codebase_principal_support",true);
    # First time around, you get two dialogs, they set this pref:
    #   user_pref("security.principal.X0","[Codebase http://www.mozilla.org/quality/mailnews/APITest.html] UniversalBrowserRead=1 UniversalXPConnect=1");
    if ($Settings::MailNewsTest) {
        print_log "Running MailNewsTest ...\n";
        # Hack: testing some partial success string for now.
        $build_status = &RunFileBasedTest("MailNewsTest", $binary_dir,
            "$binary_basename"
               ." http://www.mozilla.org/quality/mailnews/APITest.html", 
            90, "MAILNEWS TEST: Passed", 1);
        return $build_status if $build_status ne 'success';
    }
    
    # Run Editor test.
    if ($Settings::EditorTest or $Settings::DomToTextConversionTest) {
        print_log "Running  DomToTextConversionTest ...\n";
        $build_status = &RunFileBasedTest("DomToTextConversionTest",
                                          $build_dir, $binary_dir,
                                         "TestOutSinks", 15,
                                         "FAILED", 0);
        return $build_status if $build_status ne 'success';
    }
    return $build_status;
}

sub BinaryExists {
    my ($binary) = @_;
    my ($binary_basename) = basename($binary);

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

sub DeleteBinary {
    my ($binary) = @_;
    my ($binary_basename) = basename($binary);

    if (BinaryExists($binary)) {
        # Don't delete binary if we're only running tests.
        unless ($Settings::TestOnly) {
            print_log "Deleting binary: $binary_basename\n";
            print_log "unlinking $binary\n";
            unlink $binary or print_log "Error: Unlinking $binary failed\n";
        }
    } else {
        print_log "No binary detected; none deleted.\n";
    }
}

sub PrintEnv {
    local $_;

    foreach my $key (sort keys %ENV) {
        print_log "$key=$ENV{$key}\n";
    }
    if (defined $ENV{MOZCONFIG} and -e $ENV{MOZCONFIG}) {
        print_log "-->mozconfig<----------------------------------------\n";
        open CONFIG, "$ENV{MOZCONFIG}";
        print_log $_ while <CONFIG>;
        close CONFIG;
        print_log "-->end mozconfig<----------------------------------------\n";
    }
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
    my ($filehandle, $token) = @_;
    local $_;
    
    while (<$filehandle>) {
        if (/$token/) {
            print "Found a '$token'!\n";
            return 1;
        }
    }
    return 0;
}

sub killer {
    &killproc($::pid);
}

sub killproc {
    my ($local_pid) = @_;
    my $status;
    
    # try to kill 3 times, then try a kill -9
    for (my $ii=0; $ii < 3; $ii++) {
        kill('TERM',$local_pid);
        # give it 3 seconds to actually die
        sleep 3;
        $status = waitpid($local_pid, WNOHANG());
        last if $status != 0;
    }
    return $status;
}

# Start up Mozilla, test passes if Mozilla is still alive
# after $waittime (seconds).
#
sub RunAliveTest {
    my ($build_dir, $binary, $testTimeoutSec) = @_;
    my $status = 0;
    my $binary_basename = basename($binary);
    my $binary_dir = dirname($binary);
    my $binary_log = "$build_dir/runlog";

    die "Error: RunAliveTest: No binary given" unless defined $binary;
    
    $ENV{LD_LIBRARY_PATH} = $binary_dir;
    $ENV{MOZILLA_FIVE_HOME} = $binary_dir;

    $::pid = fork; # Fork off a child process.
    
    unless ($::pid) { # child
        chdir $binary_dir;
        $ENV{HOME} = $build_dir;
        open STDOUT, ">$binary_log";
        open STDERR, ">&STDOUT";
        select STDOUT; $| = 1;  # make STDOUT unbuffered
        select STDERR; $| = 1;  # make STDERR unbuffered
        exec $binary_basename
          or die "Could not exec: $binary_basename from $binary_dir";
    }
    
    # Parent - Wait $testTimeoutSec seconds then check on child
    sleep $testTimeoutSec;
    $status = waitpid($::pid, WNOHANG());
    
    if ($status == 0) {
        print_log "$binary_basename quit AliveTest with status $status\n";
    } else {
        print_log "Error: $binary_basename has crashed or quit on the AliveTest.  Turn the tree orange now.\n";
        print_log "----------- failure output from $binary_basename for alive test --------------- \n";
        open READRUNLOG, "$binary_log";
        while (my $line = <READRUNLOG>) {
            print_log $line;
        }
        close READRUNLOG;
        print_log "--------------- End of AliveTest($binary_basename) Output -------------------- \n";
        return 'testfailed';
    }
    
    &killproc($::pid);
    
    print_log "----------- success output from $binary_basename for alive test --------------- \n";
    open READRUNLOG, "$binary_log";
    while (my $line = <READRUNLOG>) {
        print_log $line;
    }
    close READRUNLOG;
    print_log "--------------- End of AliveTest ($binary_basename) Output -------------------- \n";
    return 'success';
    
} # RunAliveTest

# Run a generic test that writes output to stdout, save that output to a
# file, parse the file looking for failure token and report status based
# on that.  A hack, but should be useful for many tests.
#
# test_name = Name of test we're gonna run, in dist/bin.
# testExecString = How to run the test
# testTimeoutSec = Timeout for hung tests, minimum test time.
# statusToken = What string to look for in test output to 
#     determine test status.
# statusTokenMeansPass = Default use of status token is to look for
#     failure string.  If this is set to 1, then invert logic to look for
#     success string.
#
# Note: I tried to merge this function with RunAliveTest(),
#       the process flow control got too confusing :(  -mcafee
#
sub RunFileBasedTest {
    my ($test_name, $build_dir, $binary_dir, $testExecString, $testTimeoutSec, 
        $statusToken, $statusTokenMeansPass) = @_;
    local $_;

    print_log "Running $testExecString\n";
    
    $ENV{LD_LIBRARY_PATH} = $binary_dir;
    $ENV{MOZILLA_FIVE_HOME} = $binary_dir;
    
    # Assume the app is the first argument in the execString.
    my ($binary_basename) = (split /\s+/, $testExecString)[0];
    my $binary = "$binary_dir/$binary_basename";
    my $binary_log = "$build_dir/$test_name.log";

    print_log "Binary = $binary\n";
    print_log "Binary Log = $binary_log\n";
    
    # Fork off a child process.
    $::pid = fork;
    
    unless ($::pid) { # child
        print_log "child\n";
        print_log "2:Binary = $binary\n";
        
        # The following set of lines makes stdout/stderr show up
        # in the tinderbox logs.
        $ENV{HOME} = $build_dir;
        open STDOUT, ">$binary_log";
        open STDERR, ">&STDOUT";
        select STDOUT; $| = 1;  # make STDOUT unbuffered
        select STDERR; $| = 1;  # make STDERR unbuffered
        
        # Timestamp when we're running the test.
        print_log `date`, "\n";
        
        if (-e $binary) {
            my $cmd = "$testExecString";
            print_log "$cmd\n";
            chdir($binary_dir);
            exec $cmd;
        } else {
            print_log "Error: cannot run $test_name\n";
        }
        die "Couldn't exec()";        
    } else {
        print "parent\n";
        print_log "parent\n";
    }
    
    # Set up a timer with a signal handler.
    $SIG{ALRM} = \&killer;
    
    # Wait $testTimeoutSec seconds, then kill the process if it's still alive.
    alarm $testTimeoutSec;
    print "testTimeoutSec = $testTimeoutSec\n";
    print_log "testTimeoutSec = $testTimeoutSec\n";
    
    my $status = waitpid($::pid, 0);
    
    # Back to parent.
    
    # Clear the alarm so we don't kill the next test!
    alarm 0;
    
    # Determine proper status, look in log file for failure token.
    # XXX: What if test is supposed to exit, but crashes?  -mcafee
    
    print_log "$test_name exited with status $status\n";
    
    if ($status < 0) {
        print_log "$test_name timed out and needed to be killed.\n";
    } else {
        print_log "$test_name completed on its own, before the timeout.\n";
    }
    
    open TESTLOG, "<$binary_log" or die "Can't open $!";
    # Return 1 if we find statusToken in output.
    $status = file_has_token(*TESTLOG, $statusToken);
    
    if($status) {
        print_log "found statusToken $statusToken!\n";
    } else {
        print_log "statusToken $statusToken not found\n";
    }
    close TESTLOG;
    
    # If we're using success string, invert status logic.
    if($statusTokenMeansPass == 1) {
        # Invert $status.  This is probably sloppy perl, help me!
        if($status == 0) {
            $status = 1;
        } else {
            $status = 0;
        }
    }
    
    # Write test output to log.
    if ($status == 0) {
        print_log "----------- success output from $test_name test --------------- \n";
    } else {
        print_log "Error: $test_name has failed.  Turn the tree orange now.\n";
        print_log "----------- failure output from $test_name test --------------- \n";
    }
    
    # Parse the test log, dumping lines into tinderbox log.
    open READRUNLOG, "$binary_log";
    print_log $_ while <READRUNLOG>;
    close READRUNLOG;
    print_log "--------------- End of $test_name Output -------------------- \n";
    
    if ($status == 0) {
        return 'success';
    } else {
        return 'testfailed';
    }
} # RunFileBasedTest


sub RunBloatTest {
    my ($binary, $build_dir) = @_;
    my $binary_basename = basename($binary);
    my $binary_dir = dirname($binary);
    my $binary_log = "$build_dir/bloat-cur.log";
    my $old_binary_log = "$build_dir/bloat-prev.log";
    my $status = 0;
    local $_;

    print_log "Running BloatTest ...\n";
    
    $ENV{LD_LIBRARY_PATH} = $binary_dir;
    $ENV{MOZILLA_FIVE_HOME} = $binary_dir;
    
    # Turn on ref counting to track leaks (bloaty tool).
    $ENV{XPCOM_MEM_BLOAT_LOG} = "1"; 
    
    rename($binary_log, $old_binary_log);
    
    # Fork off a child process.
    $::pid = fork;
    
    unless ($::pid) { # child
        chdir $binary_dir;
        
        $ENV{HOME} = $build_dir;
        open STDOUT, ">$binary_log";
        open STDERR, ">&STDOUT";
        select STDOUT; $| = 1;  # make STDOUT unbuffered
        select STDERR; $| = 1;  # make STDERR unbuffered
        
        if (-e "bloaturls.txt") {
            my $cmd = "$binary_basename -f bloaturls.txt";
            print_log $cmd;
            exec $cmd;
        } else {
            print_log "Error: bloaturls.txt does not exist.\n";
        }
        die "Could not exec()";
    }
    
    $SIG{ALRM} = \&killer; # Set up a timer with a signal handler.
    
    alarm 120; # Wait 120 seconds, then kill the process if it's still alive.
    $status = waitpid($::pid, 0);
    alarm 0; # Clear the alarm to avoid killing next test.
    
    print_log "$binary_basename quit bloat test with status $status\n";
    if ($status <= 0) {
        print_log "Error: bloat test: $binary_basename has crashed or quit.\n";
        print_log "Turn the tree orange now.\n";
        print_log "----------- BloatTest Output --------------- \n";
        open READRUNLOG, "$binary_log";
        print_log $_ while <READRUNLOG>;
        close READRUNLOG;
        print_log "--------------- End of BloatTest Output ---------------- \n";
        
        # HACK.  Clobber isn't reporting bloat status properly,
        # only turn tree orange for depend build.  This has
        # been filed as bug 22052.  -mcafee
        if ($Settings::BuildDepend) {
            return 'testfailed';
        } else {
            return 'success';
        }
    }
    print_log "<a href=#bloat>\n######################## BLOAT STATISTICS\n";
    
    open DIFF, "$build_dir/../bloatdiff.pl $build_dir/bloat-prev.log $binary_log|"
      or die "Unable to run bloatdiff.pl";
    print_log $_ while <DIFF>;
    close DIFF;
    print_log "######################## END BLOAT STATISTICS\n</a>\n";
    
    print_log "----- success output from $binary_basename for BloatTest ----- \n";
    open READRUNLOG, "$binary_log";
    print_log $_ while <READRUNLOG>;
    close READRUNLOG;
    print_log "--------------- End of BloatTest Output -------------------- \n";
    return 'success';
}


__END__
#- PLEASE FILL THIS IN WITH YOUR PROPER EMAIL ADDRESS
$BuildAdministrator = "$ENV{USER}\@$ENV{HOST}";

#- You'll need to change these to suit your machine's needs
$BaseDir       = '/builds/tinderbox/SeaMonkey';
$DisplayServer = ':0.0';

#- Default values of command-line opts
#-
$BuildDepend       = 1;      # Depend or Clobber
$ReportStatus      = 1;      # Send results to server, or not
$ReportFinalStatus = 1;      # Finer control over $ReportStatus.
$UseTimeStamp      = 1;      # Use the CVS 'pull-by-timestamp' option, or not
$BuildOnce         = 0;      # Build once, don't send results to server
$RunTest           = 1;      # Run the smoke tests on successful build, or not
$TestOnly          = 0;      # Only run tests, don't pull/build
# Extra tests
$BloatTest = 0;
$BloatStats = 0;
$DomToTextConversionTest = 0;
$EditorTest = 0;
$MailNewsTest = 0;
$ViewerTest = 0;
$MozConfigFileName = 'mozconfig';

#- Set these to what makes sense for your system
$Make          = 'gmake';       # Must be GNU make
$MakeOverrides = '';
$mail          = '/bin/mail';
$CVS           = 'cvs -q';
$CVSCO         = 'checkout -P';

#- Set these proper values for your tinderbox server
$Tinderbox_server = 'tinderbox-daemon@tinderbox.mozilla.org';

#-
#- The rest should not need to be changed
#-

#- Minimum wait period from start of build to start of next build in minutes.
$BuildSleep = 10;

#- Until you get the script working. When it works,
#- change to the tree you're actually building
$BuildTree  = 'MozillaTest'; 

$BuildName = '';
$BuildTag = '';
$BuildConfigDir = 'mozilla/config';
$Topsrcdir = 'mozilla';
$BinaryName = 'mozilla-bin';
$ShellOverride = ''; # Only used if the default shell is too stupid
$ConfigureArgs = '';
$ConfigureEnvArgs = '';
$Compiler = 'gcc';
$NSPRArgs = '';
$ShellOverride = '';
