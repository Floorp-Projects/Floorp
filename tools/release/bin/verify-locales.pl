#!/usr/bin/perl
#
# TODO - use the file regexes to verify the contents of the directory.
#

use Getopt::Long;
use Cwd;

use strict;

use vars qw($DEFAULT_LOCALES_MANIFEST
            @ALL_PLATFORMS
            @NON_LOCALE_XPIS );

$DEFAULT_LOCALES_MANIFEST = 'shipped-locales';
@ALL_PLATFORMS = qw(win32 linux osx osxppc); # Bouncer platforms
@NON_LOCALE_XPIS = qw(browser talkback xpcom adt);

sub LoadLocaleManifest {
   my %args = @_;

   die "ASSERT: LoadLocaleManifest(): needs a HASH ref" if 
    (not exists($args{'localeHashRef'}) or 
    ref($args{'localeHashRef'}) ne 'HASH');

   my $localeHash = $args{'localeHashRef'};
   my $manifestFile = $args{'manifest'};

   if (not -e $manifestFile) {
      print STDERR "Can't find locale manifest $manifestFile; bailing...\n";
      PrintUsage(1);
   }

   open(MANIFEST, "$manifestFile") or return 0;
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
      $localeHash->{$locale} = scalar(@elements) ? \@elements : \@ALL_PLATFORMS;

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

sub VerifyLinuxLocales {
   my %args = @_;
   my $locales = $args{'locales'};
   my $directory = $args{'dir'};
   my $extension = $args{'extension'};
   my $product = lc($args{'product'});

   if (not -d $directory) {
      print STDERR "Can't find Linux directory!\n";
      return 0;
   }

   my $fileRegex = '^' . $product . '\-[\d\.]+(b\d+)?\.tar\.' . $extension .
    '(\.asc)?$';

   return VerifyDirectory(locales => $locales,
                          dir => $directory,
                          platform => 'linux',
                          fileRegex => $fileRegex);
}

sub VerifyWin32Locales {
   my %args = @_;
   my $locales = $args{'locales'};
   my $directory = $args{'dir'};
   my $product = ucfirst($args{'product'});

   if (not -d $directory) {
      print STDERR "Can't find Win32 directory!\n";
      return 0;
   }

   my $fileRegex = '^' . $product .
     '\ Setup\ [\d\.]+(\ Beta\ \d+)?\.exe(\.asc)?$';

   return VerifyDirectory(locales => $locales,
                          dir => $directory,
                          platform => 'win32',
                          fileRegex => $fileRegex);
}

sub VerifyMacLocales {
   my %args = @_;
   my $locales = $args{'locales'};
   my $directory = $args{'dir'};
   my $product = ucfirst($args{'product'});

   if (not -d $directory) {
      print STDERR "Can't find Mac directory!\n";
      return 0;
   }

   my $fileRegex = '^' . $product . '\ [\d\.]+(\ Beta\ \d+)?\.dmg(\.asc)?$';

   return VerifyDirectory(locales => $locales,
                          dir => $directory,
                          platform => 'osx',
                          fileRegex => $fileRegex);
}

sub VerifyDirectory {
   my %args = @_;
   my $locales = $args{'locales'};
   my $directory = $args{'dir'};
   my $fileRegex = $args{'fileRegex'};
   my $platform = $args{'platform'};


   my $localeDirStatus = VerifyLocaleDirectories(%args);
   my $xpiDirStatus = VerifyXPIDirectory(%args);
  
   return ($localeDirStatus and $xpiDirStatus);
}


sub VerifyLocaleDirectories {
   my %args = @_;
   my $locales = $args{'locales'};
   my $directory = $args{'dir'};
   my $fileRegex = $args{'fileRegex'};
   my $platform = $args{'platform'};
   my $status = 1;

   my $rv = opendir(DIR, $directory);
   if (not $rv) {
      print STDERR "opendir() on $directory FAILED: $!\n";
      return 0;
   }
   my @dirEntries = grep(!/^\.\.?$/, readdir(DIR));
   closedir(DIR);

   my %localesDirCheckCopy = %{$locales};

   foreach my $dirEnt (@dirEntries) {
      if (not -d "$directory/$dirEnt") {
         print STDERR "Non-directory entry found: $directory/$dirEnt\n";
         $status = 0;
         next;
      }

      # The xpi directory is obviously not a locale, but all of the directories
      # should have one; verification of the xpi dir is VerifyXPIDirectory's job
      next if ($dirEnt eq 'xpi');

      if (not(exists($localesDirCheckCopy{$dirEnt}))) {
         print STDERR "Invalid (extra) locale present: $dirEnt\n";
         $status = 0;
         next;
      }

      my $platformList = $localesDirCheckCopy{$dirEnt};

      if (not grep($platform eq $_, @{$platformList})) {
         print STDERR "Invalid (extra) locale for platform '$platform': $dirEnt\n";
         $status = 0;
      } else {
         delete($localesDirCheckCopy{$dirEnt});
      }

      # We verified this is a directory above...
      my $rv = opendir(DIR, "$directory/$dirEnt");
      if (not $rv) {
         print STDERR "opendir() on $directory/$dirEnt FAILED: $!\n";
         next;
      }
      my @installerFileEntries = grep(!/^\.\.?$/, readdir(DIR));
      closedir(DIR);

      # The directory should not be empty...
      if (not scalar(@installerFileEntries)) {
         print STDERR "Empty locale directory: $directory/$dirEnt";
         $status = 0;
         next;
      }

      foreach my $pkg (@installerFileEntries) {
         if (not -f "$directory/$dirEnt/$pkg") {
            print STDERR "Unexpected, non-file in $directory/$dirEnt: $pkg\n";
            $status = 0;
            next;
         }

         if ($pkg !~ /$fileRegex/) {
            print STDERR "Unknown file in $directory/$dirEnt: $pkg\n";
            $status = 0;
         }
      }
   }

   foreach my $locale (keys(%localesDirCheckCopy)) {
      if (grep($platform eq $_, @{$localesDirCheckCopy{$locale}})) {
         print STDERR "Missing locale for platform '$platform': $locale\n";
         $status = 0;
      }
   }
  
   return $status;
}


sub VerifyXPIDirectory {
   my %args = @_;
   my $locales = $args{'locales'};
   my $directory = "$args{'dir'}/xpi";
   my $fileRegex = $args{'fileRegex'};
   my $platform = $args{'platform'};
   my $status = 1;

   my $rv = opendir(XPIDIR, $directory);

   if (not $rv) {
      print STDERR "Couldn't opendir() $directory: $!\n";
      return 0;
   }

   my @xpiDirEntries = grep(!/^\.\.?$/, readdir(XPIDIR));
   closedir(XPIDIR);

   my %localesDirCheckCopy = %{$locales};

   # we last shipped an en-US.xpi for Firefox 1.5.0.4
   delete($localesDirCheckCopy{'en-US'});

   foreach my $file (@xpiDirEntries) {
      if ($file !~ /\.xpi$/) {
         print STDERR "Non-xpi file found in $directory: $file\n";
         $status = 0;
         next;
      }

      my $locale = $file;
      $locale =~ s/\.xpi//;

      # Ignore browser.xpi, talkback.xpi, etc.
      next if (grep($locale eq $_, @NON_LOCALE_XPIS));

      my $platformList = $localesDirCheckCopy{$locale};

      if (not grep($platform eq $_, @{$platformList})) {
         print STDERR "Invalid (extra) locale XPI for platform '$platform': $locale\n";
         $status = 0;
      } else {
         delete($localesDirCheckCopy{$locale});
      }
   }

   foreach my $locale (keys(%localesDirCheckCopy)) {
      if (grep($platform eq $_, @{$localesDirCheckCopy{$locale}})) {
         print STDERR "Missing XPI locale for platform '$platform': $locale\n";
         $status = 0;
      }
   }

   # we stopped shipping these at Firefox 1.5.0.4
   foreach my $nonLocaleXpi (@NON_LOCALE_XPIS) {
      if (-e "$directory/$nonLocaleXpi.xpi") {
         print STDERR "Invalid (extra) non-locale XPI: $directory/$nonLocaleXpi\n";
         $status = 0;
      }
   }

   return $status;
}

sub PrintUsage {
   my $exitCode = shift;

   print<<__END_USAGE__;

Using the shipped-locales file from CVS, verify in an FTP staging directory
structure that all the various locales that are to be shipped exist for every
platform, and that no extra locales for any platform are being shipped.

Usage:

$0 [ -m <locale manifest> ]

	-m,--locale-manifest	The 'shipped-locales' file, from CVS.
				Defaults to 'shipped-locales' in the cwd.

	-d,--check-directory	The directory to verify.
				Should have the default FTP directory structure.
				Defaults to the current working directory.
	-l,--linux-extension	The file extension used for Linux
				packages. This must be either 'gz' or 'bz2'.
__END_USAGE__

   exit($exitCode) if (defined($exitCode));
}

sub main {
   my $showHelp = 0;
   my $localeManifest = $DEFAULT_LOCALES_MANIFEST;
   my $shellReturnVal = 0;
   my $checkDir = getcwd();
   my $linuxExtension = '';
   my $product = '';

   Getopt::Long::GetOptions('h|help|?' => \$showHelp,
    'm|locale-manifest=s' => \$localeManifest,
    'd|check-directory=s' => \$checkDir,
    'l|linux-extension=s' => \$linuxExtension,
    'p|product=s' => \$product)
    or PrintUsage(1); 

   # Fullpath $checkDir...
   $checkDir = getcwd() . '/' . $checkDir if ($checkDir !~ m:^/:);
   # And strip any trailing /'s...
   $checkDir =~ s:/+$::;

   if (not -d $checkDir || not -r $checkDir || not -x $checkDir) {
      print STDERR "Can't find/access directory to validate: $checkDir\n";
      PrintUsage(1);
   }

   if ($linuxExtension ne 'gz' && $linuxExtension ne 'bz2') {
      print STDERR "--linux-extension must be 'gz' or 'bz2'\n";
      PrintUsage(1);
   }

   if (length($product) < 1) {
      print STDERR "--product/-p is a required option.\n";
      PrintUsage(1);
   }

   PrintUsage(0) if ($showHelp);

   my $locales = {};
   my $success = LoadLocaleManifest(localeHashRef => $locales,
                                    manifest => $localeManifest);

   die "Failed to load locale manifest" if (not $success);
   die "ASSERT: checkDir needs to be full-pathed at this point: $checkDir"
    if ($checkDir !~ m:^/:);

   $shellReturnVal = (
    VerifyLinuxLocales(locales => $locales, dir => "$checkDir/linux-i686",
      extension => $linuxExtension, product => $product) &
    VerifyWin32Locales(locales => $locales, dir => "$checkDir/win32",
      product => $product) &
    VerifyMacLocales(locales => $locales, dir => "$checkDir/mac",
      product => $product)
   );

   # TODO - VerifyUpdateLocales(locales => $locales, dir => 'update');

   if ($shellReturnVal) {
      print STDERR "PASS: all files present, no extras\n";
   } else {
      print STDERR "FAIL: errors occurred\n";
   }

   # Unix truth values... 0 for success, etc., etc., etc.; so, all the 
   # Verify* functions return true/false and are ANDed together; if 
   # there's a single failure, the AND is false, so return true... 
   # intuitive, eh?
   return not $shellReturnVal;
}

# call the code & propagate the error status to the outside world
exit main();
