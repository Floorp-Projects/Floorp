/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ryan Flint <rflint@dslr.net> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
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
