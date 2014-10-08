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

function ContentProcessSingleton() {}
ContentProcessSingleton.prototype = {
  classID: Components.ID("{ca2a8470-45c7-11e4-916c-0800200c9a66}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),

  observe: function(subject, topic, data) {
    switch (topic) {
    case "app-startup": {
      Services.obs.addObserver(this, "console-api-log-event", false);
      Services.obs.addObserver(this, "xpcom-shutdown", false);
      break;
    }
    case "console-api-log-event": {
      let consoleMsg = subject.wrappedJSObject;

      let msgData = {
        level: consoleMsg.level,
        filename: consoleMsg.filename,
        lineNumber: consoleMsg.lineNumber,
        functionName: consoleMsg.functionName,
        timeStamp: consoleMsg.timeStamp,
        arguments: [],
      };

      // We can't send objects over the message manager, so we sanitize
      // them out.
      for (let arg of consoleMsg.arguments) {
        if ((typeof arg == "object" || typeof arg == "function") && arg !== null) {
          msgData.arguments.push("<unavailable>");
        } else {
          msgData.arguments.push(arg);
        }
      }

      cpmm.sendAsyncMessage("Console:Log", msgData);
      break;
    }

    case "xpcom-shutdown":
      Services.obs.removeObserver(this, "console-api-log-event");
      Services.obs.removeObserver(this, "xpcom-shutdown");
      break;
    }
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([ContentProcessSingleton]);
