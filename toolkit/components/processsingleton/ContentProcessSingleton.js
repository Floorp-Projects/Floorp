/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cu = Components.utils;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");

/*
 * The message manager has an upper limit on message sizes that it can
 * reliably forward to the parent so we limit the size of console log event
 * messages that we forward here. The web console is local and receives the
 * full console message, but addons subscribed to console event messages
 * in the parent receive the truncated version. Due to fragmentation,
 * messages as small as 1MB have resulted in IPC allocation failures on
 * 32-bit platforms. To limit IPC allocation sizes, console.log messages
 * with arguments with total size > MSG_MGR_CONSOLE_MAX_SIZE (bytes) have
 * their arguments completely truncated. MSG_MGR_CONSOLE_VAR_SIZE is an
 * approximation of how much space (in bytes) a JS non-string variable will
 * require in the manager's implementation. For strings, we use 2 bytes per
 * char. The console message URI and function name are limited to
 * MSG_MGR_CONSOLE_INFO_MAX characters. We don't attempt to calculate
 * the exact amount of space the message manager implementation will require
 * for a given message so this is imperfect.
 */
const MSG_MGR_CONSOLE_MAX_SIZE = 1024 * 1024; // 1MB
const MSG_MGR_CONSOLE_VAR_SIZE = 8;
const MSG_MGR_CONSOLE_INFO_MAX = 1024;

function ContentProcessSingleton() {}
ContentProcessSingleton.prototype = {
  classID: Components.ID("{ca2a8470-45c7-11e4-916c-0800200c9a66}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),

  observe(subject, topic, data) {
    switch (topic) {
    case "app-startup": {
      Services.obs.addObserver(this, "console-api-log-event", false);
      Services.obs.addObserver(this, "xpcom-shutdown", false);
      cpmm.addMessageListener("DevTools:InitDebuggerServer", this);
      break;
    }
    case "console-api-log-event": {
      let consoleMsg = subject.wrappedJSObject;

      let msgData = {
        level: consoleMsg.level,
        filename: consoleMsg.filename.substring(0, MSG_MGR_CONSOLE_INFO_MAX),
        lineNumber: consoleMsg.lineNumber,
        functionName: consoleMsg.functionName.substring(0,
          MSG_MGR_CONSOLE_INFO_MAX),
        timeStamp: consoleMsg.timeStamp,
        arguments: [],
      };

      // We can't send objects over the message manager, so we sanitize
      // them out, replacing those arguments with "<unavailable>".
      let unavailString = "<unavailable>";
      let unavailStringLength = unavailString.length * 2; // 2-bytes per char

      // When the sum of argument sizes reaches MSG_MGR_CONSOLE_MAX_SIZE,
      // replace all arguments with "<truncated>".
      let totalArgLength = 0;

      // Walk through the arguments, checking the type and size.
      for (let arg of consoleMsg.arguments) {
        if ((typeof arg == "object" || typeof arg == "function") &&
            arg !== null) {
          arg = unavailString;
          totalArgLength += unavailStringLength;
        } else if (typeof arg == "string") {
          totalArgLength += arg.length * 2; // 2-bytes per char
        } else {
          totalArgLength += MSG_MGR_CONSOLE_VAR_SIZE;
        }

        if (totalArgLength <= MSG_MGR_CONSOLE_MAX_SIZE) {
          msgData.arguments.push(arg);
        } else {
          // arguments take up too much space
          msgData.arguments = ["<truncated>"];
          break;
        }
      }

      cpmm.sendAsyncMessage("Console:Log", msgData);
      break;
    }

    case "xpcom-shutdown":
      Services.obs.removeObserver(this, "console-api-log-event");
      Services.obs.removeObserver(this, "xpcom-shutdown");
      cpmm.removeMessageListener("DevTools:InitDebuggerServer", this);
      break;
    }
  },

  receiveMessage(message) {
    // load devtools component on-demand
    // Only reply if we are in a real content process
    if (Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT) {
      let {init} = Cu.import("resource://devtools/server/content-server.jsm", {});
      init(message);
    }
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([ContentProcessSingleton]);
