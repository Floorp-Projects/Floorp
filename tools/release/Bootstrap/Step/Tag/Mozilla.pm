#
# Tag Mozilla substep. Applies appropriate tags to Mozilla source code.
# 
package Bootstrap::Step::Tag::Mozilla;
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
    my $rc = $config->Get(var => 'rc');
    my $version = $config->Get(var => 'version');
    my $appName = $config->Get(var => 'appName');
    my $logDir = $config->Get(var => 'logDir');
    my $mozillaCvsroot = $config->Get(var => 'mozillaCvsroot');
    my $tagDir = $config->Get(var => 'tagDir');

    my $releaseTag = $productTag.'_RELEASE';
    my $rcTag = $productTag.'_RC'.$rc;
    my $releaseTagDir = catfile($tagDir, $releaseTag);
    my $cvsrootTagDir = catfile($releaseTagDir, 'cvsroot');

    # Create the RELEASE and RC tags
    foreach my $tag ($releaseTag, $rcTag) {
        $this->CvsTag(
          tagName => $tag,
          coDir => catfile($cvsrootTagDir, 'mozilla'),
          logFile => catfile($logDir, 
                             'tag-mozilla_cvsroot_tag-' . $tag . '.log'),
        );
    }
}

sub Verify {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $productTag = $config->Get(var => 'productTag');
    my $logDir = $config->Get(var => 'logDir');
    my $rc = $config->Get(var => 'rc');

    my $releaseTag = $productTag.'_RELEASE';
    my $rcTag = $productTag.'_RC'.$rc;

    foreach my $tag ($releaseTag, $rcTag) {
        $this->CheckLog(
          log => catfile($logDir, 'tag-mozilla_cvsroot_tag-' . $tag . '.log'),
          checkFor => '^T',
        );
    }
}

1;
