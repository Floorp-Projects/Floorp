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
    my $stageDir = 
      $stageHome . '/' . $product . '-' . $version . '/batch-source/rc' . $rc;

    if (not -d $stageDir) {
        MkdirWithPath('dir' => $stageDir) 
          or die "Cannot create $stageDir: $!";
    }

    $this->Shell(
      'cmd' => $stageHome . '/bin/' . $product . '-src-tarball-nobuild -r ' . $productTag . '_RELEASE -m ' . $version,
      'dir' => $stageDir,
      'logFile' => $logDir . '/source.log',
    );
              
    move("$stageDir/../*.bz2", $stageDir);
    chmod(0644, glob("$stageDir/*.bz2"));

#    $this->Shell(
#      'cmd' => 'rsync -av *.bz2 /home/ftp/pub/' . $product . '/nightly/' . $version . '-candidates/rc' . $rc,
#      'dir' => $stageDir,
#    );
}

sub Verify {
    my $this = shift;
    #$this->Shell('cmd' => 'echo Verify source');
}

1;
