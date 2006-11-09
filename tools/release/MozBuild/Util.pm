package MozBuild::Util;
use File::Path;

my $EXEC_TIMEOUT = '600';

sub RunShellCommand {
    my %args = @_;
    my $shellCommand = $args{'command'};
    my $logfile = $args{'logfile'};

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
    my $pid;

    eval {
        local $SIG{'ALRM'} = sub { die "alarm\n" };
        alarm $timeout;
 
        if (! $redirectStderr or $shellCommand =~ "2>&1") {
            $pid = open CMD, "$shellCommand |" or die "Could not run command $shellCommand: $!";
        } else {
            $pid = open CMD, "$shellCommand 2>&1 |" or die "Could not close command $shellCommand: $!";
        }

        if (defined($logfile)) {
            open(LOGFILE, ">> $logfile") or die "Could not open logfile $logfile: $!";
            LOGFILE->autoflush(1);
        }
        while (<CMD>) {
            $output .= $_;
            print $_ if ($printOutputImmediately);
            if (defined($logfile)) {
                print LOGFILE $_;
            }
        }
        if (defined($logfile)) {
            close(LOGFILE) or die "Could not close logfile $logfile: $!";
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
            kill(9, $pid) or die "Could not kill timed-out $pid: $!";
        } else {
            warn "Error running $shellCommand: $@\n";
            $output = $@;
        }
    }

    if ($exitValue or $timedOut or $dumpedCore or $signalNum) {
        if ($timedOut) {
            # callers expect exitValue to be non-zero if request timed out
            $exitValue = 1;
        }
    }

    return { timedOut => $timedOut,
             exitValue => $exitValue,
             sigName => $sigName,
             output => $output,
             dumpedCore => $dumpedCore,
             pid => $pid,
    };
}

## This is a wrapper function to get easy true/false return values from a
## mkpath()-like function. mkpath() *actually* returns the list of directories
## it created in the pursuit of your request, and keeps its actual success 
## status in $@. 

sub MkdirWithPath
{
    my %args = @_;

    my $dirToCreate = $args{'dir'};
    my $printProgress = $args{'printProgress'};
    my $dirMask = $args{'dirMask'};

    die "ASSERT: MkdirWithPath() needs an arg" if not defined($dirToCreate);

    ## Defaults based on what mkpath does...
    $printProgress = defined($printProgress) ? $printProgress : 0;
    $dirMask = defined($dirMask) ? $dirMask : 0777;

    eval { mkpath($dirToCreate, $printProgress, $dirMask) };
    return defined($@);
}

1;
