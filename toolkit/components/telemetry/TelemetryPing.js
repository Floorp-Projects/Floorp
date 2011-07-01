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
 * The Original Code is the nsTryToClose component.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Taras Glek <tglek@mozilla.com>
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

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

// When modifying the payload in incompatible ways, please bump this version number
const PAYLOAD_VERSION = 1;

const PREF_SERVER = "toolkit.telemetry.server";
const PREF_ENABLED = "toolkit.telemetry.enabled";
// Do not gather data more than once a minute
const TELEMETRY_INTERVAL = 60;
// Delay before intializing telemetry (ms)
const TELEMETRY_DELAY = 60000;
// about:memory values to turn into histograms
const MEM_HISTOGRAMS = {
  "explicit/js/gc-heap": "MEMORY_JS_GC_HEAP",
  "resident": "MEMORY_RESIDENT",
  "explicit/layout/all": "MEMORY_LAYOUT_ALL"
};

XPCOMUtils.defineLazyGetter(this, "Telemetry", function () {
  return Cc["@mozilla.org/base/telemetry;1"].getService(Ci.nsITelemetry);
});

/**
 * Returns a set of histograms that can be converted into JSON
 * @return a snapshot of the histograms of form:
 *  { histogram_name: {range:[minvalue,maxvalue], bucket_count:<number of buckets>,
 *    histogram_type: <0 for exponential, 1 for linear>, bucketX:countX, ....} ...}
 * where bucket[XY], count[XY] are positive integers.
 */
function getHistograms() {
  let hls = Telemetry.histogramSnapshots;
  let ret = {};

  for (let key in hls) {
    let hgram = hls[key];
    let r = hgram.ranges;
    let c = hgram.counts;
    let retgram = {
      range: [r[1], r[r.length - 1]],
      bucket_count: r.length,
      histogram_type: hgram.histogram_type,
      values: {},
      sum: hgram.sum
    };
    let first = true;
    let last = 0;

    for (let i = 0; i < c.length; i++) {
      let value = c[i];
      if (!value)
        continue;

      // add a lower bound
      if (i && first) {
        first = false;
        retgram.values[r[i - 1]] = 0;
      }
      first = false;
      last = i + 1;
      retgram.values[r[i]] = value;
    }

    // add an upper bound
    if (last && last < c.length)
      retgram.values[r[last]] = 0;
    ret[key] = retgram;
  }
  return ret;
}

function generateUUID() {
  let str = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator).generateUUID().toString();
  // strip {}
  return str.substring(1, str.length - 1);
}

/**
 * Gets metadata about the platform the application is running on. This
 * should remain consistent across multiple telemetry pings.
 * 
 * @param  reason
 *         The reason for the telemetry ping, this will be included in the
 *         returned metadata,
 * @return The metadata as a JS object
 */
function getMetadata(reason) {
  let ai = Services.appinfo;
  let ret = {
    reason: reason,
    OS: ai.OS,
    appID: ai.ID,
    appVersion: ai.version,
    appName: ai.name,
    appBuildID: ai.appBuildID,
    platformBuildID: ai.platformBuildID,
  };

  // sysinfo fields is not always available, get what we can.
  let sysInfo = Cc["@mozilla.org/system-info;1"].getService(Ci.nsIPropertyBag2);
  let fields = ["cpucount", "memsize", "arch", "version", "device", "manufacturer", "hardware"];
  for each (let field in fields) {
    let value;
    try {
      value = sysInfo.getProperty(field);
    } catch (e) {
      continue
    }
    if (field == "memsize") {
      // Send RAM size in megabytes. Rounding because sysinfo doesn't
      // always provide RAM in multiples of 1024.
      value = Math.round(value / 1024 / 1024)
    }
    ret[field] = value
  }
  return ret;
}

/**
 * Gets a series of simple measurements (counters). At the moment, this
 * only returns startup data from nsIAppStartup.getStartupInfo().
 * 
 * @return simple measurements as a dictionary.
 */
function getSimpleMeasurements() {
  let si = Cc["@mozilla.org/toolkit/app-startup;1"].
           getService(Ci.nsIAppStartup).getStartupInfo();

  var ret = {
    // uptime in minutes
    uptime: Math.round((new Date() - si.process) / 60000)
  }
  for each (let field in ["main", "firstPaint", "sessionRestored"]) {
    if (!(field in si))
      continue;
    ret[field] = si[field] - si.process
  }
  return ret;
}

function TelemetryPing() {}

