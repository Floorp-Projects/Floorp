#
# Build step. Calls tinderbox to produce en-US Firefox build.
#
package Bootstrap::Step::Build;

use File::Temp qw(tempfile);

use Bootstrap::Step;
use Bootstrap::Util qw(CvsCatfile SyncToStaging);

use MozBuild::Util qw(MkdirWithPath);

@ISA = ("Bootstrap::Step");

sub Execute {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $version = $config->GetVersion(longName => 0);
    my $buildDir = $config->Get(sysvar => 'buildDir');
    my $productTag = $config->Get(var => 'productTag');
    my $rc = $config->Get(var => 'rc');
    my $buildPlatform = $config->Get(sysvar => 'buildPlatform');
    my $logDir = $config->Get(sysvar => 'logDir');
    my $sysname = $config->SystemInfo(var => 'sysname');    
    my $rcTag = $productTag . '_RC' . $rc;

    if ($version eq 'nightly') {
        $this->Log(msg => 'Skip force-clobber for nightly mode');
    } else {
        my $lastBuilt = catfile($buildDir, $buildPlatform, 'last-built');
        if (! unlink($lastBuilt)) {
            $this->Log(msg => "Cannot unlink last-built file $lastBuilt: $!");
        } else {
            $this->Log(msg => "Unlinked $lastBuilt");
        }
    }

    my $buildLog = catfile($logDir, 'build_' . $rcTag . '-build.log');
 
    # For Cygwin only, ensure that the system mount point is binmode
    # This forces CVS to use Unix-style linefeed EOL characters.
    if ($sysname =~ /cygwin/i) {
        $this->Shell(
          cmd => 'mount',
          cmdArgs => ['-b', '-sc', '/cygdrive'],
          dir => $buildDir,
        );
    }
  
    $this->Shell(
      cmd => './build-seamonkey.pl',
      cmdArgs => ['--once', '--mozconfig', 'mozconfig', '--depend', 
                  '--config-cvsup-dir', 
                  catfile($buildDir, 'tinderbox-configs')],
      dir => $buildDir,
      logFile => $buildLog,
      timeout => 36000
    );

    if ($version eq 'nightly') {
        $this->Log(msg => 'Skip buildID storage for nightly mode');
    } else {
        $this->StoreBuildID();
    }

    # proxy for version is 2.0.0.* and osname is win32
    if ($sysname =~ /cygwin/i) {
        $this->PublishTalkbackSymbols();
    }
}

