#!/usr/bin/perl
# vim:sw=4:et:ts=4:ai:

use strict;

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
\$Tinderboxes = [ { tree => "SeaMonkey", args => "--depend --mozconfig mozconfig" }, { tree => "SeaMonkey-Branch", args => "--depend --mozconfig mozconfig" } ];
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

        while (<CONFIG>) {
            package Settings;
            eval;
        }

        close CONFIG;
    } else {
        warn "Error: Need tinderbox config file, multi-config.pl\n";
        exit;
    }
}


sub Run() {
    my $start_time = time();
    while (1) {
        # $BuildSleep is the minimum amount of time a build is allowed to take.
        # It prevents sending too many messages to the tinderbox server when
        # something is broken.
        foreach my $treeentry (@{$Settings::Tinderboxes}) {
            chdir($treeentry->{tree}) or
                die "Tree $treeentry->{tree} does not exist";
            system("./build-seamonkey.pl --once $treeentry->{args}");
            chdir("..");
        }

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
