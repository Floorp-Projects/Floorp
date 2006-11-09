#
# Repack step. Unpacks, modifies, repacks a Firefox en-US build.
# Primary use is for l10n (localization) builds.
#
package Bootstrap::Step::Repack;
use Bootstrap::Step;
@ISA = ("Bootstrap::Step");

sub Execute {
    my $this = shift;

    my $buildDir = $this->Config('var' => 'l10n-buildDir');
    my $productTag = $this->Config('var' => 'productTag');
    my $rc = $this->Config('var' => 'rc');
    my $logDir = $this->Config('var' => 'logDir');
    my $buildPlatform = $this->Config('var' => 'buildPlatform');
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

    my $buildDir = $this->Config('var' => 'buildDir');
    my $productTag = $this->Config('var' => 'productTag');
    my $rc = $this->Config('var' => 'rc');
    my $rcTag = $productTag.'_RC'.$rc;
    my $logDir = $this->Config('var' => 'logDir');

    my $buildLog = $logDir . '/' . $rcTag . '-build.log';

# XXX temp disabled
#    $this->CheckLog(
#        'log' => $buildLog,
#        'notAllowed' => 'failed',
#    );
}

1;
