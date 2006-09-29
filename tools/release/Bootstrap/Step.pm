#
# Base class for all Steps.
#

package Bootstrap::Step;
use MozBuild::Util;

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
    my $rv = Util::MozUtil::RunShellCommand('command' => "$cmd");
    my $exitValue = $rv->{'exitValue'};
    if ($exitValue != 0) {
        die("shell call returned bad exit code: $exitValue");
    }
    print "output: $rv->{'output'}";
}

sub Log {
    my $this = shift;
    my %args = @_;
    my $msg = $args{'msg'};
    print "log: $msg\n";
}

1;
