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
    my $milestoneTxt = catfile($appName, 'config', 'milestone.txt');
    my @bumpFiles = ('client.mk', $moduleVer, $versionTxt, $milestoneTxt);

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
    foreach my $file (catfile($parentDir, $moduleVer), 
                      catfile($parentDir, $versionTxt),
                      catfile($parentDir, $milestoneTxt)) {
        my $found = 0;
        open(INFILE,  "< $file") or die("Could not open $file: $!");
        open(OUTFILE, "> $file.tmp") or die("Could not open $file.tmp: $!");
        while(<INFILE>) {
            my $preVersion = $version . 'pre';
            if($_ =~ s/$preVersion/$version/) {
                $found = 1;
            }
            if($_ =~ /$preVersion/) {
                $found = 1;
                $_ =~ s/pre$//g;
            }

            print OUTFILE $_;
        }
        close INFILE or die("Could not close $file: $!");
        close OUTFILE or die("Coule not close $file.tmp: $!");
        if (not $found) {
            die("No " . $version . "pre in file $file: $!");
        }

        if (not move("$file.tmp",
                     "$file")) {
            die("Cannot rename $clientMk.tmp to $clientMk: $!");
        }
    }

    # Add the new product tag to the client.mk
    my $clientMk = catfile($cvsrootTagDir, 'mozilla', 'client.mk');
    my $found = 0;
    open(INFILE,  "< $clientMk");
    open(OUTFILE, "> $clientMk.tmp");
    while(<INFILE>) {
        if ($_ =~ s/$branchTag/$releaseTag/g) {
            $found = 1;
        }
        print OUTFILE $_;
    }
    close INFILE;
    close OUTFILE;

    if (not $found) {
        die("No $branchTag in file $clientMk : $!");
    }

    if (not move("$clientMk.tmp",
                 "$clientMk")) {
        die("Cannot rename $clientMk.tmp to $clientMk: $!");
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

    my $moduleVer = catfile($appName, 'app', 'module.ver');
    my $versionTxt = catfile($appName, 'config', 'version.txt');
    my $milestoneTxt = catfile($appName, 'config', 'milestone.txt');
    my @bumpFiles = ('client.mk', $moduleVer, $versionTxt, $milestoneTxt);

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
