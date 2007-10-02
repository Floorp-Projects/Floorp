#
# Updates step. Generates binary update (MAR) files as well as AUS config
# snippets.
# 
package Bootstrap::Step::Updates;

use Bootstrap::Step;
use Bootstrap::Config;
use Bootstrap::Util qw(CvsCatfile SyncNightlyDirToStaging);

use File::Find qw(find);
use POSIX qw(strftime);

use MozBuild::Util qw(MkdirWithPath);

@ISA = ("Bootstrap::Step");

sub Execute {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $product = $config->Get(var => 'product');
    my $logDir = $config->Get(sysvar => 'logDir');
    my $oldVersion = $config->Get(var => 'oldVersion');
    my $version = $config->Get(var => 'version');
    my $mozillaCvsroot = $config->Get(var => 'mozillaCvsroot');
    my $mofoCvsroot = $config->Get(var => 'mofoCvsroot');
    my $updateDir = $config->Get(var => 'updateDir');
    my $patcherConfig = $config->Get(var => 'patcherConfig');
    my $patcherToolsRev = $config->Get(var => 'patcherToolsRev');

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
                  CvsCatfile('mozilla', 'tools', 'patcher')],
      logFile => catfile($logDir, 'updates_patcher-checkout.log'),
      dir => $versionedUpdateDir,
    );

    # check out utilities
    $this->Shell(
      cmd => 'cvs',
      cmdArgs => ['-d', $mozillaCvsroot, 'co', '-d', 'MozBuild', 
                  CvsCatfile('mozilla', 'tools', 'release', 'MozBuild')],
      logFile => catfile($logDir, 'updates_patcher-utils-checkout.log'),
      dir => catfile($versionedUpdateDir, 'patcher'),
    );

    # config lives in private repo
    $this->Shell(
      cmd => 'cvs',
      cmdArgs => ['-d', $mofoCvsroot, 'co', '-d', 'config',  
                  CvsCatfile('release', 'patcher', $patcherConfig)],
      logFile => catfile($logDir, 'updates_patcher-config-checkout.log'),
      dir => $versionedUpdateDir,
    );

    # build tools
    my $originalCvsrootEnv = $ENV{'CVSROOT'};
    $ENV{'CVSROOT'} = $mozillaCvsroot;
    $this->Shell(
      cmd => './patcher2.pl',
      cmdArgs => ['--build-tools', '--tools-revision=' . $patcherToolsRev,
                  '--app=' . $product, 
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
    my $fullUpdateDir = catfile($versionedUpdateDir, 'patcher', 'temp', 
                          $product, $oldVersion . '-' . $version);
    $snippetErrors = undef;   # evil (??) global to get results from callbacks
    
    # ensure that there are only test channels in aus2.test dir
    File::Find::find(\&TestAusCallback, catfile($fullUpdateDir,"aus2.test"));

    # ensure that there are only beta channels in beta dir (if that exists)
    if (-d catfile($fullUpdateDir, "aus2.beta")) {
      File::Find::find(\&BetaAusCallback, catfile($fullUpdateDir,"aus2.beta"));
      File::Find::find(\&ReleaseAusCallback, catfile($fullUpdateDir,"aus2"));
    } 
    # otherwise allow beta and release in aus2 dir
    else {
      File::Find::find(\&ReleaseBetaAusCallback, catfile($fullUpdateDir,"aus2"));
    }    

    if ($snippetErrors) {
        $snippetErrors =~ s!$fullUpdateDir/!!g;
	die("Execute: Snippets failed location checks: $snippetErrors\n");
    }
}

sub Verify {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $logDir = $config->Get(sysvar => 'logDir');
    my $version = $config->Get(var => 'version');
    my $mozillaCvsroot = $config->Get(var => 'mozillaCvsroot');
    my $verifyDir = $config->Get(var => 'verifyDir');
    my $product = $config->Get(var => 'product');
    my $verifyConfig = $config->Get(sysvar => 'verifyConfig');

    # Create verification area.
    my $verifyDirVersion = catfile($verifyDir, $product . '-' . $version);
    MkdirWithPath(dir => $verifyDirVersion) 
      or die("Could not mkdir $verifyDirVersion: $!");

    foreach my $dir ('updates', 'common') {
        $this->Shell(
          cmd => 'cvs',
          cmdArgs => ['-d', $mozillaCvsroot, 'co', '-d', $dir,
                      CvsCatfile('mozilla', 'testing', 'release', $dir)],
          logFile => catfile($logDir, 
                               'updates_verify_checkout-' . $dir . '.log'),
          dir => $verifyDirVersion,
        );
    }
    
    # Customize updates.cfg to contain the channels you are interested in 
    # testing.
    
    my $verifyLog = catfile($logDir, 'updates_verify.log');
    $this->Shell(
      cmd => './verify.sh', 
      cmdArgs => ['-c', $verifyConfig],
      logFile => $verifyLog,
      dir => catfile($verifyDirVersion, 'updates'),
      timeout => 36000,
    );

    $this->CheckLog(
        log => $verifyLog,
        notAllowed => '^FAIL',
    );
}

# locate snippets for which the channel doesn't end in test
sub TestAusCallback { 
    my $dir = $File::Find::name;
    if ( ($dir =~ /\.txt/) and 
         (not $dir =~ /\/\w*test\/(partial|complete)\.txt$/)) {
           $snippetErrors .= "\nNon-test: $dir";
    }
}

# locate snippets for which the channel isn't beta
sub BetaAusCallback { 
    my $dir = $File::Find::name;
    if ( ($dir =~ /\.txt/) and 
         (not $dir =~ /\/beta\/(partial|complete)\.txt$/)) {
           $snippetErrors .= "\nNon-beta: $dir";
    }
}

# locate snippets for which the channel isn't release
sub ReleaseAusCallback { 
    my $dir = $File::Find::name;
    if ( ($dir =~ /\.txt/) and 
         (not $dir =~ /\/release\/(partial|complete)\.txt$/)) {
           $snippetErrors .= "\nNon-release: $dir";
    }
}

# locate snippets for which the channel isn't release or beta
sub ReleaseBetaAusCallback { 
    my $dir = $File::Find::name;
    if ( ($dir =~ /\.txt/) and 
         (not $dir =~ /\/(release|beta)\/(partial|complete)\.txt$/)) {
           $snippetErrors .= "\nNon-release: $dir";
    }
}

sub Push {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $logDir = $config->Get(sysvar => 'logDir');
    my $product = $config->Get(var => 'product');
    my $version = $config->Get(var => 'version');
    my $rc = $config->Get(var => 'rc');
    my $oldVersion = $config->Get(var => 'oldVersion');
    my $stagingUser = $config->Get(var => 'stagingUser');
    my $stagingServer = $config->Get(var => 'stagingServer');
    my $ausUser = $config->Get(var => 'ausUser');
    my $ausServer = $config->Get(var => 'ausServer');
    my $updateDir = $config->Get(var => 'updateDir');

    my $pushLog = catfile($logDir, 'updates_push.log');
    my $fullUpdateDir = catfile($updateDir, $product . '-' . $version,
                                 'patcher', 'temp', $product, 
                                  $oldVersion . '-' . $version);
    my $candidateDir = $config->GetFtpCandidateDir(bitsUnsigned => 0);

    # push partial mar files up to ftp server
    $this->Shell(
     cmd => 'rsync',
     cmdArgs => ['-av', '-e', 'ssh',
                 '--include=*partial.mar', 
                 '--exclude=*',
                 catfile('ftp', $product, 'nightly', $version . '-candidates', 
                          'rc' . $rc) . '/',
                 $stagingUser . '@' . $stagingServer . ':' . $candidateDir],
     dir => $fullUpdateDir,
     logFile => $pushLog,
   );

    # push update snippets to AUS server
    my $targetPrefix =  CvsCatfile('/opt','aus2','snippets','staging',
                          strftime("%Y%m%d", localtime) . '-' . 
                          ucfirst($product) . '-' . $version);
    $config->Set(var => 'ausDeliveryDir', value => $targetPrefix);

    my @snippetDirs = glob(catfile($fullUpdateDir, "aus2*"));

    foreach $dir (@snippetDirs) {
      my $targetDir = $targetPrefix;
      if ($dir =~ /aus2\.(.*)$/) {
        $targetDir .= '-' . $1;
      }

      $this->Shell(
        cmd => 'rsync',
        cmdArgs => ['-av', 
                    '-e', 'ssh -i ' . catfile($ENV{'HOME'},'.ssh','aus'),
                    $dir . '/', 
                    $ausUser . '@' . $ausServer . ':' . $targetDir],
        logFile => $pushLog,
      );
    }

    SyncNightlyDirToStaging();
}

sub Announce {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $product = $config->Get(var => 'product');
    my $version = $config->Get(var => 'version');
    my $ausDeliveryDir = $config->Get(var => 'ausDeliveryDir');

    $this->SendAnnouncement(
      subject => "$product $version update step finished",
      message => "$product $version updates finished. Partial mars were copied to the candidates dir, and the snippets to AUS in $ausDeliveryDir*.",
    );
}

1;