TelemetryPing.prototype = {
  _histograms: {},

  /**
   * Pull values from about:memory into corresponding histograms
   */
  gatherMemory: function gatherMemory() {
    let mgr;
    try {
      mgr = Cc["@mozilla.org/memory-reporter-manager;1"].
            getService(Ci.nsIMemoryReporterManager);
    } catch (e) {
      // OK to skip memory reporters in xpcshell
      return
    }

    let e = mgr.enumerateReporters();
    let memReporters = {};
    while (e.hasMoreElements()) {
      let mr = e.getNext().QueryInterface(Ci.nsIMemoryReporter);
      //  memReporters[mr.path] = mr.memoryUsed;
      let id = MEM_HISTOGRAMS[mr.path];
      if (!id) {
        continue;
      }

      let name = "Memory:" + mr.path + " (KB)";
      let h = this._histograms[name];
      if (!h) {
        h = Telemetry.getHistogramById(id);
        this._histograms[name] = h;
      }
      let v = Math.floor(mr.memoryUsed / 1024);
      h.add(v);
    }
    return memReporters;
  },
  
  /**
   * Send data to the server. Record success/send-time in histograms
   */
  send: function send(reason, server) {
    // populate histograms one last time
    this.gatherMemory();
    let payload = {
      ver: PAYLOAD_VERSION,
      info: getMetadata(reason),
      simpleMeasurements: getSimpleMeasurements(),
      histograms: getHistograms()
    };
    let isTestPing = (reason == "test-ping");
    // Generate a unique id once per session so the server can cope with duplicate submissions.
    // Use a deterministic url for testing.
    if (!this._path)
      this._path = "/submit/telemetry/" + (isTestPing ? reason : generateUUID());
    
    let hping = Telemetry.getHistogramById("TELEMETRY_PING");
    let hsuccess = Telemetry.getHistogramById("TELEMETRY_SUCCESS");

    let url = server + this._path;
    let request = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
                  .createInstance(Ci.nsIXMLHttpRequest);
    request.mozBackgroundRequest = true;
    request.open("POST", url, true);
    request.overrideMimeType("text/plain");
    request.setRequestHeader("Content-Type", "application/json");

    let startTime = new Date()

    function finishRequest(channel) {
      let success = false;
      try {
        success = channel.QueryInterface(Ci.nsIHttpChannel).requestSucceeded;
      } catch(e) {
      }
      hsuccess.add(success ? 1 : 0);
      hping.add(new Date() - startTime);
      if (isTestPing)
        Services.obs.notifyObservers(null, "telemetry-test-xhr-complete", null);
    }
    request.onerror = function(aEvent) finishRequest(request.channel);
    request.onload = function(aEvent) finishRequest(request.channel);

    request.send(JSON.stringify(payload));
  },
  
  /**
   * Initializes telemetry within a timer. If there is no PREF_SERVER set, don't turn on telemetry.
   */
  setup: function setup() {
    let enabled = false; 
    try {
      enabled = Services.prefs.getBoolPref(PREF_ENABLED);
      this._server = Services.prefs.getCharPref(PREF_SERVER);
    } catch (e) {
      // Prerequesite prefs aren't set
    }
    if (!enabled) 
      return;
  
    let self = this;
    this._timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    let timerCallback = function() {
      let idleService = Cc["@mozilla.org/widget/idleservice;1"].
                        getService(Ci.nsIIdleService);
      idleService.addIdleObserver(self, TELEMETRY_INTERVAL);
      Services.obs.addObserver(self, "idle-daily", false);
      Services.obs.addObserver(self, "profile-before-change", false);
      self.gatherMemory();
      delete self._timer
    }
    this._timer.initWithCallback(timerCallback, TELEMETRY_DELAY, Ci.nsITimer.TYPE_ONE_SHOT);
  },

  /** 
   * Remove observers to avoid leaks
   */
  uninstall: function uninstall() {
    let idleService = Cc["@mozilla.org/widget/idleservice;1"].
                      getService(Ci.nsIIdleService);
    idleService.removeIdleObserver(this, TELEMETRY_INTERVAL);
    Services.obs.removeObserver(this, "idle-daily");
    Services.obs.removeObserver(this, "profile-before-change");
  },

  /**
   * This observer drives telemetry.
   */
  observe: function (aSubject, aTopic, aData) {
    // Allows to change the server for testing
    var server = this._server;

    switch (aTopic) {
    case "profile-after-change":
      this.setup();
      break;
    case "profile-before-change":
      this.uninstall();
      break;
    case "idle":
      this.gatherMemory();
      break;
    case "test-ping":
      server = aData;
      // fall through
    case "idle-daily":
      this.send(aTopic, server);
      break;
    }
  },

  classID: Components.ID("{55d6a5fa-130e-4ee6-a158-0133af3b86ba}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),
};

let NSGetFactory = XPCOMUtils.generateNSGetFactory([TelemetryPing]);
