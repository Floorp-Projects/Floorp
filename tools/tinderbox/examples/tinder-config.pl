#- tinder-config.pl - Tinderbox configuration file.
#-    Uncomment the variables you need to set.
#-    The default values are the same as the commented variables

#- PLEASE FILL THIS IN WITH YOUR PROPER EMAIL ADDRESS
$BuildAdministrator = 'mcafee@mocha.com';

#- You'll need to change these to suit your machine's needs
$BaseDir       = '/builds/mcafee/tinderbox';
$DisplayServer = ':0.0'; # costarica:0.0

#- Until you get the script working. When it works,
#- change to the tree you're actually building
$BuildTree  = 'MozillaTest';
#$BuildTree  = 'SeaMonkey';

$AliveTest                = 1;
$ViewerTest               = 0;
$BloatTest                = 0;
$DomToTextConversionTest  = 0;
$EmbedTest                = 0;
$MailNewsTest             = 0;
$LayoutPerformanceTest    = 0;
$StartupPerformanceTest   = 0;

$MozProfileName = "default";
$BloatTestTimeout  = 240;    # seconds
$LayoutPerformanceTestTimeout  = 1400;    # seconds
$StartupPerformanceTestTimeout = 60;      # seconds

# Testing...
# $TestOnly = 1;
# $ObjDir = 'obj-i686-unknown-linux-gnu';

$UserComment = "Sample tinder-config.pl, changeme!";

# Need to end with a true value
1;
