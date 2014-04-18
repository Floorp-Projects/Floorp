/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this,
    "Alerts",
    "@mozilla.org/alerts-service;1", "nsIAlertsService");

XPCOMUtils.defineLazyModuleGetter(this,
    "Prompt",
    "resource://gre/modules/Prompt.jsm");

let Strings = {};
XPCOMUtils.defineLazyGetter(Strings, "debugger",
    () => Services.strings.createBundle("chrome://global/locale/devtools/debugger.properties"));
XPCOMUtils.defineLazyGetter(Strings, "browser",
    () => Services.strings.createBundle("chrome://browser/locale/browser.properties"));

function DebuggerServerController() {
}

DebuggerServerController.prototype = {
  classID: Components.ID("{f6e8e269-ae4a-4c4a-bf80-fb4164fb072c}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDebuggerServerController, Ci.nsIObserver]),

  init: function(debuggerServer) {
    this.debugger = debuggerServer;
    Services.obs.addObserver(this, "debugger-server-started", false);
    Services.obs.addObserver(this, "debugger-server-stopped", false);
    Services.obs.addObserver(this, "xpcom-shutdown", false);
  },

  uninit: function() {
    this.debugger = null;
    Services.obs.removeObserver(this, "debugger-server-started");
    Services.obs.removeObserver(this, "debugger-server-stopped");
    Services.obs.removeObserver(this, "xpcom-shutdown");
  },

  start: function(pathOrPort) {
    if (!this.debugger.initialized) {
      this.debugger.init(this.prompt.bind(this));
      this.debugger.addBrowserActors();
      this.debugger.addActors("chrome://browser/content/dbg-browser-actors.js");
    }

    if (!pathOrPort) {
      // If the "devtools.debugger.unix-domain-socket" pref is set, we use a unix socket.
      // If not, we use a regular TCP socket.
      try {
        pathOrPort = Services.prefs.getCharPref("devtools.debugger.unix-domain-socket");
      } catch (e) {
        pathOrPort = Services.prefs.getIntPref("devtools.debugger.remote-port");
      }
    }

    try {
      this.debugger.openListener(pathOrPort);
    } catch (e) {
      dump("Unable to start debugger server (" + pathOrPort + "): " + e + "\n");
    }
  },

  stop: function() {
    this.debugger.destroy();
  },

  prompt: function() {
    let title = Strings.browser.GetStringFromName("remoteIncomingPromptTitle");
    let msg = Strings.browser.GetStringFromName("remoteIncomingPromptMessage");
    let disable = Strings.browser.GetStringFromName("remoteIncomingPromptDisable");
    let cancel = Strings.browser.GetStringFromName("remoteIncomingPromptCancel");
    let agree = Strings.browser.GetStringFromName("remoteIncomingPromptAccept");

    // Make prompt. Note: button order is in reverse.
    let prompt = new Prompt({
      window: null,
      hint: "remotedebug",
      title: title,
      message: msg,
      buttons: [ agree, cancel, disable ],
      priority: 1
    });

    // The debugger server expects a synchronous response, so spin on result since Prompt is async.
    let result = null;

    prompt.show(function(data) {
      result = data.button;
    });

    // Spin this thread while we wait for a result.
    let thread = Services.tm.currentThread;
    while (result == null)
      thread.processNextEvent(true);

    if (result === 0)
      return true;
    if (result === 2) {
      Services.prefs.setBoolPref("devtools.debugger.remote-enabled", false);
      this.debugger.destroy();
    }
    return false;
  },

  // nsIObserver

  observe: function (subject, topic, data) {
    if (topic == "xpcom-shutdown")
      this.uninit();
    if (topic == "debugger-server-started")
      this._onDebuggerStarted(data);
    if (topic == "debugger-server-stopped")
      this._onDebuggerStopped();
  },

  _onDebuggerStarted: function(portOrPath) {
    if (!Services.prefs.getBoolPref("devtools.debugger.show-server-notifications"))
      return;
    let title = l10n.GetStringFromName("debuggerStartedAlert.title");
    let port = Number(portOrPath);
    let detail;
    if (port) {
      detail = l10n.formatStringFromName("debuggerStartedAlert.detailPort", [portOrPath], 1);
    } else {
      detail = l10n.formatStringFromName("debuggerStartedAlert.detailPath", [portOrPath], 1);
    }
    Alerts.showAlertNotification(null, title, detail, false, "", function(){});
  },

  _onDebuggerStopped: function() {
    if (!Services.prefs.getBoolPref("devtools.debugger.show-server-notifications"))
      return;
    let title = l10n.GetStringFromName("debuggerStopped.title");
    Alerts.showAlertNotification(null, title, null, false, "", function(){});
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([DebuggerServerController]);
