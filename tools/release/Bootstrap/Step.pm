#
# Base class for all Steps.
#

package Bootstrap::Step;
use MozBuild::Util;
use Config::General;

# shared static config
my $conf = new Config::General("bootstrap.cfg");
if (not $conf) {
    die "Config is null: $!";
}

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
    my $timeout = $args{'timeout'};
    my $rv = '';

    if ($dir) {
        chdir($dir) || die "Cannot chdir to $dir: $!";
        $this->Log('msg' => 'Running shell command ' . $cmd . ' in dir ' . $dir);
    } else {
        $this->Log('msg' => 'Running shell command ' . $cmd);
    }
    if ($timeout) {
        $rv = MozBuild::Util::RunShellCommand(
           'command' => "$cmd",
           'timeout' => "$timeout",
        );
    } else {
        $rv = MozBuild::Util::RunShellCommand(
           'command' => "$cmd",
        );
    }

    my $exitValue = $rv->{'exitValue'};
    my $timedOut  = $rv->{'timedOut'};
    my $signalName  = $rv->{'signalName'};
    my $dumpedCore = $rv->{'dumpedCore'};
    if ($exitValue) {
        if ($exitValue != 0) {
            $this->Log('msg' => "output: $rv->{'output'}");
            die("shell call returned bad exit code: $exitValue");
        }
    }
    if ($timedOut) {
        $this->Log('msg' => "output: $rv->{'output'}");
        die("FAIL shell call timed out after $timeout seconds");
    }
    if ($signalName) {
        $this->Log('msg' => "output: $rv->{'output'}");
        print ("WARNING shell recieved signal $signalName");
    }
    if ($dumpedCore) {
        $this->Log('msg' => "output: $rv->{'output'}");
        die("FAIL shell call dumped core");
    }

    if($rc->{'output'}) {
        $this->Log('msg' => "output: $rv->{'output'}");
    }
}

sub Log {
    my $this = shift;
    my %args = @_;
    my $msg = $args{'msg'};
    print "log: $msg\n";
}

sub Config {
    my $this = shift;

    my %args = @_;
    use Data::Dumper;
    my $var = $args{'var'};

    my %config = $conf->getall();

    if ($config{'app'}{'firefox'}{'release'}{'1.5.0.7'}{$var}) {
        return $config{'app'}{'firefox'}{'release'}{'1.5.0.7'}{$var};
    } else {
        die("No such config variable: $var\n");
    }
}

sub CheckLog {
    my $this = shift;
    my %args = @_;

    my $log = $args{'log'};
    my $notAllowed = $args{'notAllowed'};
    my $checkFor = $args{'checkFor'};
    my $checkForOnly = $args{'checkForOnly'};

    open (FILE, "< $log") || die "Cannot open file $log: $!";
    my @contents = <FILE>;
    close FILE || die "Cannot close file $log: $!";
  
    if ($notAllowed) {
        my @errors = grep(/$notAllowed/i, @contents);
        if (@errors) {
            die "Errors in log ($log): \n\n @errors \n $!";
        }
    }
    if ($checkFor) {
        if (not grep(/$checkFor/i, @contents)) {
            die "$checkFor is not present in file $log: $!";
        }
    }
    if ($checkForOnly) {
        if (not grep(/$checkForOnly/i, @contents)) {
            die "$checkForOnly is not present in file $log: $!";
        }
        my @errors = grep(!/$checkForOnly/i, @contents);
        if (@errors) {
            die "Errors in log ($log): \n\n @errors \n $!";
        }
    }
}

1;
