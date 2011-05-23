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

const PREF_SERVER = "toolkit.telemetry.server";
const PREF_ENABLED = "toolkit.telemetry.enabled";
// Do not gather data more than once a minute
const TELEMETRY_INTERVAL = 60;
// Delay before intializing telemetry (ms)
const TELEMETRY_DELAY = 60000;
// about:memory values to turn into histograms
const MEM_HISTOGRAMS = {
  "heap-used/js/gc-heap": [1024, 1024 * 500, 10],
  "mapped/heap/used": [1024, 2 * 1024 * 1024, 10],
  "heap-used/layout/all": [1024, 50 * 1025, 10]
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
      values: {}
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
  return Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator)
         .generateUUID().toString();
}

/**
 * @return request metadata
 */
function getMetadata(reason) {
  let si = Cc["@mozilla.org/toolkit/app-startup;1"].
           getService(Ci.nsIAppStartup).getStartupInfo();
  let ai = Services.appinfo;
  let ret = {
    uptime: (new Date() - si.process),
    reason: reason,
    OS: ai.OS,
    XPCOMABI: ai.XPCOMABI,
    ID: ai.ID,
    version: ai.version,
    name: ai.name,
    appBuildID: ai.appBuildID,
    platformBuildID: ai.platformBuildID,
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
      let specs = MEM_HISTOGRAMS[mr.path];
      if (!specs) {
        continue;
      }

      let name = "Memory:" + mr.path + " (KB)";
      let h = this._histograms[name];
      if (!h) {
        h = Telemetry.newHistogram(name, specs[0], specs[1], specs[2], Telemetry.HISTOGRAM_EXPONENTIAL);
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
    let nativeJSON = Cc["@mozilla.org/dom/json;1"].createInstance(Ci.nsIJSON);
    let payload = {
      info: getMetadata(reason),
      histograms: getHistograms()
    };
    let isTestPing = (reason == "test-ping");
    // Generate a unique id once per session so the server can cope with duplicate submissions.
    // Use a deterministic url for testing.
    if (!this._path)
      this._path = "/submit/telemetry/" + (isTestPing ? reason : generateUUID());
    
    const TELEMETRY_PING = "telemetry.ping (ms)";
    const TELEMETRY_SUCCESS = "telemetry.success (No, Yes)";

    let hping = Telemetry.newHistogram(TELEMETRY_PING, 1, 3000, 10, Telemetry.HISTOGRAM_EXPONENTIAL);
    let hsuccess = Telemetry.newHistogram(TELEMETRY_SUCCESS, 1, 2, 3, Telemetry.HISTOGRAM_LINEAR);

    let url = server + this._path;
    let request = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
                  .createInstance(Ci.nsIXMLHttpRequest);
    request.mozBackgroundRequest = true;
    request.open("POST", url, true);
    request.overrideMimeType("text/plain");

    let startTime = new Date()

    function finishRequest(success_metric) {
      hsuccess.add(success_metric);
      hping.add(new Date() - startTime);
      if (isTestPing)
        Services.obs.notifyObservers(null, "telemetry-test-xhr-complete", null);
    }
    request.onerror = function(aEvent) finishRequest(1);
    request.onload = function(aEvent) finishRequest(2);
    request.send(nativeJSON.encode(payload));
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
