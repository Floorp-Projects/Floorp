#
# Tag step. Sets up the tagging directory, and checks out the mozilla source.
# 
package Bootstrap::Step::Tag;
use Bootstrap::Step;
use Bootstrap::Step::Tag::Bump;
use Bootstrap::Step::Tag::Mozilla;
use Bootstrap::Step::Tag::l10n;
use Bootstrap::Step::Tag::Talkback;
use Bootstrap::Config;
use File::Copy qw(move);
use MozBuild::Util qw(MkdirWithPath);
@ISA = qw(Bootstrap::Step);

my $config = new Bootstrap::Config;

my @subSteps = ('Bump', 'Mozilla', 'l10n', 'Talkback');

sub Execute {
    my $this = shift;

    my $productTag = $config->Get('var' => 'productTag');
    my $rc = $config->Get('var' => 'rc');
    my $tagDir = $config->Get('var' => 'tagDir');
    my $mozillaCvsroot = $config->Get('var' => 'mozillaCvsroot');
    my $branchTag = $config->Get('var' => 'branchTag');
    my $pullDate = $config->Get('var' => 'pullDate');
    my $logDir = $config->Get('var' => 'logDir');

    my $releaseTag = $productTag.'_RELEASE';
    my $rcTag = $productTag.'_RC'.$rc;
    my $releaseTagDir = catfile($tagDir, $releaseTag);

    # create the main tag directory
    if (not -d $releaseTagDir) {
        MkdirWithPath('dir' => $releaseTagDir) 
          or die "Cannot mkdir $releaseTagDir: $!";
    }

    # Symlink to to RC dir
    my $fromLink = catfile($tagDir, $releaseTag);
    my $toLink   = catfile($tagDir, $rcTag);
    if (not -e $toLink) {
        symlink($fromLink, $toLink) 
          or die "Cannot symlink $fromLink $toLink: $!";
    }

    # Tagging area for Mozilla
    my $cvsrootTagDir = catfile($releaseTagDir, 'cvsroot');
    if (not -d $cvsrootTagDir) {
        MkdirWithPath('dir' => $cvsrootTagDir) 
          or die "Cannot mkdir $cvsrootTagDir: $!";
    }

    # Check out Mozilla from the branch you want to tag.
    # TODO this should support running without branch tag or pull date.

    $this->Shell(
      'cmd' => 'cvs',
      'cmdArgs' => ['-d', $mozillaCvsroot, 
                    'co', 
                    '-r', $branchTag, 
                    '-D', $pullDate, 
                     'mozilla/client.mk',
                   ],
      'dir' => $cvsrootTagDir,
      'logFile' => catfile($logDir, 'tag_checkout_client_mk.log'),
    );

    $this->CheckLog(
      'log' => catfile($logDir, 'tag_checkout_client_mk.log'),
      'checkForOnly' => '^U mozilla/client.mk',
    );

    $this->Shell(
      'cmd' => 'gmake',
      'cmdArgs' => ['-f', 'client.mk', 'checkout', 'MOZ_CO_PROJECT=all', 
                    'MOZ_CO_DATE=' . $pullDate],
      'dir' => catfile($cvsrootTagDir, 'mozilla'),
      'logFile' => catfile($logDir, 'tag_mozilla-checkout.log'),
    );

    # Call substeps
    my $numSteps = scalar(@subSteps);
    my $currentStep = 0;
    while ($currentStep < $numSteps) {
        my $stepName = $subSteps[$currentStep];
        eval {
            $this->Log(msg => 'Tag running substep' . $stepName);
            my $step = "Bootstrap::Step::Tag::$stepName"->new();
            $step->Execute();
        };
        if ($@) {
            die("Tag substep $stepName Execute died: $@");
        }
        $currentStep += 1;
    }
}

sub Verify {
    my $this = shift;

    my $logDir = $config->Get('var' => 'logDir');

    $this->CheckLog(
      'log' => catfile($logDir, 'tag_mozilla-checkout.log'),
      'checkFor' => '^U',
    );

    # Call substeps
    my $numSteps = scalar(@subSteps);
    my $currentStep = 0;
    while ($currentStep < $numSteps) {
        my $stepName = $subSteps[$currentStep];
        eval {
            $this->Log(msg => 'Tag running substep' . $stepName);
            my $step = "Bootstrap::Step::Tag::$stepName"->new();
            $step->Verify();
        };
        if ($@) {
            die("Tag substep $stepName Verify died: $@");
        }
        $currentStep += 1;
    }
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
        die("ASSERT: Bootstrap::Step::Tag::CvsTag(): Cannot specify force without files");
    } elsif ($branch and scalar(@{$files}) <= 0) {
        die("ASSERT: Bootstrap::Step::Tag::CvsTag(): Cannot specify branch without files");
    } elsif ($branch and $force) {
        die("ASSERT: Bootstrap::Step::Tag::CvsTag(): Cannot specify both branch and force");
    } elsif (not $tagName) {
        die("ASSERT: Bootstrap::Step::Tag::CvsTag(): tagName must be specified");
    } elsif (not $logFile) {
        die("ASSERT: Bootstrap::Step::Tag::CvsTag(): logFile must be specified");
    }

    my @cmdArgs;
    push(@cmdArgs, '-q');
    push(@cmdArgs, 'tag');
    push(@cmdArgs, '-F') if ($force);
    push(@cmdArgs, '-b') if ($branch);
    push(@cmdArgs, $tagName);
    push(@cmdArgs, @$files) if defined($files);

    $this->Shell(
      'cmd' => 'cvs',
      'cmdArgs' => \@cmdArgs,
      'dir' => $coDir,
      'logFile' => $logFile,
    );
}

1;
