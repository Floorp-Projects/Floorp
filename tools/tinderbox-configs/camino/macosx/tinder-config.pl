#- tinder-config.pl - Tinderbox configuration file.
#-    Uncomment the variables you need to set.
#-    The default values are the same as the commented variables.

$ENV{'CVS_RSH'}='ssh';

# $ENV{MOZ_PACKAGE_MSI}
#-----------------------------------------------------------------------------
#  Default: 0
#   Values: 0 | 1
#  Purpose: Controls whether a MSI package is made.
# Requires: Windows and a local MakeMSI installation.
#$ENV{MOZ_PACKAGE_MSI} = 0;

# $ENV{MOZ_SYMBOLS_TRANSFER_TYPE}
#-----------------------------------------------------------------------------
#  Default: scp
#   Values: scp | rsync
#  Purpose: Use scp or rsync to transfer symbols to the Talkback server.
# Requires: The selected type requires the command be available both locally
#           and on the Talkback server.
#$ENV{MOZ_SYMBOLS_TRANSFER_TYPE} = "scp";

#- PLEASE FILL THIS IN WITH YOUR PROPER EMAIL ADDRESS
$BuildAdministrator = "mento, the fresh MAKEr";

#- You'll need to change these to suit your machine's needs
#$DisplayServer = ':0.0';

#- Default values of command-line opts
#-
#$BuildDepend       = 1;      # Depend or Clobber
#$BuildDebug        = 0;      # Debug or Opt (Darwin)
#$ReportStatus      = 1;      # Send results to server, or not
#$ReportFinalStatus = 1;      # Finer control over $ReportStatus.
#$UseTimeStamp      = 1;      # Use the CVS 'pull-by-timestamp' option, or not
#$BuildOnce         = 0;      # Build once, don't send results to server
#$ConfigureOnly     = 0;      # Configure, but do not build.
#$TestOnly          = 0;      # Only run tests, don't pull/build
#$BuildEmbed        = 0;      # After building seamonkey, go build embed app.
#$SkipMozilla       = 0;      # Use to debug post-mozilla.pl scripts.
#$SkipCheckout      = 0;      # Use to debug build process without checking out new source.
#$BuildLocales      = 0;      # Do l10n packaging?

# Only used when $BuildLocales = 1
%WGetFiles         = ();  # Pull files from the web, URL => Location
#$WGetTimeout       = 360; # Wget timeout, in seconds
#$BuildLocalesArgs  = "";  # Extra attributes to add to the makefile command
                          # which builds the "installers-<locale>" target.
                          # Typically used to set ZIP_IN and WIN32_INSTALLER_IN

# Tests
$CleanProfile             = 1;
#$ResetHomeDirForTests     = 1;
$ProductName              = "Camino";
$VendorName               = 'Mozilla';

#$RunMozillaTests          = 1;  # Allow turning off of all tests if needed.
$RegxpcomTest             = 0;
#$AliveTest                = 1;
#$JavaTest                 = 0;
#$ViewerTest               = 0;
#$BloatTest                = 0;  # warren memory bloat test
#$BloatTest2               = 0;  # dbaron memory bloat test, require tracemalloc
#$DomToTextConversionTest  = 0;  
#$XpcomGlueTest            = 0;
#$CodesizeTest             = 0;  # Z,  require mozilla/tools/codesighs
#$EmbedCodesizeTest        = 0;  # mZ, require mozilla/tools/codesigns
#$MailBloatTest            = 0;
#$EmbedTest                = 0;  # Assumes you wanted $BuildEmbed=1
$LayoutPerformanceTest    = 1;  # Tp
$DHTMLPerformanceTest     = 1;  # Tdhtml
#$QATest                   = 0;  
#$XULWindowOpenTest        = 0;  # Txul
$StartupPerformanceTest   = 1;  # Ts
#$NeckoUnitTest            = 0;
#$RenderPerformanceTest    = 0;  # Tgfx
#@CompareLocaleDirs        = (); # Run compare-locales test on these directories
# ("network","dom","toolkit","security/manager");
#$CompareLocalesAviary     = 0;  # Should the compare-locales commands use the
                                # aviary directory structure?

#$TestsPhoneHome           = 0;  # Should test report back to server?
#$GraphNameOverride        = ''; # Override name built from ::hostname() and $BuildTag

# $results_server
#----------------------------------------------------------------------------
# Server on which test results will be accessible.  This was originally tegu,
# then became axolotl.  Once we moved services from axolotl, it was time
# to give this service its own hostname to make future transitions easier.
# - cmp@mozilla.org
#$results_server           = "build-graphs.mozilla.org";

#$pageload_server          = "spider";  # localhost
$pageload_server = "axolotl.mozilla.org";

#
# Timeouts, values are in seconds.
#
#$CVSCheckoutTimeout               = 3600;
#$CreateProfileTimeout             = 45;
#$RegxpcomTestTimeout              = 120;

$AliveTestTimeout                 = 15;
#$ViewerTestTimeout                = 45;
#$EmbedTestTimeout                 = 45;
#$BloatTestTimeout                 = 120;   # seconds
#$MailBloatTestTimeout             = 120;   # seconds
#$JavaTestTimeout                  = 45;
#$DomTestTimeout	                  = 45;    # seconds
#$XpcomGlueTestTimeout             = 15;
#$CodesizeTestTimeout              = 900;     # seconds
#$CodesizeTestType                 = "auto";  # {"auto"|"base"}
$LayoutPerformanceTestTimeout     = 600;  # entire test, seconds
$DHTMLPerformanceTestTimeout      = 300;  # entire test, seconds
#$QATestTimeout                    = 1200;   # entire test, seconds
#$LayoutPerformanceTestPageTimeout = 30000; # each page, ms
$StartupPerformanceTestTimeout    = 5;    # seconds
#$NeckoUnitTestTimeout             = 30;    # seconds
$XULWindowOpenTestTimeout	      = 30;   # seconds
#$RenderTestTimeout                = 1800;  # seconds

