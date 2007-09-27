#
# Tag Mozilla substep. Applies appropriate tags to Mozilla source code.
# 
package Bootstrap::Step::Tag::Mozilla;

use File::Copy qw(move);
use File::Spec::Functions;

use MozBuild::Util qw(MkdirWithPath);
use Bootstrap::Util qw(GetDiffFileList);

use Bootstrap::Config;
use Bootstrap::Step::Tag;

use strict;

our @ISA = ("Bootstrap::Step::Tag");

sub Execute {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $productTag = $config->Get(var => 'productTag');
    my $rc = int($config->Get(var => 'rc'));
    my $logDir = $config->Get(sysvar => 'logDir');
    my $mozillaCvsroot = $config->Get(var => 'mozillaCvsroot');
    my $tagDir = $config->Get(var => 'tagDir');

    my $releaseTag = $productTag . '_RELEASE';
    my $rcTag = $productTag . '_RC' . $rc;
    my $rcTagDir = catfile($tagDir, $rcTag);
    my $cvsrootTagDir = catfile($rcTagDir, 'cvsroot');

    # Create the RC tag
    $this->CvsTag(
      tagName => $rcTag,
      coDir => catfile($cvsrootTagDir, 'mozilla'),
      logFile => catfile($logDir, 
                         'tag-mozilla_cvsroot_tag-' . $rcTag . '.log'),
    );

    # Create or move the RELEASE tag
    #
    # This is for the Verify() method; we assume that we actually set (or reset,
    # in the case of rc > 1) the _RELEASE tag; if that's not the case, we reset
    # this value below.
    $config->Set(var => 'tagModifyMozillaReleaseTag', value => 1);

    if ($rc > 1) {
        my $previousRcTag = $productTag . '_RC' . ($rc - 1);
        my $diffFileList = GetDiffFileList(cvsDir => catfile($cvsrootTagDir,
                                                             'mozilla'),
                                           prevTag => $previousRcTag,
                                           newTag => $rcTag);

        if (scalar(@{$diffFileList}) > 0) {
            $this->CvsTag(
              tagName => $releaseTag,
              coDir => catfile($cvsrootTagDir, 'mozilla'),
              force => 1,
              files => $diffFileList,
              logFile => catfile($logDir, 
                                 'tag-mozilla_cvsroot_tag-' . $releaseTag . 
                                 '.log'),
              );
        } else {
            $config->Set(var => 'tagModifyMozillaReleaseTag', value => 0,
             force => 1);
            $this->Log(msg => "No diffs found in cvsroot for RC $rc; NOT " .
             "modifying $releaseTag");
        }
    } else {
        $this->CvsTag(
          tagName => $releaseTag,
          coDir => catfile($cvsrootTagDir, 'mozilla'),
          logFile => catfile($logDir, 
                             'tag-mozilla_cvsroot_tag-' . $releaseTag . '.log'),
        );
    }

}

sub Verify {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $productTag = $config->Get(var => 'productTag');
    my $logDir = $config->Get(sysvar => 'logDir');
    my $rc = $config->Get(var => 'rc');

    my $releaseTag = $productTag . '_RELEASE';
    my $rcTag = $productTag . '_RC' . $rc;

    my @checkTags = ($rcTag);

    # If RC > 1 and we took no changes in cvsroot for that RC, the _RELEASE
    # tag won't have changed, so we shouldn't attempt to check it.
    if ($config->Get(var => 'tagModifyMozillaReleaseTag')) {
        push(@checkTags, $releaseTag);      
    }

    # TODO: should this complain about W's?
    foreach my $tag (@checkTags) {
        $this->CheckLog(
          log => catfile($logDir, 'tag-mozilla_cvsroot_tag-' . $tag . '.log'),
          checkFor => '^T',
        );
    }
}

1;
