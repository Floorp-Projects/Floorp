#
# Tag step. Applies a CVS tag to the appropriate repositories.
# 
package Bootstrap::Step::Tag::l10n;

use Cwd;
use File::Copy qw(move);
use File::Spec::Functions;

use MozBuild::Util qw(MkdirWithPath);

use Bootstrap::Step;
use Bootstrap::Config;
use Bootstrap::Step::Tag;
use Bootstrap::Util qw(CvsCatfile GetDiffFileList);

use strict;

our @ISA = ("Bootstrap::Step::Tag");

sub Execute {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $product = $config->Get(var => 'product');
    my $productTag = $config->Get(var => 'productTag');
    my $branchTag = $config->Get(var => 'branchTag');
    my $l10n_pullDate = $config->Get(var => 'l10n_pullDate');
    my $rc = int($config->Get(var => 'rc'));
    my $appName = $config->Get(var => 'appName');
    my $logDir = $config->Get(sysvar => 'logDir');
    my $l10nCvsroot = $config->Get(var => 'l10nCvsroot');
    my $tagDir = $config->Get(var => 'tagDir');

    my $releaseTag = $productTag . '_RELEASE';
    my $rcTag = $productTag . '_RC' . $rc;
    my $releaseTagDir = catfile($tagDir, $rcTag);

    # Create the l10n tag directory.
    my $l10nTagDir = catfile($releaseTagDir, 'l10n');

    if (not -d $l10nTagDir) {
        MkdirWithPath(dir => $l10nTagDir) or
         die("Cannot mkdir $l10nTagDir: $!");
    }

    # Grab list of shipped locales
    #
    # Note: GetLocaleInfo() has a dependency on the $releaseTag above already
    # being set; it should be by when the l10n tagging step gets run, though.

    my $localeInfo = $config->GetLocaleInfo();

    # Config::Set() for us by Step::Tag::Execute() 
    my $geckoTag = $config->Get(var => 'geckoBranchTag');

    # Check out the l10n files from the branch you want to tag.

    my @l10nCheckoutArgs = (1 == $rc) ?
     # For rc1, pull by date on the branch
     ('-r', $branchTag, '-D', $l10n_pullDate) :
     # For rc(N > 1), pull the _RELBRANCH tag and tag that
     ('-r', $geckoTag);

    for my $locale (sort(keys(%{$localeInfo}))) {
        # skip en-US; it's kept in the main repo
        next if ($locale eq 'en-US');

        $this->Shell(
            cmd => 'cvs',
            cmdArgs => ['-d', $l10nCvsroot, 'co', @l10nCheckoutArgs, 
                        CvsCatfile('l10n', $locale)],
            dir => $l10nTagDir,
            logFile => catfile($logDir, 'tag-l10n_checkout.log'),
        );
    }

    my $cwd = getcwd();
    chdir(catfile($l10nTagDir, 'l10n')) or
     die "chdir() to $releaseTagDir/l10n failed: $!\n";
    my @topLevelFiles = grep(!/^CVS$/, glob('*'));
    chdir($cwd) or die "Couldn't chdir() home: $!\n";

    if (1 == $rc) {
        $this->CvsTag(tagName => $geckoTag,
                      branch => 1,
                      files => \@topLevelFiles,
                      coDir => catfile($l10nTagDir, 'l10n'),
                      logFile => catfile($logDir, 'tag-l10n_relbranch_tag_' . 
                                         $geckoTag));

    
        $this->Shell(cmd => 'cvs',
                     cmdArgs => ['up',
                                 '-r', $geckoTag],
                     dir => catfile($l10nTagDir, 'l10n'),
                     logFile => catfile($logDir, 'tag-l10n_relbranch_update_' .
                                        $geckoTag));
    }

    # Create the l10n RC tag
    $this->CvsTag(
      tagName => $rcTag,
      coDir => catfile($l10nTagDir, 'l10n'),
      logFile => catfile($logDir, 'tag-l10n_tag_' . $rcTag. '.log'),
    );

    # Create the l10n RELEASE tag
    my %releaseTagArgs = (tagName => $releaseTag,
                          coDir => catfile($l10nTagDir, 'l10n'),
                          logFile => catfile($logDir, 'tag-l10n_tag_' . 
                           $releaseTag. '.log'));

    # This is for the Verify() method; we assume that we actually set (or reset,
    # in the case of rc > 1) the _RELEASE tag; if that's not the case, we reset
    # this value below.
    $config->Set(var => 'tagModifyl10nReleaseTag', value => 1);

    # If we're retagging rc(N > 1), we need to tag -F
    if ($rc > 1) {
        my $previousRcTag = $productTag . '_RC' . ($rc - 1);
        my $diffFileList = GetDiffFileList(cvsDir => catfile($l10nTagDir,
                                                             'l10n'),
                                           prevTag => $previousRcTag,
                                           newTag => $rcTag);

        if (scalar(@{$diffFileList}) > 0) {
            $releaseTagArgs{'force'} = 1;
            $releaseTagArgs{'files'} = $diffFileList;
            $this->CvsTag(%releaseTagArgs); 
        } else {
            $this->Log(msg => "No diffs found in l10n for RC $rc; NOT " .
             "modifying $releaseTag");
            $config->Set(var => 'tagModifyl10nReleaseTag', value => 0,
              force => 1);
        }
    } else {
        # If we're RC 1, we obviously need to apply the _RELEASE tag...
        $this->CvsTag(%releaseTagArgs); 
    }
}

sub Verify {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $logDir = $config->Get(sysvar => 'logDir');
    my $productTag = $config->Get(var => 'productTag');
    my $rc = $config->Get(var => 'rc');

    my $releaseTag = $productTag . '_RELEASE';
    my $rcTag = $productTag . '_RC' . $rc;

    my @checkTags = ($rcTag);

    # If RC > 1 and we took no changes in cvsroot for that RC, the _RELEASE
    # tag won't have changed, so we shouldn't attempt to check it.
    if ($config->Get(var => 'tagModifyl10nReleaseTag')) {
        push(@checkTags, $releaseTag);
    }

    foreach my $tag (@checkTags) {
        $this->CheckLog(
          log => catfile($logDir, 'tag-l10n_tag_' . $tag . '.log'),
          checkFor => '^T',
        );
    }
}

1;
