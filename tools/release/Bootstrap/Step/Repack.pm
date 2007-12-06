#
# Repack step. Unpacks, modifies, repacks a Firefox en-US build.
# Primary use is for l10n (localization) builds.
#
package Bootstrap::Step::Repack;
use Bootstrap::Step;
use Bootstrap::Config;
use Bootstrap::Util qw(CvsCatfile SyncToStaging);
use MozBuild::Util qw(MkdirWithPath);
@ISA = ("Bootstrap::Step");

sub Execute {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $l10n_buildDir = $config->Get(sysvar => 'l10n_buildDir');
    my $productTag = $config->Get(var => 'productTag');
    my $rc = $config->Get(var => 'rc');
    my $logDir = $config->Get(sysvar => 'logDir');
    my $l10n_buildPlatform = $config->Get(sysvar => 'l10n_buildPlatform');
    my $sysname = $config->SystemInfo(var => 'sysname');
    my $rcTag = $productTag . '_RC' . $rc;

    my $buildLog = catfile($logDir, 'repack_' . $rcTag . '-build-l10n.log');
    my $lastBuilt = catfile($l10n_buildDir, $l10n_buildPlatform, 'last-built');
    unlink($lastBuilt) 
      or $this->Log(msg => "Cannot unlink last-built file $lastBuilt: $!");
    $this->Log(msg => "Unlinked $lastBuilt");

    # For Cygwin only, ensure that the system mount point is textmode
    # This forces CVS to use DOS-style carriage-return EOL characters.
    if ($sysname =~ /cygwin/i) {
        $this->Shell(
          cmd => 'mount',
          cmdArgs => ['-t', '-sc', '/cygdrive'],
          dir => $buildDir,
        );
    }

    $this->Shell(
      cmd => './build-seamonkey.pl',
      cmdArgs => ['--once', '--mozconfig', 'mozconfig', '--depend', 
                  '--config-cvsup-dir', 
                  catfile($l10n_buildDir, 'tinderbox-configs')],
      dir => $l10n_buildDir,
      logFile => $buildLog,
      timeout => 36000
    );

    # For Cygwin only, set the system mount point back to binmode
    # This forces CVS to use Unix-style linefeed EOL characters.
    if ($sysname =~ /cygwin/i) {
        $this->Shell(
          cmd => 'mount',
          cmdArgs => ['-b', '-sc', '/cygdrive'],
          dir => $buildDir,
        );
    }
}

sub Verify {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $productTag = $config->Get(var => 'productTag');
    my $product = $config->Get(var => 'product');
    my $rc = $config->Get(var => 'rc');
    my $oldRc = $config->Get(var => 'oldRc');
    my $logDir = $config->Get(sysvar => 'logDir');
    my $version = $config->Get(var => 'version');
    my $oldVersion = $config->Get(var => 'oldVersion');
    my $mozillaCvsroot = $config->Get(var => 'mozillaCvsroot');
    my $verifyDir = $config->Get(var => 'verifyDir');
    my $stagingServer = $config->Get(var => 'stagingServer');
    my $useTarGz = $config->Exists(var => 'useTarGz') ?
     $config->Get(var => 'useTarGz') : 0;
    my $rcTag = $productTag.'_RC'.$rc;

    my $linuxExtension = ($useTarGz) ? '.gz' : '.bz2';
    # l10n metadiff test

    my $verifyDirVersion = catfile($verifyDir, $product . '-' . $version);

    MkdirWithPath(dir => $verifyDirVersion)
      or die("Cannot mkdir $verifyDirVersion: $!");

    # check out l10n verification scripts
    foreach my $dir ('common', 'l10n') {
        $this->CvsCo(cvsroot => $mozillaCvsroot,
                     checkoutDir => $dir,
                     modules => [CvsCatfile('mozilla', 'testing', 'release',
                                            $dir)],
                     workDir => $verifyDirVersion,
                     logFile => catfile($logDir,
                                 'repack_checkout-l10n_verification.log')
        );
    }

    # Download current release
    $this->Shell(
      cmd => 'rsync',
      cmdArgs => ['-Lav', 
                  '-e', 'ssh', 
                  '--include=*.dmg',
                  '--include=*.exe',
                  '--include=*.tar'.$linuxExtension,
                  '--exclude=*',
                  $stagingServer . ':/home/ftp/pub/' . $product
                  . '/nightly/' . $version . '-candidates/rc' . $rc . '/*',
                  $product . '-' . $version . '-rc' . $rc . '/',
                 ],
      dir => catfile($verifyDirVersion, 'l10n'),
      logFile => 
        catfile($logDir, 'repack_verify-download_' . $version . '.log'),
      timeout => 3600
    );

    # Download previous release
    $this->Shell(
      cmd => 'rsync',
      cmdArgs => ['-Lav', 
                  '-e', 'ssh', 
                  '--include=*.dmg',
                  '--include=*.exe',
                  '--include=*.tar'.$linuxExtension,
                  '--exclude=*',
                  $stagingServer . ':/home/ftp/pub/' . $product
                  . '/nightly/' . $oldVersion . '-candidates/rc' 
                  . $oldRc . '/*',
                  $product . '-' . $oldVersion . '-rc' . $oldRc . '/',
                 ],
      dir => catfile($verifyDirVersion, 'l10n'),
      logFile => 
        catfile($logDir, 'repack_verify-download_' . $oldVersion . '.log'),
      timeout => 3600
    );

    my $newProduct = $product . '-' . $version . '-' . 'rc' . $rc;
    my $oldProduct = $product . '-' . $oldVersion . '-' . 'rc' . $oldRc;

    foreach my $product ($newProduct, $oldProduct) {
        MkdirWithPath(dir => catfile($verifyDirVersion, 'l10n', $product))
          or die("Cannot mkdir $verifyDirVersion/$product: $!");

        $this->Shell(
          cmd => './verify_l10n.sh',
          cmdArgs => [$product],
          dir => catfile($verifyDirVersion, 'l10n'),
          logFile => catfile($logDir, 
                             'repack_' . $product . '-l10n_verification.log'),
        );


        foreach my $rule ('^FAIL', '^Binary') {
            eval {
                $this->CheckLog(
                    log => $logDir . 
                           '/repack_' . $product . '-l10n_verification.log',
                    notAllowed => $rule,
                );
            };
            if ($@) {
                $this->Log('msg' => 
                           "WARN: $rule found in l10n metadiff output!");
            }
        }

        $this->CheckLog(
            log => $logDir . '/repack_' . $product . '-l10n_verification.log',
            notAllowed => '^Only',
        );
    }

    # generate metadiff
    $this->Shell(
      cmd => 'diff',
      cmdArgs => ['-r', 
                  catfile($newProduct, 'diffs'),
                  catfile($oldProduct, 'diffs'),
                 ],
      ignoreExitValue => 1,
      dir => catfile($verifyDirVersion, 'l10n'),
      logFile => catfile($logDir, 'repack_metadiff-l10n_verification.log'),
    );
}

