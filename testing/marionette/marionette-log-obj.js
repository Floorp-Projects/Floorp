/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

this.MarionetteLogObj = function MarionetteLogObj() {
  this.logs = [];
}
MarionetteLogObj.prototype = {
  /**
   * Log message. Accepts user defined log-level.
   * @param msg String
   *        The message to be logged
   * @param level String
   *        The logging level to be used
   */
  log: function ML_log(msg, level) {
    let lev = level ? level : "INFO";
    this.logs.push( [lev, msg, (new Date()).toString()]);
  },

  /**
   * Add a list of logs to its list
   * @param msgs Object
   *        Takes a list of strings
   */
  addLogs: function ML_addLogs(msgs) {
    for (let i = 0; i < msgs.length; i++) {
      this.logs.push(msgs[i]);
    }
  },
  
  /**
   * Return all logged messages.
   */
  getLogs: function ML_getLogs() {
    let logs = this.logs;
    this.clearLogs();
    return logs;
  },

  /**
   * Clears the logs
   */
  clearLogs: function ML_clearLogs() {
    this.logs = [];
  },
}
