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
  let extraPath = OS.Path.join(cr.minidumpPath.path, id + ".extra");

  return Task.spawn(function* () {
    try {
      let decoder = new TextDecoder();
      let extraFile = yield OS.File.read(extraPath);
      let extraData = decoder.decode(extraFile);

      return parseKeyValuePairs(extraData);
    } catch (e) {
      Cu.reportError(e);
    }

    return {};
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

  addCrash: function(processType, crashType, id) {
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

    AsyncShutdown.profileBeforeChange.addBlocker(
      "CrashService waiting for content crash ping to be sent",
      processExtraFile(id).then(metadata => {
        return Services.crashmanager.addCrash(processType, crashType, id,
                                              new Date(), metadata)
      })
    );
  },

  observe: function(subject, topic, data) {
    switch (topic) {
      case "profile-after-change":
        // Side-effect is the singleton is instantiated.
        Services.crashmanager;
        break;
    }
  },
});

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([CrashService]);
