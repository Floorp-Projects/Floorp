#
# Stage step. Copies nightly build format filenames to release directory
# structure.
# 
package Bootstrap::Step::Stage;

use Bootstrap::Step;
use Bootstrap::Config;
use File::Copy qw(copy move);
use File::Find qw(find);
use File::Path qw(rmtree);
use File::Basename;

use MozBuild::Util qw(MkdirWithPath);

@ISA = ("Bootstrap::Step");

use strict;

#
# List of directories that are allowed to be in the prestage-trimmed directory,
# theoretically because TrimCallback() will know what to do with them.
#
my @ALLOWED_DELIVERABLE_DIRECTORIES = qw(windows-xpi mac-xpi linux-xpi);

# List of bouncer platforms; also used in shipped-locales files...
my @ALL_PLATFORMS = qw(win32 linux osx osxppc);

# Various platform maps that map things to to the platform name used in the
# shipped-locale files (which happen to be bouncer OSes, for some reason).
my %DELIVERABLE_TO_PLATFORM = ('win32' => 'win32',
                               'mac' => 'osx',
                               'linux-i686' => 'linux');

my %XPIDIR_TO_PLATFORM = ('windows-xpi' => 'win32',
                          'mac-xpi' => 'osx',
                          'linux-xpi' => 'linux');

my @NON_LOCALE_XPIS = qw(adt.xpi browser.xpi talkback.xpi xpcom.xpi);

# Loads and parses the shipped-locales manifest file, so get hash of
# locale -> [bouncer] platform mappings; returns success/failure in 
# reading/parsing the locales file.

sub LoadLocaleManifest {
    my $this = shift;
    my %args = @_;

    die "ASSERT: LoadLocaleManifest(): needs a HASH ref" if 
     (not exists($args{'localeHashRef'}) or 
     ref($args{'localeHashRef'}) ne 'HASH');

    my $localeHash = $args{'localeHashRef'};
    my $manifestFile = $args{'manifest'};

    if (not -e $manifestFile) {
       $this->Log(msg => "Can't find manifest $manifestFile");
       return 0;
    }

    open(MANIFEST, "<$manifestFile") or return 0;
    my @manifestLines = <MANIFEST>;
    close(MANIFEST);

    foreach my $line (@manifestLines) {
       my @elements = split(/\s+/, $line);
       # Grab the locale; we do it this way, so we can use the rest of the
       # array as a platform list, if we need it...
       my $locale = shift(@elements);
 
       # We don't add a $ on the end, because of things like ja-JP-mac
       if ($locale !~ /^[a-z]{2}(\-[A-Z]{2})?/) {
          die "ASSERT: invalid locale in manifest file: $locale";
       }

       # So this is kinda weird; if the locales are followed by a platform,
       # then they don't exist for all the platforms; if they're all by their
       # lonesome, then they exist for all platforms. So, since we shifted off
       # the locale above, if there's anything left in the array, it's the
       # platforms that are valid for this locale; if there isn't, then that
       # platform is valid for all locales.
       $localeHash->{$locale} = scalar(@elements) ? \@elements : 
                                                    \@ALL_PLATFORMS;

       foreach my $platform (@{$localeHash->{$locale}}) {
          die "ASSERT: invalid platform: $platform" if 
           (not grep($platform eq $_, @ALL_PLATFORMS));
       }
    }

    # Add en-US, which isn't in shipped-locales, because it's assumed that
    # we always need en-US, for all platforms, to ship.
    $localeHash->{'en-US'} = \@ALL_PLATFORMS;

    return 1;
}

sub GetStageDir {
    my $this = shift;

    my $config = new Bootstrap::Config();

    my $stageHome = $config->Get(var => 'stageHome');
    my $product = $config->Get(var => 'product');
    my $version = $config->Get(var => 'version');
    return catfile($stageHome, $product . '-' . $version);
}

