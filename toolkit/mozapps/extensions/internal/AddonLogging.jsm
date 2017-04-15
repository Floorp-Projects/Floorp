/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

const KEY_PROFILEDIR                  = "ProfD";
const FILE_EXTENSIONS_LOG             = "extensions.log";
const PREF_LOGGING_ENABLED            = "extensions.logging.enabled";

const LOGGER_FILE_PERM                = parseInt("666", 8);

const NS_PREFBRANCH_PREFCHANGE_TOPIC_ID = "nsPref:changed";

Components.utils.import("resource://gre/modules/FileUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

this.EXPORTED_SYMBOLS = [ "LogManager" ];

var gDebugLogEnabled = false;

function formatLogMessage(aType, aName, aStr, aException) {
  let message = aType.toUpperCase() + " " + aName + ": " + aStr;
  if (aException) {
    if (typeof aException == "number")
      return message + ": " + Components.Exception("", aException).name;

    message  = message + ": " + aException;
    // instanceOf doesn't work here, let's duck type
    if (aException.fileName)
      message = message + " (" + aException.fileName + ":" + aException.lineNumber + ")";

    if (aException.message == "too much recursion")
      dump(message + "\n" + aException.stack + "\n");
  }
  return message;
}

function getStackDetails(aException) {
  // Defensively wrap all this to ensure that failing to get the message source
  // doesn't stop the message from being logged
  try {
    if (aException) {
      if (aException instanceof Ci.nsIException) {
        return {
          sourceName: aException.filename,
          lineNumber: aException.lineNumber
        };
      }

      if (typeof aException == "object") {
        return {
          sourceName: aException.fileName,
          lineNumber: aException.lineNumber
        };
      }
    }

    let stackFrame = Components.stack.caller.caller.caller;
    return {
      sourceName: stackFrame.filename,
      lineNumber: stackFrame.lineNumber
    };
  } catch (e) {
    return {
      sourceName: null,
      lineNumber: 0
    };
  }
}

function AddonLogger(aName) {
  this.name = aName;
}

AddonLogger.prototype = {
  name: null,

  error(aStr, aException) {
    let message = formatLogMessage("error", this.name, aStr, aException);

    let stack = getStackDetails(aException);

    let consoleMessage = Cc["@mozilla.org/scripterror;1"].
                         createInstance(Ci.nsIScriptError);
    consoleMessage.init(message, stack.sourceName, null, stack.lineNumber, 0,
                        Ci.nsIScriptError.errorFlag, "component javascript");
    Services.console.logMessage(consoleMessage);

    // Always dump errors, in case the Console Service isn't listening yet
    dump("*** " + message + "\n");

    function formatTimestamp(date) {
      // Format timestamp as: "%Y-%m-%d %H:%M:%S"
      let year = String(date.getFullYear());
      let month = String(date.getMonth() + 1).padStart(2, "0");
      let day = String(date.getDate()).padStart(2, "0");
      let hours = String(date.getHours()).padStart(2, "0");
      let minutes = String(date.getMinutes()).padStart(2, "0");
      let seconds = String(date.getSeconds()).padStart(2, "0");

      return `${year}-${month}-${day} ${hours}:${minutes}:${seconds}`;
    }

    try {
      var tstamp = new Date();
      var logfile = FileUtils.getFile(KEY_PROFILEDIR, [FILE_EXTENSIONS_LOG]);
      var stream = Cc["@mozilla.org/network/file-output-stream;1"].
                   createInstance(Ci.nsIFileOutputStream);
      stream.init(logfile, 0x02 | 0x08 | 0x10, LOGGER_FILE_PERM, 0); // write, create, append
      var writer = Cc["@mozilla.org/intl/converter-output-stream;1"].
                   createInstance(Ci.nsIConverterOutputStream);
      writer.init(stream, "UTF-8", 0, 0x0000);
      writer.writeString(formatTimestamp(tstamp) + " " +
                         message + " at " + stack.sourceName + ":" +
                         stack.lineNumber + "\n");
      writer.close();
    } catch (e) { }
  },

  warn(aStr, aException) {
    let message = formatLogMessage("warn", this.name, aStr, aException);

    let stack = getStackDetails(aException);

    let consoleMessage = Cc["@mozilla.org/scripterror;1"].
                         createInstance(Ci.nsIScriptError);
    consoleMessage.init(message, stack.sourceName, null, stack.lineNumber, 0,
                        Ci.nsIScriptError.warningFlag, "component javascript");
    Services.console.logMessage(consoleMessage);

    if (gDebugLogEnabled)
      dump("*** " + message + "\n");
  },

  log(aStr, aException) {
    if (gDebugLogEnabled) {
      let message = formatLogMessage("log", this.name, aStr, aException);
      dump("*** " + message + "\n");
      Services.console.logStringMessage(message);
    }
  }
};

this.LogManager = {
  getLogger(aName, aTarget) {
    let logger = new AddonLogger(aName);

    if (aTarget) {
      ["error", "warn", "log"].forEach(function(name) {
        let fname = name.toUpperCase();
        delete aTarget[fname];
        aTarget[fname] = function(aStr, aException) {
          logger[name](aStr, aException);
        };
      });
    }

    return logger;
  }
};

var PrefObserver = {
  init() {
    Services.prefs.addObserver(PREF_LOGGING_ENABLED, this);
    Services.obs.addObserver(this, "xpcom-shutdown");
    this.observe(null, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID, PREF_LOGGING_ENABLED);
  },

  observe(aSubject, aTopic, aData) {
    if (aTopic == "xpcom-shutdown") {
      Services.prefs.removeObserver(PREF_LOGGING_ENABLED, this);
      Services.obs.removeObserver(this, "xpcom-shutdown");
    } else if (aTopic == NS_PREFBRANCH_PREFCHANGE_TOPIC_ID) {
      gDebugLogEnabled = Services.prefs.getBoolPref(PREF_LOGGING_ENABLED, false);
    }
  }
};

PrefObserver.init();
