/* -*- indent-tabs-mode: nil -*- */
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
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Taras Glek <tglek@mozilla.com>
 *   Vladan Djeric <vdjeric@mozilla.com>
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
Cu.import("resource://gre/modules/LightweightThemeManager.jsm");
Cu.import("resource://gre/modules/ctypes.jsm");

// When modifying the payload in incompatible ways, please bump this version number
const PAYLOAD_VERSION = 1;

const PREF_SERVER = "toolkit.telemetry.server";
const PREF_ENABLED = "toolkit.telemetry.enabled";
// Do not gather data more than once a minute
const TELEMETRY_INTERVAL = 60000;
// Delay before intializing telemetry (ms)
const TELEMETRY_DELAY = 60000;
// about:memory values to turn into histograms
const MEM_HISTOGRAMS = {
  "js-gc-heap": "MEMORY_JS_GC_HEAP",
  "js-compartments-system": "MEMORY_JS_COMPARTMENTS_SYSTEM",
  "js-compartments-user": "MEMORY_JS_COMPARTMENTS_USER",
  "explicit": "MEMORY_EXPLICIT",
  "resident": "MEMORY_RESIDENT",
  "storage-sqlite": "MEMORY_STORAGE_SQLITE",
  "images-content-used-uncompressed":
    "MEMORY_IMAGES_CONTENT_USED_UNCOMPRESSED",
  "heap-allocated": "MEMORY_HEAP_ALLOCATED",
  "page-faults-hard": "PAGE_FAULTS_HARD",
  "low-memory-events-virtual": "LOW_MEMORY_EVENTS_VIRTUAL",
  "low-memory-events-physical": "LOW_MEMORY_EVENTS_PHYSICAL"
};
// Seconds of idle time before pinging.
// On idle-daily a gather-telemetry notification is fired, during it probes can
// start asynchronous tasks to gather data.  On the next idle the data is sent.
const IDLE_TIMEOUT_SECONDS = 5 * 60;

var gLastMemoryPoll = null;

function getLocale() {
  return Cc["@mozilla.org/chrome/chrome-registry;1"].
         getService(Ci.nsIXULChromeRegistry).
         getSelectedLocale('global');
}

XPCOMUtils.defineLazyServiceGetter(this, "Telemetry",
                                   "@mozilla.org/base/telemetry;1",
                                   "nsITelemetry");
XPCOMUtils.defineLazyServiceGetter(this, "idleService",
                                   "@mozilla.org/widget/idleservice;1",
                                   "nsIIdleService");

function generateUUID() {
  let str = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator).generateUUID().toString();
  // strip {}
  return str.substring(1, str.length - 1);
}

/**
 * Gets a series of simple measurements (counters). At the moment, this
 * only returns startup data from nsIAppStartup.getStartupInfo().
 * 
 * @return simple measurements as a dictionary.
 */
function getSimpleMeasurements() {
  let si = Services.startup.getStartupInfo();

  var ret = {
    // uptime in minutes
    uptime: Math.round((new Date() - si.process) / 60000)
  }

  // Look for app-specific timestamps
  var appTimestamps = {};
  try {
    let o = {};
    Cu.import("resource:///modules/TelemetryTimestamps.jsm", o);
    appTimestamps = o.TelemetryTimestamps.get();
  } catch (ex) {}

  if (si.process) {
    for each (let field in Object.keys(si)) {
      if (field == "process")
        continue;
      ret[field] = si[field] - si.process
    }

    for (let p in appTimestamps) {
      if (!(p in ret) && appTimestamps[p])
        ret[p] = appTimestamps[p] - si.process;
    }
  }

  ret.startupInterrupted = new Number(Services.startup.interrupted);

  ret.js = Cc["@mozilla.org/js/xpc/XPConnect;1"]
           .getService(Ci.nsIJSEngineTelemetryStats)
           .telemetryValue;

  return ret;
}

/**
 * Read the update channel from defaults only.  We do this to ensure that
 * the channel is tightly coupled with the application and does not apply
 * to other installations of the application that may use the same profile.
 */
