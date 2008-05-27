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
    my $build = int($config->Get(var => 'build'));
    my $logDir = $config->Get(sysvar => 'logDir');
    my $mozillaCvsroot = $config->Get(var => 'mozillaCvsroot');
    my $tagDir = $config->Get(var => 'tagDir');

    my $releaseTag = $productTag . '_RELEASE';
    my $buildTag = $productTag . '_BUILD' . $build;
    my $buildTagDir = catfile($tagDir, $buildTag);
    my $cvsrootTagDir = catfile($buildTagDir, 'cvsroot');

    # Create the BUILD tag
    $this->CvsTag(
      tagName => $buildTag,
      coDir => catfile($cvsrootTagDir, 'mozilla'),
      logFile => catfile($logDir, 
                         'tag-mozilla_cvsroot_tag-' . $buildTag . '.log'),
    );

    # Create or move the RELEASE tag
    #
    # This is for the Verify() method; we assume that we actually set (or reset,
    # in the case of build > 1) the _RELEASE tag; if that's not the case, we reset
    # this value below.
    $config->Set(var => 'tagModifyMozillaReleaseTag', value => 1);

    if ($build > 1) {
        my $previousBuildTag = $productTag . '_BUILD' . ($build - 1);
        my $diffFileList = GetDiffFileList(cvsDir => catfile($cvsrootTagDir,
                                                             'mozilla'),
                                           prevTag => $previousBuildTag,
                                           newTag => $buildTag);

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
            $this->Log(msg => "No diffs found in cvsroot for build $build; NOT " .
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
    my $build = $config->Get(var => 'build');

    my $releaseTag = $productTag . '_RELEASE';
    my $buildTag = $productTag . '_BUILD' . $build;

    my @checkTags = ($buildTag);

    # If build > 1 and we took no changes in cvsroot for that build, the _RELEASE
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
