#- PLEASE FILL THIS IN WITH YOUR PROPER EMAIL ADDRESS
$BuildAdministrator = "$ENV{USER}\@$ENV{HOST}";

#- You'll need to change these to suit your machine's needs
$DisplayServer = ':0.0';

#- Default values of command-line opts
#-
$BuildDepend       = 1;      # Depend or Clobber
$BuildDebug        = 0;      # Debug or Opt (Darwin)
$ReportStatus      = 1;      # Send results to server, or not
$ReportFinalStatus = 1;      # Finer control over $ReportStatus.
$UseTimeStamp      = 1;      # Use the CVS 'pull-by-timestamp' option, or not
$BuildOnce         = 0;      # Build once, don't send results to server
$TestOnly          = 0;      # Only run tests, don't pull/build
$BuildEmbed        = 0;      # After building seamonkey, go build embed app.
$SkipMozilla       = 0;      # Use to debug post-mozilla.pl scripts.

# Tests
$CleanProfile             = 0;
$ResetHomeDirForTests     = 1;
$ProductName              = "Mozilla";

$RunMozillaTests          = 1;  # Allow turning off of all tests if needed.
$RegxpcomTest             = 1;
$AliveTest                = 1;
$JavaTest                 = 0;
$ViewerTest               = 0;
$BloatTest                = 0;
$BloatTest2               = 0;
$DomToTextConversionTest  = 0;
$XpcomGlueTest            = 0;
$CodesizeTest             = 0;
$MailBloatTest            = 0;
$EmbedTest                = 0;  # Assumes you wanted $BuildEmbed=1
$LayoutPerformanceTest    = 0;
$QATest                   = 0;
$XULWindowOpenTest        = 0;
$StartupPerformanceTest   = 0;

$TestsPhoneHome           = 0;  # Should test report back to server?
$results_server           = "tegu.mozilla.org";
$pageload_server          = "spider";  # localhost

#
# Timeouts, values are in seconds.
#

$CreateProfileTimeout             = 45;
$RegxpcomTestTimeout              = 15;

$AliveTestTimeout                 = 45;
$ViewerTestTimeout                = 45;
$EmbedTestTimeout                 = 45;
$BloatTestTimeout                 = 120;   # seconds
$MailBloatTestTimeout             = 120;   # seconds
$JavaTestTimeout                  = 45;
$DomTestTimeout	                  = 45;    # seconds
$XpcomGlueTestTimeout             = 15;
$CodesizeTestTimeout              = 900;     # seconds
$CodesizeTestType                 = "auto";  # {"auto"|"base"}
$LayoutPerformanceTestTimeout     = 1200;  # entire test, seconds
$QATestTimeout                    = 60;  # entire test, seconds
$LayoutPerformanceTestPageTimeout = 30000; # each page, ms
$StartupPerformanceTestTimeout    = 60;    # seconds
$XULWindowOpenTestTimeout	      = 150;   # seconds


$MozConfigFileName = 'mozconfig';
$MozProfileName = 'default';

#- Set these to what makes sense for your system
$Make          = 'gmake';       # Must be GNU make
$MakeOverrides = '';
$mail          = '/bin/mail';
$CVS           = 'cvs -q';
$CVSCO         = 'checkout -P';

# win32 usually doesn't have /bin/mail
$blat           = 'c:/nstools/bin/blat';
$use_blat       = 0;

# Set moz_cvsroot to something like:
# :pserver:$ENV{USER}%netscape.com\@cvs.mozilla.org:/cvsroot
# :pserver:anonymous\@cvs-mirror.mozilla.org:/cvsroot
$moz_cvsroot   = $ENV{CVSROOT};

#- Set these proper values for your tinderbox server
$Tinderbox_server = 'tinderbox-daemon@tinderbox.mozilla.org';

#- Set if you want to build in a separate object tree
$ObjDir = '';

# Extra build name, if needed.
$BuildNameExtra = '';

# User comment, eg. ip address for dhcp builds.
# ex: $UserComment = "ip = 208.12.36.108";
$UserComment = 0;

#-
#- The rest should not need to be changed
#-

#- Minimum wait period from start of build to start of next build in minutes.
$BuildSleep = 10;

#- Until you get the script working. When it works,
#- change to the tree you're actually building
$BuildTree  = 'MozillaTest';

$BuildName = '';
$BuildTag = '';
$BuildConfigDir = 'mozilla/config';
$Topsrcdir = 'mozilla';

$BinaryName = 'mozilla-bin';

#
# For embedding app, use:
$EmbedBinaryName = 'TestGtkEmbed';
$EmbedDistDir    = 'dist/bin'


$ShellOverride = ''; # Only used if the default shell is too stupid
$ConfigureArgs = '';
$ConfigureEnvArgs = '';
$Compiler = 'gcc';
$NSPRArgs = '';
$ShellOverride = '';

# allow override of timezone value (for win32 POSIX::strftime)
$Timezone = '';

# Reboot the OS at the end of build-and-test cycle. This is primarily
# intended for Win9x, which can't last more than a few cycles before
# locking up (and testing would be suspect even after a couple of cycles).
# Right now, there is only code to force the reboot for Win9x, so even
# setting this to 1, will not have an effect on other platforms. Setting
# up win9x to automatically logon and begin running tinderbox is left 
# as an exercise to the reader. 
$RebootSystem = 0;

