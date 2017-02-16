/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/AsyncShutdown.jsm", this);
Cu.import("resource://gre/modules/KeyValueParser.jsm");
Cu.import("resource://gre/modules/osfile.jsm", this);
Cu.import("resource://gre/modules/Promise.jsm", this);
Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/Task.jsm", this);
Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);

/**
 * Computes the SHA256 hash of the minidump file associated with a crash
 *
 * @param crashID {string} Crash ID. Likely a UUID.
 *
 * @returns {Promise} A promise that resolves to the hash value of the
 *          minidump. If the hash could not be computed then null is returned
 *          instead.
 */
function computeMinidumpHash(id) {
  let cr = Cc["@mozilla.org/toolkit/crash-reporter;1"]
             .getService(Components.interfaces.nsICrashReporter);

  return Task.spawn(function* () {
    try {
      let minidumpFile = cr.getMinidumpForID(id);
      let minidumpData = yield OS.File.read(minidumpFile.path);
      let hasher = Cc["@mozilla.org/security/hash;1"]
                     .createInstance(Ci.nsICryptoHash);
      hasher.init(hasher.SHA256);
      hasher.update(minidumpData, minidumpData.length);

      let hashBin = hasher.finish(false);
      let hash = "";

      for (let i = 0; i < hashBin.length; i++) {
        // Every character in the hash string contains a byte of the hash data
        hash += ("0" + hashBin.charCodeAt(i).toString(16)).slice(-2);
      }

      return hash;
    } catch (e) {
      Cu.reportError(e);
      return null;
    }
  });
}

/**
 * Process the .extra file associated with the crash id and return the
 * annotations it contains in an object.
 *
 * @param crashID {string} Crash ID. Likely a UUID.
 *
 * @return {Promise} A promise that resolves to an object holding the crash
 *         annotations, this object may be empty if no annotations were found.
 */
function processExtraFile(id) {
  let cr = Cc["@mozilla.org/toolkit/crash-reporter;1"]
             .getService(Components.interfaces.nsICrashReporter);

  return Task.spawn(function* () {
    try {
      let extraFile = cr.getExtraFileForID(id);
      let decoder = new TextDecoder();
      let extraData = yield OS.File.read(extraFile.path);

      return parseKeyValuePairs(decoder.decode(extraData));
    } catch (e) {
      Cu.reportError(e);
      return {};
    }
  });
}

/**
 * This component makes crash data available throughout the application.
 *
 * It is a service because some background activity will eventually occur.
 */
this.CrashService = function() {};

CrashService.prototype = Object.freeze({
  classID: Components.ID("{92668367-1b17-4190-86b2-1061b2179744}"),
  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsICrashService,
    Ci.nsIObserver,
  ]),

  addCrash(processType, crashType, id) {
    switch (processType) {
    case Ci.nsICrashService.PROCESS_TYPE_MAIN:
      processType = Services.crashmanager.PROCESS_TYPE_MAIN;
      break;
    case Ci.nsICrashService.PROCESS_TYPE_CONTENT:
      processType = Services.crashmanager.PROCESS_TYPE_CONTENT;
      break;
    case Ci.nsICrashService.PROCESS_TYPE_PLUGIN:
      processType = Services.crashmanager.PROCESS_TYPE_PLUGIN;
      break;
    case Ci.nsICrashService.PROCESS_TYPE_GMPLUGIN:
      processType = Services.crashmanager.PROCESS_TYPE_GMPLUGIN;
      break;
    case Ci.nsICrashService.PROCESS_TYPE_GPU:
      processType = Services.crashmanager.PROCESS_TYPE_GPU;
      break;
    default:
      throw new Error("Unrecognized PROCESS_TYPE: " + processType);
    }

    switch (crashType) {
    case Ci.nsICrashService.CRASH_TYPE_CRASH:
      crashType = Services.crashmanager.CRASH_TYPE_CRASH;
      break;
    case Ci.nsICrashService.CRASH_TYPE_HANG:
      crashType = Services.crashmanager.CRASH_TYPE_HANG;
      break;
    default:
      throw new Error("Unrecognized CRASH_TYPE: " + crashType);
    }

    let blocker = Task.spawn(function* () {
      let metadata = yield processExtraFile(id);
      let hash = yield computeMinidumpHash(id);

      if (hash) {
        metadata.MinidumpSha256Hash = hash;
      }

      yield Services.crashmanager.addCrash(processType, crashType, id,
                                           new Date(), metadata);
    });

    AsyncShutdown.profileBeforeChange.addBlocker(
      "CrashService waiting for content crash ping to be sent", blocker
    );

    blocker.then(AsyncShutdown.profileBeforeChange.removeBlocker(blocker));
  },

  observe(subject, topic, data) {
    switch (topic) {
      case "profile-after-change":
        // Side-effect is the singleton is instantiated.
        Services.crashmanager;
        break;
    }
  },
});

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([CrashService]);
