/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
function run_test() {
  var formatter = Cc["@mozilla.org/toolkit/URLFormatterService;1"].
                  getService(Ci.nsIURLFormatter);
  var locale = Cc["@mozilla.org/chrome/chrome-registry;1"].
               getService(Ci.nsIXULChromeRegistry).
               getSelectedLocale('global');
  var prefs = Cc['@mozilla.org/preferences-service;1'].
              getService(Ci.nsIPrefBranch);
  var sysInfo = Cc["@mozilla.org/system-info;1"].
                getService(Ci.nsIPropertyBag2);
  var OSVersion = sysInfo.getProperty("name") + " " +
                  sysInfo.getProperty("version");
  try {
    OSVersion += " (" + sysInfo.getProperty("secondaryLibrary") + ")";
  } catch (e) {}
  OSVersion = encodeURIComponent(OSVersion);
  var macutils = null;
  try {
    macutils = Cc["@mozilla.org/xpcom/mac-utils;1"].
               getService(Ci.nsIMacUtils);
  } catch (e) {}
  var appInfo = Cc["@mozilla.org/xre/app-info;1"].
                getService(Ci.nsIXULAppInfo).
                QueryInterface(Ci.nsIXULRuntime);
  var abi = macutils && macutils.isUniversalBinary ? "Universal-gcc3" : appInfo.XPCOMABI;

  let channel = "default";
  let defaults = prefs.QueryInterface(Ci.nsIPrefService).getDefaultBranch(null);
  try {
    channel = defaults.getCharPref("app.update.channel");
  } catch (e) {}
  // Set distribution values.
  defaults.setCharPref("distribution.id", "bacon");
  defaults.setCharPref("distribution.version", "1.0");

  var upperUrlRaw = "http://%LOCALE%.%VENDOR%.foo/?name=%NAME%&id=%ID%&version=%VERSION%&platversion=%PLATFORMVERSION%&abid=%APPBUILDID%&pbid=%PLATFORMBUILDID%&app=%APP%&os=%OS%&abi=%XPCOMABI%";
  var lowerUrlRaw = "http://%locale%.%vendor%.foo/?name=%name%&id=%id%&version=%version%&platversion=%platformversion%&abid=%appbuildid%&pbid=%platformbuildid%&app=%app%&os=%os%&abi=%xpcomabi%";
  //XXX %APP%'s RegExp is not global, so it only replaces the first space
  var ulUrlRef = "http://" + locale + ".Mozilla.foo/?name=Url Formatter Test&id=urlformattertest@test.mozilla.org&version=1&platversion=2.0&abid=2007122405&pbid=2007122406&app=urlformatter test&os=XPCShell&abi=" + abi;
  var multiUrl = "http://%VENDOR%.%VENDOR%.%NAME%.%VENDOR%.%NAME%";
  var multiUrlRef = "http://Mozilla.Mozilla.Url Formatter Test.Mozilla.Url Formatter Test";
  var encodedUrl = "https://%LOCALE%.%VENDOR%.foo/?q=%E3%82%BF%E3%83%96&app=%NAME%&ver=%PLATFORMVERSION%";
  var encodedUrlRef = "https://" + locale + ".Mozilla.foo/?q=%E3%82%BF%E3%83%96&app=Url Formatter Test&ver=2.0";
  var advancedUrl = "http://test.mozilla.com/%NAME%/%VERSION%/%APPBUILDID%/%BUILD_TARGET%/%LOCALE%/%CHANNEL%/%OS_VERSION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/";
  var advancedUrlRef = "http://test.mozilla.com/Url Formatter Test/1/2007122405/XPCShell_" + abi + "/" + locale + "/" + channel + "/" + OSVersion + "/bacon/1.0/";

  var pref = "xpcshell.urlformatter.test";
  var str = Cc["@mozilla.org/supports-string;1"].
            createInstance(Ci.nsISupportsString);
  str.data = upperUrlRaw;
  prefs.setComplexValue(pref, Ci.nsISupportsString, str);

  do_check_eq(formatter.formatURL(upperUrlRaw), ulUrlRef);
  do_check_eq(formatter.formatURLPref(pref), ulUrlRef);
  // Keys must be uppercase
  do_check_neq(formatter.formatURL(lowerUrlRaw), ulUrlRef);
  do_check_eq(formatter.formatURL(multiUrl), multiUrlRef);
  // Encoded strings must be kept as is (Bug 427304)
  do_check_eq(formatter.formatURL(encodedUrl), encodedUrlRef);

  do_check_eq(formatter.formatURL(advancedUrl), advancedUrlRef);
}
