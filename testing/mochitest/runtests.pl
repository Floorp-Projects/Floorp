#!/usr/bin/perl
use File::Path;

# URL parameters:
#
# autorun -- kick off tests automatically
# closeWhenDone -- runs quit.js after tests 
# logFile -- logs test run to an absolute path
# quiet -- turns of console dumps
#
# consoleLevel, fileLevel -- set the logging level of the console and file logs, if activated.
#                            <http://mochikit.com/doc/html/MochiKit/Logging.html>

$test_url = "http://localhost:8888/tests/index.html?autorun=1";
# XXXsayrer these are specific to my mac, need to make them general
$app = "/Users/sayrer/Desktop/Minefield.app/Contents/MacOS/firefox-bin";
$profile = "dhtml_test_profile";
$profile_dir = "/tmp/$profile";
$chrome_dir = "$profile_dir/chrome";

$pref_content = <<PREFEND;
user_pref("browser.dom.window.dump.enabled", true);
user_pref("capability.principal.codebase.p1.granted", "UniversalXPConnect");
user_pref("capability.principal.codebase.p1.id", "http://localhost:8888");
user_pref("capability.principal.codebase.p1.subjectName", "");
user_pref("dom.disable_open_during_load", false);
user_pref("signed.applets.codebase_principal_support", true);
PREFEND

$chrome_content = <<CHROMEEND;
@namespace url("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"); /* set default namespace to XUL */
toolbar,
toolbarpalette { 
  background-repeat: repeat-x !important;
  background-position: top right !important;
  background-color: rgb(235, 235, 235) !important;
  background-image: url("chrome://browser/skin/bookmark_toolbar_background.gif") !important;
}
toolbar#nav-bar {
  background-image: none !important;
}
CHROMEEND

# set env vars so Firefox doesn't quit weirdly and break the script
$ENV{'MOZ_NO_REMOTE'} = '1';
$ENV{'NO_EM_RESTART'} = '1';

# mark the start
$start = localtime;

mkdir($profile_dir);
mkdir($chrome_dir);

# first create our profile
@args = ($app, '-CreateProfile', "$profile $profile_dir");
$rc = 0xffff & system @args;
if ($rc != 0) {
  die("Creating profile failed!\n");
} else {
  print "Creating profile succeeded\n";
}

# append magic prefs to user.js
open(PREFOUTFILE, ">>$profile_dir/user.js") || die("Could not open user.js file $!");
print PREFOUTFILE ($pref_content);
close(PREFOUTFILE);

# append magic prefs to user.js
open(CRHOMEOUTFILE, ">>$chrome_dir/userChrome.css") || die("Could not open userChrome.css file $!");
print CRHOMEOUTFILE ($chrome_content);
close(CRHOMEOUTFILE);

# now run with the profile we created
@runargs = ($app, '-P', "$profile", $test_url);
$rc = 0xffff & system @runargs;

# remove the profile we created
rmtree($profile_dir, 0, 0);

# print test run times
$finish = localtime;
print " started: $start\n";
print "finished: $finish\n";
