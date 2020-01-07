#!/usr/bin/perl
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

use POSIX qw(:sys_wait_h);
use POSIX qw(setsid);
use FileHandle;

# Constants
$WINOS = "MSWin32";

$osname = $^O;

use Cwd;
if ($osname =~ $WINOS) {
    # Windows
    require Win32::Process;
    require Win32;
}

# Get environment variables.
$output_file = $ENV{NSPR_TEST_LOGFILE};
$timeout = $ENV{TEST_TIMEOUT};

$timeout = 0 if (!defined($timeout));

sub getTime {
    ($second, $minute, $hour, $dayOfMonth, $month, $yearOffset, $dayOfWeek, $dayOfYear, $daylightSavings) = localtime();

    $year = 1900 + $yearOffset;

    $theTime = sprintf("%04d-%02d-%02d %02d:%02d:%02d",$year,$month,$dayOfMonth,$hour,$minute,$second);
    return $theTime;
}

sub open_log {

    if (!defined($output_file)) {
        print "No output file.\n";
        # null device
        if ($osname =~ $WINOS) {
            $output_file = "nul";
        } else {
            $output_file = "/dev/null";
        }
    }
    
    # use STDOUT for OF (to print summary of test results)
    open(OF, ">&STDOUT") or die "Can't reuse STDOUT for OF\n";
    OF->autoflush;
    # reassign STDOUT to $output_file (to print details of test results)
    open(STDOUT, ">$output_file") or die "Can't open file $output_file for STDOUT\n";
    STDOUT->autoflush;
    # redirect STDERR to STDOUT
    open(STDERR, ">&STDOUT") or die "Can't redirect STDERR to STDOUT\n";
    STDERR->autoflush;
    
    # Print header test in summary
    $now = getTime;
    print OF "\nNSPR Test Results - tests\n";
    print OF "\nBEGIN\t\t\t$now\n";
    print OF "NSPR_TEST_LOGFILE\t$output_file\n";
    print OF "TEST_TIMEOUT\t$timeout\n\n";
    print OF "\nTest\t\t\tResult\n\n";
}

sub close_log {
    # end of test marker in summary
    $now = getTime;
    print OF "END\t\t\t$now\n";

    close(OF) or die "Can't close file OF\n";
    close(STDERR) or die "Can't close STDERR\n";
    close(STDOUT) or die "Can't close STDOUT\n";
}

sub print_begin {
$lprog = shift;

    # Summary output
    print OF "$prog";
    # Full output
    $now = getTime;
    print "BEGIN TEST: $lprog ($now)\n\n";
}

sub print_end {
($lprog, $exit_status, $exit_signal, $exit_core) = @_;

    if (($exit_status == 0) && ($exit_signal == 0) && ($exit_core == 0)) {
        $str_status = "Passed";
    } else {
        $str_status = "FAILED";
    }
    if ($exit_signal != 0) {
    	$str_signal = " - signal $exit_signal";
    } else {
    	$str_signal = "";
    }
    if ($exit_core != 0) {
    	$str_core = " - core dumped";
    } else {
    	$str_core = "";
    }
    $now = getTime;
    # Full output
    print "\nEND TEST: $lprog ($now)\n";
    print "TEST STATUS: $lprog = $str_status (exit status " . $exit_status . $str_signal . $str_core . ")\n";
    print "--------------------------------------------------\n\n";
    # Summary output
    print OF "\t\t\t$str_status\n";
}

sub ux_start_prog {
# parameters:
$lprog = shift; # command to run

    # Create a process group for the child
    # so we can kill all of it if needed
    setsid or die "setsid failed: $!";
    # Start test program    
    exec("./$lprog");
    # We should not be here unless exec failed.
    print "Faild to exec $lprog";
    exit 1 << 8;
}   

sub ux_wait_timeout {
# parameters:
$lpid = shift;     # child process id
$ltimeout = shift; # timeout

    if ($ltimeout == 0) {
        # No timeout: use blocking wait
        $ret = waitpid($lpid,0);
        # Exit and don't kill
        $lstatus = $?;
        $ltimeout = -1;
    } else {
        while ($ltimeout > 0) {
            # Check status of child using non blocking wait
            $ret = waitpid($lpid, WNOHANG);
            if ($ret == 0) {
                # Child still running
    #           print "Time left=$ltimeout\n";
                sleep 1;
                $ltimeout--;
            } else {
                # Child has ended
                $lstatus = $?;
                # Exit the wait loop and don't kill
                $ltimeout = -1;
            }
        }
    }
    
    if ($ltimeout == 0) {
        # we ran all the timeout: it's time to kill the child
        print "Timeout ! Kill child process $lpid\n";
        # Kill the child process and group
        kill(-9,$lpid);
        $lstatus = 9;
    }
    
    return $lstatus;
}

