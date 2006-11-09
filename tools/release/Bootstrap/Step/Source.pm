#
# Source step. Creates a source tarball equivalent to what was used to 
# build the binary release, in the Build step.
# 
package Bootstrap::Step::Source;
use Bootstrap::Step;
use File::Copy;
use MozBuild::Util;
@ISA = ("Bootstrap::Step");

sub Execute {
    my $this = shift;

    my $product = $this->Config('var' => 'product');
    my $productTag = $this->Config('var' => 'productTag');
    my $version = $this->Config('var' => 'version');
    my $rc = $this->Config('var' => 'rc');
    my $logDir = $this->Config('var' => 'logDir');
    my $stageHome = $this->Config('var' => 'stageHome');

    # create staging area
    my $stageDir = 
      $stageHome . '/' . $product . '-' . $version . '/batch-source/rc' . $rc;

    if (not -d $stageDir) {
        MkdirWithPath('dir' => $stageDir) or die "Cannot create $stageDir: $!";
    }

    $this->Shell(
      'cmd' => $stageHome . '/bin/' . $product . '-src-tarball-nobuild -r ' . $productTag . '_RELEASE -m ' . $version,
      'dir' => $stageDir,
      'logFile' => $logDir . '/source.log',
    );
              
    File::Copy::move("$stageDir/../*.bz2", $stageDir);
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
