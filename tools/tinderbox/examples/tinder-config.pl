#- tinder-config.pl - Tinderbox configuration file.
#-    Uncomment the variables you need to set.
#-    The default values are the same as the commented variables

#- PLEASE FILL THIS IN WITH YOUR PROPER EMAIL ADDRESS
$BuildAdministrator = 'mcafee@mocha.com';

#- You'll need to change these to suit your machine's needs
$BaseDir       = '/builds/mcafee/tinderbox';
$DisplayServer = 'coffee:0.0'; # costarica:0.0

#- Until you get the script working. When it works,
#- change to the tree you're actually building
$BuildTree  = 'MozillaTest';
#$BuildTree  = 'SeaMonkey';

$AliveTest                = 1;
$ViewerTest               = 1;
$BloatTest                = 1;
$DomToTextConversionTest  = 1;
$EmbedTest                = 1;
$MailNewsTest             = 0;
$LayoutPerformanceTest    = 1;
$StartupPerformanceTest   = 1;

$MozProfileName = "default";
$BloatTestTimeout  = 240;    # seconds
$LayoutPerformanceTestTimeout  = 1400;    # seconds
$StartupPerformanceTestTimeout = 60;      # seconds

# Testing...
# $TestOnly = 1;
# $ObjDir = 'obj-i686-unknown-linux-gnu';

$UserComment = "Testing new LayoutPerformanceTest. -mcafee";

# Need to end with a true value
1;