sub Verify {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $version = $config->GetVersion(longName => 0);
    my $buildDir = $config->Get(sysvar => 'buildDir');
    my $productTag = $config->Get(var => 'productTag');
    my $rc = $config->Get(var => 'rc');
    my $rcTag = $productTag.'_RC'.$rc;
    my $logDir = $config->Get(sysvar => 'logDir');

    if ($version eq 'nightly') {
        $this->Log(msg => 'Skip Verify for nightly mode');
        return;
    }

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

sub Push {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $version = $config->GetVersion(longName => 0);
    my $productTag = $config->Get(var => 'productTag');
    my $rc = $config->Get(var => 'rc');
    my $logDir = $config->Get(sysvar => 'logDir');
    my $stagingUser = $config->Get(var => 'stagingUser');
    my $stagingServer = $config->Get(var => 'stagingServer');

    if ($version eq 'nightly') {
        $this->Log(msg => 'Skip Push for nightly mode');
        return;
    }

    my $rcTag = $productTag . '_RC' . $rc;
    my $buildLog = catfile($logDir, 'build_' . $rcTag . '-build.log');
    my $pushLog  = catfile($logDir, 'build_' . $rcTag . '-push.log');

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

    # TODO - use a more generic function for this kind of remapping
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

    # Make sure permissions are created on the server correctly;
    #
    # Note the '..' at the end of the chmod string; this is because
    # Config::GetFtpCandidateDir() returns the full path, including the
    # rcN directories on the end. What we really want to ensure
    # have the correct permissions (from the mkdir call above) is the
    # firefox/nightly/$version-candidates/ directory.
    #
    # XXX - This is ugly; another solution is to fix the umask on stage, or
    # change what GetFtpCandidateDir() returns.

    my $chmodArg = CvsCatfile($config->GetFtpCandidateDir(bitsUnsigned => 0), 
     '..');

    $this->Shell(
      cmd => 'ssh',
      cmdArgs => ['-2', '-l', $stagingUser, $stagingServer,
                  'chmod 0755 ' . $chmodArg],
      logFile => $pushLog,
    );

    $this->Shell(
      cmd => 'ssh',
      cmdArgs => ['-2', '-l', $stagingUser, $stagingServer,
                  'rsync', '-av', 
                  '--include=*' . $osFileMatch . '*',
                  '--exclude=*', 
                  $pushDir, 
                  $candidateDir],
      logFile => $pushLog,
    );

    SyncToStaging(); 
}

sub Announce {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $version = $config->GetVersion(longName => 0);
    my $product = $config->Get(var => 'product');
    my $productTag = $config->Get(var => 'productTag');
    my $rc = $config->Get(var => 'rc');
    my $logDir = $config->Get(sysvar => 'logDir');

    if ($version eq 'nightly') {
        $this->Log(msg => 'Skip Announce for nightly mode');
        return;
    }

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
      message => "$product $version en-US build was copied to the candidates dir.\nBuild ID is $buildID\nPush Dir was $pushDir",
    );
}

sub StoreBuildID() {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $productTag = $config->Get(var => 'productTag');
    my $rc = $config->Get(var => 'rc');
    my $logDir = $config->Get(sysvar => 'logDir');
    my $stagingUser = $config->Get(var => 'stagingUser');
    my $stagingServer = $config->Get(var => 'stagingServer');

    my $rcTag = $productTag . '_RC' . $rc;
    my $buildLog = catfile($logDir, 'build_' . $rcTag . '-build.log');
    my $pushLog  = catfile($logDir, 'build_' . $rcTag . '-push.log');

    my $logParser = new MozBuild::TinderLogParse(
        logFile => $buildLog,
    );
    my $pushDir = $logParser->GetPushDir();
    if (! defined($pushDir)) {
        die("No pushDir found in $buildLog");
    }
    $pushDir =~ s!^http://ftp.mozilla.org/pub/mozilla.org!/home/ftp/pub!;

    # drop os-specific buildID file on FTP
    my $buildID = $logParser->GetBuildID();
    if (! defined($buildID)) {
        die("No buildID found in $buildLog");
    }
    if (! $buildID =~ /^\d+$/) {
        die("ASSERT: Build: build ID is not numerical: $buildID")
    }


    my $osFileMatch = $config->SystemInfo(var => 'osname');    

    my ($bh, $buildIDTempFile) = tempfile(DIR => '.');
    print $bh 'buildID=' . $buildID;
    $bh->close() || 
     die("Could not open buildID temp file $buildIDTempFile: $!");
    chmod(0644, $buildIDTempFile);

    my $buildIDFile = $osFileMatch . '_info.txt';
    $this->Shell(
      cmd => 'scp',
      cmdArgs => ['-p', $buildIDTempFile, 
                  $stagingUser . '@' . $stagingServer . ':' .
                  $pushDir . '/' . $buildIDFile],
      logFile => $pushLog,
    );
}

