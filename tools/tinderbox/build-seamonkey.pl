#!/usr/bin/perl -w

require 5.000;

# This script has split some functions off into a util
# script so they can be re-used by other scripts.
require "build-seamonkey-util.pl";

use strict;
use POSIX qw(sys_wait_h strftime);
use Cwd;
use File::Basename; # for basename();
use Config; # for $Config{sig_name} and $Config{sig_num}
$::Version = '$Revision: 1.76 $ ';

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
    my $test_result = AliveTest($build_dir, $binary, 45);

    # Viewer alive test
    if ($Settings::ViewerTest and $test_result eq 'success') {
        print_log "Running ViewerTest ...\n";
        $test_result = AliveTest($build_dir, "$binary_dir/viewer", 45);
    }
    
    # Bloat test
    if ($Settings::BloatStats or $Settings::BloatTest
        and $test_result eq 'success') {
        print_log "Running BloatTest ...\n";
        $test_result = BloatTest($binary, $build_dir);
    }
    
    # MailNews test needs this preference set:
    #   user_pref("signed.applets.codebase_principal_support",true);
    # First run gives two dialogs; they set this preference:
    #   user_pref("security.principal.X0","[Codebase http://www.mozilla.org/quality/mailnews/APITest.html] UniversalBrowserRead=1 UniversalXPConnect=1");
    if ($Settings::MailNewsTest and $test_result eq 'success') {
        print_log "Running MailNewsTest ...\n";
        my $cmd = "$binary_basename "
                  ."http://www.mozilla.org/quality/mailnews/APITest.html";
        $test_result = FileBasedTest("MailNewsTest", $binary_dir, $cmd, 
                                      90, "MAILNEWS TEST: Passed", 1);
    }
    
    # Editor test
    if (($Settings::EditorTest or $Settings::DomToTextConversionTest)
       and $test_result eq 'success') {
        print_log "Running  DomToTextConversionTest ...\n";
        $test_result =
          FileBasedTest("DomToTextConversionTest", $build_dir, $binary_dir,
                        "TestOutSinks", 15, "FAILED", 0);
    }
    return $test_result;
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

sub kill_process {
    my ($target_pid) = @_;
    my $status;
    
    # Try to kill 3 times, then try a kill -9
    for (my $ii=0; $ii < 3; $ii++) {
        kill 'TERM' => $target_pid;
        sleep 3;  # Give it 3 seconds to actually die
        my $pid = waitpid($target_pid, WNOHANG());
        return if ($pid == $target_pid and WIFEXITED($?)) or $pid == -1;
    }
    for (my $ii=0; $ii < 3; $ii++) {
        kill 'KILL' => $target_pid;
        sleep 2;  # Give it 2 second to actually die
        my $pid = waitpid($target_pid, WNOHANG());
        return if ($pid == $target_pid and WIFEXITED($?)) or $pid == -1;
    }
    die "Unable to kill process: $target_pid";
}

