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

use strict;

sub Execute {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $product = $config->Get(var => 'product');
    my $version = $config->Get(var => 'version');
    my $rc = $config->Get(var => 'rc');
    my $logDir = $config->Get(var => 'logDir');
    my $stageHome = $config->Get(var => 'stageHome');
 
    ## Prepare the staging directory for the release.
    # Create the staging directory.

    my $stageDir = catfile($stageHome, $product . '-' . $version);
    my $mergeDir = catfile($stageDir, 'stage-merged');

    if (not -d $stageDir) {
        MkdirWithPath(dir => $stageDir) 
          or die("Could not mkdir $stageDir: $!");
        $this->Log(msg => "Created directory $stageDir");
    }
 
    # Create skeleton batch directory.
    my $skelDir = catfile($stageDir, 'batch-skel', 'stage');
    if (not -d "$skelDir") {
        MkdirWithPath(dir => $skelDir) 
          or die "Cannot create $skelDir: $!";
        $this->Log(msg => "Created directory $skelDir");
    }

    my (undef, undef, $gid) = getgrnam($product)
      or die "Could not getgrname for $product: $!";

    # Create the contrib and contrib-localized directories with expected 
    # access rights.
    for my $dir ('contrib', 'contrib-localized') {
        my $fullDir = catfile($skelDir, $dir);
        if (not -d $fullDir) {
            MkdirWithPath(dir => $fullDir) 
              or die "Could not mkdir $fullDir : $!";
            $this->Log(msg => "Created directory $fullDir");
        }

        chmod(oct(2775), $fullDir)
          or die "Cannot change mode on $fullDir to 2775: $!";
        $this->Log(msg => "Changed mode of $fullDir to 2775");
        chown(-1, $gid, $fullDir)
          or die "Cannot chgrp $fullDir to $product: $!";
        $this->Log(msg => "Changed group of $fullDir to $product");
    }

    # NOTE - should have a standard "master" copy somewhere else
    # Copy the KEY file from the previous release directory.
    my $keyFile = catfile('/home', 'ftp', 'pub', $product, 'releases', '1.5',
                          'KEY');
    copy($keyFile, $skelDir) or die("Could not copy $keyFile to $skelDir: $!");

    ## Prepare the merging directory.
    $this->Shell(
      cmd => 'rsync',
      cmdArgs => ['-av', 'batch-skel/stage/', 'stage-merged/'],
      logFile => catfile($logDir, 'stage_merge_skel.log'),
      dir => $stageDir,
    );
    
    # Collect the release files onto stage.mozilla.org.

    my $prestageDir = catfile($stageDir, 'batch1', 'prestage');
    if (not -d $prestageDir) {
        MkdirWithPath(dir => $prestageDir) 
          or die "Cannot create $prestageDir: $!";
        $this->Log(msg => "Created directory $prestageDir");
    }

    $this->Shell(
      cmd => 'rsync',
      cmdArgs => ['-Lav', catfile('/home', 'ftp', 'pub', $product, 'nightly',
                                    $version . '-candidates', 'rc' . $rc ) . 
                                 '/',
                    './'],
      logFile => catfile($logDir, 'stage_collect.log'),
      dir => catfile($stageDir, 'batch1', 'prestage'),
    );

    # Remove unreleased builds
    $this->Shell(
      cmd => 'rsync',
      cmdArgs => ['-av', 'prestage/', 'prestage-trimmed/'],
      logFile => catfile($logDir, 'stage_collect_trimmed.log'),
      dir => catfile($stageDir, 'batch1'),
    );

    # Remove unshipped files and set proper mode on dirs
    find(sub { return $this->TrimCallback(); },
     catfile($stageDir, 'batch1', 'prestage-trimmed'));
    
    $this->Shell(
      cmd => 'rsync',
      cmdArgs => ['-Lav', 'prestage-trimmed/', 'stage/'],
      logFile => catfile($logDir, 'stage_collect_stage.log'),
      dir => catfile($stageDir, 'batch1'),
    );

    # Nightly builds using a different naming scheme than production.
    # Rename the files.
    # TODO should support --long filenames, for e.g. Alpha and Beta
    $this->Shell(
      cmd => catfile($stageHome, 'bin', 'groom-files'),
      cmdArgs => ['--short=' . $version, '.'],
      logFile => catfile($logDir, 'stage_groom_files.log'),
      dir => catfile($stageDir, 'batch1', 'stage'),
    );

    # fix xpi dir names

    my %xpiDirs = ('windows-xpi' => 'win32',
                   'linux-xpi' => 'linux-i686',
                   'mac-xpi' => 'mac');

    foreach my $xpiDir (keys(%xpiDirs)) {
        my $fromDir = catfile($stageDir, 'batch1', 'stage', $xpiDir);
        my $toDir = catfile($stageDir, 'batch1', 'stage', $xpiDirs{$xpiDir},
         'xpi');

        if (-e $fromDir) {
           move($fromDir, $toDir)
            or die(msg => "Cannot rename $fromDir $toDir: $!");
           $this->Log(msg => "Moved $fromDir $toDir");
        }
    }
}

