#
# Updates step. Generates binary update (MAR) files as well as AUS config
# snippets.
# 
package Bootstrap::Step::Updates;
use Bootstrap::Step;
use File::Find;
use MozBuild::Util;
@ISA = ("Bootstrap::Step");

sub Execute {
    my $this = shift;

    my $product = $this->Config('var' => 'product');
    my $logDir = $this->Config('var' => 'logDir');
    my $version = $this->Config('var' => 'version');
    my $mozillaCvsroot = $this->Config('var' => 'mozillaCvsroot');
    my $mofoCvsroot = $this->Config('var' => 'mofoCvsroot');
    my $updateDir = $this->Config('var' => 'updateDir');
    my $patcherConfig = $this->Config('var' => 'patcherConfig');

    # Create updates area.
    if (not -d $updateDir) {
        MkdirWithPath('dir' => $updateDir) or die "Cannot mkdir $updateDir: $!";
    }

    $this->Shell(
      'cmd' => 'cvs -d ' . $mozillaCvsroot . ' co -d patcher mozilla/tools/patcher',
      'logFile' => $logDir . '/patcher_checkout.log',
      'dir' => $updateDir,
      'timeout' => 3600,
    );

    # config lives in private repo
    $this->Shell(
      'cmd' => 'cvs -d ' . $mofoCvsroot . ' co -d config release/patcher/' . $patcherConfig,
      'logFile' => $logDir . '/patcher_config-checkout.log',
      'dir' => $updateDir,
    );

    # build tools
    my $originalCvsrootEnv = $ENV{'CVSROOT'};
    $ENV{'CVSROOT'} = $mozillaCvsroot;
    $this->Shell(
      'cmd' => './patcher2.pl --build-tools --app=' . $product . ' --config=../config/moz180-branch-patcher2.cfg',
      'logFile' => $logDir . '/patcher_build-tools.log',
      'dir' => $updateDir . '/patcher',
      'timeout' => 3600,
    );
    if ($originalCvsrootEnv) {
        $ENV{'CVSROOT'} = $originalCvsrootEnv;
    }
    
    # download complete MARs
    $this->Shell(
      'cmd' => './patcher2.pl --download --app=' . $product . ' --config=../config/moz180-branch-patcher2.cfg',
      'logFile' => $logDir . '/patcher_download.log',
      'dir' => $updateDir . '/patcher',
      'timeout' => 3600,
    );

    # Create partial patches and snippets
    $this->Shell(
      'cmd' => './patcher2.pl --create-patches -app=' . $product . ' --config=../config/moz180-branch-patcher2.cfg',
      'logFile' => $logDir . '/patcher_create-patches.log',
      'dir' => $updateDir . '/patcher',
      'timeout' => 18000,
    );
    
    # prepare aus2-staging
    # ssh aus2-staging.mozilla.org
    # cd /opt/aus2/incoming/3-staging
    # tmpdir="`date +%Y%m%d`-${SHORT_PRODUCT}-${VERSION}"
    # sudo mkdir ${tmpdir}-test
    # sudo chown cltbld ${tmpdir}-test
    # sudo mkdir ${tmpdir}
    # sudo chown cltbld ${tmpdir}
    
    # # copy updates from prometheus-vm.mozilla.org
    # ssh prometheus-vm.mozilla.org
    # cd /builds/${VERSION}-updates/release/patcher/temp/firefox/${PREVIOUS_VERSION}-${VERSION}/
    # rsync -nav -e "ssh -i $HOME/.ssh/aus" aus2.test/ aus2-staging.mozilla.org:/opt/aus2/incoming/3-staging/${tmpdir}-test/
    # rsync -nav -e "ssh -i $HOME/.ssh/aus" aus2/ aus2-staging.mozilla.org:/opt/aus2/incoming/3-staging/${tmpdir}/
}

sub Verify {
    my $this = shift;

    my $logDir = $this->Config('var' => 'logDir');
    my $version = $this->Config('var' => 'version');
    my $oldVersion = $this->Config('var' => 'oldVersion');
    my $mozillaCvsroot = $this->Config('var' => 'mozillaCvsroot');
    my $updateDir = $this->Config('var' => 'updateDir');
    my $verifyDir = $this->Config('var' => 'verifyDir');

    ### quick verification
    # ensure that there are only test channels
    my $testDir = $verifyDir . '/' . $version . '-updates/patcher/temp/' . $product . $oldVersion . '-' . $version . '/aus2.test';

    File::Find::find(\&TestAusCallback, $testDir);

    # Create verification area.
    my $verifyDirVersion = $verifyDir . $version;
    MkdirWithPath('dir' => $verifyDirVersion) 
      or die("Could not mkdir $verifyDirVersion: $!");

    $this->Shell(
      'cmd' => 'cvs -d ' . $mozillaCvsroot . ' co -d updates mozilla/testing/release/updates/',
      'logFile' => $logDir . '/verify-updates_checkout.log',
      'dir' => $verifyDirVersion,
    );
    $this->Shell(
      'cmd' => 'cvs -d ' . $mozillaCvsroot . ' co -d common mozilla/testing/release/common/',
      'logFile' => $logDir . '/verify-updates_checkout.log',
      'dir' => $verifyDirVersion,
    );
    
    # Customize updates.cfg to contain the channels you are interested in 
    # testing.
    $this->Shell(
      'cmd' => './verify.sh -t',
      'logFile' => $logDir . '/verify_updates.log',
      'dir' => $verifyDirVersion . '/updates',
      'timeout' => 3600,
    );
}

sub TestAusCallback { 
    my $dir = $File::Find::name;
    if ($dir =~ /test/) { 
        die("Non-test directory found in $testDir/aus2.test: $dir");
    }
}

1;