BEGIN {
    my %sig_num = ();
    my @sig_name = ();

    sub sig_name {
        # Find the name of a signal number
        my ($number) = @_;
        
        unless (@sig_name) {
            unless($Config{sig_name} && $Config{sig_num}) {
                die "No sigs?";
            } else {
                my @names = split ' ', $Config{sig_name};
                @sig_num{@names} = split ' ', $Config{sig_num};
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
    my ($home, $dir, $cmd, $logfile) = @_;

    my $pid = fork; # Fork off a child process.
    
    unless ($pid) { # child
        $ENV{HOME} = $home;
        chdir $dir;
        open STDOUT, ">$logfile";
        open STDERR, ">&STDOUT";
        select STDOUT; $| = 1;  # make STDOUT unbuffered
        select STDERR; $| = 1;  # make STDERR unbuffered
        my $now = localtime();
        print "$cmd started $now\n";
        exec $cmd;
        die "Could not exec()";
    }
    return $pid;
}

sub wait_for_pid {
    # Wait for a process to exit or kill it if it takes too long.
    my ($pid, $timeout_secs) = @_;
    my ($exit_value, $signal_num, $dumped_core, $timed_out) = (0,0,0,0);

    $SIG{ALRM} = sub { die "timeout" };
    eval {
        alarm $timeout_secs;
        while (1) {
            my $wait_pid = waitpid($pid, WNOHANG());
            last if ($wait_pid == $pid and WIFEXITED($?)) or $wait_pid == -1;
            sleep 1;
        }
        $exit_value = $? >> 8;
        $signal_num = $? >> 127;
        $dumped_core = $? & 128;
        alarm 0; # Clear the alarm
    };
    if ($@) {
        if ($@ =~ /timeout/) {
            kill_process($pid);
            $timed_out = 1;
        } else { # Died for some other reason.
            alarm(0);
            die; # Propagate the error up.
        }
    }
    unless ($timed_out) {
        my $now = localtime();
        print_log "Child exited $now\n";
    }
    return $timed_out, $exit_value, sig_name($signal_num), $dumped_core;
}

# Start up Mozilla, test passes if Mozilla is still alive
# after $timeout_secs (seconds).
#
sub AliveTest {
    my ($build_dir, $binary, $timeout_secs) = @_;
    my $binary_basename = basename($binary);
    my $binary_dir = dirname($binary);
    my $binary_log = "$build_dir/runlog";
    local $_;

    my $pid = fork_and_log($build_dir, $binary_dir, $binary_basename,
                           $binary_log);
    my ($timed_out, $exit_value, $sig_name, $dumped_core)
      = wait_for_pid($pid, $timeout_secs);
    print_log "----------- Output from $binary_basename for alive test --------------- \n";
    open READRUNLOG, "$binary_log";
    print_log "  $_" while <READRUNLOG>;
    close READRUNLOG;
    print_log "----------- End Output from $binary_basename for alive test ----------- \n";
    if (not $timed_out) {
        print_log "Error: alive test: $binary_basename received"
                  ." SIG$sig_name\n" if $sig_name ne 'ZERO';
        print_log "Error: alive test: $binary_basename exited with status "
                  ." $exit_value\n";
        if ($dumped_core) {
            print_log "Error: alive test: $binary_basename dumped core.\n";
        }
        print_log "Turn the tree orange now.\n";
        return 'testfailed';
    } else {
        print_log "$binary_basename still up after $timeout_secs seconds\n";
        return 'success';
    }
}

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
# Note: I tried to merge this function with AliveTest(),
#       the process flow control got too confusing :(  -mcafee
#
sub FileBasedTest {
    my ($test_name, $build_dir, $binary_dir, $test_command, $timeout_secs, 
        $status_token, $status_token_means_pass) = @_;
    local $_;

    # Assume the app is the first argument in the execString.
    my ($binary_basename) = (split /\s+/, $test_command)[0];
    my $binary = "$binary_dir/$binary_basename";
    my $binary_log = "$build_dir/$test_name.log";

    my $pid = fork_and_log($build_dir, $binary_dir, $test_command, $binary_log);
    my ($timed_out, $exit_value, $sig_name, $dumped_core)
      = wait_for_pid($pid, $timeout_secs);

    print_log "----------- Output from $test_name test --------------- \n";
    open READRUNLOG, "$binary_log";
    print_log $_ while <READRUNLOG>;
    close READRUNLOG;
    print_log "----------- End of output from $test_name test -------- \n";

    if ($timed_out) {
        print_log "Error: $test_name timed out after $timeout_secs.\n";
        return 'testfailed';
    } elsif ($exit_value == 0) {
        print_log "$test_name exited normally\n";
    } else {
        print_log "Error: $test_name received SIG$sig_name\n"
          if $sig_name ne 'ZERO';
        print_log "Error: $test_name exited with status $exit_value\n";
        if ($dumped_core) {
            print_log "Error: $test_name dumped core.\n";
        }
        return 'testfailed';
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
        print_log "Error: $test_name has failed.  Turn the tree orange now.\n";
        return 'testfailed';
    }
} # FileBasedTest

sub BloatTest {
    my ($binary, $build_dir) = @_;
    my $binary_basename = basename($binary);
    my $binary_dir = dirname($binary);
    my $binary_log = "$build_dir/bloat-cur.log";
    my $old_binary_log = "$build_dir/bloat-prev.log";
    my $timeout_secs = 120;
    local $_;

    # Turn on ref counting to track leaks (bloaty tool).
    $ENV{XPCOM_MEM_BLOAT_LOG} = "1"; 
    
    rename($binary_log, $old_binary_log);
    
    unless (-e "$binary_dir/bloaturls.txt") {
        print_log "Error: bloaturls.txt does not exist.\n";
        return 'testfailed';
    }

    my $cmd = "$binary_basename -f bloaturls.txt";
    my $pid = fork_and_log($build_dir, $binary_dir, $cmd, $binary_log);
    my ($timed_out, $exit_value, $sig_name, $dumped_core)
      = wait_for_pid($pid, $timeout_secs);
    
    print_log "----------- BloatTest Output --------------- \n";
    open READRUNLOG, "$binary_log";
    print_log "  $_" while <READRUNLOG>;
    close READRUNLOG;
    print_log "----------- End of BloatTest Output -------- \n";
    
    if ($timed_out or $exit_value) {
        if ($timed_out) {
            print_log "Error: BloatTest timed out after $timeout_secs.\n";
        } else {
            print_log "Error: BloatTest received SIG$sig_name\n"
              if $sig_name ne 'ZERO';
            print_log "Error: BloatTest exited with status $exit_value\n";
            if ($dumped_core) {
                print_log "Error: BloatTest dumped core.\n";
            }
            print_log "Turn the tree orange now.\n";
        }
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