sub Execute {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $product = $config->Get(var => 'product');
    my $version = $config->Get(var => 'version');
    my $rc = $config->Get(var => 'rc');
    my $logDir = $config->Get(var => 'logDir');
    my $stageHome = $config->Get(var => 'stageHome');
    my $appName = $config->Get(var => 'appName');
    my $mozillaCvsroot = $config->Get(var => 'mozillaCvsroot');
    my $releaseTag = $config->Get(var => 'productTag') . '_RELEASE';
 
    ## Prepare the staging directory for the release.
    # Create the staging directory.

    my $stageDir = $this->GetStageDir();
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

    # TODO - should have a standard "master" copy somewhere else
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
    
    # Collect the release files from the candidates directory into a prestage
    # directory.
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

    # Create a pruning/"trimmed" area; this area will be used to remove
    # locales and deliverables we don't ship. 
    $this->Shell(
      cmd => 'rsync',
      cmdArgs => ['-av', 'prestage/', 'prestage-trimmed/'],
      logFile => catfile($logDir, 'stage_collect_trimmed.log'),
      dir => catfile($stageDir, 'batch1'),
    );

    # Remove unknown/unrecognized directories from the -candidates dir; after
    # this, the only directories that should be in the prestage-trimmed
    # directory are directories that we expliciately handle below, to prep
    # for groom-files.
    $this->{'scrubTrimmedDirDeleteList'} = [];
    find(sub { return $this->ScrubTrimmedDirCallback(); },
     catfile($stageDir, 'batch1', 'prestage-trimmed'));

    foreach my $delDir (@{$this->{'scrubTrimmedDirDeleteList'}}) {
        if (-e $delDir && -d $delDir) {
            $this->Log(msg => "rmtree() ing $delDir");
            if (rmtree($delDir, 1, 1) <= 0) {
                die("ASSERT: rmtree() called on $delDir, but nothing deleted.");
            }
        }
    }

    # Remove unshipped files/locales and set proper mode on dirs; start
    # by checking out the shipped-locales file
    $ENV{'CVS_RSH'} = 'ssh';
    $this->Shell(cmd => 'cvs',
      cmdArgs => ['-d', $mozillaCvsroot, 
                  'co', '-dconfig',
                  '-r', $releaseTag,
                  catfile('mozilla', $appName, 'locales', 'shipped-locales')],
      dir => catfile($stageDir, 'batch1'),
      logFile => catfile($logDir, 'stage-shipped-locales_checkout.log'));

    $this->{'localeManifest'} = {};
    if (not $this->LoadLocaleManifest(localeHashRef =>
                                       $this->{'localeManifest'},
                                       manifest => catfile($stageDir, 'batch1',
                                        'config', 'shipped-locales'))) {
        die "Failed to load locale manifest\n";
    }

    # All the magic happens here; we remove unshipped deliverables and cross-
    # check the locales we do ship in this callback.
    find(sub { return $this->TrimCallback(); },
     catfile($stageDir, 'batch1', 'prestage-trimmed'));
   
    # Create a stage-unsigned directory to run groom-files in.
    $this->Shell(
      cmd => 'rsync',
      cmdArgs => ['-av', 'prestage-trimmed/', 'stage-unsigned/'],
      logFile => catfile($logDir, 'stage_collect_stage.log'),
      dir => catfile($stageDir, 'batch1'),
    );

    find(sub { return $this->RemoveMarsCallback(); },
     catfile($stageDir, 'batch1', 'stage-unsigned'));

    # Nightly builds using a different naming scheme than production.
    # Rename the files.
    # TODO should support --long filenames, for e.g. Alpha and Beta
    $this->Shell(
      cmd => catfile($stageHome, 'bin', 'groom-files'),
      cmdArgs => ['--short=' . $version, '.'],
      logFile => catfile($logDir, 'stage_groom_files.log'),
      dir => catfile($stageDir, 'batch1', 'stage-unsigned'),
    );

    # fix xpi dir names - This is a hash of directory names in the pre-stage
    # dir -> directories under which those directories should be moved to;
    # the name will be "xpi", so windows-xpi becomes win32/xpi, etc.
    my %xpiDirs = ('windows-xpi' => 'win32',
                   'linux-xpi' => 'linux-i686',
                   'mac-xpi' => 'mac');

    foreach my $xpiDir (keys(%xpiDirs)) {
        my $fromDir = catfile($stageDir, 'batch1', 'stage-unsigned', $xpiDir);
        my $toDir = catfile($stageDir, 'batch1', 'stage-unsigned',
         $xpiDirs{$xpiDir}, 'xpi');

        if (-e $fromDir) {
           move($fromDir, $toDir)
            or die(msg => "Cannot rename $fromDir $toDir: $!");
           $this->Log(msg => "Moved $fromDir -> $toDir");
        } else {
           $this->Log(msg => "Couldn't find $fromDir; not moving to $toDir");
        }
    }

    # Create an rsync'ed stage-signed directory to rsync over to the 
    # signing server.
    $this->Shell(
      cmd => 'rsync',
      cmdArgs => ['-av', 'stage-unsigned/', 'stage-signed/'],
      logFile => catfile($logDir, 'stage_unsigned_to_sign.log'),
      dir => catfile($stageDir, 'batch1'),
    );


    # Process the update mars; we copy everything from the trimmed directory
    # that we created above; this will have only the locales/deliverables
    # we actually ship; then, remove everything but the mars, including
    # the [empty] directories; then, run groom-files in the directory that 
    # has only updates now.
    $this->Shell(
      cmd => 'rsync',
      cmdArgs => ['-av', 'prestage-trimmed/', 'mar/'],
      logFile => catfile($logDir, 'stage_trimmed_to_mars.log'),
      dir => catfile($stageDir, 'batch1'),
    );

    $this->{'leaveOnlyMarsDirDeleteList'} = [];
    find(sub { return $this->LeaveOnlyUpdateMarsCallback(); },
     catfile($stageDir, 'batch1', 'mar'));

    foreach my $delDir (@{$this->{'leaveOnlyUpdateMarsDirDeleteList'}}) {
        if (-e $delDir && -d $delDir) {
            $this->Log(msg => "rmtree() ing $delDir");
            if (rmtree($delDir, 1, 1) <= 0) {
                die("ASSERT: rmtree() called on $delDir, but nothing deleted.");
            }
        }
    }

    $this->Shell(
      cmd => catfile($stageHome, 'bin', 'groom-files'),
      cmdArgs => ['--short=' . $version, '.'],
      logFile => catfile($logDir, 'stage_groom_files_updates.log'),
      dir => catfile($stageDir, 'batch1', 'mar'),
    );
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

    my $stageDir = $this->GetStageDir();

    # Verify locales
    $this->Shell(
      cmd => catfile($stageHome, 'bin', 'verify-locales.pl'),
      cmdArgs => ['-m', catfile($stageDir, 'batch1', 'config',
                  'shipped-locales')],
      logFile => catfile($logDir, 'stage_verify_l10n.log'),
      dir => catfile($stageDir, 'batch1', 'stage-signed'),
    );
}

