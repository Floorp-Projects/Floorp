#
# Source step. Creates a source tarball equivalent to what was used to 
# build the binary release, in the Build step.
# 
package Bootstrap::Step::Source;
use Bootstrap::Step;
use Bootstrap::Config;
use File::Copy qw(move);
use MozBuild::Util qw(MkdirWithPath);
use Bootstrap::Util qw(SyncNightlyDirToStaging);
@ISA = ("Bootstrap::Step");

sub Execute {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $product = $config->Get(var => 'product');
    my $productTag = $config->Get(var => 'productTag');
    my $version = $config->Get(var => 'version');
    my $rc = $config->Get(var => 'rc');
    my $logDir = $config->Get(sysvar => 'logDir');
    my $stageHome = $config->Get(var => 'stageHome');

    # create staging area
    my $stageDir = catfile($stageHome, $product . '-' . $version, 
                           'batch-source', 'rc' . $rc);

    if (not -d $stageDir) {
        MkdirWithPath(dir => $stageDir) 
          or die("Cannot create $stageDir: $!");
    }

    my $srcScript = $product . '-src-tarball-nobuild';
    $this->Shell(
      cmd => catfile($stageHome, 'bin', $srcScript),
      cmdArgs => ['-r', $productTag . '_RELEASE', '-m', $version],
      dir => $stageDir,
      logFile => catfile($logDir, 'source.log'),
    );
              
    move("$stageDir/../*.bz2", $stageDir);
    chmod(0644, glob("$stageDir/*.bz2"));
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
    my $version = $config->Get(var => 'version');
    my $rc = $config->Get(var => 'rc');
    my $logDir = $config->Get(sysvar => 'logDir');
    my $stageHome = $config->Get(var => 'stageHome');

    my $stageDir =  catfile($stageHome, $product . '-' . $version);
    my $candidateDir = catfile('/home', 'ftp', 'pub', $product, 'nightly',
                            $version . '-candidates', 'rc' . $rc ) . '/';

    if (not -d $candidateDir) {
        MkdirWithPath(dir => $candidateDir) 
          or die("Could not mkdir $candidateDir: $!");
        $this->Log(msg => "Created directory $candidateDir");
    }

    $this->Shell(
      cmd => 'rsync',
      cmdArgs => ['-av', catfile('batch-source', 'rc' . $rc, 
                            $product . '-' . $version . '-source.tar.bz2'),
                  $candidateDir],
      logFile => catfile($logDir, 'source.log'),
      dir => catfile($stageDir),
    );

    SyncNightlyDirToStaging();
}

sub Announce {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $product = $config->Get(var => 'product');
    my $version = $config->Get(var => 'version');

    $this->SendAnnouncement(
      subject => "$product $version source step finished",
      message => "$product $version source archive was copied to the candidates dir.",
    );
}

1;
