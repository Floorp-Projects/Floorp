/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);

/**
 * This component makes crash data available throughout the application.
 *
 * It is a service because some background activity will eventually occur.
 */
this.CrashService = function () {};

CrashService.prototype = Object.freeze({
  classID: Components.ID("{92668367-1b17-4190-86b2-1061b2179744}"),
  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsICrashService,
    Ci.nsIObserver,
  ]),

  addCrash: function (processType, crashType, id) {
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

    Services.crashmanager.addCrash(processType, crashType, id, new Date());
  },

  observe: function (subject, topic, data) {
    switch (topic) {
      case "profile-after-change":
        // Side-effect is the singleton is instantiated.
        let m = Services.crashmanager;
        break;
    }
  },
});

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([CrashService]);
