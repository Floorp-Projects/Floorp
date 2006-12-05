#
# Tag step. Applies a CVS tag to the appropriate repositories.
# 
package Bootstrap::Step::Tag;
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
    my $branchTag = $config->Get('var' => 'branchTag');
    my $pullDate = $config->Get('var' => 'pullDate');
    my $rc = $config->Get('var' => 'rc');
    my $version = $config->Get('var' => 'version');
    my $appName = $config->Get('var' => 'appName');
    my $logDir = $config->Get('var' => 'logDir');
    my $mozillaCvsroot = $config->Get('var' => 'mozillaCvsroot');
    my $l10nCvsroot = $config->Get('var' => 'l10nCvsroot');
    my $mofoCvsroot = $config->Get('var' => 'mofoCvsroot');
    my $tagDir = $config->Get('var' => 'tagDir');

    my $releaseTag = $productTag.'_RELEASE';
    my $rcTag = $productTag.'_RC'.$rc;
    my $minibranchTag = $productTag.'_MINIBRANCH';
    my $releaseTagDir = $tagDir . '/' . $releaseTag;

    # create the main tag directory
    if (not -d $releaseTagDir) {
        MkdirWithPath('dir' => $releaseTagDir) 
          or die "Cannot mkdir $releaseTagDir: $!";
    }

    # Symlink to to RC dir
    my $fromLink = $tagDir . '/' . $releaseTag;
    my $toLink   = $tagDir . '/' .  $rcTag;
    if (not -e $toLink) {
        symlink($fromLink, $toLink) 
          or die "Cannot symlink $fromLink $toLink: $!";
    }

    # Tagging area for Mozilla
    if (not -d $releaseTagDir . '/cvsroot') {
        MkdirWithPath('dir' => $releaseTagDir . '/cvsroot') 
          or die "Cannot mkdir $releaseTagDir/cvsroot: $!";
    }

    # Check out Mozilla from the branch you want to tag.
    # TODO this should support running without branch tag or pull date.
    $this->Shell(
      'cmd' => 'cvs -d ' . $mozillaCvsroot . ' co -r ' . $branchTag . ' -D "' . $pullDate . '" mozilla/client.mk ',
      'dir' => $releaseTagDir . '/cvsroot',
      'logFile' => $logDir . '/client_mk.log',
    );
    $this->CheckLog(
      'log' => $logDir . '/client_mk.log',
      'checkForOnly' => '^U mozilla/client.mk',
    );
    $this->Shell(
      'cmd' => 'gmake -f client.mk checkout MOZ_CO_PROJECT=all MOZ_CO_DATE="' . $pullDate . '"',
      'dir' => $releaseTagDir . '/cvsroot/mozilla',
      'timeout' => '3600',
      'logFile' => $logDir . '/mozilla-checkout.log',
    );
  
    # Create the RELEASE tag
    $this->CvsTag(
      'tagName' => $releaseTag,
      'coDir'   => $releaseTagDir . '/cvsroot/mozilla',
      'timeout' => '3600',
      'logFile' => $logDir . '/cvsroot_tag-' . $releaseTag . '.log',
    );

    # Create the RC tag
    $this->CvsTag(
      'tagName' => $rcTag,
      'coDir'   => $releaseTagDir . '/cvsroot/mozilla',
      'timeout' => '3600',
      'logFile' => $logDir . '/cvsroot_tag-' . $rcTag . '.log',
    );

    # Create a minibranch for the pull scripts so we can change them without
    # changing anything on the original branch.
    $this->CvsTag(
      'tagName' => $minibranchTag,
      'branch'  => '1',
      'files'   => ['client.mk'],
      'coDir'   => $releaseTagDir . '/cvsroot/mozilla',
      'logFile' => $logDir . '/cvsroot_tag-' . $minibranchTag. '.log',
    );

    # Update client.mk to the minibranch you just created.
    $this->Shell(
      'cmd' => 'cvs up -r ' . $minibranchTag . ' client.mk',
      'dir' => $releaseTagDir . '/cvsroot/mozilla',
      'logFile' => $logDir . '/client_mk-update.log',
    );

    # Add the new product tag to the client.mk
    open(INFILE,  "<$releaseTagDir/cvsroot/mozilla/client.mk");
    open(OUTFILE, ">$releaseTagDir/cvsroot/mozilla/client.mk.tmp");
    while(<INFILE>) {
        $_ =~ s/$branchTag/$releaseTag/g;
        print OUTFILE $_;
    }
    close INFILE;
    close OUTFILE;

    if (not move("$releaseTagDir/cvsroot/mozilla/client.mk.tmp", 
                   "$releaseTagDir/cvsroot/mozilla/client.mk")) {
        die "Cannot rename $releaseTagDir/cvsroot/mozilla/client.mk.tmp to $releaseTagDir/cvsroot/mozilla/client.mk";
    }

    $this->Shell(
      'cmd' => 'cvs commit -m "For ' . $product . ' ' . $version . ', redirect client.mk onto the ' . $releaseTag . ' tag." client.mk',
      'dir' => $releaseTagDir . '/cvsroot/mozilla',
      'logFile' => $logDir . '/client_mk-release_tag.log',
    );
    $this->CheckLog(
      'log' => $logDir . '/client_mk-release_tag.log',
      'checkFor' => '^Checking in client.mk;',
    );
    $this->CheckLog(
      'log' => $logDir . '/client_mk-release_tag.log',
      'checkFor' => '^done',
    );

    # Move the release tag onto the modified version of the pull scripts.
    $this->CvsTag(
      'tagName' => $releaseTag,
      'force'   => '1',
      'files'   => ['client.mk'],
      'coDir'   => $releaseTagDir . '/cvsroot/mozilla',
      'logFile' => $logDir . '/cvsroot_clientmk_tag-' . $releaseTag. '.log',
    );

    # Move the RC tag onto the modified version of the pull scripts.
    $this->CvsTag(
      'tagName' => $rcTag,
      'force'   => '1',
      'files'   => ['client.mk'],
      'coDir'   => $releaseTagDir . '/cvsroot/mozilla',
      'logFile' => $logDir . '/cvsroot_clientmk_tag-' . $rcTag. '.log',
    );

    # Create the mofo tag directory.
    if (not -d "$releaseTagDir/mofo") {
        MkdirWithPath('dir' => "$releaseTagDir/mofo") 
          or die "Cannot mkdir $releaseTagDir/mofo: $!";
    }

    # Check out the talkback files from the branch you want to tag.
    $this->Shell(
      'cmd' => 'cvs -d ' . $mofoCvsroot . ' co -r ' . $branchTag . ' -D "' . $pullDate . '" talkback/fullsoft',
      'dir' => $releaseTagDir . '/mofo',
      'logFile' => $logDir . '/mofo-checkout.log'
    );

    # Create the talkback RELEASE tag.
    $this->CvsTag(
      'tagName' => $releaseTag,
      'coDir'   => $releaseTagDir . '/mofo/talkback/fullsoft',
      'logFile' => $logDir . '/mofo_tag-' . $releaseTag. '.log',
    );

    # Create the l10n tag directory.
    if (not -d "$releaseTagDir/l10n") {
        MkdirWithPath('dir' => "$releaseTagDir/l10n") 
          or die "Cannot mkdir $releaseTagDir/l10n: $!";
    }

    # Grab list of shipped locales
    my $shippedLocales = 
      $releaseTagDir . '/cvsroot/mozilla/' . $appName . '/locales/shipped-locales';
    open (FILE, "< $shippedLocales") 
      or die "Cannot open file $shippedLocales: $!";
    my @locales = <FILE>;
    close FILE or die "Cannot close file $shippedLocales: $!";

    # Check out the l10n files from the branch you want to tag.
    for my $locale (@locales) {
        # only keep first column
        $locale =~ s/(\s+).*//;
        # skip en-US, this is the default locale
        if ($locale eq 'en-US') {
            next;
        }
        $this->Shell(
            'cmd' => 'cvs -d ' . $l10nCvsroot . ' co -r ' . $branchTag . ' -D "' . $pullDate . '" l10n/' . $locale,
            'dir' => $releaseTagDir . '/l10n',
            'logFile' => $logDir . '/l10n-checkout.log',
        );
    }

    # Create the l10n RELEASE tag.
    $this->CvsTag(
      'tagName' => $releaseTag,
      'coDir'   => $releaseTagDir . '/l10n/l10n',
      'logFile' => $logDir . '/l10n_tag-' . $releaseTag. '.log',
    );

    # Create the RC tag.
    $this->CvsTag(
      'tagName' => $rcTag,
      'coDir'   => $releaseTagDir . '/l10n/l10n',
      'logFile' => $logDir . '/l10n_tag-' . $rcTag. '.log',
    );
}

