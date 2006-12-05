#
# Stage step. Copies nightly build format filenames to release directory
# structure.
# 
package Bootstrap::Step::Stage;
use Bootstrap::Step;
use Bootstrap::Config;
use File::Copy qw(copy move);
use File::Find qw(find);
use MozBuild::Util qw(MkdirWithPath);
@ISA = ("Bootstrap::Step");

my $config = new Bootstrap::Config;

sub Execute {
    my $this = shift;

    my $product = $config->Get('var' => 'product');
    my $version = $config->Get('var' => 'version');
    my $rc = $config->Get('var' => 'rc');
    my $logDir = $config->Get('var' => 'logDir');
    my $stageHome = $config->Get('var' => 'stageHome');
 
    ## Prepare the staging directory for the release.
    # Create the staging directory.

    my $stageDir = $stageHome . '/' . $product . '-' . $version;
    my $mergeDir = $stageDir . '/stage-merged';

    if (not -d $stageDir) {
        MkdirWithPath('dir' => $stageDir) 
          or die "Could not mkdir $stageDir: $!";
    }
 
    # Create skeleton batch directory.
    my $skelDir = "$stageDir/batch-skel/stage";
    if (not -d "$skelDir") {
        MkdirWithPath('dir' => $skelDir) 
          or die "Cannot create $stageDir: $!";
    }

    # Create the contrib and contrib-localized directories with expected 
    # access rights.
    if (not -d "$skelDir/contrib") {
        MkdirWithPath('dir' => "$skelDir/contrib") 
          or die "Could not mkdir $skelDir/contrib: $!";
    }

    my (undef, undef, $gid) = getgrnam($product)
      or die "Could not getgrname for $product: $!";

    my $dir = "$skelDir/contrib";
    chmod(oct(2775), $dir)
      or die "Cannot change mode on $skelDir/contrib to 2775: $!";
    $this->Log('msg' => "Changed mode of $dir to 2775");
    chown(-1, $gid, $dir)
      or die "Cannot chgrp $skelDir/contrib to $product: $!";
    $this->Log('msg' => "Changed group of $dir to $product");
 
    # NOTE - should have a standard "master" copy somewhere else
    # Copy the KEY file from the previous release directory.
    copy("$product/releases/1.5/KEY", "$skelDir/");

    ## Prepare the merging directory.
    $this->Shell(
      'cmd' => 'rsync -av batch-skel/stage/ stage-merged/',
      'logFile' => $logDir . '/stage-merge_skel.log',
      'dir' => $stageDir,
    );
    
    # Collect the release files onto stage.mozilla.org.

    if (not -d "$stageDir/batch1/prestage") {
        MkdirWithPath('dir' => "$stageDir/batch1/prestage") 
          or die "Cannot create $stageDir/batch1/prestage: $!";
    }

    $this->Shell(
      'cmd' => 'rsync -Lav /home/ftp/pub/' . $product . '/nightly/' . $version . '-candidates/rc' . $rc . '/ ./',
      'logFile' => $logDir . '/stage-collect.log',
      'dir' => $stageDir . '/batch1/prestage',
    );

    # Remove unreleased builds
    $this->Shell(
      'cmd' => 'rsync -av prestage/ prestage-trimmed/',
      'logFile' => $logDir . '/stage-collect_trimmed.log',
      'dir' => $stageDir . '/batch1/',
    );

    # Remove unshipped files and set proper mode on dirs
    find(&TrimCallback, $stageDir . '/batch1/prestage-trimmed/');
    
    $this->Shell(
      'cmd' => 'rsync -Lav prestage-trimmed/ stage/',
      'logFile' => $logDir . '/stage-collect_stage.log',
      'dir' => $stageDir . '/batch1',
    );

    # Nightly builds using a different naming scheme than production.
    # Rename the files.
    # TODO should support --long filenames, for e.g. Alpha and Beta
    $this->Shell(
      'cmd' => $stageHome . '/bin/groom-files --short=' . $version . ' .',
      'logFile' => $logDir . '/stage-groom_files.log',
      'dir' => $stageDir . '/batch1/stage',
    );

    # fix xpi dir names
    move("$stageDir/batch1/stage/linux-xpi", 
           "$stageDir/batch1/stage/linux-i686/xpi")
      or die('msg' => "Cannot rename $stageDir/batch1/stage/linux-xpi $stageDir/batch1/stage/linux-i686/xpi: $!");
    move("$stageDir/batch1/stage/windows-xpi", 
           "$stageDir/batch1/stage/win32/xpi")
      or die('msg' => "Cannot rename $stageDir/batch1/stage/windows-xpi $stageDir/batch1/stage/win32/xpi: $!");
    move("$stageDir/batch1/stage/mac-xpi", 
           "$stageDir/batch1/stage/mac/xpi")
      or die('msg' => "Cannot rename $stageDir/batch1/stage/mac-xpi $stageDir/batch1/stage/mac/xpi: $!");
}

sub Verify {
    my $this = shift;

    my $product = $config->Get('var' => 'product');
    my $appName = $config->Get('var' => 'appName');
    my $logDir = $config->Get('var' => 'logDir');
    my $version = $config->Get('var' => 'version');
    my $rc = $config->Get('var' => 'rc');
    my $stageHome = $config->Get('var' => 'stageHome');
 
    ## Prepare the staging directory for the release.
    # Create the staging directory.

    my $stageDir = $stageHome . '/' . $product . '-' . $version;

    # Verify locales
    $this->Shell(
      'cmd' => $stageHome . '/bin/verify-locales.pl -m ' . $stageDir . '/batch-source/rc' . $rc . '/mozilla/' . $appName . '/locales/shipped-locales',
      'logFile' => $logDir . '/stage-verify_l10n.log',
      'dir' => $stageDir . '/batch1/stage',
    );
}

sub TrimCallback {
    my $dirent = $File::Find::name;

    my (undef, undef, $gid) = getgrnam($product)
      or die "Could not getgrname for $product: $!";

    if (-f $dirent) {
        if (($dirent =~ /xforms\.xpi/) || 
        # ja-JP-mac is the JA locale for mac, do not ship ja
        ($dirent =~ /ja\.mac/) ||
        # ja is the JA locale for win32/linux, do not ship ja-JP-mac
        ($dirent =~ /ja-JP-mac\.win32/) ||
        ($dirent =~ /ja-JP-mac\.linux/) ||
        ($dirent =~ /^windows-xpi\/ja-JP-mac\.xpi$/) ||
        ($dirent =~ /^linux-xpi\/ja-JP-mac\.xpi$/) ||
        # MAR files are merged in later
        ($dirent =~ /\.mar$/) ||
        # ZIP files are not shipped
        ($dirent =~ /en-US\.xpi$/)) {
            unlink($dirent) || die "Could not unlink $dirent: $!";
            $this->Log('msg' => "Unlinked $dirent");
        } else {
            chmod(0644, $dirent) 
              || die "Could not chmod $dirent to 0644: $!";
            $this->Log('msg' => "Changed mode of $dirent to 0644");
       }
    } elsif (-d $dirent) { 
        chown(-1, $gid, $dirent)
          or die "Cannot chgrp $dirent to $product: $!";
        $this->Log('msg' => "Changed group of $dirent to $product");
        chmod(0755, $dirent) 
          or die "Could not chmod $dirent to 0755: $!";
        $this->Log('msg' => "Changed mode of $dirent to 0755");
    } else {
        die("Unexpected non-file/non-dir directory entry: $dirent");
    }
}

1;
