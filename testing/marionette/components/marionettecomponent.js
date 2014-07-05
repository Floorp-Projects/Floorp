/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

this.CC = Components.Constructor;
this.Cc = Components.classes;
this.Ci = Components.interfaces;
this.Cu = Components.utils;
this.Cr = Components.results;

const MARIONETTE_CONTRACTID = "@mozilla.org/marionette;1";
const MARIONETTE_CID = Components.ID("{786a1369-dca5-4adc-8486-33d23c88010a}");
const MARIONETTE_ENABLED_PREF = 'marionette.defaultPrefs.enabled';
const MARIONETTE_FORCELOCAL_PREF = 'marionette.force-local';
const MARIONETTE_LOG_PREF = 'marionette.logging';

this.ServerSocket = CC("@mozilla.org/network/server-socket;1",
                       "nsIServerSocket",
                       "initSpecialConnection");

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/Log.jsm");

let loader = Cc["@mozilla.org/moz/jssubscript-loader;1"]
               .getService(Ci.mozIJSSubScriptLoader);

function MarionetteComponent() {
  this._loaded = false;
  this.observerService = Services.obs;

  // set up the logger
  this.logger = Log.repository.getLogger("Marionette");
  this.logger.level = Log.Level["Trace"];
  let dumper = false;
#ifdef DEBUG
  dumper = true;
#endif
#ifdef MOZ_B2G
  dumper = true;
#endif
  try {
    if (dumper || Services.prefs.getBoolPref(MARIONETTE_LOG_PREF)) {
      let formatter = new Log.BasicFormatter();
      this.logger.addAppender(new Log.DumpAppender(formatter));
    }
  }
  catch(e) {}
}

MarionetteComponent.prototype = {
  classDescription: "Marionette component",
  classID: MARIONETTE_CID,
  contractID: MARIONETTE_CONTRACTID,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsICommandLineHandler, Ci.nsIObserver]),
  _xpcom_categories: [{category: "command-line-handler", entry: "b-marionette"},
                      {category: "profile-after-change", service: true}],
  appName: Services.appinfo.name,
  enabled: false,
  finalUiStartup: false,
  _marionetteServer: null,

  onSocketAccepted: function mc_onSocketAccepted(aSocket, aTransport) {
    this.logger.info("onSocketAccepted for Marionette dummy socket");
  },

  onStopListening: function mc_onStopListening(aSocket, status) {
    this.logger.info("onStopListening for Marionette dummy socket, code " + status);
    aSocket.close();
  },

  // Check cmdLine argument for --marionette
  handle: function mc_handle(cmdLine) {
    // If the CLI is there then lets do work otherwise nothing to see
    if (cmdLine.handleFlag("marionette", false)) {
      this.enabled = true;
      this.logger.info("marionette enabled via command-line");
      this.init();
    }
  },

  observe: function mc_observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "profile-after-change":
        // Using final-ui-startup as the xpcom category doesn't seem to work,
        // so we wait for that by adding an observer here.
        this.observerService.addObserver(this, "final-ui-startup", false);
#ifdef ENABLE_MARIONETTE
        let enabledPref = false;
        try {
          enabledPref = Services.prefs.getBoolPref(MARIONETTE_ENABLED_PREF);
        } catch(e) {}
        if (enabledPref) {
          this.enabled = true;
          this.logger.info("marionette enabled via build flag and pref");

          // We want to suppress the modal dialog that's shown
          // when starting up in safe-mode to enable testing.
          if (Services.appinfo.inSafeMode) {
            this.observerService.addObserver(this, "domwindowopened", false);
          }
        }
#endif
        break;
      case "final-ui-startup":
        this.finalUiStartup = true;
        this.observerService.removeObserver(this, aTopic);
        this.observerService.addObserver(this, "xpcom-shutdown", false);
        this.init();
        break;
      case "domwindowopened":
        this.observerService.removeObserver(this, aTopic);
        this._suppressSafeModeDialog(aSubject);
        break;
      case "xpcom-shutdown":
        this.observerService.removeObserver(this, "xpcom-shutdown");
        this.uninit();
        break;
    }
  },

  _suppressSafeModeDialog: function mc_suppressSafeModeDialog(aWindow) {
    // Wait for the modal dialog to finish loading.
    aWindow.addEventListener("load", function onLoad() {
      aWindow.removeEventListener("load", onLoad);

      if (aWindow.document.getElementById("safeModeDialog")) {
        aWindow.setTimeout(() => {
          // Accept the dialog to start in safe-mode.
          aWindow.document.documentElement.getButton("accept").click();
        });
      }
    });
  },

  init: function mc_init() {
    if (!this._loaded && this.enabled && this.finalUiStartup) {
      this._loaded = true;

      let marionette_forcelocal = this.appName == 'B2G' ? false : true;
      try {
        marionette_forcelocal = Services.prefs.getBoolPref(MARIONETTE_FORCELOCAL_PREF);
      }
      catch(e) {}
      Services.prefs.setBoolPref(MARIONETTE_FORCELOCAL_PREF, marionette_forcelocal);

      if (!marionette_forcelocal) {
        // See bug 800138.  Because the first socket that opens with
        // force-local=false fails, we open a dummy socket that will fail.
        // keepWhenOffline=true so that it still work when offline (local).
        // This allows the following attempt by Marionette to open a socket
        // to succeed.
        let insaneSacrificialGoat = new ServerSocket(666, Ci.nsIServerSocket.KeepWhenOffline, 4);
        insaneSacrificialGoat.asyncListen(this);
      }

      let port;
      try {
        port = Services.prefs.getIntPref('marionette.defaultPrefs.port');
      }
      catch(e) {
        port = 2828;
      }
      try {
        loader.loadSubScript("chrome://marionette/content/marionette-server.js");
        let forceLocal = Services.prefs.getBoolPref(MARIONETTE_FORCELOCAL_PREF);
        this._marionetteServer = new MarionetteServer(port, forceLocal);
        this.logger.info("Marionette server ready");
      }
      catch(e) {
        this.logger.error('exception: ' + e.name + ', ' + e.message);
      }
    }
  },

  uninit: function mc_uninit() {
    if (this._marionetteServer) {
      this._marionetteServer.closeListener();
    }
    this._loaded = false;
  },

};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([MarionetteComponent]);