sub Push {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $productTag = $config->Get(var => 'productTag');
    my $rc = $config->Get(var => 'rc');
    my $logDir = $config->Get(sysvar => 'logDir');  
    my $stagingUser = $config->Get(var => 'stagingUser');
    my $stagingServer = $config->Get(var => 'stagingServer');

    my $rcTag = $productTag . '_RC' . $rc;
    my $buildLog = catfile($logDir, 'repack_' . $rcTag . '-build-l10n.log');
    my $pushLog  = catfile($logDir, 'repack_' . $rcTag . '-push-l10n.log');
    
    my $logParser = new MozBuild::TinderLogParse(
        logFile => $buildLog,
    );
    my $pushDir = $logParser->GetPushDir();
    if (! defined($pushDir)) {
        die("No pushDir found in $buildLog");
    }
    $pushDir =~ s!^http://ftp.mozilla.org/pub/mozilla.org!/home/ftp/pub!;

    my $candidateDir = $config->GetFtpCandidateDir(bitsUnsigned => 1);

    my $osFileMatch = $config->SystemInfo(var => 'osname');
    if ($osFileMatch eq 'win32')  {
      $osFileMatch = 'win';
    } elsif ($osFileMatch eq 'macosx') {
      $osFileMatch = 'mac';
    }

    $this->Shell(
      cmd => 'ssh',
      cmdArgs => ['-2', '-l', $stagingUser, $stagingServer,
                  'mkdir -p ' . $candidateDir],
      logFile => $pushLog,
    );

    $this->Shell(
      cmd => 'ssh',
      cmdArgs => ['-2', '-l', $stagingUser, $stagingServer,
                  'rsync', '-av',
                  '--include=*' . $osFileMatch . '*',
                  '--include=*.xpi',
                  '--exclude=*', 
                  $pushDir, $candidateDir],
      logFile => $pushLog,
    );

    SyncToStaging(); 
}

sub Announce {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $product = $config->Get(var => 'product');
    my $productTag = $config->Get(var => 'productTag');
    my $version = $config->Get(var => 'version');
    my $rc = $config->Get(var => 'rc');
    my $logDir = $config->Get(sysvar => 'logDir');

    my $rcTag = $productTag . '_RC' . $rc;
    my $buildLog = catfile($logDir, 'repack_' . $rcTag . '-build-l10n.log');

    my $logParser = new MozBuild::TinderLogParse(
        logFile => $buildLog,
    );
    my $buildID = $logParser->GetBuildID();
    my $pushDir = $logParser->GetPushDir();

    if (! defined($pushDir)) {
        die("No pushDir found in $buildLog");
    } 

    $this->SendAnnouncement(
      subject => "$product $version l10n repack step finished",
      message => "$product $version l10n builds were copied to the candidates directory.\nPush Dir was $pushDir",
    );
}

1;
