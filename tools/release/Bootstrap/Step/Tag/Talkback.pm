#
# Tag step. Applies a CVS tag to the appropriate repositories.
# 
package Bootstrap::Step::Tag::Talkback;
use Bootstrap::Step;
use Bootstrap::Config;
use Bootstrap::Step::Tag;
use File::Copy qw(move);
use MozBuild::Util qw(MkdirWithPath);
@ISA = ("Bootstrap::Step::Tag");

sub Execute {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $product = $config->Get(var => 'product');
    my $productTag = $config->Get(var => 'productTag');
    my $branchTag = $config->Get(var => 'branchTag');
    my $pullDate = $config->Get(var => 'pullDate');
    my $logDir = $config->Get(var => 'logDir');
    my $mofoCvsroot = $config->Get(var => 'mofoCvsroot');
    my $tagDir = $config->Get(var => 'tagDir');

    my $releaseTag = $productTag.'_RELEASE';
    my $releaseTagDir = catfile($tagDir, $releaseTag);

    # Create the mofo tag directory.
    my $mofoDir = catfile($releaseTagDir, 'mofo');
    if (not -d $mofoDir) {
        MkdirWithPath(dir => $mofoDir) 
          or die("Cannot mkdir $mofoDir: $!");
    }

    # Check out the talkback files from the branch you want to tag.
    $this->Shell(
      cmd => 'cvs',
      cmdArgs => ['-d', $mofoCvsroot, 'co', '-r', $branchTag, '-D', 
                  $pullDate, catfile('talkback', 'fullsoft')],
      dir => catfile($releaseTagDir, 'mofo'),
      logFile => catfile($logDir, 'tag-talkback_mofo-checkout.log'),
    );

    # Create the talkback RELEASE tag.
    $this->CvsTag(
      tagName => $releaseTag,
      coDir => catfile($releaseTagDir, 'mofo', 'talkback', 'fullsoft'),
      logFile => catfile($logDir, 
                         'tag-talkback_mofo-tag-' . $releaseTag . '.log'),
    );
}

sub Verify {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $logDir = $config->Get(var => 'logDir');
    my $productTag = $config->Get(var => 'productTag');

    my $releaseTag = $productTag.'_RELEASE';

    $this->CheckLog(
      log => catfile($logDir, 
                     'tag-talkback_mofo-tag-' . $releaseTag . '.log'),
      checkFor => '^T',
    );
}

1;
