#
# Base class for all Steps.
#

package Bootstrap::Step;

use IO::Handle;
use File::Spec::Functions;
use POSIX qw(strftime);

use Bootstrap::Config;
use MozBuild::Util qw(RunShellCommand Email);

use base 'Exporter';

our @EXPORT = qw(catfile);

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
    my $cmdArgs = defined($args{'cmdArgs'}) ? $args{'cmdArgs'} : [];
    my $dir = $args{'dir'};
    my $timeout = $args{'timeout'} ? $args{'timeout'} : $DEFAULT_TIMEOUT;
    my $logFile = $args{'logFile'};
    my $ignoreExitValue = $args{'ignoreExitValue'};
    my $rv = '';
    my $config = new Bootstrap::Config();

    if (ref($cmdArgs) ne 'ARRAY') {
        die("ASSERT: Bootstrap::Step::Shell(): cmdArgs is not an array ref\n");
    }

    my %runShellCommandArgs = (command => $cmd,
                               args => $cmdArgs,
                               timeout => $timeout,
                               logfile => $logFile);

    if ($config->Exists(var => 'dumpLogs')) {
        if ($config->Get(var => 'dumpLogs')) {
            $runShellCommandArgs{'output'} = 1;
        }
    }

    if ($dir) {
        $runShellCommandArgs{'dir'} = $dir;
    }

    $this->Log(msg => 'Running shell command' .
     (defined($dir) ? " in $dir" : '') . ':');
    $this->Log(msg => '  arg0: ' . $cmd); 
    my $argNum = 1;
    foreach my $arg (@{$cmdArgs}) {
        $this->Log(msg => '  arg' . $argNum . ': ' . $arg); 
        $argNum += 1;
    }
    $this->Log(msg => 'Starting time is ' . $this->CurrentTime());
    $this->Log(msg => 'Logging output to ' . $logFile);

    $this->Log(msg => 'Timeout: ' . $timeout);

    $rv = RunShellCommand(%runShellCommandArgs);

    my $exitValue = $rv->{'exitValue'};
    my $timedOut  = $rv->{'timedOut'};
    my $signalNum  = $rv->{'signalNum'};
    my $dumpedCore = $rv->{'dumpedCore'};
    if ($timedOut) {
        $this->Log(msg => "output: $rv->{'output'}") if $rv->{'output'};
        die('FAIL shell call timed out after ' . $timeout . ' seconds');
    }
    if ($signalNum) {
        $this->Log(msg => 'WARNING shell recieved signal ' . $signalNum);
    }
    if ($dumpedCore) {
        $this->Log(msg => "output: $rv->{'output'}") if $rv->{'output'};
        die("FAIL shell call dumped core");
    }
    if ($ignoreExitValue) {
        $this->Log(msg => "Exit value $rv->{'output'}, but ignoring as told");
    } elsif ($exitValue) {
        if ($exitValue != 0) {
            $this->Log(msg => "output: $rv->{'output'}") if $rv->{'output'};
            die("shell call returned bad exit code: $exitValue");
        }
    }

    if ($rv->{'output'} && not defined($logFile)) {
        $this->Log(msg => "output: $rv->{'output'}");
    }

    # current time
    $this->Log(msg => 'Ending time is ' . $this->CurrentTime());
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
        die("No log file specified");
    }

    open (FILE, "< $log") or die("Cannot open file $log: $!");
    my @contents = <FILE>;
    close FILE or die("Cannot close file $log: $!");
  
    if ($notAllowed) {
        my @errors = grep(/$notAllowed/i, @contents);
        if (@errors) {
            die("Errors in log ($log): \n\n @errors");
        }
    }
    if ($checkFor) {
        if (not grep(/$checkFor/i, @contents)) {
            die("$checkFor is not present in file $log");
        }
    }
    if ($checkForOnly) {
        if (not grep(/$checkForOnly/i, @contents)) {
            die("$checkForOnly is not present in file $log");
        }
        my @errors = grep(!/$checkForOnly/i, @contents);
        if (@errors) {
            die("Errors in log ($log): \n\n @errors");
        }
    }
}

sub CurrentTime {
    my $this = shift;

    return strftime("%T %D", localtime());
}

# Overridden by child if needed
sub Push {
    my $this = shift;
}

# Overridden by child if needed
sub Announce {
    my $this = shift;
}

sub SendAnnouncement {
    my $this = shift;
    my %args = @_;
    
    my $config = new Bootstrap::Config();

    my $blat = $config->Get(var => 'blat');
    my $sendmail = $config->Get(var => 'sendmail');
    my $from = $config->Get(var => 'from');
    my $to = $config->Get(var => 'to');
    my @ccList = $config->Exists(var => 'cc') ? split(/[,\s]+/, 
     $config->Get(var => 'cc')) : ();
    my $hostname = $config->SystemInfo(var => 'hostname');

    my $subject = $hostname . ' - ' . $args{'subject'};
    my $message = $hostname . ' - ' . $args{'message'};

    eval {
        Email(
          blat => $blat,
          sendmail => $sendmail,
          from => $from,
          to => $to,
          cc => \@ccList,
          subject => $subject,
          message => $message,
        );
    };
    if ($@) {
        die("Could not send announcement email: $@");
    }
}
    
1;
