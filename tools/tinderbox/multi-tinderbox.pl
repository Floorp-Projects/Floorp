#!/usr/bin/perl
# vim:sw=4:et:ts=4:ai:

use strict;
use Cwd;

# Globals that the signal handlers need access to...
my $CURRENT_BUILD_PID = 0;
my $HALT_AFTER_THIS_BUILD = 0;
my $RELOAD_CONFIG = 0;

my $TBOX_CLIENT_CVSUP_CMD = 'cvs update';
my $TBOX_CLIENT_CVS_TIMEOUT = 300;

sub PrintUsage() {
    die <<END_USAGE
    usage: $0 [options]
Options:
  --example-config       Instead of running, print an example
                           'multi-config.pl' to help get started.
END_USAGE
}

sub PrintExample() {
    print <<END_EXAMPLE
# multi-config.pl
\$BuildSleep = 10;                       # minutes
\$Tinderboxes = [
  { tree => "SeaMonkey", args => "--depend --mozconfig mozconfig" },
  { tree => "SeaMonkey-Branch", args => "--depend --mozconfig mozconfig" },
];
END_EXAMPLE
    ;
    exit;
}

sub HandleArgs() {
    return if ($#ARGV == -1);
    PrintUsage() if ($#ARGV != 0 || $ARGV[0] ne "--example-config");
    PrintExample();
}

sub LoadConfig() {
    if (-r 'multi-config.pl') {
        no strict 'vars';

        open CONFIG, 'multi-config.pl' or
            print "can't open multi-config.pl, $?\n";

        local $/ = undef;
        my $config = <CONFIG>;
        close CONFIG;

        package Settings;
        eval $config or 
            die "Malformed multi-config.pl: $@\n";

    } else {
        warn "Error: Need tinderbox config file, multi-config.pl\n";
        exit;
    }
}

sub HandleSigTerm() {
    if ($CURRENT_BUILD_PID > 0) {
        kill(15, $CURRENT_BUILD_PID);
        exit 0;
    }
}

sub HandleSigHup() {
    $HALT_AFTER_THIS_BUILD = 1;
    $RELOAD_CONFIG = 1;
}

sub HandleSigInt() {
    $HALT_AFTER_THIS_BUILD = 1;
}

sub HandleSigAlrm() {
   die 'timeout';
}

sub UpdateTinderboxScripts() {
    if (exists($ENV{'TBOX_CLIENT_CVS_DIR'})) {
        print STDERR "Updating tinderbox scripts in $ENV{'TBOX_CLIENT_CVS_DIR'}\n";
        eval {
            alarm($TBOX_CLIENT_CVS_TIMEOUT);
            system("cd $ENV{'TBOX_CLIENT_CVS_DIR'} && $TBOX_CLIENT_CVSUP_CMD") 
             == 0 or print STDERR "$TBOX_CLIENT_CVSUP_CMD failed: $!\n";
            alarm(0);
        };

        if ($@) {
            print STDERR 'CVS update of client tinderbox scripts ' . 
              ($@ eq 'timeout' ? "timed out" : "failed: $@") . "\n";
        }
    }
}

sub Run() {
    OUTER: while (1) {
        my $start_time = time();
        UpdateTinderboxScripts();

        foreach my $treeentry (@{$Settings::Tinderboxes}) {
            my $buildPid = fork();

            if ($buildPid) {
                if ($buildPid > 0) {
                    $CURRENT_BUILD_PID = $buildPid;
                    my $reapedPid = waitpid($buildPid, 0);
                    if ($reapedPid != $buildPid) {
                        print STDERR "PID $buildPid died too quickly; " .
                         "status was: " . ($? >> 8);
                    }
                } else {
                    warn "fork() of build sub-process failed: $!";
                }

            } else {
                chdir($treeentry->{tree}) or
                 die "Tree $treeentry->{tree} does not exist";
                exec("./build-seamonkey.pl --once $treeentry->{args}");
            }

            # We sleep 15 seconds to open up a window for stopping a build.
            sleep 15;

            # Provide a fall-over technique that stops the multi-tinderbox
            # script once the current build cycle has completed.
            last OUTER if ( $HALT_AFTER_THIS_BUILD );
        }

        # $BuildSleep is the minimum amount of time a build is allowed to take.
        # It prevents sending too many messages to the tinderbox server when
        # something is broken.
        my $sleep_time = ($Settings::BuildSleep * 60) - (time() - $start_time);
        if ($sleep_time > 0) {
            print "\n\nSleeping $sleep_time seconds ...\n";
            sleep $sleep_time;
        }
    }
}

$SIG{'TERM'} = \&HandleSigTerm;
$SIG{'HUP'} = \&HandleSigHup;
$SIG{'INT'} = \&HandleSigInt;
$SIG{'ALRM'} = \&HandleSigAlrm;

HandleArgs();

while (not $HALT_AFTER_THIS_BUILD) {
    LoadConfig();
    Run();

    ## Run can exit for two reasons: SIGHUP or SIGTERM; if we got a TERM,
    ## we won't reload the config, and we'll exit; if we got a HUP, then
    ## let's reload the config, and retoggle the bit so we continue building 
    if ($RELOAD_CONFIG) {
        $HALT_AFTER_THIS_BUILD = 0;
        $RELOAD_CONFIG = 0;
    }
}
