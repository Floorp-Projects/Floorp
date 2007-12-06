#
# Tag::Bump substep. Bumps version files for Mozilla appropriately.
# 
package Bootstrap::Step::Tag::Bump;

use strict;

use File::Copy qw(move);

use MozBuild::Util qw(MkdirWithPath);

use Bootstrap::Util qw(CvsCatfile);
use Bootstrap::Step;
use Bootstrap::Config;
use Bootstrap::Step::Tag;

our @ISA = ("Bootstrap::Step::Tag");

sub Execute {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $product = $config->Get(var => 'product');
    my $productTag = $config->Get(var => 'productTag');
    my $branchTag = $config->Get(var => 'branchTag');
    my $pullDate = $config->Get(var => 'pullDate');
    my $version = $config->Get(var => 'version');
    my $rc = int($config->Get(var => 'rc'));
    my $milestone = $config->Exists(var => 'milestone') ? 
     $config->Get(var => 'milestone') : undef;
    my $appName = $config->Get(var => 'appName');
    my $logDir = $config->Get(sysvar => 'logDir');
    my $mozillaCvsroot = $config->Get(var => 'mozillaCvsroot');
    my $tagDir = $config->Get(var => 'tagDir');
    my $geckoBranchTag = $config->Get(var => 'geckoBranchTag');

    my $releaseTag = $productTag . '_RELEASE';
    my $rcTag = $productTag . '_RC' . $rc;

    my $rcTagDir = catfile($tagDir, $rcTag);
    my $cvsrootTagDir = catfile($rcTagDir, 'cvsroot');
 
    ## TODO - we need to handle the case here where we're in security firedrill
    ## mode, and we need to bump versions on the GECKO_ branch, but they
    ## won't have "pre" in them. :-o
    #
    # We only do the bump step for rc1

    if ($rc > 1) {
        $this->Log(msg => "Skipping Tag::Bump::Execute substep for RC $rc.");
        return;
    }

    # pull version files
    my $moduleVer = CvsCatfile($appName, 'app', 'module.ver');
    my $versionTxt = CvsCatfile($appName, 'config', 'version.txt');
    my $milestoneTxt = CvsCatfile('config', 'milestone.txt');

    my @bumpFiles = ('client.mk', $moduleVer, $versionTxt);

    # only bump milestone if it's defined in the config
    if (defined($milestone)) {
        @bumpFiles = (@bumpFiles, $milestoneTxt);
    }

    # Check out Mozilla from the branch you want to tag.
    # TODO this should support running without branch tag or pull date.
    $this->CvsCo(
      cvsroot => $mozillaCvsroot,
      tag => $geckoBranchTag,
      modules => [CvsCatfile('mozilla', 'client.mk'),
                  CvsCatfile('mozilla', $appName, 'app', 'module.ver'),
                  CvsCatfile('mozilla', $appName, 'config', 'version.txt'),
                  CvsCatfile('mozilla', 'config', 'milestone.txt')],
      workDir => $cvsrootTagDir,
      logFile => catfile($logDir, 'tag-bump_checkout.log')
    );

    ### Perform version bump

    my $parentDir = catfile($cvsrootTagDir, 'mozilla');
    foreach my $fileName (@bumpFiles) {
        my $found = 0;

        my $file = catfile($parentDir, $fileName);

        my $bumpVersion = undef;
        my $preVersion = undef;
        my %searchReplace = ();

        # Order or searching for these values is not preserved, so make
        # sure that the order replacement happens does not matter.
        if ($fileName eq 'client.mk') {
            %searchReplace = (
             # MOZ_CO_TAG is commented out on some branches, make sure to
             # accommodate that
             '^#?MOZ_CO_TAG\s+=.+$' =>
              'MOZ_CO_TAG           = ' . $releaseTag,
             '^NSPR_CO_TAG\s+=\s+\w*' => 
              'NSPR_CO_TAG          = ' . $releaseTag,
             '^NSS_CO_TAG\s+=\s+\w*' =>
              'NSS_CO_TAG           = ' . $releaseTag,
             '^LOCALES_CO_TAG\s+=\s+' . $branchTag . '$' =>
              'LOCALES_CO_TAG       = ' . $releaseTag,
             '^LDAPCSDK_CO_TAG\s+=\s+' . $branchTag . '$' =>
              'LDAPCSDK_CO_TAG      = ' . $releaseTag);
        } elsif ($fileName eq $moduleVer) {
            $preVersion = $version . 'pre';
            %searchReplace = ('^WIN32_MODULE_PRODUCTVERSION_STRING=' . 
             $preVersion . '$' => 'WIN32_MODULE_PRODUCTVERSION_STRING=' . 
             $version);
        } elsif ($fileName eq $versionTxt) {
            $preVersion = $version . 'pre';
            %searchReplace = ('^' . $preVersion . '$' => $version);
        } elsif ($fileName eq $milestoneTxt) {
            $preVersion = $milestone . 'pre';
            %searchReplace = ('^' . $preVersion . '$' => $milestone);
        } else {
            die("ASSERT: do not know how to bump file $fileName");
        }

        if (scalar(keys(%searchReplace)) <= 0) {
            die("ASSERT: no search/replace to perform");
        }
        
        open(INFILE,  "< $file") or die("Could not open $file: $!");
        open(OUTFILE, "> $file.tmp") or die("Could not open $file.tmp: $!");
        while(<INFILE>) {
            foreach my $search (keys(%searchReplace)) {
                my $replace = $searchReplace{$search};
                if($_ =~ /$search/) {
                    $this->Log(msg => "$search found");
                    $found = 1;
                    $_ =~ s/$search/$replace/;
                    $this->Log(msg => "$search replaced with $replace");
                }
            }

            print OUTFILE $_;
        }
        close INFILE or die("Could not close $file: $!");
        close OUTFILE or die("Coule not close $file.tmp: $!");
        if (not $found) {
            die("None of " . join(' ', keys(%searchReplace)) . 
             " found in file $file: $!");
        }

        if (not move("$file.tmp",
                     "$file")) {
            die("Cannot rename $file.tmp to $file: $!");
        }
    }

    my $bumpCiMsg = 'Automated checkin: version bump, remove pre tag for ' 
                        . $product . ' ' . $version . ' release on ' 
                        . $geckoBranchTag;
    $this->Shell(
      cmd => 'cvs',
      cmdArgs => ['commit', '-m', $bumpCiMsg, 
                  @bumpFiles,
                 ],
      dir => catfile($rcTagDir, 'cvsroot', 'mozilla'),
      logFile => catfile($logDir, 'tag-bump_checkin.log'),
    );
}

sub Verify {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $logDir = $config->Get(sysvar => 'logDir');
    my $appName = $config->Get(var => 'appName');
    my $milestone = $config->Exists(var => 'milestone') ? 
     $config->Get(var => 'milestone') : undef;
    my $rc = $config->Get(var => 'rc');

    if ($rc > 1) {
        $this->Log(msg => "Skipping Tag::Bump::Verify substep for RC $rc.");
        return;
    }

    my $moduleVer = catfile($appName, 'app', 'module.ver');
    my $versionTxt = catfile($appName, 'config', 'version.txt');
    my $milestoneTxt = catfile('config', 'milestone.txt');
    my @bumpFiles = ('client.mk', $moduleVer, $versionTxt);

    # only bump milestone if it's defined in the config
    if (defined($milestone)) {
        @bumpFiles = (@bumpFiles, $milestoneTxt);
    }

    foreach my $file (@bumpFiles) {
        $this->CheckLog(
          log => catfile($logDir, 'tag-bump_checkin.log'),
          checkFor => $file,
        );
    }
}

1;
