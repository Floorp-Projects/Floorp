/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This component handles the -start-debugger-server command line
 * option. It also starts and stops the debugger server when the
 * devtools.debugger.remote-enable pref changes.
 */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

const REMOTE_ENABLED_PREF = "devtools.debugger.remote-enabled";
const UNIX_SOCKET_PREF = "devtools.debugger.unix-domain-socket";
const REMOTE_PORT_PREF = "devtools.debugger.remote-port";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this,
    "DebuggerServer",
    "resource://gre/modules/devtools/dbg-server.jsm");

function DevToolsAppStartup() {
}

DevToolsAppStartup.prototype = {
  classID: Components.ID("{9ba9bbe7-5866-46f1-bea6-3299066b7933}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsICommandLineHandler, Ci.nsIObserver]),

  get dbgPortOrPath() {
    if (!this._dbgPortOrPath) {
      // set default dbgPortOrPath value from preferences:
      try {
        this._dbgPortOrPath = Services.prefs.getCharPref(UNIX_SOCKET_PREF);
      } catch(e) {
        try {
          this._dbgPortOrPath = Services.prefs.getIntPref(REMOTE_PORT_PREF);
        } catch(e) {}
      }
    }
    return this._dbgPortOrPath;
  },

  set dbgPortOrPath(value) {
    this._dbgPortOrPath = value;
  },

  // nsICommandLineHandler

  get helpInfo() {
    let str = "";

    // Starting the debugger is handled on the app side (not in /toolkit/).
    // If the app didn't expose a debugger controller component, we don't
    // support the -start-debugger-server option.

    if (DebuggerServer.controller) {
      str += "  -start-debugger-server [<port or unix domain socket path>]";
      if (this.dbgPortOrPath) {
        str += " (default: " + this.dbgPortOrPath + ")";
      }
      str += "\n";
    }

    return str;
  },

  handle: function(cmdLine) {
    if (!DebuggerServer.controller) {
      // This app doesn't expose a debugger controller.
      // We can't handle the -start-debugger-server option
      // or the remote-enable pref.
      return;
    }

    let startDebuggerServerBecauseCmdLine = false;

    try {
      // Returns null if the argument was not specified. Throws
      // NS_ERROR_INVALID_ARG if there is no parameter specified (because
      // it was the last argument or the next argument starts with '-').
      // However, someone could still explicitly pass an empty argument.
      let param = cmdLine.handleFlagWithParam("start-debugger-server", false);
      if (param) {
        startDebuggerServerBecauseCmdLine = true;
        this.dbgPortOrPath = param;
      }
    } catch(e) {
      startDebuggerServerBecauseCmdLine = true;
    }

    // App has started and we handled the command line options (if any).
    // Time to start the debugger if needed and observe the remote-enable
    // pref.

    if (startDebuggerServerBecauseCmdLine ||
        Services.prefs.getBoolPref(REMOTE_ENABLED_PREF)) {
      if (this.dbgPortOrPath) {
        DebuggerServer.controller.start(this.dbgPortOrPath);
      } else {
        dump("Can't start debugger: no port or path specified\n");
      }
    }

    Services.prefs.addObserver(REMOTE_ENABLED_PREF, this, false);
    Services.prefs.addObserver(UNIX_SOCKET_PREF, this, false);
    Services.prefs.addObserver(REMOTE_PORT_PREF, this, false);
  },

  // nsIObserver

  observe: function (subject, topic, data) {
    if (topic == "nsPref:changed") {
      switch (data) {
        case REMOTE_ENABLED_PREF:
          if (Services.prefs.getBoolPref(data)) {
            DebuggerServer.controller.start(this.dbgPortOrPath);
          } else {
            DebuggerServer.controller.stop();
          }
        break;
        case UNIX_SOCKET_PREF:
        case REMOTE_PORT_PREF:
          // reset dbgPortOrPath value
          this.dbgPortOrPath = null;
        break;
      }
    }
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([DevToolsAppStartup]);
