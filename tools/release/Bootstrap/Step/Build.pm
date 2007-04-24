#
# Build step. Calls tinderbox to produce en-US Firefox build.
#
package Bootstrap::Step::Build;
use Bootstrap::Step;
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
    if (! unlink($lastBuilt)) {
        $this->Log(msg => "Cannot unlink last-built file $lastBuilt: $!");
    } else {
        $this->Log(msg => "Unlinked $lastBuilt");
    }

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
    my $buildDir = $config->Get(sysvar => 'buildDir');
    my $productTag = $config->Get(var => 'productTag');
    my $rc = $config->Get(var => 'rc');
    my $rcTag = $productTag.'_RC'.$rc;
    my $logDir = $config->Get(var => 'logDir');

    my $buildLog = catfile($logDir, 'build_' . $rcTag . '-build.log');

    $this->CheckLog(
        log => $buildLog,
        notAllowed => 'tinderbox: status: failed',
    );

    my $logParser = new MozBuild::TinderLogParse(
        logFile => $buildLog,
    );

    if (! defined($logParser->GetBuildID())) {
        die("No buildID found in $buildLog");
    }
    if (! defined($logParser->GetPushDir())) {
        die("No pushDir found in $buildLog");
    }
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

    my $logParser = new MozBuild::TinderLogParse(
        logFile => $buildLog,
    );
    my $buildID = $logParser->GetBuildID();
    my $pushDir = $logParser->GetPushDir();

    if (! defined($buildID)) {
        die("No buildID found in $buildLog");
    } 
    if (! defined($pushDir)) {
        die("No pushDir found in $buildLog");
    } 

    $this->SendAnnouncement(
      subject => "$product $version build step finished",
      message => "$product $version en-US build is ready to be copied to the candidates dir.\nBuild ID is $buildID\nPush Dir is $pushDir",
    );
}

1;
