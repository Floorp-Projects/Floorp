#
# Build step. Calls tinderbox to produce en-US Firefox build.
#
package Bootstrap::Step::Build;
use Bootstrap::Step;
use Bootstrap::Config;
@ISA = ("Bootstrap::Step");

sub Execute {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $buildDir = $config->Get(sysvar => 'buildDir');
    my $productTag = $config->Get(var => 'productTag');
    my $rc = $config->Get(var => 'rc');
    my $buildPlatform = $config->Get(sysvar => 'buildPlatform');
    my $logDir = $config->Get(var => 'logDir');
    my $rcTag = $productTag . '_RC' . $rc;

    my $lastBuilt = catfile($buildDir, $buildPlatform, 'last-built');
    unlink($lastBuilt) 
      or $this->Log(msg => "Cannot unlink last-built file $lastBuilt: $!");
    $this->Log(msg => "Unlinked $lastBuilt");

    my $buildLog = catfile($logDir, 'build_' . $rcTag . '-build.log');
 
    $this->Shell(
      cmd => './build-seamonkey.pl',
      cmdArgs => ['--once', '--mozconfig', 'mozconfig', '--depend', 
                  '--config-cvsup-dir', 
                  catfile($buildDir, 'tinderbox-configs')],
      dir => $buildDir,
      logFile => $buildLog,
      timeout => 36000
    );
}

sub Verify {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $buildDir = $config->Get(var => 'buildDir');
    my $productTag = $config->Get(var => 'productTag');
    my $rc = $config->Get(var => 'rc');
    my $rcTag = $productTag.'_RC'.$rc;
    my $logDir = $config->Get(var => 'logDir');

    my $buildLog = catfile($logDir, 'build_' . $rcTag . '-build.log');

    $this->CheckLog(
        log => $buildLog,
        notAllowed => 'tinderbox: status: failed',
    );
}

sub Announce {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $product = $config->Get(var => 'product');
    my $productTag = $config->Get(var => 'productTag');
    my $version = $config->Get(var => 'version');
    my $rc = $config->Get(var => 'rc');
    my $logDir = $config->Get(var => 'logDir');

    my $rcTag = $productTag . '_RC' . $rc;
    my $buildLog = catfile($logDir, 'build_' . $rcTag . '-build.log');

    $this->SendAnnouncement(
      subject => "$product $version build step finished",
      message => "$product $version en-US build is ready to be copied to the candidates dir.",
    );
}

1;
