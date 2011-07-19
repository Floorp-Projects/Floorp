/*
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Extension Manager.
#
# The Initial Developer of the Original Code is
# the Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2010
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Dave Townsend <dtownsend@oxymoronical.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****
*/

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

var EXPORTED_SYMBOLS = [ "LogManager" ];

var gDebugLogEnabled = false;

function formatLogMessage(aType, aName, aStr, aException) {
  let message = aType.toUpperCase() + " " + aName + ": " + aStr;
  if (aException)
    return message + ": " + aException;
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

      return {
        sourceName: aException.fileName,
        lineNumber: aException.lineNumber
      };
    }

    let stackFrame = Components.stack.caller.caller.caller;
    return {
      sourceName: stackFrame.filename,
      lineNumber: stackFrame.lineNumber
    };
  }
  catch (e) {
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

  error: function(aStr, aException) {
    let message = formatLogMessage("error", this.name, aStr, aException);

    let stack = getStackDetails(aException);

    let consoleMessage = Cc["@mozilla.org/scripterror;1"].
                         createInstance(Ci.nsIScriptError);
    consoleMessage.init(message, stack.sourceName, null, stack.lineNumber, 0,
                        Ci.nsIScriptError.errorFlag, "component javascript");
    Services.console.logMessage(consoleMessage);

    if (gDebugLogEnabled)
      dump("*** " + message + "\n");

    try {
      var tstamp = new Date();
      var logfile = FileUtils.getFile(KEY_PROFILEDIR, [FILE_EXTENSIONS_LOG]);
      var stream = Cc["@mozilla.org/network/file-output-stream;1"].
                   createInstance(Ci.nsIFileOutputStream);
      stream.init(logfile, 0x02 | 0x08 | 0x10, LOGGER_FILE_PERM, 0); // write, create, append
      var writer = Cc["@mozilla.org/intl/converter-output-stream;1"].
                   createInstance(Ci.nsIConverterOutputStream);
      writer.init(stream, "UTF-8", 0, 0x0000);
      writer.writeString(tstamp.toLocaleFormat("%Y-%m-%d %H:%M:%S ") +
                         message + " at " + stack.sourceName + ":" +
                         stack.lineNumber + "\n");
      writer.close();
    }
    catch (e) { }
  },

  warn: function(aStr, aException) {
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

  log: function(aStr, aException) {
    if (gDebugLogEnabled) {
      let message = formatLogMessage("log", this.name, aStr, aException);
      dump("*** " + message + "\n");
      Services.console.logStringMessage(message);
    }
  }
};

var LogManager = {
  getLogger: function(aName, aTarget) {
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
  init: function() {
    Services.prefs.addObserver(PREF_LOGGING_ENABLED, this, false);
    Services.obs.addObserver(this, "xpcom-shutdown", false);
    this.observe(null, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID, PREF_LOGGING_ENABLED);
  },

  observe: function(aSubject, aTopic, aData) {
    if (aTopic == "xpcom-shutdown") {
      Services.prefs.removeObserver(PREF_LOGGING_ENABLED, this);
      Services.obs.removeObserver(this, "xpcom-shutdown");
    }
    else if (aTopic == NS_PREFBRANCH_PREFCHANGE_TOPIC_ID) {
      try {
        gDebugLogEnabled = Services.prefs.getBoolPref(PREF_LOGGING_ENABLED);
      }
      catch (e) {
        gDebugLogEnabled = false;
      }
    }
  }
};

PrefObserver.init();
