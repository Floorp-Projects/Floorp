#
# Updates step. Generates binary update (MAR) files as well as AUS config
# snippets.
# 
package Bootstrap::Step::Updates;
use Bootstrap::Step;
use Bootstrap::Config;
use File::Find qw(find);
use MozBuild::Util qw(MkdirWithPath);
@ISA = ("Bootstrap::Step");

sub Execute {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $product = $config->Get(var => 'product');
    my $logDir = $config->Get(var => 'logDir');
    my $version = $config->Get(var => 'version');
    my $mozillaCvsroot = $config->Get(var => 'mozillaCvsroot');
    my $mofoCvsroot = $config->Get(var => 'mofoCvsroot');
    my $updateDir = $config->Get(var => 'updateDir');
    my $patcherConfig = $config->Get(var => 'patcherConfig');

    my $versionedUpdateDir = catfile($updateDir, $product . '-' . $version);

    # Create updates area.
    if (not -d $versionedUpdateDir) {
        MkdirWithPath(dir => $versionedUpdateDir) 
          or die("Cannot mkdir $versionedUpdateDir: $!");
    }

    # check out patcher
    $this->Shell(
      cmd => 'cvs',
      cmdArgs => ['-d', $mozillaCvsroot, 'co', '-d', 'patcher', 
                    catfile('mozilla', 'tools', 'patcher')],
      logFile => catfile($logDir, 'updates_patcher-checkout.log'),
      dir => $versionedUpdateDir,
    );

    # check out utilities
    $this->Shell(
      cmd => 'cvs',
      cmdArgs => ['-d', $mozillaCvsroot, 'co', '-d', 'MozBuild', 
                    catfile('mozilla', 'tools', 'release', 'MozBuild')],
      logFile => catfile($logDir, 'updates_patcher-utils-checkout.log'),
      dir => catfile($versionedUpdateDir, 'patcher'),
    );

    # config lives in private repo
    $this->Shell(
      cmd => 'cvs',
      cmdArgs => ['-d', $mofoCvsroot, 'co', '-d', 'config',  
                    catfile('release', 'patcher', $patcherConfig)],
      logFile => catfile($logDir, 'updates_patcher-config-checkout.log'),
      dir => $versionedUpdateDir,
    );

    # build tools
    my $originalCvsrootEnv = $ENV{'CVSROOT'};
    $ENV{'CVSROOT'} = $mozillaCvsroot;
    $this->Shell(
      cmd => './patcher2.pl',
      cmdArgs => ['--build-tools', '--app=' . $product,
                    '--config=../config/' . $patcherConfig],
      logFile => catfile($logDir, 'updates_patcher-build-tools.log'),
      dir => catfile($versionedUpdateDir, 'patcher'),
    );
    if ($originalCvsrootEnv) {
        $ENV{'CVSROOT'} = $originalCvsrootEnv;
    }
    
    # download complete MARs
    $this->Shell(
      cmd => './patcher2.pl',
      cmdArgs => ['--download', '--app=' . $product,
                    '--config=../config/' . $patcherConfig],
      logFile => catfile($logDir, 'updates_patcher-download.log'),
      dir => catfile($versionedUpdateDir, 'patcher'),
    );

    # Create partial patches and snippets
    $this->Shell(
      cmd => './patcher2.pl',
      cmdArgs => ['--create-patches', '--app=' . $product, 
                    '--config=../config/' . $patcherConfig],
      logFile => catfile($logDir, 'updates_patcher-create-patches.log'),
      dir => catfile($versionedUpdateDir, 'patcher'),
      timeout => 18000,
    );
    
    ### quick verification
    # ensure that there are only test channels
    my $testDir = catfile($versionedUpdateDir, 'patcher', 'temp', $product,  
                          $oldVersion . '-' . $version, 'aus2.test');

    File::Find::find(\&TestAusCallback, $testDir);
}

sub Verify {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $logDir = $config->Get(var => 'logDir');
    my $version = $config->Get(var => 'version');
    my $oldVersion = $config->Get(var => 'oldVersion');
    my $mozillaCvsroot = $config->Get(var => 'mozillaCvsroot');
    my $verifyDir = $config->Get(var => 'verifyDir');
    my $product = $config->Get(var => 'product');

    # Create verification area.
    my $verifyDirVersion = catfile($verifyDir, $product . '-' . $version);
    MkdirWithPath(dir => $verifyDirVersion) 
      or die("Could not mkdir $verifyDirVersion: $!");

    foreach my $dir ('updates', 'common') {
        $this->Shell(
          cmd => 'cvs',
          cmdArgs => ['-d', $mozillaCvsroot, 'co', '-d', $dir,
                        catfile('mozilla', 'testing', 'release', $dir)],
          logFile => catfile($logDir, 
                               'updates_verify_checkout-' . $dir . '.log'),
          dir => $verifyDirVersion,
        );
    }
    
    # Customize updates.cfg to contain the channels you are interested in 
    # testing.
    $this->Shell(
      cmd => './verify.sh', 
      cmdArgs => ['-c'],
      logFile => catfile($logDir, 'updates_verify.log'),
      dir => catfile($verifyDirVersion, 'updates'),
    );
}

sub TestAusCallback { 
    my $dir = $File::Find::name;
    if (($dir =~ /beta/) or ($dir =~ /release/)) {
        if (not $dir =~ /test/) { 
            die("Non-test directory found in $testDir/aus2.test: $dir");
        }
    }
}

sub Announce {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $product = $config->Get(var => 'product');
    my $version = $config->Get(var => 'version');

    $this->SendAnnouncement(
      subject => "$product $version update step finished",
      message => "$product $version updates are ready to be deployed to AUS and the candidates dir.",
    );
}

1;
