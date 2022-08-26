/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Log"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const { Log: StdLog } = ChromeUtils.import("resource://gre/modules/Log.jsm");

const PREF_REMOTE_LOG_LEVEL = "remote.log.level";

// We still check the marionette log preference for backward compatibility.
// This can be removed when geckodriver 0.30 (bug 1686110) has been released.
const PREF_MARIONETTE_LOG_LEVEL = "marionette.log.level";

const lazy = {};

// Lazy getter which will return the preference (remote or marionette) which has
// the most verbose log level.
XPCOMUtils.defineLazyGetter(lazy, "prefLogLevel", () => {
  function getLogLevelNumber(pref) {
    const level = Services.prefs.getCharPref(pref, "Fatal");
    return (
      StdLog.Level.Numbers[level.toUpperCase()] || StdLog.Level.Numbers.FATAL
    );
  }

  const marionetteNumber = getLogLevelNumber(PREF_MARIONETTE_LOG_LEVEL);
  const remoteNumber = getLogLevelNumber(PREF_REMOTE_LOG_LEVEL);

  if (marionetteNumber < remoteNumber) {
    return PREF_MARIONETTE_LOG_LEVEL;
  }

  return PREF_REMOTE_LOG_LEVEL;
});

/** E10s compatible wrapper for the standard logger from Log.jsm. */
class Log {
  static TYPES = {
    CDP: "CDP",
    MARIONETTE: "Marionette",
    REMOTE_AGENT: "RemoteAgent",
    WEBDRIVER_BIDI: "WebDriver BiDi",
  };

  /**
   * Get a logger instance. For each provided type, a dedicated logger instance
   * will be returned, but all loggers are relying on the same preference.
   *
   * @param {String} type
   *     The type of logger to use. Protocol-specific modules should use the
   *     corresponding logger type. Eg. files under /marionette should use
   *     Log.TYPES.MARIONETTE.
   */
  static get(type = Log.TYPES.REMOTE_AGENT) {
    const logger = StdLog.repository.getLogger(type);
    if (!logger.ownAppenders.length) {
      logger.addAppender(new StdLog.DumpAppender());
      logger.manageLevelFromPref(lazy.prefLogLevel);
    }
    return logger;
  }

  /**
   * Check if the current log level matches the Trace log level. This should be
   * used to guard logger.trace calls and avoid instanciating logger instances
   * unnecessarily.
   */
  static get isTraceLevel() {
    return [StdLog.Level.All, StdLog.Level.Trace].includes(lazy.prefLogLevel);
  }

  static get verbose() {
    // we can't use Preferences.jsm before first paint,
    // see ../browser/base/content/test/performance/browser_startup.js
    const level = Services.prefs.getStringPref(PREF_REMOTE_LOG_LEVEL, "Info");
    return StdLog.Level[level] >= StdLog.Level.Info;
  }
}
