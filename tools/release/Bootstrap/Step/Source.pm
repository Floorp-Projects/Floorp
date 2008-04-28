#
# Source step. Creates a source tarball equivalent to what was used to 
# build the binary release, in the Build step.
# 
package Bootstrap::Step::Source;
use Bootstrap::Step;
use Bootstrap::Config;
use File::Copy qw(move);
use File::Find qw(find);
use MozBuild::Util qw(MkdirWithPath);
@ISA = ("Bootstrap::Step");

sub Execute {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $product = $config->Get(var => 'product');
    my $appName = $config->Get(var => 'appName');
    my $productTag = $config->Get(var => 'productTag');
    my $version = $config->GetVersion(longName => 0);
    my $build = $config->Get(var => 'build');
    my $logDir = $config->Get(sysvar => 'logDir');
    my $sourceDir = $config->Get(var => 'sourceDir');
    my $mozillaCvsroot = $config->Get(var => 'mozillaCvsroot');

    # create staging area
    my $versionedSourceDir = catfile($sourceDir, $product . '-' . $version, 
                           'batch-source', 'build' . $build);

    if (not -d $versionedSourceDir) {
        MkdirWithPath(dir => $versionedSourceDir) 
          or die("Cannot create $versionedSourceDir: $!");
    }

    $this->CvsCo(cvsroot => $mozillaCvsroot,
                 tag => $productTag . '_RELEASE',
                 modules => ['mozilla/client.mk',
                             catfile('mozilla', $appName, 'config')],
                 workDir => $versionedSourceDir,
                 logFile => catfile($logDir, 'source.log')
    );
                 
    $this->Shell(
      cmd => 'make',
      cmdArgs => ['-f', 'client.mk', 'checkout',
                  'MOZ_CO_PROJECT=' . $appName . ',xulrunner'],
      dir => catfile($versionedSourceDir, 'mozilla'),
      logFile => catfile($logDir, 'source.log'),
    );

    # change all CVS/Root files to anonymous CVSROOT
    File::Find::find(\&CvsChrootCallback, catfile($versionedSourceDir, 
                     'mozilla'));

    # remove leftover mozconfig files
    unlink(glob(catfile($versionedSourceDir, 'mozilla', '.mozconfig*')));

    my $tarFile = $product . '-' . $version . '-' . 'source' . '.tar.bz2';

    $this->Shell(
      cmd => 'tar',
      cmdArgs => ['-cjf', $tarFile, 'mozilla'],
      dir => catfile($versionedSourceDir),
      logFile => catfile($logDir, 'source.log'),
    );
              
    chmod(0644, glob("$versionedSourceDir/$tarFile"));
}

sub Verify {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $logDir = $config->Get(sysvar => 'logDir');

    my $logFile = catfile($logDir, 'source.log');

    $this->CheckLog(
        log => $logFile,
        checkFor => '^checkout finish',
    );

    $this->CheckLog(
        log => $logFile,
        notAllowed => '^tar',
    );
}

sub Push {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $product = $config->Get(var => 'product');
    my $version = $config->GetVersion(longName => 0);
    my $build = $config->Get(var => 'build');
    my $logDir = $config->Get(sysvar => 'logDir');
    my $sourceDir = $config->Get(var => 'sourceDir');
    my $stagingUser = $config->Get(var => 'stagingUser');
    my $stagingServer = $config->Get(var => 'stagingServer');

    my $candidateDir = $config->GetFtpCandidateDir(bitsUnsigned => 0);

    my $versionedSourceDir =  catfile($sourceDir, $product . '-' . $version);

    $this->CreateCandidatesDir();

    $this->Shell(
      cmd => 'rsync',
      cmdArgs => ['-av', '-e', 'ssh', catfile('batch-source', 'build' . $build, 
                            $product . '-' . $version . '-source.tar.bz2'),
                  $stagingUser . '@' . $stagingServer . ':' . $candidateDir],
      logFile => catfile($logDir, 'source.log'),
      dir => catfile($versionedSourceDir),
    );
}

sub Announce {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $product = $config->Get(var => 'product');
    my $version = $config->GetVersion(longName => 0);

    $this->SendAnnouncement(
      subject => "$product $version source step finished",
      message => "$product $version source archive was copied to the candidates dir.",
    );
}

# Change the CVS/Root file to be the anonymous CVS Root
sub CvsChrootCallback {
    my $config = new Bootstrap::Config();
    my $anonCvsroot = $config->Get(var => 'anonCvsroot');

    my $dirent = $File::Find::name;
    if ((-f $dirent) and ($dirent =~ /.*CVS\/Root$/)) {
        open(FILE, "> $dirent");
        print FILE "$anonCvsroot\n";
        close(FILE);
    }
}

1;
