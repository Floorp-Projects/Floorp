/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["controller", "utils", "elementslib", "os",
                        "getBrowserController", "newBrowserController",
                        "getAddonsController", "getPreferencesController",
                        "newMail3PaneController", "getMail3PaneController",
                        "wm", "platform", "getAddrbkController",
                        "getMsgComposeController", "getDownloadsController",
                        "Application", "cleanQuit",
                        "getPlacesController", 'isMac', 'isLinux', 'isWindows',
                        "firePythonCallback"
                       ];

// imports
var controller = {};  Components.utils.import('resource://mozmill/modules/controller.js', controller);
var utils = {};       Components.utils.import('resource://mozmill/modules/utils.js', utils);
var elementslib = {}; Components.utils.import('resource://mozmill/modules/elementslib.js', elementslib);
var frame = {}; Components.utils.import('resource://mozmill/modules/frame.js', frame);
var os = {}; Components.utils.import('resource://mozmill/stdlib/os.js', os);

try {
  Components.utils.import("resource://gre/modules/AddonManager.jsm");
} catch(e) { /* Firefox 4 only */ }

// platform information
var platform = os.getPlatform();
var isMac = false;
var isWindows = false;
var isLinux = false;
if (platform == "darwin"){
  isMac = true;
}
if (platform == "winnt"){
  isWindows = true;
}
if (platform == "linux"){
  isLinux = true;
}

var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
           .getService(Components.interfaces.nsIWindowMediator);

var appInfo = Components.classes["@mozilla.org/xre/app-info;1"]
               .getService(Components.interfaces.nsIXULAppInfo);

var locale = Components.classes["@mozilla.org/chrome/chrome-registry;1"]
               .getService(Components.interfaces.nsIXULChromeRegistry)
               .getSelectedLocale("global");

var aConsoleService = Components.classes["@mozilla.org/consoleservice;1"].
    getService(Components.interfaces.nsIConsoleService);


applicationDictionary = {
  "{718e30fb-e89b-41dd-9da7-e25a45638b28}": "Sunbird",
  "{92650c4d-4b8e-4d2a-b7eb-24ecf4f6b63a}": "SeaMonkey",
  "{ec8030f7-c20a-464f-9b0e-13a3a9e97384}": "Firefox",
  "{3550f703-e582-4d05-9a08-453d09bdfdc6}": 'Thunderbird',
}

var Application = applicationDictionary[appInfo.ID];

if (Application == undefined) {
  // Default to Firefox
  var Application = 'Firefox';
}

// get startup time if available
// see http://blog.mozilla.com/tglek/2011/04/26/measuring-startup-speed-correctly/
var startupInfo = {};
try {
    var _startupInfo = Components.classes["@mozilla.org/toolkit/app-startup;1"]
        .getService(Components.interfaces.nsIAppStartup).getStartupInfo();
    for (var i in _startupInfo) {
        startupInfo[i] = _startupInfo[i].getTime(); // convert from Date object to ms since epoch
    }
} catch(e) {
    startupInfo = null;
}


// keep list of installed addons to send to jsbridge for test run report
var addons = "null"; // this will be JSON parsed
if(typeof AddonManager != "undefined") {
  AddonManager.getAllAddons(function(addonList) {
      var converter = Components.classes["@mozilla.org/intl/scriptableunicodeconverter"]
          .createInstance(Components.interfaces.nsIScriptableUnicodeConverter);
      converter.charset = 'utf-8';

      function replacer(key, value) {
          if (typeof(value) == "string") {
              try {
                  return converter.ConvertToUnicode(value);
              } catch(e) {
                  var newstring = '';
                  for (var i=0; i < value.length; i++) {
                      replacement = '';
                      if ((32 <= value.charCodeAt(i)) && (value.charCodeAt(i) < 127)) {
                          // eliminate non-convertable characters;
                          newstring += value.charAt(i);
                      } else {
                          newstring += replacement;
                      }
                  }
                  return newstring;
              }
          }
          return value;
      }

      addons = converter.ConvertToUnicode(JSON.stringify(addonList, replacer))
  });
}

function cleanQuit () {
  utils.getMethodInWindows('goQuitApplication')();
}

function addHttpResource (directory, namespace) {
  return 'http://localhost:4545/'+namespace;
}

function newBrowserController () {
  return new controller.MozMillController(utils.getMethodInWindows('OpenBrowserWindow')());
}

function getBrowserController () {
  var browserWindow = wm.getMostRecentWindow("navigator:browser");
  if (browserWindow == null) {
    return newBrowserController();
  }
  else {
    return new controller.MozMillController(browserWindow);
  }
}

function getPlacesController () {
  utils.getMethodInWindows('PlacesCommandHook').showPlacesOrganizer('AllBookmarks');
  return new controller.MozMillController(wm.getMostRecentWindow(''));
}

function getAddonsController () {
  if (Application == 'SeaMonkey') {
    utils.getMethodInWindows('toEM')();
  } else if (Application == 'Thunderbird') {
    utils.getMethodInWindows('openAddonsMgr')();
  } else if (Application == 'Sunbird') {
    utils.getMethodInWindows('goOpenAddons')();
  } else {
    utils.getMethodInWindows('BrowserOpenAddonsMgr')();
  }
  return new controller.MozMillController(wm.getMostRecentWindow(''));
}

function getDownloadsController() {
  utils.getMethodInWindows('BrowserDownloadsUI')();
  return new controller.MozMillController(wm.getMostRecentWindow(''));
}

function getPreferencesController() {
  if (Application == 'Thunderbird') {
    utils.getMethodInWindows('openOptionsDialog')();
  } else {
    utils.getMethodInWindows('openPreferences')();
  }
  return new controller.MozMillController(wm.getMostRecentWindow(''));
}

// Thunderbird functions
function newMail3PaneController () {
  return new controller.MozMillController(utils.getMethodInWindows('toMessengerWindow')());
}

function getMail3PaneController () {
  var mail3PaneWindow = wm.getMostRecentWindow("mail:3pane");
  if (mail3PaneWindow == null) {
    return newMail3PaneController();
  }
  else {
    return new controller.MozMillController(mail3PaneWindow);
  }
}

// Thunderbird - Address book window
function newAddrbkController () {
  utils.getMethodInWindows("toAddressBook")();
  utils.sleep(2000);
  var addyWin = wm.getMostRecentWindow("mail:addressbook");
  return new controller.MozMillController(addyWin);
}

function getAddrbkController () {
  var addrbkWindow = wm.getMostRecentWindow("mail:addressbook");
  if (addrbkWindow == null) {
    return newAddrbkController();
  }
  else {
    return new controller.MozMillController(addrbkWindow);
  }
}

function firePythonCallback (filename, method, args, kwargs) {
  obj = {'filename': filename, 'method': method};
  obj['test'] = frame.events.currentModule.__file__;
  obj['args'] = args || [];
  obj['kwargs'] = kwargs || {};
  frame.events.fireEvent("firePythonCallback", obj);
}

function timer (name) {
  this.name = name;
  this.timers = {};
  frame.timers.push(this);
  this.actions = [];
}
timer.prototype.start = function (name) {
  this.timers[name].startTime = (new Date).getTime();
}
timer.prototype.stop = function (name) {
  var t = this.timers[name];
  t.endTime = (new Date).getTime();
  t.totalTime = (t.endTime - t.startTime);
}
timer.prototype.end = function () {
  frame.events.fireEvent("timer", this);
  frame.timers.remove(this);
}
