#!/usr/bin/perl
# vim:sw=4:et:ts=4:ai:

use strict;
use Cwd;

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
        eval $config;

    } else {
        warn "Error: Need tinderbox config file, multi-config.pl\n";
        exit;
    }
}


sub Run() {
    my $start_time = time();
    OUTER: while (1) {
        foreach my $treeentry (@{$Settings::Tinderboxes}) {
            my $multidir = getcwd();
            chdir($treeentry->{tree}) or
                die "Tree $treeentry->{tree} does not exist";
            system("./build-seamonkey.pl --once $treeentry->{args}");
            chdir($multidir);

            # We sleep 15 seconds to open up a window for stopping a build.
            sleep 15;

            # Provide a fall-over technique that stops the multi-tinderbox
            # script once the current build cycle has completed.
            if ( -e "fall-over" ) {
                system("mv fall-over fall-over.$$.done");
                last OUTER;
            }
        }

        # $BuildSleep is the minimum amount of time a build is allowed to take.
        # It prevents sending too many messages to the tinderbox server when
        # something is broken.
        my $sleep_time = ($Settings::BuildSleep * 60) - (time() - $start_time);
        if ($sleep_time > 0) {
            print "\n\nSleeping $sleep_time seconds ...\n";
            sleep $sleep_time;
        }
        $start_time = time();
    }
}

HandleArgs();
LoadConfig();
Run();