sub PublishTalkbackSymbols() {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $product = $config->Get(var => 'product');
    my $version = $config->Get(var => 'version');
    my $rc = $config->Get(var => 'rc');
    my $productTag = $config->Get(var => 'productTag');
    my $logDir = $config->Get(sysvar => 'logDir');
    my $buildDir = $config->Get(sysvar => 'buildDir');
    my $buildPlatform = $config->Get(sysvar => 'buildPlatform');
    my $symbolDir = $config->Get(var => 'symbolDir');
    my $mozillaCvsroot = $config->Get(var => 'mozillaCvsroot');
    my $symbolServer = $config->Get(var => 'symbolServer');
    my $symbolServerUser = $config->Get(var => 'symbolServerUser');
    my $symbolServerPath = $config->Get(var => 'symbolServerPath');
    my $symbolServerKey = $config->Get(var => 'symbolServerKey');

    my $rcTag = $productTag . '_RC' . $rc;
    my $buildLog = catfile($logDir, 'build_' . $rcTag . '-build.log');
    my $symbolLog  = catfile($logDir, 'build_' . $rcTag . '-symbols.log');
    my $versionedSymbolDir = catfile($symbolDir, $product . '-' . $version,
                                     'rc' . $rc);

    # Create symbols work area.
    if (-e $versionedSymbolDir) {
       die("ASSERT: Build:PublishTalkbackSymbols(): $versionedSymbolDir already exists?"); 
    }
    MkdirWithPath(dir => $versionedSymbolDir) 
       or die("Cannot mkdir $versionedSymbolDir: $!");

    # checkouts
    $this->CvsCo(cvsroot => $mozillaCvsroot,
                 checkoutDir => 'tools',
                 modules => [CvsCatfile('mozilla', 'toolkit', 
                                        'crashreporter', 'tools')],
                 logFile => $symbolLog,
                 workDir => $versionedSymbolDir
    );

    # unpack the symbols tarball from tinderbox
    my $logParser = new MozBuild::TinderLogParse(
        logFile => $buildLog,
    );
    my $buildID = $logParser->GetBuildID();
    if (! $buildID =~ /^\d{10}$/) {
       die("ASSERT: Build:PublishTalkbackSymbols(): No buildID found in $buildLog");
    }

    # yields dir $versionedSymbolDir/$buildID
    $this->Shell(
       cmd => 'tar',
       cmdArgs => ['xfj', 
                   catfile($buildDir, $buildPlatform, $buildID, 'symbols', 
                           $buildID.'.tar.bz2')],
       dir => $versionedSymbolDir,
       logFile => $symbolLog,
    );

    # process symbols
    my $symbolOutputDir = catfile($versionedSymbolDir,'symbol');
    MkdirWithPath(dir => $symbolOutputDir) 
       or die("Cannot mkdir $symbolOutputDir: $!");

    my @pdbFiles = glob(catfile($versionedSymbolDir, $buildID, '*.pdb'));
    foreach $pdbFile (@pdbFiles) {
       $pdbFile =~ s/.*($buildID.*)/$1/;
       $this->Shell(
          cmd => 'tools/symbolstore.py',
          cmdArgs => ['-c', 'tools/win32/dump_syms.exe',
                      'symbol',
                      $pdbFile],
          logFile => catfile($symbolOutputDir, 
                             $product . '-' . $version . 'rc' . $rc .
                                '-WINNT-' . $buildID . '-symbols.txt'),
          dir => $versionedSymbolDir,
          timeout => 600,
       );
    }

    # push the symbols to the server
    $this->Shell(
       cmd => 'zip',
       cmdArgs => ['-r', catfile($versionedSymbolDir, 'symbols.zip'), '.'],
       dir => $symbolOutputDir,
       logFile => $symbolLog,
       timeout => 600,
    );

    $ENV{'SYMBOL_SERVER_HOST'} = $symbolServer;
    $ENV{'SYMBOL_SERVER_USER'} = $symbolServerUser;
    $ENV{'SYMBOL_SERVER_SSH_KEY'} = $symbolServerKey;
    $ENV{'SYMBOL_SERVER_PATH'} = $symbolServerPath;

    $this->Shell(
       cmd => catfile('tools', 'upload_symbols.sh'),
       cmdArgs => ['symbols.zip'],
       dir => $versionedSymbolDir,
       logFile => $symbolLog,
       timeout => 600,
    );
}

1;
