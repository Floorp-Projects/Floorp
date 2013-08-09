"use strict";

this.EXPORTED_SYMBOLS = ["TelemetryFile"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

let imports = {};
Cu.import("resource://gre/modules/Services.jsm", imports);
Cu.import("resource://gre/modules/Deprecated.jsm", imports);
Cu.import("resource://gre/modules/NetUtil.jsm", imports);

let {Services, Deprecated, NetUtil} = imports;

// Constants from prio.h for nsIFileOutputStream.init
const PR_WRONLY = 0x2;
const PR_CREATE_FILE = 0x8;
const PR_TRUNCATE = 0x20;
const PR_EXCL = 0x80;
const RW_OWNER = parseInt("0600", 8);
const RWX_OWNER = parseInt("0700", 8);

// Delete ping files that have been lying around for longer than this.
const MAX_PING_FILE_AGE = 7 * 24 * 60 * 60 * 1000; // 1 week

// The number of outstanding saved pings that we have issued loading
// requests for.
let pingsLoaded = 0;

// The number of those requests that have actually completed.
let pingLoadsCompleted = 0;

// If |true|, send notifications "telemetry-test-save-complete"
// and "telemetry-test-load-complete" once save/load is complete.
let shouldNotifyUponSave = false;

// Data that has neither been saved nor sent by ping
let pendingPings = [];

this.TelemetryFile = {

  /**
   * Save a single ping to a file.
   *
   * @param {object} ping The content of the ping to save.
   * @param {nsIFile} file The destination file.
   * @param {bool} sync If |true|, write synchronously. Deprecated.
   * This argument should be |false|.
   * @param {bool} overwrite If |true|, the file will be overwritten
   * if it exists.
   */
  savePingToFile: function(ping, file, sync, overwrite) {
    let pingString = JSON.stringify(ping);

    let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                    .createInstance(Ci.nsIScriptableUnicodeConverter);
    converter.charset = "UTF-8";

    let ostream = Cc["@mozilla.org/network/file-output-stream;1"]
                  .createInstance(Ci.nsIFileOutputStream);
    let initFlags = PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE;
    if (!overwrite) {
      initFlags |= PR_EXCL;
    }
    try {
      ostream.init(file, initFlags, RW_OWNER, 0);
    } catch (e) {
      // Probably due to PR_EXCL.
      return;
    }

    if (sync) {
      let utf8String = converter.ConvertFromUnicode(pingString);
      utf8String += converter.Finish();
      let success = false;
      try {
        let amount = ostream.write(utf8String, utf8String.length);
        success = amount == utf8String.length;
      } catch (e) {
      }
      finishTelemetrySave(success, ostream);
    } else {
      let istream = converter.convertToInputStream(pingString);
      let self = this;
      NetUtil.asyncCopy(istream, ostream,
                        function(result) {
                          finishTelemetrySave(Components.isSuccessCode(result),
                              ostream);
                        });
    }
  },

  /**
   * Save a ping to its file, synchronously.
   *
   * @param {object} ping The content of the ping to save.
   * @param {bool} overwrite If |true|, the file will be overwritten
   * if it exists.
   */
  savePing: function(ping, overwrite) {
    this.savePingToFile(ping,
      getSaveFileForPing(ping), true, overwrite);
  },

  /**
   * Save all pending pings, synchronously.
   *
   * @param {object} sessionPing The additional session ping.
   */
  savePendingPings: function(sessionPing) {
    this.savePing(sessionPing, true);
    pendingPings.forEach(function sppcb(e, i, a) {
      this.savePing(e, false);
    }, this);
    pendingPings = [];
  },

  /**
   * Remove the file for a ping
   *
   * @param {object} ping The ping.
   */
  cleanupPingFile: function(ping) {
    // FIXME: We shouldn't create the directory just to remove the file.
    let file = getSaveFileForPing(ping);
    try {
      file.remove(true); // FIXME: Should be |false|, isn't it?
    } catch(e) {
    }
  },

  /**
   * Load all saved pings.
   *
   * Once loaded, the saved pings can be accessed (destructively only)
   * through |popPendingPings|.
   *
   * @param {bool} sync If |true|, loading takes place synchronously.
   * @param {function*} onLoad A function called upon loading of each
   * ping. It is passed |true| in case of success, |false| in case of
   * format error.
   */
  loadSavedPings: function(sync, onLoad = null) {
    let directory = ensurePingDirectory();
    let entries = directory.directoryEntries
                           .QueryInterface(Ci.nsIDirectoryEnumerator);
    pingsLoaded = 0;
    pingLoadsCompleted = 0;
    try {
      while (entries.hasMoreElements()) {
        this.loadHistograms(entries.nextFile, sync, onLoad);
      }
    } finally {
      entries.close();
    }
  },

  /**
   * Load the histograms from a file.
   *
   * Once loaded, the saved pings can be accessed (destructively only)
   * through |popPendingPings|.
   *
   * @param {nsIFile} file The file to load.
   * @param {bool} sync If |true|, loading takes place synchronously.
   * @param {function*} onLoad A function called upon loading of the
   * ping. It is passed |true| in case of success, |false| in case of
   * format error.
   */
  loadHistograms: function loadHistograms(file, sync, onLoad = null) {
    let now = new Date();
    if (now - file.lastModifiedTime > MAX_PING_FILE_AGE) {
      // We haven't had much luck in sending this file; delete it.
      file.remove(true);
      return;
    }

    pingsLoaded++;
    if (sync) {
      let stream = Cc["@mozilla.org/network/file-input-stream;1"]
                   .createInstance(Ci.nsIFileInputStream);
      stream.init(file, -1, -1, 0);
      addToPendingPings(file, stream, onLoad);
    } else {
      let channel = NetUtil.newChannel(file);
      channel.contentType = "application/json";

      NetUtil.asyncFetch(channel, (function(stream, result) {
        if (!Components.isSuccessCode(result)) {
          return;
        }
        addToPendingPings(file, stream, onLoad);
      }).bind(this));
    }
  },

  /**
   * The number of pings loaded since the beginning of time.
   */
  get pingsLoaded() {
    return pingsLoaded;
  },

  /**
   * Iterate destructively through the pending pings.
   *
   * @return {iterator}
   */
  popPendingPings: function(reason) {
    while (pendingPings.length > 0) {
      let data = pendingPings.pop();
      // Send persisted pings to the test URL too.
      if (reason == "test-ping") {
        data.reason = reason;
      }
      yield data;
    }
  },

  set shouldNotifyUponSave(value) {
    shouldNotifyUponSave = value;
  },

  testLoadHistograms: function(file, sync, onLoad) {
    pingsLoaded = 0;
    pingLoadsCompleted = 0;
    this.loadHistograms(file, sync, onLoad);
  }
};

///// Utility functions

function getSaveFileForPing(ping) {
  let file = ensurePingDirectory();
  file.append(ping.slug);
  return file;
};

function ensurePingDirectory() {
  let directory = Services.dirsvc.get("ProfD", Ci.nsILocalFile).clone();
  directory.append("saved-telemetry-pings");
  try {
    directory.create(Ci.nsIFile.DIRECTORY_TYPE, RWX_OWNER);
  } catch (e) {
    // Already exists, just ignore this.
  }
  return directory;
};

function addToPendingPings(file, stream, onLoad) {
  let success = false;

  try {
    let string = NetUtil.readInputStreamToString(stream, stream.available(),
      { charset: "UTF-8" });
    stream.close();
    let ping = JSON.parse(string);
    // The ping's payload used to be stringified JSON.  Deal with that.
    if (typeof(ping.payload) == "string") {
      ping.payload = JSON.parse(ping.payload);
    }
    pingLoadsCompleted++;
    pendingPings.push(ping);
    if (shouldNotifyUponSave &&
        pingLoadsCompleted == pingsLoaded) {
      Services.obs.notifyObservers(null, "telemetry-test-load-complete", null);
    }
    success = true;
  } catch (e) {
    // An error reading the file, or an error parsing the contents.
    stream.close();           // close is idempotent.
    file.remove(true); // FIXME: Should be false, isn't it?
  }
  if (onLoad) {
    onLoad(success);
  }
};

function finishTelemetrySave(ok, stream) {
  stream.close();
  if (shouldNotifyUponSave && ok) {
    Services.obs.notifyObservers(null, "telemetry-test-save-complete", null);
  }
};