sub ux_test_prog {
# parameters:
$prog = shift;  # Program to test

    $child_pid = fork;
    if ($child_pid == 0) {
        # we are in the child process
        print_begin($prog);
        ux_start_prog($prog);
    } else {
        # we are in the parent process
        $status = ux_wait_timeout($child_pid,$timeout);
        # See Perlvar for documentation of $?
        # exit status = $status >> 8
        # exit signal = $status & 127 (no signal = 0)
        # core dump = $status & 128 (no core = 0)
        print_end($prog, $status >> 8, $status & 127, $status & 128);
    }

    return $status;
}

sub win_path {
$lpath = shift;

    # MSYS drive letter = /c/ -> c:/
    $lpath =~ s/^\/(\w)\//$1:\//;
    # Cygwin drive letter = /cygdrive/c/ -> c:/
    $lpath =~ s/^\/cygdrive\/(\w)\//$1:\//;
    # replace / with \\
    $lpath =~ s/\//\\\\/g;
    
    return $lpath;
}

sub win_ErrorReport{
    print Win32::FormatMessage( Win32::GetLastError() );
}

sub win_test_prog {
# parameters:
$prog = shift;  # Program to test

    $status = 1;
    $curdir = getcwd;
    $curdir = win_path($curdir);
    $prog_path = "$curdir\\$prog.exe";
    
    print_begin($prog);
    
    Win32::Process::Create($ProcessObj,
                           "$prog_path",
                           "$prog",
                           0,
                           NORMAL_PRIORITY_CLASS,
                           ".")|| die win_ErrorReport();
    $retwait = $ProcessObj->Wait($timeout * 1000);
        
    if ( $retwait == 0) {
        # the prog didn't finish after the timeout: kill
        $ProcessObj->Kill($status);
        print "Timeout ! Process killed with exit status $status\n";
    } else {
        # the prog finished before the timeout: get exit status
        $ProcessObj->GetExitCode($status);
    }
    # There is no signal, no core on Windows
    print_end($prog, $status, 0, 0);

    return $status
}

# MAIN ---------------
@progs = (
"abstract",
"accept",
"acceptread",
"acceptreademu",
"affinity",
"alarm",
"anonfm",
"atomic",
"attach",
"bigfile",
"cleanup",
"cltsrv",
"concur",
"cvar",
"cvar2",
"dlltest",
"dtoa",
"errcodes",
"exit",
"fdcach",
"fileio",
"foreign",
"formattm",
"fsync",
"gethost",
"getproto",
"i2l",
"initclk",
"inrval",
"instrumt",
"intrio",
"intrupt",
"io_timeout",
"ioconthr",
"join",
"joinkk",
"joinku",
"joinuk",
"joinuu",
"layer",
"lazyinit",
"libfilename",
"lltest",
"lock",
"lockfile",
"logfile",
"logger",
"many_cv",
"nameshm1",
"nblayer",
"nonblock",
"ntioto",
"ntoh",
"op_2long",
"op_excl",
"op_filnf",
"op_filok",
"op_nofil",
"parent",
"parsetm",
"peek",
"perf",
"pipeping",
"pipeping2",
"pipeself",
"poll_nm",
"poll_to",
"pollable",
"prftest",
"prfz",
"primblok",
"provider",
"prpollml",
"pushtop",
"ranfile",
"randseed",
"reinit",
"rwlocktest",
"sel_spd",
"selct_er",
"selct_nm",
"selct_to",
"selintr",
"sema",
"semaerr",
"semaping",
"sendzlf",
"server_test",
"servr_kk",
"servr_uk",
"servr_ku",
"servr_uu",
"short_thread",
"sigpipe",
"socket",
"sockopt",
"sockping",
"sprintf",
"stack",
"stdio",
"str2addr",
"strod",
"switch",
"system",
"testbit",
"testfile",
"threads",
"timemac",
"timetest",
"tpd",
"udpsrv",
"vercheck",
"version",
"writev",
"xnotify",
"zerolen");

open_log;

foreach $current_prog (@progs) {
    if ($osname =~ $WINOS) {
        win_test_prog($current_prog);
    } else {
        ux_test_prog($current_prog);
    }
}

close_log;
