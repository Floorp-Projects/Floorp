#
# Sign step. Applies digital signatures to builds.
# 
package Bootstrap::Step::Sign;
use Bootstrap::Step;
use Bootstrap::Config;
@ISA = ("Bootstrap::Step");

sub Execute {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $logDir = $config->Get(var => 'logDir');

    $this->Shell(
      cmd => 'echo',
      cmdArgs => ['sign'],
      logFile => catfile($logDir, 'sign.log'),
    );
}

sub Verify {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $logDir = $config->Get(var => 'logDir');

    $this->Shell(
      cmd => 'echo',
      cmdArgs => ['Verify sign'],
      logFile => catfile($logDir, 'sign_verify.log'),
    );
}

1;
