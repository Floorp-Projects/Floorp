#
# Tag::Bump substep. Bumps version files for Mozilla appropriately.
# 
package Bootstrap::Step::Tag::Bump;
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
    my $version = $config->Get(var => 'version');
    my $milestone = $config->Exists(var => 'milestone') ? 
     $config->Get(var => 'milestone') : undef;
    my $appName = $config->Get(var => 'appName');
    my $logDir = $config->Get(var => 'logDir');
    my $mozillaCvsroot = $config->Get(var => 'mozillaCvsroot');
    my $tagDir = $config->Get(var => 'tagDir');

    my $minibranchTag = $productTag.'_MINIBRANCH';
    my $releaseTag = $productTag.'_RELEASE';
    my $releaseTagDir = catfile($tagDir, $releaseTag);
    my $cvsrootTagDir = catfile($releaseTagDir, 'cvsroot');

    # pull version files
    my $moduleVer = catfile($appName, 'app', 'module.ver');
    my $versionTxt = catfile($appName, 'config', 'version.txt');
    my $milestoneTxt = catfile('config', 'milestone.txt');

    my @bumpFiles = ('client.mk', $moduleVer, $versionTxt);

    # only bump milestone if it's defined in the config
    if (defined($milestone)) {
        @bumpFiles = (@bumpFiles, $milestoneTxt);
    }

    # Check out Mozilla from the branch you want to tag.
    # TODO this should support running without branch tag or pull date.
    $this->Shell(
      cmd => 'cvs',
      cmdArgs => ['-d', $mozillaCvsroot, 
                  'co', 
                  '-r', $branchTag, 
                  '-D', $pullDate, 
                  catfile('mozilla', 'client.mk'),
                  catfile('mozilla', $moduleVer),
                  catfile('mozilla', $versionTxt),
                 ],
      dir => $cvsrootTagDir,
      logFile => catfile($logDir, 'tag-bump_checkout.log'),
    );

    # Create a minibranch for the pull scripts so we can change them without
    # changing anything on the original branch.
    $this->CvsTag(
      tagName => $minibranchTag,
      branch => '1',
      files => \@bumpFiles,
      coDir => catfile($cvsrootTagDir, 'mozilla'),
      logFile => catfile($logDir, 'tag-bump_cvsroot_tag-' . $minibranchTag . '.log'),
    );

    # pull version files from the version bump minibranch
    $this->Shell(
      cmd => 'cvs',
      cmdArgs => ['up', '-r', $minibranchTag, 
                  @bumpFiles,
                 ],
      dir => catfile($cvsrootTagDir, 'mozilla'),
      logFile => catfile($logDir, 'tag-bump-pull_minibranch.log'),
    );

    ### Perform version bump

    my $parentDir = catfile($cvsrootTagDir, 'mozilla');
    foreach my $fileName (@bumpFiles) {
        my $found = 0;

        my $file = catfile($parentDir, $fileName);

        my $bumpVersion = undef;
        my $preVersion = undef;
        my $search = undef;
        my $replace = undef;

        if ($fileName eq 'client.mk') {
            $search = '^MOZ_CO_TAG\s+=\s+' . $branchTag . '$';
            $replace = 'MOZ_CO_TAG           = ' . $releaseTag;
        } elsif ($fileName eq $moduleVer) {
            $preVersion = $version . 'pre';
            $search = '^WIN32_MODULE_PRODUCTVERSION_STRING=' . $preVersion . '$';
            $replace = 'WIN32_MODULE_PRODUCTVERSION_STRING=' . $version;
        } elsif ($fileName eq $versionTxt) {
            $preVersion = $version . 'pre';
            $search = '^' . $preVersion . '$';
            $replace = $version;
        } elsif ($fileName eq $milestoneTxt) {
            $preVersion = $milestone . 'pre';
            $search = '^' . $preVersion . '$';
            $replace = $milestone;
        } else {
            die("ASSERT: do not know how to bump file $fileName");
        }
        
        open(INFILE,  "< $file") or die("Could not open $file: $!");
        open(OUTFILE, "> $file.tmp") or die("Could not open $file.tmp: $!");
        while(<INFILE>) {
            if($_ =~ /$search/) {
                $this->Log(msg => "$search found");
                $found = 1;
                $_ =~ s/$search/$replace/;
                $this->Log(msg => "$search replaced with $replace");
            }

            print OUTFILE $_;
        }
        close INFILE or die("Could not close $file: $!");
        close OUTFILE or die("Coule not close $file.tmp: $!");
        if (not $found) {
            die("No " . $search . " found in file $file: $!");
        }

        if (not move("$file.tmp",
                     "$file")) {
            die("Cannot rename $file.tmp to $file: $!");
        }
    }

    my $bumpCiMsg = 'version bump, remove pre tag for ' 
                        . $product . ' ' . $version . ' release on ' 
                        . $minibranchTag;
    $this->Shell(
      cmd => 'cvs',
      cmdArgs => ['commit', '-m', $bumpCiMsg, 
                  @bumpFiles,
                 ],
      dir => catfile($releaseTagDir, 'cvsroot', 'mozilla'),
      logFile => catfile($logDir, 'tag-bump_checkin.log'),
    );
}

sub Verify {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $logDir = $config->Get(var => 'logDir');
    my $appName = $config->Get(var => 'appName');
    my $milestone = $config->Exists(var => 'milestone') ? 
     $config->Get(var => 'milestone') : undef;

    my $moduleVer = catfile($appName, 'app', 'module.ver');
    my $versionTxt = catfile($appName, 'config', 'version.txt');
    my $milestoneTxt = catfile('config', 'milestone.txt');
    my @bumpFiles = ('client.mk', $moduleVer, $versionTxt);

    # only bump milestone if it's defined in the config
    if (defined($milestone)) {
        @bumpFiles = (@bumpFiles, $milestoneTxt);
    }

    foreach my $file (@bumpFiles) {
        foreach my $rule ('^Checking in ' . $file, '^done') {
            $this->CheckLog(
              log => catfile($logDir, 'tag-bump_checkin.log'),
              checkFor => $rule,
            );
        }
    }
}

1;
