#
# Bootstrap utility functions.
# 

package Bootstrap::Util;

use File::Temp qw(tempfile tempdir);

use MozBuild::Util qw(RunShellCommand);

use base qw(Exporter);

our @EXPORT_OK = qw(CvsCatfile GetLocaleManifest
                    GetBouncerPlatforms GetPatcherPlatforms
                    GetBouncerToPatcherPlatformMap);

use strict;

# This maps Bouncer platforms, used in bouncer and the shipped-locales file
# to patcher2 platforms used in... patcher2. They're different for some
# historical reason, which should be considered a bug.
#
# This is somewhat incomplete, as Bouncer has the 'osxppc' platform and
# patcher2 has the 'unimac' platform, neither of which we need any more
# beacuse we don't need to disambiguate between PPC mac and universal binaries
# anymore.
#
# Also, bouncer uses "win", not win32; shipped-locales uses win32. They
# couldn't be the same, of course.

my %PLATFORM_MAP = (# bouncer/shipped-locales platform => patcher2 platform
                    'win32' => 'win32',
                    'linux' => 'linux-i686',
                    'osx' => 'mac',
                    'osxppc' => 'macppc');

my $DEFAULT_CVSROOT = ':pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot';


##
# Turn an array of directory/filenames into a CVS module path.
# ( name comes from File::Spec::catfile() )
#
# Note that this function does not take any arguments, to make the usage
# more like File::Spec::catfile()
##
sub CvsCatfile {
   return join('/', @_);
}

sub GetBouncerToPatcherPlatformMap {
   return %PLATFORM_MAP;
}

sub GetBouncerPlatforms {
   return keys(%PLATFORM_MAP);
}

sub GetPatcherPlatforms {
   return values(%PLATFORM_MAP);
}

# Loads and parses the shipped-locales manifest file, so get hash of
# locale -> [bouncer] platform mappings; returns success/failure in 
# reading/parsing the locales file.

sub LoadLocaleManifest {
    my %args = @_;

    die "ASSERT: LoadLocaleManifest(): needs a HASH ref" if 
     (not exists($args{'localeHashRef'}) or 
     ref($args{'localeHashRef'}) ne 'HASH');

    my $localeHash = $args{'localeHashRef'};
    my $manifestFile = $args{'manifest'};

    if (not -e $manifestFile) {
       die ("ASSERT: Bootstrap::Util::LoadLocaleManifest(): Can't find manifest"
        . " $manifestFile");
    }

    open(MANIFEST, "<$manifestFile") or return 0;
    my @manifestLines = <MANIFEST>;
    close(MANIFEST);

    my @bouncerPlatforms = sort(GetBouncerPlatforms());

    foreach my $line (@manifestLines) {   
       # We create an instance variable so if the caller munges the reference
       # to which a certain locale points, they won't screw up all the other
       # locales; previously, this was a shared array ref, which is bad.
       my @bouncerPlatformsInstance = @bouncerPlatforms;

       my @elements = split(/\s+/, $line);
       # Grab the locale; we do it this way, so we can use the rest of the
       # array as a platform list, if we need it...
       my $locale = shift(@elements);
       @elements = sort(@elements);

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
                                                    \@bouncerPlatformsInstance;

       foreach my $platform (@{$localeHash->{$locale}}) {
          die "ASSERT: invalid platform: $platform" if 
           (not grep($platform eq $_, @bouncerPlatforms));
       }
    }

    # Add en-US, which isn't in shipped-locales, because it's assumed that
    # we always need en-US, for all platforms, to ship.
    $localeHash->{'en-US'} = \@bouncerPlatforms;
    return 1;
}
   

sub GetLocaleManifest {
    my %args = @_;

    my $mozillaCvsroot = $args{'cvsroot'} || $DEFAULT_CVSROOT;
    # XXX - The cruel joke is that this default value won't work; there is
    # no shipped-locales file on the trunk... yet...
    my $releaseTag = $args{'tag'} || 'HEAD';
    my $appName = $args{'app'}; 

    my $localeManifest = {};

    # Remove unshipped files/locales and set proper mode on dirs; start
    # by checking out the shipped-locales file
    $ENV{'CVS_RSH'} = 'ssh';

    my ($shippedLocalesTmpHandle, $shippedLocalesTmpFile) = tempfile();
    $shippedLocalesTmpHandle->close();

    # We dump stderr here so we can ignore having to parse all of the
    # RCS info that cvs dumps when you co with -p
    my $rv = RunShellCommand(command => 'cvs',
                             args => ['-d', $mozillaCvsroot, 
                                      'co', '-p',
                                      '-r', $releaseTag,
                                      CvsCatfile('mozilla', $appName,
                                       'locales', 'shipped-locales')],
                             redirectStderr => 0,
                             logfile => $shippedLocalesTmpFile);

    if ($rv->{'exitValue'} != 0) {
        die "ASSERT: GetLocaleManifest(): shipped-locale checkout failed\n";
    }

    if (not LoadLocaleManifest(localeHashRef => $localeManifest,
                               manifest => $shippedLocalesTmpFile)) {
        die "Bootstrap::Util: GetLocaleManifest() failed to load manifest\n";
    }

    return $localeManifest;
}

1;
