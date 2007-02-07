#
# Tag step. Applies a CVS tag to the appropriate repositories.
# 
package Bootstrap::Step::Tag::l10n;
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
    my $l10n_pullDate = $config->Get(var => 'l10n_pullDate');
    my $rc = $config->Get(var => 'rc');
    my $appName = $config->Get(var => 'appName');
    my $logDir = $config->Get(var => 'logDir');
    my $l10nCvsroot = $config->Get(var => 'l10nCvsroot');
    my $tagDir = $config->Get(var => 'tagDir');

    my $releaseTag = $productTag.'_RELEASE';
    my $rcTag = $productTag.'_RC'.$rc;
    my $releaseTagDir = catfile($tagDir, $releaseTag);

    # Create the l10n tag directory.
    my $l10nDir = catfile($releaseTagDir, 'l10n');
    if (not -d $l10nDir) {
        MkdirWithPath(dir => $l10nDir) or die("Cannot mkdir $l10nDir: $!");
    }

    # Grab list of shipped locales
    my $shippedLocales = catfile($releaseTagDir, 'cvsroot', 'mozilla', 
                                 $appName, 'locales', 'shipped-locales');
    open (FILE, "< $shippedLocales") 
      or die("Cannot open file $shippedLocales: $!");
    my @locales = <FILE>;
    close FILE or die("Cannot close file $shippedLocales: $!");

    # Check out the l10n files from the branch you want to tag.
    for my $locale (@locales) {
        # only keep first column
        $locale =~ s/(\s+).*//;
        # remove line endings
        $locale =~ s/(\n)//;
        # skip en-US, this is the default locale
        if ($locale eq 'en-US') {
            next;
        }
        $this->Shell(
            cmd => 'cvs',
            cmdArgs => ['-d', $l10nCvsroot, 'co', '-r', $branchTag, '-D',
                        $l10n_pullDate, catfile('l10n', $locale)],
            dir => catfile($releaseTagDir, 'l10n'),
            logFile => catfile($logDir, 'tag-l10n_checkout.log'),
        );
    }

    # Create the l10n RELEASE and RC tags.
    foreach my $tag ($releaseTag, $rcTag) {
        $this->CvsTag(
          tagName => $tag,
          coDir => catfile($releaseTagDir, 'l10n', 'l10n'),
          logFile => catfile($logDir, 'tag-l10n_tag_' . $tag. '.log'),
        );
    }
}

sub Verify {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $logDir = $config->Get(var => 'logDir');
    my $productTag = $config->Get(var => 'productTag');
    my $rc = $config->Get(var => 'rc');

    my $releaseTag = $productTag.'_RELEASE';
    my $rcTag = $productTag.'_RC'.$rc;

    foreach my $tag ($releaseTag, $rcTag) {
        $this->CheckLog(
          log => catfile($logDir, 'tag-l10n_tag_' . $tag . '.log'),
          checkFor => '^T',
        );
    }
}

1;
