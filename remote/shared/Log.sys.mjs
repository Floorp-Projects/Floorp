/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Log as StdLog } from "resource://gre/modules/Log.sys.mjs";

const PREF_REMOTE_LOG_LEVEL = "remote.log.level";

const lazy = {};

// Lazy getter which returns a cached value of the remote log level. Should be
// used for static getters used to guard hot paths for logging, eg
// isTraceLevelOrMore.
ChromeUtils.defineLazyGetter(lazy, "logLevel", () =>
  Services.prefs.getCharPref(PREF_REMOTE_LOG_LEVEL, StdLog.Level.Fatal)
);

/** E10s compatible wrapper for the standard logger from Log.sys.mjs. */
export class Log {
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
   * @param {string} type
   *     The type of logger to use. Protocol-specific modules should use the
   *     corresponding logger type. Eg. files under /marionette should use
   *     Log.TYPES.MARIONETTE.
   */
  static get(type = Log.TYPES.REMOTE_AGENT) {
    const logger = StdLog.repository.getLogger(type);
    if (!logger.ownAppenders.length) {
      logger.addAppender(new StdLog.DumpAppender());
      logger.manageLevelFromPref(PREF_REMOTE_LOG_LEVEL);
    }
    return logger;
  }

  /**
   * Check if the current log level matches the Debug log level, or any level
   * above that. This should be used to guard logger.debug calls and avoid
   * instanciating logger instances unnecessarily.
   */
  static get isDebugLevelOrMore() {
    // Debug is assigned 20, more verbose log levels have lower values.
    return StdLog.Level[lazy.logLevel] <= StdLog.Level.Debug;
  }

  /**
   * Check if the current log level matches the Trace log level, or any level
   * above that. This should be used to guard logger.trace calls and avoid
   * instanciating logger instances unnecessarily.
   */
  static get isTraceLevelOrMore() {
    // Trace is assigned 10, more verbose log levels have lower values.
    return StdLog.Level[lazy.logLevel] <= StdLog.Level.Trace;
  }

  static get verbose() {
    // we can't use Preferences.sys.mjs before first paint,
    // see ../browser/base/content/test/performance/browser_startup.js
    const level = Services.prefs.getStringPref(PREF_REMOTE_LOG_LEVEL, "Info");
    return StdLog.Level[level] >= StdLog.Level.Info;
  }
}
