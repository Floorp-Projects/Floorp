/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["UserAgentUpdates"];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;

Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(
  this, "FileUtils", "resource://gre/modules/FileUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(
  this, "NetUtil", "resource://gre/modules/NetUtil.jsm");

XPCOMUtils.defineLazyModuleGetter(
  this, "OS", "resource://gre/modules/osfile.jsm");

XPCOMUtils.defineLazyModuleGetter(
  this, "Promise", "resource://gre/modules/Promise.jsm");

XPCOMUtils.defineLazyModuleGetter(
  this, "UpdateUtils", "resource://gre/modules/UpdateUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(
  this, "gUpdateTimer", "@mozilla.org/updates/timer-manager;1", "nsIUpdateTimerManager");

XPCOMUtils.defineLazyGetter(this, "gApp",
  function() {
    return Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULAppInfo)
                                            .QueryInterface(Ci.nsIXULRuntime);
  });

XPCOMUtils.defineLazyGetter(this, "gDecoder",
  function() { return new TextDecoder(); }
);

XPCOMUtils.defineLazyGetter(this, "gEncoder",
  function() { return new TextEncoder(); }
);

const TIMER_ID = "user-agent-updates-timer";

const PREF_UPDATES = "general.useragent.updates.";
const PREF_UPDATES_ENABLED = PREF_UPDATES + "enabled";
const PREF_UPDATES_URL = PREF_UPDATES + "url";
const PREF_UPDATES_INTERVAL = PREF_UPDATES + "interval";
const PREF_UPDATES_RETRY = PREF_UPDATES + "retry";
const PREF_UPDATES_TIMEOUT = PREF_UPDATES + "timeout";
const PREF_UPDATES_LASTUPDATED = PREF_UPDATES + "lastupdated";

const KEY_PREFDIR = "PrefD";
const KEY_APPDIR = "XCurProcD";
const FILE_UPDATES = "ua-update.json";

const PREF_APP_DISTRIBUTION = "distribution.id";
const PREF_APP_DISTRIBUTION_VERSION = "distribution.version";

var gInitialized = false;

function readChannel(url) {
  return new Promise((resolve, reject) => {
    try {
      let channel = NetUtil.newChannel({uri: url, loadUsingSystemPrincipal: true});
      channel.contentType = "application/json";

      NetUtil.asyncFetch(channel, (inputStream, status) => {
        if (!Components.isSuccessCode(status)) {
          reject();
          return;
        }

        let data = JSON.parse(
          NetUtil.readInputStreamToString(inputStream, inputStream.available())
        );
        resolve(data);
      });
    } catch (ex) {
      reject(new Error("UserAgentUpdates: Could not fetch " + url + " " +
                       ex + "\n" + ex.stack));
    }
  });
}

