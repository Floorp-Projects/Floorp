#
# Sign step. Applies digital signatures to builds.
# 
package Bootstrap::Step::Sign;
use Bootstrap::Step;
use Bootstrap::Config;
@ISA = ("Bootstrap::Step");

my $config = new Bootstrap::Config;

sub Execute {
    my $this = shift;
    my $logDir = $config->Get('var' => 'logDir');

    $this->Shell(
      'cmd' => 'echo sign',
      'logFile' => $logDir . '/sign.log',
    );
}

sub Verify {
    my $this = shift;
    my $logDir = $config->Get('var' => 'logDir');

    $this->Shell(
      'cmd' => 'echo Verify sign',
      'logFile' => $logDir . '/verify-sign.log',
    );
}

1;
