/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const StdLog = ChromeUtils.import("resource://gre/modules/Log.jsm", {}).Log;

this.EXPORTED_SYMBOLS = ["Log"];

const PREF_LOG_LEVEL = "marionette.log.level";

/**
 * Shorthand for accessing the Marionette logging repository.
 *
 * Using this class to retrieve the `Log.jsm` repository for
 * Marionette will ensure the logger is set up correctly with the
 * appropriate stdout dumper and with the correct log level.
 *
 * Unlike `Log.jsm` this logger is E10s safe, meaning repository
 * configuration is communicated across processes.
 */
class Log {
  /**
   * Obtain the `Marionette` logger.
   *
   * The returned {@link Logger} instance is shared among all
   * callers in the same process.
   *
   * @return {Logger}
   */
  static get() {
    let logger = StdLog.repository.getLogger("Marionette");
    if (logger.ownAppenders.length == 0) {
      logger.addAppender(new StdLog.DumpAppender());
      logger.manageLevelFromPref(PREF_LOG_LEVEL);
    }
    return logger;
  }

  /**
   * Obtain a logger that logs all messages with a prefix.
   *
   * Unlike {@link LoggerRepository.getLoggerWithMessagePrefix()}
   * this function will ensure invoke {@link #get()} first to ensure
   * the logger has been properly set up.
   *
   * This returns a new object with a prototype chain that chains
   * up the original {@link Logger} instance.  The new prototype has
   * log functions that prefix `prefix` to each message.
   *
   * @param {string} prefix
   *     String to prefix each logged message with.
   *
   * @return {Proxy.<Logger>}
   */
  static getWithPrefix(prefix) {
    this.get();
    return StdLog.repository.getLoggerWithMessagePrefix("Marionette", `[${prefix}] `);
  }
}

this.Log = Log;
