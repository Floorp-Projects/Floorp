/* -*- Mode: javascript; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Update logging lives in its own module for several reasons. First, it avoids
 * having some very similar logging code duplicated in several places. And
 * second, we only want to open the update messages file once. Opening it
 * multiple times from multiple files does not result in all those messages
 * ending up in the same log file.
 *
 * Note that simply importing this module can cause the value of the
 * `app.update.log.file` pref to change. This may seem a bit weird, but we are
 * going to consider it to be acceptable because any other module that wants to
 * interact with that pref really ought to be doing it by interacting with this
 * module instead, making the pref more of an internal implementation detail.
 * This option was chosen over doing this work more lazily (say, on a log
 * message) because this initialization step has user-visible consequences
 * (the log file moves) and we'd prefer that those happen close to startup so
 * that the user doesn't observe it happening at a seemingly random time.
 */

const PREF_APP_UPDATE_LOG = "app.update.log";
const PREF_APP_UPDATE_LOG_FILE = "app.update.log.file";

const FILE_UPDATE_MESSAGES = "update_messages.log";
const FILE_BACKUP_MESSAGES = "update_messages_old.log";

const KEY_PROFILE_DIR = "ProfD";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  FileUtils: "resource://gre/modules/FileUtils.sys.mjs",
});

ChromeUtils.defineLazyGetter(
  lazy,
  "BinaryOutputStream",
  function ul_GetBinaryStream() {
    return Components.Constructor(
      "@mozilla.org/binaryoutputstream;1",
      "nsIBinaryOutputStream",
      "setOutputStream"
    );
  }
);

let gLogEnabled;
let gLogFileEnabled;
let gLogFileOutputStream;
let gLogFileBinaryStream;

let gChangeListeners = [];

function updateLogEnabledVars() {
  let prevLogEnabled = gLogEnabled;
  let prefLogFileEnabled = gLogFileEnabled;

  gLogFileEnabled = Services.prefs.getBoolPref(PREF_APP_UPDATE_LOG_FILE, false);
  if (gLogFileEnabled) {
    gLogEnabled = true;
  } else {
    gLogEnabled = Services.prefs.getBoolPref(PREF_APP_UPDATE_LOG, false);
  }

  if (gLogEnabled != prevLogEnabled || gLogFileEnabled != prefLogFileEnabled) {
    // Since we use this function during initialization, technically any present
    // listeners will be fired at that point. But it's not really possible for
    // any listeners to have been added yet.
    for (const listener of gChangeListeners) {
      try {
        listener();
      } catch (ex) {
        LOG(`Listener error: ${ex}`);
      }
    }
  }
}
updateLogEnabledVars();

function shutdown() {
  Services.prefs.removeObserver(PREF_APP_UPDATE_LOG, updateLogEnabledVars);
  Services.obs.removeObserver(shutdown, "quit-application");

  // Release references to listeners.
  gChangeListeners = [];

  if (gLogFileOutputStream) {
    gLogFileOutputStream.QueryInterface(Ci.nsISafeOutputStream);
    gLogFileOutputStream.finish();
  }
}

Services.obs.addObserver(shutdown, "quit-application");
// This one call observes PREF_APP_UPDATE_LOG and PREF_APP_UPDATE_LOG_FILE
Services.prefs.addObserver(PREF_APP_UPDATE_LOG, updateLogEnabledVars);

function logPrefixedString(prefix, message) {
  message = message.toString();

  if (gLogEnabled) {
    dump("*** " + prefix + " " + message + "\n");
    if (!Cu.isInAutomation) {
      Services.console.logStringMessage(prefix + " " + message);
    }

    if (gLogFileEnabled) {
      if (!gLogFileOutputStream) {
        let logfile = Services.dirsvc.get(KEY_PROFILE_DIR, Ci.nsIFile);
        logfile.append(FILE_UPDATE_MESSAGES);
        gLogFileOutputStream =
          lazy.FileUtils.openAtomicFileOutputStream(logfile);
      }
      if (!gLogFileBinaryStream) {
        gLogFileBinaryStream = new lazy.BinaryOutputStream(
          gLogFileOutputStream
        );
      }

      try {
        let encoded = new TextEncoder().encode(prefix + " " + message + "\n");
        gLogFileBinaryStream.writeByteArray(encoded);
        gLogFileOutputStream.flush();
      } catch (e) {
        dump(
          "*** " + prefix + " Unable to write to messages file: " + e + "\n"
        );
        Services.console.logStringMessage(
          prefix + " Unable to write to messages file: " + e
        );
      }
    }
  }
}

// Prevent file logging from persisting for more than a session by disabling
// it on startup.
function deactivateUpdateLogFile() {
  if (!Services.prefs.getBoolPref(PREF_APP_UPDATE_LOG_FILE, false)) {
    return;
  }
  Services.prefs.setBoolPref(PREF_APP_UPDATE_LOG_FILE, false);
  LOG("Application update file logging being automatically turned off");
  let logFile = Services.dirsvc.get(KEY_PROFILE_DIR, Ci.nsIFile);
  logFile.append(FILE_UPDATE_MESSAGES);

  try {
    logFile.moveTo(null, FILE_BACKUP_MESSAGES);
  } catch (e) {
    LOG(
      "Failed to backup update messages log (" +
        e +
        "). Attempting to " +
        "remove it."
    );
    try {
      logFile.remove(false);
    } catch (e) {
      LOG("Also failed to remove the update messages log: " + e);
    }
  }
}
// This can potentially cause I/O at startup, which isn't great. But it's pretty
// rare to have this option turned on, especially since we automatically turn it
// off.
deactivateUpdateLogFile();

/**
 * Logs a string to the error console.
 * @param string
 *        The string to write to the error console.
 */
function LOG(string) {
  logPrefixedString("AUS:LOG", string);
}

export const UpdateLog = {
  logPrefixedString,
  get enabled() {
    return gLogEnabled;
  },
  get logFileEnabled() {
    return gLogFileEnabled;
  },

  /**
   * Adds a callback function to be called when `UpdateLog.enabled` or
   * `UpdateLog.logFileEnabled` change values.
   *
   * Adding listeners here is preferable to adding pref listeners to the
   * underlying prefs both because it keeps callers out of the implementation
   * details and because this file also uses those listeners. Since it's hard to
   * guarantee what order the listeners run in, the actual logging behavior may
   * not have changed yet when another pref listener is invoked.
   *
   * @param listener
   *        The callback function that will be called when the configuration
   *        changes. It will be called with no arguments.
   */
  addConfigChangeListener(listener) {
    gChangeListeners.push(listener);
  },
};