sub Verify {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $product = $config->Get(var => 'product');
    my $appName = $config->Get(var => 'appName');
    my $logDir = $config->Get(var => 'logDir');
    my $version = $config->Get(var => 'version');
    my $rc = $config->Get(var => 'rc');
    my $stageHome = $config->Get(var => 'stageHome');
 
    ## Prepare the staging directory for the release.
    # Create the staging directory.

    my $stageDir = catfile($stageHome, $product . '-' . $version);

    # Verify locales
    $this->Shell(
      cmd => catfile($stageHome, 'bin', 'verify-locales.pl'),
      cmdArgs => ['-m', catfile($stageDir, 'batch-source', 'rc' . $rc,
                  'mozilla', $appName, 'locales', 'shipped-locales')],
      logFile => catfile($logDir, 'stage_verify_l10n.log'),
      dir => catfile($stageDir, 'batch1', 'stage'),
    );
}

sub TrimCallback {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $dirent = $File::Find::name;

    if (-f $dirent) {
        if (($dirent =~ /xforms\.xpi/) || 
        # ja-JP-mac is the JA locale for mac, do not ship ja
        ($dirent =~ /ja\.mac/) ||
        ($dirent =~ /mac\-xpi\/ja\.xpi$/) ||
        # ja is the JA locale for win32/linux, do not ship ja-JP-mac
        ($dirent =~ /ja\-JP\-mac\.win32/) ||
        ($dirent =~ /ja\-JP\-mac\.linux/) ||
        ($dirent =~ /windows\-xpi\/ja\-JP\-mac\.xpi$/) ||
        ($dirent =~ /linux\-xpi\/ja\-JP\-mac\.xpi$/) ||
        # MAR files are merged in later
        ($dirent =~ /\.mar$/) ||
        # ZIP files are not shipped
        ($dirent =~ /\.zip$/) ||
        ($dirent =~ /en-US\.xpi$/)) {
            unlink($dirent) || die "Could not unlink $dirent: $!";
            $this->Log(msg => "Unlinked $dirent");
        } else {
            chmod(0644, $dirent) 
              || die "Could not chmod $dirent to 0644: $!";
            $this->Log(msg => "Changed mode of $dirent to 0644");
       }
    } elsif (-d $dirent) { 
        my $product = $config->Get(var => 'product');
        my (undef, undef, $gid) = getgrnam($product)
         or die "Could not getgrname for $product: $!";
        chown(-1, $gid, $dirent)
          or die "Cannot chgrp $dirent to $product: $!";
        $this->Log(msg => "Changed group of $dirent to $product");
        chmod(0755, $dirent) 
          or die "Could not chmod $dirent to 0755: $!";
        $this->Log(msg => "Changed mode of $dirent to 0755");
    } else {
        die("Unexpected non-file/non-dir directory entry: $dirent");
    }
}

sub Announce {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $product = $config->Get(var => 'product');
    my $version = $config->Get(var => 'version');

    $this->SendAnnouncement(
      subject => "$product $version stage step finished",
      message => "$product $version staging area has been created.",
    );
}

1;