sub LeaveOnlyUpdateMarsCallback {
    my $this = shift;
    my $dirent = $File::Find::name;

    if (-f $dirent) {
        if ($dirent !~ /\.mar$/) {
            $this->Log(msg => "Unlinking non-mar deliverable: $dirent");
            unlink($dirent) or die("Couldn't unlink $dirent");
        }
    } elsif (-d $dirent) {
        push(@{$this->{'leaveOnlyMarsDirDeleteList'}}, $dirent);
    } else {
        $this->Log(msg => 'WARNING: LeaveOnlyUpdateMarsCallback(): '. 
         "Unknown dirent type: $dirent");
    }
}

sub RemoveMarsCallback {
    my $this = shift;
    my $dirent = $File::Find::name;

    if (-f $dirent) {
        if ($dirent =~ /\.mar$/) {
            $this->Log(msg => "Unlinking mar: $dirent");
            unlink($dirent) or die("Couldn't unlink $dirent");
        }
    } elsif (-d $dirent) {
        # do nothing
    } else {
        $this->Log(msg => 'WARNING: RemoveMarsCallback(): '. 
         "Unknown dirent type: $dirent");
    }
}

#
# This is a bit of an odd callback; the idea is to emulate find's -maxdepth
# option with an argument of 1; unfortunately, find2perl barfs on this option.
# Also, File::Find::find() is an annoying combination of depth and breadth-first
# search, depending on which version is installed (find() used to be different
# than finddepth(), but now they're the same?).
#
# Anyway, what this does is add an entry in the object to add the list of
# directories that should be deleted; if we rmtree() those directories here,
# find() will go bonkers, because we removed things out from under it. So,
# the next step after find() is called with this callback is to rmtree()
# all the directories on the list that is populated in this callback.
#

sub ScrubTrimmedDirCallback {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $dirent = $File::Find::name;

    my $trimmedDir = catfile($this->GetStageDir(), 'batch1', 
     'prestage-trimmed');
  
    # if $dirent is a directory and is a direct child of the prestage-trimmed
    # directory (a hacky attempt at the equivalent of find's maxdepth 1 option);
    if (-d $dirent && dirname($dirent) eq $trimmedDir) {
        foreach my $allowedDir (@ALLOWED_DELIVERABLE_DIRECTORIES) {
            return if (basename($dirent) eq $allowedDir);
        }

        $this->Log(msg => "Adding extra RC directory entry for deletion: " .
         $dirent);
        push(@{$this->{'scrubTrimmedDirDeleteList'}}, $dirent);
    }
}

