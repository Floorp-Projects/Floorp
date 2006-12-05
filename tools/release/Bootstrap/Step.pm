#
# Base class for all Steps.
#

package Bootstrap::Step;
use IO::Handle;
use MozBuild::Util qw(RunShellCommand);
use POSIX qw(strftime);

my $DEFAULT_TIMEOUT = 3600;

sub new {
    my $proto = shift;
    my $class = ref($proto) || $proto;
    my $this = {};
    bless($this, $class);
    return $this;
}

sub Shell {
    my $this = shift;
    my %args = @_;
    my $cmd = $args{'cmd'};
    my $dir = $args{'dir'};
    my $timeout = $args{'timeout'} ? $args{'timeout'} : $DEFAULT_TIMEOUT;
    my $logFile = $args{'logFile'};
    my $rv = '';

    if ($dir) {
        $this->Log('msg' => 'Changing directory to ' . $dir);
        chdir($dir) or die "Cannot chdir to $dir: $!";
        $this->Log('msg' => 'Running shell command ' . $cmd . ' in dir ' . $dir);
    } else {
        $this->Log('msg' => 'Running shell command ' . $cmd);
    }

    $this->Log('msg' => 'Starting time is ' . $this->CurrentTime());

    print "Timeout: $timeout\n";

    if ($timeout) {
        $rv = RunShellCommand(
           'command' => "$cmd",
           'timeout' => "$timeout",
           'logfile' => "$logFile",
        );
    } else {
        $rv = RunShellCommand(
           'command' => "$cmd",
           'logfile' => "$logFile",
        );
    }

    my $exitValue = $rv->{'exitValue'};
    my $timedOut  = $rv->{'timedOut'};
    my $signalName  = $rv->{'signalName'};
    my $dumpedCore = $rv->{'dumpedCore'};
    my $pid = $rv->{'pid'};
    print "Pid: $pid\n";
    if ($timedOut) {
        $this->Log('msg' => "output: $rv->{'output'}") if $rv->{'output'};
        die("FAIL shell call timed out after $timeout seconds");
    }
    if ($signalName) {
        print ("WARNING shell recieved signal $signalName");
    }
    if ($dumpedCore) {
        $this->Log('msg' => "output: $rv->{'output'}") if $rv->{'output'};
        die("FAIL shell call dumped core");
    }
    if ($exitValue) {
        if ($exitValue != 0) {
            $this->Log('msg' => "output: $rv->{'output'}") if $rv->{'output'};
            die("shell call returned bad exit code: $exitValue");
        }
    }

    if ($rv->{'output'} && not defined($logFile)) {
        $this->Log('msg' => "output: $rv->{'output'}");
    }

    # current time
    $this->Log('msg' => 'Ending time is ' . $this->CurrentTime());
}

sub Log {
    my $this = shift;
    my %args = @_;
    my $msg = $args{'msg'};
    print "log: $msg\n";
}

sub CheckLog {
    my $this = shift;
    my %args = @_;

    my $log = $args{'log'};
    my $notAllowed = $args{'notAllowed'};
    my $checkFor = $args{'checkFor'};
    my $checkForOnly = $args{'checkForOnly'};

    if (not defined($log)) {
        die "No log file specified";
    }

    open (FILE, "< $log") or die "Cannot open file $log: $!";
    my @contents = <FILE>;
    close FILE or die "Cannot close file $log: $!";
  
    if ($notAllowed) {
        my @errors = grep(/$notAllowed/i, @contents);
        if (@errors) {
            die "Errors in log ($log): \n\n @errors \n";
        }
    }
    if ($checkFor) {
        if (not grep(/$checkFor/i, @contents)) {
            die "$checkFor is not present in file $log \n";
        }
    }
    if ($checkForOnly) {
        if (not grep(/$checkForOnly/i, @contents)) {
            die "$checkForOnly is not present in file $log \n";
        }
        my @errors = grep(!/$checkForOnly/i, @contents);
        if (@errors) {
            die "Errors in log ($log): \n\n @errors \n";
        }
    }
}

sub CurrentTime() {
    my $this = shift;

    return strftime("%T %D", localtime());
}

1;
