package Util::MozUtil;

# Stolen from the code for bug 336463

my $EXEC_TIMEOUT = '600';

sub RunShellCommand {
    my %args = @_;
    my $shellCommand = $args{'command'};

    # optional
    my $timeout = exists($args{'timeout'}) ? $args{'timeout'} : $EXEC_TIMEOUT;
    my $redirectStderr = exists($args{'redirectStderr'}) ? $args{'redirectStderr'} : 1;
    my $printOutputImmediately = exists($args{'output'}) ? $args{'output'} : 0;

    my $now = localtime();
    local $_;
 
    chomp($shellCommand);

    my $exitValue = 1;
    my $signalNum;
    my $sigName;
    my $dumpedCore;
    my $timedOut;
    my $output = '';

    eval {
        local $SIG{'ALRM'} = sub { die "alarm\n" };
        alarm $timeout;
 
        if (! $redirectStderr || $shellCommand =~ "2>&1") {
            open CMD, "$shellCommand |" or die "Could not run command $shellCommand: $!";
        } else {
            open CMD, "$shellCommand 2>&1 |" or die "Could not close command $shellCommand: $!";
        }

        while (<CMD>) {
            $output .= $_;
            print $_ if ($printOutputImmediately);
        }

        close CMD;# or die "Could not close command: $!";
        $exitValue = $? >> 8;
        $signalNum = $? >> 127;
        $dumpedCore = $? & 128;
        $timedOut = 0;
        alarm 0;
    };

    if ($@) {
        if ($@ eq "alarm\n") {
            $timedOut = 1;
        } else {
            warn "Error running $shellCommand: $@\n";
            $output = $@;
        }
    }

    if ($exitValue || $timedOut || $dumpedCore || $signalNum) {
        if ($timedOut) {
            # callers expect exitValue to be non-zero if request timed out
            $exitValue = 1;
        }
    }

    return { timedOut => $timedOut,
             exitValue => $exitValue,
             sigName => $sigName,
             output => $output,
             dumpedCore => $dumpedCore };
}

1;
