#
# Source step. Creates a source tarball equivalent to what was used to 
# build the binary release, in the Build step.
# 
package Bootstrap::Step::Source;
use Bootstrap::Step;
use Bootstrap::Config;
use File::Copy qw(move);
use MozBuild::Util qw(MkdirWithPath);
@ISA = ("Bootstrap::Step");

my $config = new Bootstrap::Config;

sub Execute {
    my $this = shift;

    my $product = $config->Get('var' => 'product');
    my $productTag = $config->Get('var' => 'productTag');
    my $version = $config->Get('var' => 'version');
    my $rc = $config->Get('var' => 'rc');
    my $logDir = $config->Get('var' => 'logDir');
    my $stageHome = $config->Get('var' => 'stageHome');

    # create staging area
    my $stageDir = catfile($stageHome, $product . '-' . $version, 
                           'batch-source', 'rc' . $rc);

    if (not -d $stageDir) {
        MkdirWithPath('dir' => $stageDir) 
          or die "Cannot create $stageDir: $!";
    }

    my $srcScript = $product . '-src-tarball-nobuild';
    $this->Shell(
      'cmd' => catfile($stageHome, 'bin', $srcScript),
      'cmdArgs' => ['-r', $productTag . '_RELEASE', '-m', $version],
      'dir' => $stageDir,
      'logFile' => catfile($logDir, 'source.log'),
    );
              
    move("$stageDir/../*.bz2", $stageDir);
    chmod(0644, glob("$stageDir/*.bz2"));
}

sub Verify {
    my $this = shift;
    # TODO verify source archive
}

sub Announce {
    my $this = shift;

    my $product = $config->Get('var' => 'product');
    my $version = $config->Get('var' => 'version');
    my $logDir = $config->Get('var' => 'logDir');

    my $logFile = catfile($logDir, 'source.log');

    $this->SendAnnouncement(
      subject => "$product $version source step finished",
      message => "$product $version source archive is ready to be copied to the candidates dir.",
    );
}

1;