this.UserAgentUpdates = {
  init: function(callback) {
    if (gInitialized) {
      return;
    }
    gInitialized = true;

    this._callback = callback;
    this._lastUpdated = 0;
    this._applySavedUpdate();

    Services.prefs.addObserver(PREF_UPDATES, this);
  },

  uninit: function() {
    if (!gInitialized) {
      return;
    }
    gInitialized = false;
    Services.prefs.removeObserver(PREF_UPDATES, this);
  },

  _applyUpdate: function(update) {
    // Check pref again in case it has changed
    if (update && this._getPref(PREF_UPDATES_ENABLED, false)) {
      this._callback(update);
    } else {
      this._callback(null);
    }
  },

  _applySavedUpdate: function() {
    if (!this._getPref(PREF_UPDATES_ENABLED, false)) {
      // remove previous overrides
      this._applyUpdate(null);
      return;
    }
    // try loading from profile dir, then from app dir
    let dirs = [KEY_PREFDIR, KEY_APPDIR];

    dirs.reduce((prevLoad, dir) => {
      let file = FileUtils.getFile(dir, [FILE_UPDATES], true).path;
      // tryNext returns promise to read file under dir and parse it
      let tryNext = () => OS.File.read(file).then(
        (bytes) => {
          let update = JSON.parse(gDecoder.decode(bytes));
          if (!update) {
            throw new Error("invalid update");
          }
          return update;
        }
      );
      // try to load next one if the previous load failed
      return prevLoad ? prevLoad.then(null, tryNext) : tryNext();
    }, null).then(null, (ex) => {
      if (AppConstants.platform !== "android") {
        // All previous (non-Android) load attempts have failed, so we bail.
        throw new Error("UserAgentUpdates: Failed to load " + FILE_UPDATES +
                         ex + "\n" + ex.stack);
      }
      // Make one last attempt to read from the Fennec APK root.
      return readChannel("resource://android/" + FILE_UPDATES);
    }).then((update) => {
      // Apply update if loading was successful
      this._applyUpdate(update);
    }).catch(Cu.reportError);
    this._scheduleUpdate();
  },

  _saveToFile: function(update) {
    let file = FileUtils.getFile(KEY_PREFDIR, [FILE_UPDATES], true);
    let path = file.path;
    let bytes = gEncoder.encode(JSON.stringify(update));
    OS.File.writeAtomic(path, bytes, {tmpPath: path + ".tmp"}).then(
      () => {
        this._lastUpdated = Date.now();
        Services.prefs.setCharPref(
          PREF_UPDATES_LASTUPDATED, this._lastUpdated.toString());
      },
      Cu.reportError
    );
  },

  _getPref: function(name, def) {
    try {
      switch (typeof def) {
        case "number": return Services.prefs.getIntPref(name);
        case "boolean": return Services.prefs.getBoolPref(name);
      }
      return Services.prefs.getCharPref(name);
    } catch (e) {
      return def;
    }
  },

  _getParameters() {
    return {
      "%DATE%": function() { return Date.now().toString(); },
      "%PRODUCT%": function() { return gApp.name; },
      "%APP_ID%": function() { return gApp.ID; },
      "%APP_VERSION%": function() { return gApp.version; },
      "%BUILD_ID%": function() { return gApp.appBuildID; },
      "%OS%": function() { return gApp.OS; },
      "%CHANNEL%": function() { return UpdateUtils.UpdateChannel; },
      "%DISTRIBUTION%": function() { return this._getPref(PREF_APP_DISTRIBUTION, ""); },
      "%DISTRIBUTION_VERSION%": function() { return this._getPref(PREF_APP_DISTRIBUTION_VERSION, ""); },
    };
  },

  _getUpdateURL: function() {
    let url = this._getPref(PREF_UPDATES_URL, "");
    let params = this._getParameters();
    return url.replace(/%[A-Z_]+%/g, function(match) {
      let param = params[match];
      // preserve the %FOO% string (e.g. as an encoding) if it's not a valid parameter
      return param ? encodeURIComponent(param()) : match;
    });
  },

  _fetchUpdate: function(url, success, error) {
    let request = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
                    .createInstance(Ci.nsIXMLHttpRequest);
    request.mozBackgroundRequest = true;
    request.timeout = this._getPref(PREF_UPDATES_TIMEOUT, 60000);
    request.open("GET", url, true);
    request.overrideMimeType("application/json");
    request.responseType = "json";

    request.addEventListener("load", function() {
      let response = request.response;
      response ? success(response) : error();
    });
    request.addEventListener("error", error);
    request.send();
  },

  _update: function() {
    let url = this._getUpdateURL();
    url && this._fetchUpdate(url,
      (function(response) { // success
        // apply update and save overrides to profile
        this._applyUpdate(response);
        this._saveToFile(response);
        this._scheduleUpdate(); // cancel any retries
      }).bind(this),
      (function(response) { // error
        this._scheduleUpdate(true /* retry */);
      }).bind(this));
  },

  _scheduleUpdate: function(retry) {
    // only schedule updates in the main process
    if (gApp.processType !== Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT) {
      return;
    }
    let interval = this._getPref(PREF_UPDATES_INTERVAL, 604800 /* 1 week */);
    if (retry) {
      interval = this._getPref(PREF_UPDATES_RETRY, interval);
    }
    gUpdateTimer.registerTimer(TIMER_ID, this, Math.max(1, interval));
  },

  notify: function(timer) {
    // timer notification
    if (this._getPref(PREF_UPDATES_ENABLED, false)) {
      this._update();
    }
  },

  observe: function(subject, topic, data) {
    switch (topic) {
      case "nsPref:changed":
        if (data === PREF_UPDATES_ENABLED) {
          this._applySavedUpdate();
        } else if (data === PREF_UPDATES_INTERVAL) {
          this._scheduleUpdate();
        } else if (data === PREF_UPDATES_LASTUPDATED) {
          // reload from file if there has been an update
          let lastUpdated = parseInt(
            this._getPref(PREF_UPDATES_LASTUPDATED, "0"), 0);
          if (lastUpdated > this._lastUpdated) {
            this._applySavedUpdate();
            this._lastUpdated = lastUpdated;
          }
        }
        break;
    }
  },

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIObserver,
    Ci.nsITimerCallback,
  ]),
};