function getUpdateChannel() {
  var channel = "default";
  var prefName;
  var prefValue;

  var defaults = Services.prefs.getDefaultBranch(null);
  try {
    channel = defaults.getCharPref("app.update.channel");
  } catch (e) {
    // use default when pref not found
  }

  try {
    var partners = Services.prefs.getChildList("app.partner.");
    if (partners.length) {
      channel += "-cck";
      partners.sort();

      for each (prefName in partners) {
        prefValue = Services.prefs.getCharPref(prefName);
        channel += "-" + prefValue;
      }
    }
  }
  catch (e) {
    Cu.reportError(e);
  }

  return channel;
}

function TelemetryPing() {}

TelemetryPing.prototype = {
  _histograms: {},
  _initialized: false,
  _prevValues: {},
  // Generate a unique id once per session so the server can cope with
  // duplicate submissions.
  _uuid: generateUUID(),
  // Regex that matches histograms we carea bout during startup.
  _startupHistogramRegex: /SQLITE|HTTP|SPDY|CACHE|DNS/,
  _slowSQLStartup: {},
  _prevSession: null,

  /**
   * When reflecting a histogram into JS, Telemetry hands us an object
   * with the following properties:
   * 
   * - min, max, histogram_type, sum: simple integers;
   * - counts: array of counts for histogram buckets;
   * - ranges: array of calculated bucket sizes.
   * 
   * This format is not straightforward to read and potentially bulky
   * with lots of zeros in the counts array.  Packing histograms makes
   * raw histograms easier to read and compresses the data a little bit.
   *
   * Returns an object:
   * { range: [min, max], bucket_count: <number of buckets>,
   *   histogram_type: <histogram_type>, sum: <sum>
   *   values: { bucket1: count1, bucket2: count2, ... } }
   */
  packHistogram: function packHistogram(hgram) {
    let r = hgram.ranges;;
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
        retgram.values[r[i - 1]] = 0;
      }
      first = false;
      last = i + 1;
      retgram.values[r[i]] = value;
    }

    // add an upper bound
    if (last && last < c.length)
      retgram.values[r[last]] = 0;
    return retgram;
  },

  getHistograms: function getHistograms() {
    let hls = Telemetry.histogramSnapshots;
    let info = Telemetry.registeredHistograms;
    let ret = {};

    for (let name in hls) {
      if (info[name]) {
        ret[name] = this.packHistogram(hls[name]);
        let startup_name = "STARTUP_" + name;
        if (hls[startup_name])
          ret[startup_name] = this.packHistogram(hls[startup_name]);
      }
    }

    return ret;
  },

  getAddonHistograms: function getAddonHistograms() {
    let ahs = Telemetry.addonHistogramSnapshots;
    let ret = {};

    for (let name in ahs) {
      ret[name] = this.convertHistogram(ahs[name]);
    }

    return ret;
  },

  addValue: function addValue(name, id, val) {
    let h = this._histograms[name];
    if (!h) {
      h = Telemetry.getHistogramById(id);
      this._histograms[name] = h;
    }
    h.add(val);
  },

  /**
   * Descriptive metadata
   * 
   * @param  reason
   *         The reason for the telemetry ping, this will be included in the
   *         returned metadata,
   * @return The metadata as a JS object
   */
  getMetadata: function getMetadata(reason) {
    let ai = Services.appinfo;
    let ret = {
      reason: reason,
      OS: ai.OS,
      appID: ai.ID,
      appVersion: ai.version,
      appName: ai.name,
      appBuildID: ai.appBuildID,
      appUpdateChannel: getUpdateChannel(),
      platformBuildID: ai.platformBuildID
    };

    // sysinfo fields are not always available, get what we can.
    let sysInfo = Cc["@mozilla.org/system-info;1"].getService(Ci.nsIPropertyBag2);
    let fields = ["cpucount", "memsize", "arch", "version", "device", "manufacturer", "hardware",
                  "hasMMX", "hasSSE", "hasSSE2", "hasSSE3",
                  "hasSSSE3", "hasSSE4A", "hasSSE4_1", "hasSSE4_2",
                  "hasEDSP", "hasARMv6", "hasNEON"];
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

    // gfxInfo fields are not always available, get what we can.
    let gfxInfo = Cc["@mozilla.org/gfx/info;1"].getService(Ci.nsIGfxInfo);
    let gfxfields = ["adapterDescription", "adapterVendorID", "adapterDeviceID",
                     "adapterRAM", "adapterDriver", "adapterDriverVersion",
                     "adapterDriverDate", "adapterDescription2",
                     "adapterVendorID2", "adapterDeviceID2", "adapterRAM2",
                     "adapterDriver2", "adapterDriverVersion2",
                     "adapterDriverDate2", "isGPU2Active", "D2DEnabled;",
                     "DWriteEnabled", "DWriteVersion"
                    ];

    if (gfxInfo) {
      for each (let field in gfxfields) {
        try {
          let value = "";
          value = gfxInfo[field];
          if (value != "")
            ret[field] = value;
        } catch (e) {
          continue
        }
      }
    }

    let theme = LightweightThemeManager.currentTheme;
    if (theme)
      ret.persona = theme.id;

    if (this._addons)
      ret.addons = this._addons;

    return ret;
  },

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
      return;
    }

    let e = mgr.enumerateReporters();
    while (e.hasMoreElements()) {
      let mr = e.getNext().QueryInterface(Ci.nsIMemoryReporter);
      let id = MEM_HISTOGRAMS[mr.path];
      if (!id) {
        continue;
      }
      // mr.amount is expensive to read in some cases, so get it only once.
      let amount = mr.amount;
      if (amount == -1) {
        continue;
      }

      let val;
      if (mr.units == Ci.nsIMemoryReporter.UNITS_BYTES) {
        val = Math.floor(amount / 1024);
      }
      else if (mr.units == Ci.nsIMemoryReporter.UNITS_COUNT) {
        val = amount;
      }
      else if (mr.units == Ci.nsIMemoryReporter.UNITS_COUNT_CUMULATIVE) {
        // If the reporter gives us a cumulative count, we'll report the
        // difference in its value between now and our previous ping.

        if (!(mr.path in this._prevValues)) {
          // If this is the first time we're reading this reporter, store its
          // current value but don't report it in the telemetry ping, so we
          // ignore the effect startup had on the reporter.
          this._prevValues[mr.path] = amount;
          continue;
        }

        val = amount - this._prevValues[mr.path];
        this._prevValues[mr.path] = amount;
      }
      else {
        NS_ASSERT(false, "Can't handle memory reporter with units " + mr.units);
        continue;
      }
      this.addValue(mr.path, id, val);
    }
  },

  /**
   * Return true if we're interested in having a STARTUP_* histogram for
   * the given histogram name.
   */
  isInterestingStartupHistogram: function isInterestingStartupHistogram(name) {
    return this._startupHistogramRegex.test(name);
  },
  
  /** 
   * Make a copy of interesting histograms at startup.
   */
  gatherStartupInformation: function gatherStartupInformation() {
    let info = Telemetry.registeredHistograms;
    let snapshots = Telemetry.histogramSnapshots;
    for (let name in info) {
      // Only duplicate histograms with actual data.
      if (this.isInterestingStartupHistogram(name) && name in snapshots) {
        Telemetry.histogramFrom("STARTUP_" + name, name);
      }
    }
    this._slowSQLStartup = Telemetry.slowSQL;
  },

  getSessionPayloadAndSlug: function getSessionPayloadAndSlug(reason) {
    // Use a deterministic url for testing.
    let isTestPing = (reason == "test-ping");
    let havePreviousSession = !!this._prevSession;
    let slug = (isTestPing
                ? reason
                : (havePreviousSession
                   ? this._prevSession.uuid
                   : this._uuid));
    let payloadObj = {
      ver: PAYLOAD_VERSION,
      info: this.getMetadata(reason)
    };

    if (havePreviousSession) {
      payloadObj.histograms = this.getHistograms(this._prevSession.snapshots);
    }
    else {
      payloadObj.simpleMeasurements = getSimpleMeasurements();
      payloadObj.histograms = this.getHistograms(Telemetry.histogramSnapshots);
      payloadObj.slowSQL = Telemetry.slowSQL;
      payloadObj.addonHistograms = this.getAddonHistograms();
    }
    if (Object.keys(this._slowSQLStartup.mainThread).length
	|| Object.keys(this._slowSQLStartup.otherThreads).length) {
      payloadObj.slowSQLStartup = this._slowSQLStartup;
    }

    return { previous: !!havePreviousSession,
             slug: slug, payload: JSON.stringify(payloadObj) };
  },

  /**
   * Send data to the server. Record success/send-time in histograms
   */
  send: function send(reason, server) {
    // populate histograms one last time
    this.gatherMemory();

    let data = this.getSessionPayloadAndSlug(reason);

    // Don't record a successful ping for previous session data.
    this.doPing(server, data.slug, data.payload, !data.previous);
    this._prevSession = null;

    // We were sending off data from before; now send the actual data
    // we've collected this session.
    if (data.previous) {
      data = this.getSessionPayloadAndSlug(reason);
      this.doPing(server, data.slug, data.payload, true);
    }
  },

  doPing: function doPing(server, slug, payload, recordSuccess) {
    let submitPath = "/submit/telemetry/" + slug;
    let url = server + submitPath;
    let request = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
                  .createInstance(Ci.nsIXMLHttpRequest);
    request.mozBackgroundRequest = true;
    request.open("POST", url, true);
    request.overrideMimeType("text/plain");
    request.setRequestHeader("Content-Type", "application/json; charset=UTF-8");

    let startTime = new Date();
    let file = this.savedHistogramsFile();

    function finishRequest(channel) {
      let success = false;
      try {
        success = channel.QueryInterface(Ci.nsIHttpChannel).requestSucceeded;
      } catch(e) {
      }
      if (recordSuccess) {
        let hping = Telemetry.getHistogramById("TELEMETRY_PING");
        let hsuccess = Telemetry.getHistogramById("TELEMETRY_SUCCESS");

        hsuccess.add(success);
        hping.add(new Date() - startTime);
      }
      if (success && file.exists()) {
        file.remove(true);
      }
      if (slug == "test-ping")
        Services.obs.notifyObservers(null, "telemetry-test-xhr-complete", null);
    }
    request.addEventListener("error", function(aEvent) finishRequest(request.channel), false);
    request.addEventListener("load", function(aEvent) finishRequest(request.channel), false);

    request.setRequestHeader("Content-Encoding", "gzip");
    let payloadStream = Cc["@mozilla.org/io/string-input-stream;1"]
                        .createInstance(Ci.nsIStringInputStream);
    payloadStream.data = this.gzipCompressString(payload);
    request.send(payloadStream);
  },

  gzipCompressString: function gzipCompressString(string) {
    let observer = {
      buffer: "",
      onStreamComplete: function(loader, context, status, length, result) {
	this.buffer = String.fromCharCode.apply(this, result);
      }
    };

    let scs = Cc["@mozilla.org/streamConverters;1"]
              .getService(Ci.nsIStreamConverterService);
    let listener = Cc["@mozilla.org/network/stream-loader;1"]
                  .createInstance(Ci.nsIStreamLoader);
    listener.init(observer);
    let converter = scs.asyncConvertData("uncompressed", "gzip",
                                         listener, null);
    let stringStream = Cc["@mozilla.org/io/string-input-stream;1"]
                       .createInstance(Ci.nsIStringInputStream);
    stringStream.data = string;
    converter.onStartRequest(null, null);
    converter.onDataAvailable(null, null, stringStream, 0, string.length);
    converter.onStopRequest(null, null, null);
    return observer.buffer;
  },

  attachObservers: function attachObservers() {
    if (!this._initialized)
      return;
    Services.obs.addObserver(this, "cycle-collector-begin", false);
    Services.obs.addObserver(this, "idle-daily", false);
  },

  detachObservers: function detachObservers() {
    if (!this._initialized)
      return;
    Services.obs.removeObserver(this, "idle-daily");
    Services.obs.removeObserver(this, "cycle-collector-begin");
    if (this._isIdleObserver) {
      idleService.removeIdleObserver(this, IDLE_TIMEOUT_SECONDS);
      this._isIdleObserver = false;
    }
  },

  savedHistogramsFile: function savedHistogramsFile() {
    let profileDirectory = Services.dirsvc.get("ProfD", Ci.nsILocalFile);
    let profileFile = profileDirectory.clone();

    // There's a bunch of binary data in the file, so we need to be
    // sensitive to multiple machine types.  Use ctypes to get some
    // discriminating information.
    let size = ctypes.voidptr_t.size;
    // Hack to figure out endianness.
    let uint32_array_t = ctypes.uint32_t.array(1);
    let array = uint32_array_t([0xdeadbeef]);
    let uint8_array_t = ctypes.uint8_t.array(4);
    let array_as_bytes = ctypes.cast(array, uint8_array_t);
    let endian = (array_as_bytes[0] === 0xde) ? "big" : "little"
    let name = "sessionHistograms.dat." + size + endian;
    profileFile.append(name);
    return profileFile;
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
    if (!enabled) {
      // Turn off local telemetry if telemetry is disabled.
      // This may change once about:telemetry is added.
      Telemetry.canRecord = false;
      return;
    }
    Services.obs.addObserver(this, "private-browsing", false);
    Services.obs.addObserver(this, "profile-before-change", false);
    Services.obs.addObserver(this, "sessionstore-windows-restored", false);
    Services.obs.addObserver(this, "quit-application-granted", false);

    // Delay full telemetry initialization to give the browser time to
    // run various late initializers. Otherwise our gathered memory
    // footprint and other numbers would be too optimistic.
    let self = this;
    this._timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    let timerCallback = function() {
      self._initialized = true;
      self.attachObservers();
      self.gatherMemory();
      delete self._timer
    }
    this._timer.initWithCallback(timerCallback, TELEMETRY_DELAY, Ci.nsITimer.TYPE_ONE_SHOT);

    // Load data from the previous session.
    let loadCallback = function(data) {
      self._prevSession = data;
    }
    Telemetry.loadHistograms(this.savedHistogramsFile(), loadCallback);
  },

  /** 
   * Remove observers to avoid leaks
   */
  uninstall: function uninstall() {
    this.detachObservers()
    Services.obs.removeObserver(this, "sessionstore-windows-restored");
    Services.obs.removeObserver(this, "profile-before-change");
    Services.obs.removeObserver(this, "private-browsing");
    Services.obs.removeObserver(this, "quit-application-granted");
  },

  /**
   * This observer drives telemetry.
   */
  observe: function (aSubject, aTopic, aData) {
    // Allows to change the server for testing
    var server = this._server;

    switch (aTopic) {
    case "Add-ons":
      this._addons = aData;
      break;
    case "profile-after-change":
      this.setup();
      break;
    case "profile-before-change":
      this.uninstall();
      break;
    case "cycle-collector-begin":
      let now = new Date();
      if (!gLastMemoryPoll
          || (TELEMETRY_INTERVAL <= now - gLastMemoryPoll)) {
        gLastMemoryPoll = now;
        this.gatherMemory();
      }
      break;
    case "private-browsing":
      Telemetry.canRecord = aData == "exit";
      if (aData == "enter") {
        this.detachObservers()
      } else {
        this.attachObservers()
      }
      break;
    case "sessionstore-windows-restored":
      this.gatherStartupInformation();
      break;
    case "idle-daily":
      // Enqueue to main-thread, otherwise components may be inited by the
      // idle-daily category and miss the gather-telemetry notification.
      Services.tm.mainThread.dispatch((function() {
        // Notify that data should be gathered now, since ping will happen soon.
        Services.obs.notifyObservers(null, "gather-telemetry", null);
        // The ping happens at the first idle of length IDLE_TIMEOUT_SECONDS.
        idleService.addIdleObserver(this, IDLE_TIMEOUT_SECONDS);
        this._isIdleObserver = true;
      }).bind(this), Ci.nsIThread.DISPATCH_NORMAL);
      break;
    case "get-payload":
      this.gatherMemory();
      this.gatherStartupInformation();
      let data = this.getSessionPayloadAndSlug("gather-payload");

      aSubject.QueryInterface(Ci.nsISupportsString).data = data.payload;
      break;
    case "test-ping":
      server = aData;
      // fall through
    case "idle":
      if (this._isIdleObserver) {
        idleService.removeIdleObserver(this, IDLE_TIMEOUT_SECONDS);
        this._isIdleObserver = false;
      }
      reason = (Telemetry.canSend && aTopic == "idle"
		? "idle-daily"
		: "test-ping");
      this.send(reason, server);
      break;
    case "quit-application-granted":
      Telemetry.saveHistograms(this.savedHistogramsFile(),
                               this._uuid, function (success) success,
			      /*isSynchronous=*/true);
      break;
    }
  },

  classID: Components.ID("{55d6a5fa-130e-4ee6-a158-0133af3b86ba}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),
};

let NSGetFactory = XPCOMUtils.generateNSGetFactory([TelemetryPing]);