sub Verify {
    my $this = shift;
    # TODO - independently verify that tag was applied
    #$this->Shell('cmd' => 'echo Verify tag');
}

sub CvsTag {
    my $this = shift;
    my %args = @_;

    my $tagName = $args{'tagName'};
    my $coDir = $args{'coDir'};
    my $branch = $args{'branch'};
    my $files = $args{'files'};
    my $force = $args{'force'};
    my $logFile = $args{'logFile'};
   
    my $logDir = $config->Get('var' => 'logDir');

    # only force or branch specific files, not the whole tree
    if ($force and scalar(@{$files}) <= 0 ) {
        die("ASSERT: Cannot specify force without files");
    } elsif ($branch and scalar(@{$files}) <= 0) {
        die("ASSERT: Cannot specify branch without files");
    } elsif ($branch and $force) {
        die("ASSERT: Cannot specify both branch and force");
    } elsif (not $tagName) {
        die("ASSERT: tagName must be specified");
    } elsif (not $logFile) {
        die("ASSERT: logFile must be specified");
    }

    my $cmd = 'cvs -q tag';
    $cmd .= ' -F ' if ($force);
    $cmd .= ' -b ' if ($branch);
    $cmd .= ' ' . $tagName;
    $cmd .= ' ' . join(' ', @{$files}) if (defined($files));

    $this->Shell(
      'cmd' => $cmd,
      'dir' => $coDir,
      'timeout' => 3600,
      'logFile' => $logFile,
    );

    $this->CheckLog(
      'log' => $logFile,
      'checkForOnly' => '^T ',
    );
}

1;