sub TrimCallback {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $dirent = $File::Find::name;

    if (-f $dirent) {
        # Don't ship xforms in the release area
        if (($dirent =~ /xforms\.xpi/) || 
         # ZIP files are not shipped; neither are en-US lang packs
         ($dirent =~ /\.zip$/) ||
         ($dirent =~ /en-US\.xpi$/)) {
          unlink($dirent) || die "Could not unlink $dirent: $!";
            $this->Log(msg => "Unlinked $dirent");
          return;
        }

        # source tarballs don't have a locale, so don't check them for one;
        # all other deliverables need to be checked to make sure they should
        # be in prestage-trimmed, i.e. if their locale shipped.
        if ($dirent !~ /\-source\.tar\.bz2$/) {
            my $validDeliverable = 0;
           
            # This logic is kinda weird, and I'd do it differently if I had
            # more time; basically, for 1.5.0.x branch, there are a set of
            # non-locale xpis that get shipped in the windows-xpi dir; so,
            # if the entry is an xpi, check it against the list of non-locale
            # xpis that we should ship. If it's one of those, let it through.
            # If not, it could still be a (shipable) locale-xpi, so give
            # IsValidLocaleDeliverable() a crack at it.
            #
            # If it's neither of those, then delete it.
            if ($dirent =~ /\.xpi$/) {
                my $xpiBasename = basename($dirent);
                $validDeliverable = grep(/^$xpiBasename$/, @NON_LOCALE_XPIS);
            }
           
            if (!$validDeliverable) {
                $validDeliverable = $this->IsValidLocaleDeliverable();
            }
            
            if (not $validDeliverable) {
                $this->Log(msg => "Deleting unwanted locale deliverable: " . 
                 $dirent);
                unlink($dirent) or die("Couldn't unlink() $dirent\n");
                return;
            }
        }

        chmod(0644, $dirent) 
         || die "Could not chmod $dirent to 0644: $!";
         $this->Log(msg => "Changed mode of $dirent to 0644");
    } elsif (-d $dirent) { 
        chmod(0755, $dirent) 
          or die "Could not chmod $dirent to 0755: $!";
        $this->Log(msg => "Changed mode of $dirent to 0755");
    } else {
        die("Unexpected non-file/non-dir directory entry: $dirent");
    }

    my $product = $config->Get(var => 'product');
    my (undef, undef, $gid) = getgrnam($product)
     or die "Could not getgrname for $product: $!";
    chown(-1, $gid, $dirent)
     or die "Cannot chgrp $dirent to $product: $!";
    $this->Log(msg => "Changed group of $dirent to $product");
}

sub IsValidLocaleDeliverable {
    my $this = shift;
    my %args = @_;

    my $dirent = $File::Find::name;

    my ($locale, $platform);
    my @parts = split(/\./, basename($dirent));
    my $partsCount = scalar(@parts);

    if ($dirent =~ /\.tar\.gz/) {
        # e.g. firefox-2.0.0.2.sk.linux-i686.tar.gz
        $locale = $parts[$partsCount - 4];
        $platform = 'linux';
    } elsif ($dirent =~ /\.exe/) {
        # e.g. firefox-2.0.0.2.zh-TW.win32.installer.exe
        $locale = $parts[$partsCount - 4];
        $platform = 'win32';
    } elsif ($dirent =~ /\.dmg/) {
        # e.g. firefox-2.0.0.2.tr.mac.dmg
        $locale = $parts[$partsCount - 3];
        $platform = 'osx';
    } elsif ($dirent =~ /\.xpi/) {
        # e.g. en-GB.xpi
        $locale = basename($File::Find::name);
        $locale =~ s/\.xpi$//;
        my $parentDir = basename($File::Find::dir);

        if (exists($XPIDIR_TO_PLATFORM{$parentDir})) {
            $platform = $XPIDIR_TO_PLATFORM{$parentDir};
        } else {
            die 'ASSERT: IsValidLocaleDeliverable(): xpis found in an ' .
             'unexpected directory';
        }
    } elsif ($dirent =~ /\.mar/) {
        # e.g. firefox-2.0.0.2.tr.win32.[partial,complete].mar
        $locale = $parts[$partsCount - 4];
        $platform = $DELIVERABLE_TO_PLATFORM{$parts[$partsCount - 3]};
    } else {
        $this->Log(msg => "WARNING: Unknown file type in tree: $dirent");
    }

    foreach my $allowedPlatform (@{$this->{'localeManifest'}->{$locale}}) {
        return 1 if ($allowedPlatform eq $platform);
    }

    return 0;
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
