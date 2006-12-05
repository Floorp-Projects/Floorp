#
# Repack step. Unpacks, modifies, repacks a Firefox en-US build.
# Primary use is for l10n (localization) builds.
#
package Bootstrap::Step::Repack;
use Bootstrap::Step;
use Bootstrap::Config;
@ISA = ("Bootstrap::Step");

my $config = new Bootstrap::Config;

sub Execute {
    my $this = shift;

    my $buildDir = $config->Get('var' => 'l10n_buildDir');
    my $productTag = $config->Get('var' => 'productTag');
    my $rc = $config->Get('var' => 'rc');
    my $logDir = $config->Get('var' => 'logDir');
    my $buildPlatform = $config->Get('var' => 'buildPlatform');
    my $rcTag = $productTag . '_RC' . $rc;

    my $buildLog = $logDir . '/' . $rcTag . '-build-l10n.log';
    my $lastBuilt = $buildDir . '/' . $buildPlatform . '/last-built';
    unlink($lastBuilt) 
      or $this->Log('msg' => "Cannot unlink last-built file $lastBuilt: $!");
    $this->Log('msg' => "Unlinked $lastBuilt");

    $this->Shell(
      'cmd' => './build-seamonkey.pl --once --mozconfig mozconfig --depend --config-cvsup-dir ' . $buildDir . '/tinderbox-configs',
      'dir' => $buildDir,
      'logFile' => $buildLog,
      'timeout' => 36000
    );
}

sub Verify {
    my $this = shift;

    my $buildDir = $config->Get('var' => 'buildDir');
    my $productTag = $config->Get('var' => 'productTag');
    my $rc = $config->Get('var' => 'rc');
    my $rcTag = $productTag.'_RC'.$rc;
    my $logDir = $config->Get('var' => 'logDir');

    my $buildLog = $logDir . '/' . $rcTag . '-build.log';

# XXX temp disabled
#    $this->CheckLog(
#        'log' => $buildLog,
#        'notAllowed' => 'failed',
#    );
}

1;
