#- PLEASE FILL THIS IN WITH YOUR PROPER EMAIL ADDRESS
$BuildAdministrator = "$ENV{USER}\@$ENV{HOST}";

#- You'll need to change these to suit your machine's needs
$BaseDir       = '/builds/tinderbox/SeaMonkey';
$DisplayServer = ':0.0';

#- Default values of command-line opts
#-
$BuildDepend       = 1;      # Depend or Clobber
$ReportStatus      = 1;      # Send results to server, or not
$ReportFinalStatus = 1;      # Finer control over $ReportStatus.
$UseTimeStamp      = 1;      # Use the CVS 'pull-by-timestamp' option, or not
$BuildOnce         = 0;      # Build once, don't send results to server
$RunTest           = 1;      # Run the smoke tests on successful build, or not
$TestOnly          = 0;      # Only run tests, don't pull/build
# Tests
$AliveTest = 1;
$ViewerTest = 0;
$BloatTest = 0;
$DomToTextConversionTest = 0;
$MailNewsTest = 0;
$MozConfigFileName = 'mozconfig';
$MozProfileName = 'mozProfile';

#- Set these to what makes sense for your system
$Make          = 'gmake';       # Must be GNU make
$MakeOverrides = '';
$mail          = '/bin/mail';
$CVS           = 'cvs -q';
$CVSCO         = 'checkout -P';

#- Set these proper values for your tinderbox server
$Tinderbox_server = 'tinderbox-daemon@tinderbox.mozilla.org';

#- Set if you want to build in a separate object tree
$ObjDir = '';

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
$ShellOverride = ''; # Only used if the default shell is too stupid
$ConfigureArgs = '';
$ConfigureEnvArgs = '';
$Compiler = 'gcc';
$NSPRArgs = '';
$ShellOverride = '';
