/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["controller", "utils", "elementslib", "os",
                        "getBrowserController", "newBrowserController",
                        "getAddonsController", "getPreferencesController",
                        "newMail3PaneController", "getMail3PaneController",
                        "wm", "platform", "getAddrbkController",
                        "getMsgComposeController", "getDownloadsController",
                        "Application", "findElement",
                        "getPlacesController", 'isMac', 'isLinux', 'isWindows',
                        "firePythonCallback", "getAddons"
                       ];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;


Cu.import("resource://gre/modules/AddonManager.jsm");
Cu.import("resource://gre/modules/Services.jsm");

// imports
var assertions = {};  Cu.import('resource://mozmill/modules/assertions.js', assertions);
var broker = {};      Cu.import('resource://mozmill/driver/msgbroker.js', broker);
var controller = {};  Cu.import('resource://mozmill/driver/controller.js', controller);
var elementslib = {}; Cu.import('resource://mozmill/driver/elementslib.js', elementslib);
var findElement = {}; Cu.import('resource://mozmill/driver/mozelement.js', findElement);
var os = {};          Cu.import('resource://mozmill/stdlib/os.js', os);
var utils = {};       Cu.import('resource://mozmill/stdlib/utils.js', utils);
var windows = {};     Cu.import('resource://mozmill/modules/windows.js', windows);


const DEBUG = false;

// This is a useful "check" timer. See utils.js, good for debugging
if (DEBUG) {
  utils.startTimer();
}

var assert = new assertions.Assert();

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

var wm = Services.wm;

var appInfo = Services.appinfo;
var Application = utils.applicationName;


/**
 * Retrieves the list with information about installed add-ons.
 *
 * @returns {String} JSON data of installed add-ons
 */
function getAddons() {
  var addons = null;

  AddonManager.getAllAddons(function (addonList) {
    var tmp_list = [ ];

    addonList.forEach(function (addon) {
      var tmp = { };

      // We have to filter out properties of type 'function' of the addon
      // object, which will break JSON.stringify() and result in incomplete
      // addon information.
      for (var key in addon) {
        if (typeof(addon[key]) !== "function") {
          tmp[key] = addon[key];
        }
      }

      tmp_list.push(tmp);
    });

    addons = tmp_list;
  });

  try {
    // Sychronize with getAllAddons so we do not return too early
    assert.waitFor(function () {
      return !!addons;
    })

    return addons;
  } catch (e) {
    return null;
  }
}

/**
 * Retrieves application details for the Mozmill report
 *
 * @return {String} JSON data of application details
 */
function getApplicationDetails() {
  var locale = Cc["@mozilla.org/chrome/chrome-registry;1"]
               .getService(Ci.nsIXULChromeRegistry)
               .getSelectedLocale("global");

  // Put all our necessary information into JSON and return it:
  // appinfo, startupinfo, and addons
  var details = {
    application_id: appInfo.ID,
    application_name: Application,
    application_version: appInfo.version,
    application_locale: locale,
    platform_buildid: appInfo.platformBuildID,
    platform_version: appInfo.platformVersion,
    addons: getAddons(),
    startupinfo: getStartupInfo(),
    paths: {
      appdata: Services.dirsvc.get('UAppData', Ci.nsIFile).path,
      profile: Services.dirsvc.get('ProfD', Ci.nsIFile).path
    }
  };

  return JSON.stringify(details);
}

// get startup time if available
// see http://blog.mozilla.com/tglek/2011/04/26/measuring-startup-speed-correctly/
function getStartupInfo() {
  var startupInfo = {};

  try {
    var _startupInfo = Services.startup.getStartupInfo();
    for (var time in _startupInfo) {
      // convert from Date object to ms since epoch
      startupInfo[time] = _startupInfo[time].getTime();
    }
  } catch (e) {
    startupInfo = null;
  }

  return startupInfo;
}



function newBrowserController () {
  return new controller.MozMillController(utils.getMethodInWindows('OpenBrowserWindow')());
}

function getBrowserController () {
  var browserWindow = wm.getMostRecentWindow("navigator:browser");

  if (browserWindow == null) {
    return newBrowserController();
  } else {
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
  }
  else if (Application == 'Thunderbird') {
    utils.getMethodInWindows('openAddonsMgr')();
  }
  else if (Application == 'Sunbird') {
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
  } else {
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
  } else {
    return new controller.MozMillController(addrbkWindow);
  }
}

function firePythonCallback (filename, method, args, kwargs) {
  obj = {'filename': filename, 'method': method};
  obj['args'] = args || [];
  obj['kwargs'] = kwargs || {};

  broker.sendMessage("firePythonCallback", obj);
}

function timer (name) {
  this.name = name;
  this.timers = {};
  this.actions = [];

  frame.timers.push(this);
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

// Initialization

/**
 * Initialize Mozmill
 */
function initialize() {
  windows.init();
}

initialize();