#$MozConfigFileName = 'mozconfig';

#$UseMozillaProfile = 1;
#$MozProfileName = 'default';

#- Set these to what makes sense for your system
#$Make          = 'gmake';       # Must be GNU make
#$MakeOverrides = '';
#$mail          = '/bin/mail';
#$CVS           = 'cvs -q';
#$CVSCO         = 'checkout -P';

# win32 usually doesn't have /bin/mail
#$blat           = 'c:/nstools/bin/blat';
#$use_blat       = 0;

# Set moz_cvsroot to something like:
# :pserver:$ENV{USER}%netscape.com\@cvs.mozilla.org:/cvsroot
# :pserver:anonymous\@cvs-mirror.mozilla.org:/cvsroot
#
# Note that win32 may not need \@, depends on ' or ".
# :pserver:$ENV{USER}%netscape.com@cvs.mozilla.org:/cvsroot

$moz_cvsroot = ':pserver:anonymous@cvs.mozilla.org:/cvsroot';

#- Set these proper values for your tinderbox server
#$Tinderbox_server = 'tinderbox-daemon@tinderbox.mozilla.org';

# Allow for non-client builds, e.g. camino.
#$moz_client_mk = 'client.mk';

#- Set if you want to build in a separate object tree
$ObjDir = '../build';

# Extra build name, if needed.
$BuildNameExtra = 'Camino Trunk';

# User comment, eg. ip address for dhcp builds.
# ex: $UserComment = "ip = 208.12.36.108";
#$UserComment = 0;

#-
#- The rest should not need to be changed
#-

#- Minimum wait period from start of build to start of next build in minutes.
$BuildSleep = 5;

#- Until you get the script working. When it works,
#- change to the tree you're actually building
$BuildTree  = 'Camino';

#$BuildName = '';
#$BuildTag = '';
#$BuildConfigDir = 'mozilla/config';
#$Topsrcdir = 'mozilla';

$BinaryName = 'Camino';

#
# For embedding app, use:
#$EmbedBinaryName = 'TestGtkEmbed';
#$EmbedDistDir    = 'dist/bin';


#$ShellOverride = ''; # Only used if the default shell is too stupid
#$ConfigureArgs = '';
#$ConfigureEnvArgs = '';
#$Compiler = 'gcc';
#$NSPRArgs = '';
#$ShellOverride = '';

# Release build options
#$ReleaseBuild  = 1;
#$clean_objdir = 1; # remove objdir when starting release cycle?
#$clean_srcdir = 1; # remove srcdir when starting release cycle?
#$LocaleProduct = "browser";
#$shiptalkback  = 1;
#$ReleaseToLatest = 1; # Push the release to latest-<milestone>?
#$ReleaseToDated = 1; # Push the release to YYYY-MM-DD-HH-<milestone>?
#$OfficialBuildMachinery = 1; # Allow official clobber nightlies.  When false, 
#$ReleaseGroup = ''; # group to set uploaded files to (if non-empty)
$build_hour    = "22";
$package_creation_path = "/camino/installer";
# path to make in to recreate mac bundle, needed for mac + talkback:
$mac_bundle_path = "/camino";
#$ssh_version   = "2";
$ssh_user      = "caminobld";
#$ssh_server    = "stage.mozilla.org";
$ftp_path      = "/home/ftp/pub/camino/nightly";
$url_path      = "http://ftp.mozilla.org/pub/mozilla.org/camino/nightly";
$tbox_ftp_path = '/home/ftp/pub/camino/tinderbox-builds';
$tbox_url_path = "http://ftp.mozilla.org/pub/mozilla.org/camino/tinderbox-builds";
$milestone     = "trunk";
#$notify_list   = "build-announce\@mozilla.org";
#$stub_installer = 1;
#$sea_installer = 1;
$archive       = 1;
#$push_raw_xpis = 1;
#$update_package = 0;
#$update_product = "Firefox";
#$update_version = "trunk";
#$update_platform = "WINNT_x86-msvc";
#$update_hash = "md5";
#$update_filehost = "ftp.mozilla.org";
#$update_appv = "1.0+";
#$update_extv = "1.0+";
#$update_pushinfo = 1;

# Reboot the OS at the end of build-and-test cycle. This is primarily
# intended for Win9x, which can't last more than a few cycles before
# locking up (and testing would be suspect even after a couple of cycles).
# Right now, there is only code to force the reboot for Win9x, so even
# setting this to 1, will not have an effect on other platforms. Setting
# up win9x to automatically logon and begin running tinderbox is left 
# as an exercise to the reader. 
#$RebootSystem = 0;

# LogCompression specifies the type of compression used on the log file.
# Valid options are 'gzip', and 'bzip2'. Please make sure the binaries
# for 'gzip' or 'bzip2' are in the user's path before setting this
# option.
$LogCompression = 'gzip';

# LogEncoding specifies the encoding format used for the logs. Valid
# options are 'base64', and 'uuencode'. If $LogCompression is set above,
# this needs to be set to 'base64' or 'uuencode' to ensure that the
# binary data is transferred properly.
$LogEncoding = 'base64';

# Prevent Extension Manager from spawning child processes during tests
# - processes that tbox scripts cannot kill. 
#$ENV{NO_EM_RESTART} = '1';

# Build Mac OS X universal binaries (must be used with an objdir and
# universal support from mozilla/build/macosx/universal)
$MacUniversalBinary = 1;
