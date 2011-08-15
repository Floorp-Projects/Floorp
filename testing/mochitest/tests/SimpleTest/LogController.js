/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

var LogController = {}; //create the logger object

LogController.counter = 0; //current log message number
LogController.listeners = [];
LogController.logLevel = {
    FATAL: 50,
    ERROR: 40,
    WARNING: 30,
    INFO: 20,
    DEBUG: 10
};

/* set minimum logging level */
LogController.logLevelAtLeast = function(minLevel) {
    if (typeof(minLevel) == 'string') {
        minLevel = LogController.logLevel[minLevel];
    }
    return function (msg) {
        var msgLevel = msg.level;
        if (typeof(msgLevel) == 'string') {
            msgLevel = LogController.logLevel[msgLevel];
        }
        return msgLevel >= minLevel;
    };
};

/* creates the log message with the given level and info */
LogController.createLogMessage = function(level, info) {
    var msg = {};
    msg.num = LogController.counter;
    msg.level = level;
    msg.info = info;
    msg.timestamp = new Date();
    return msg;
};

/* helper method to return a sub-array */
LogController.extend = function (args, skip) {
    var ret = [];
    for (var i = skip; i<args.length; i++) {
        ret.push(args[i]);
    }
    return ret;
};

/* logs message with given level. Currently used locally by log() and error() */
LogController.logWithLevel = function(level, message/*, ...*/) {
    var msg = LogController.createLogMessage(
        level,
        LogController.extend(arguments, 1)
    );
    LogController.dispatchListeners(msg);
    LogController.counter += 1;
};

/* log with level INFO */
LogController.log = function(message/*, ...*/) {
    LogController.logWithLevel('INFO', message);
};

/* log with level ERROR */
LogController.error = function(message/*, ...*/) {
    LogController.logWithLevel('ERROR', message);
};

/* send log message to listeners */
LogController.dispatchListeners = function(msg) {
    for (var k in LogController.listeners) {
        var pair = LogController.listeners[k];
        if (pair.ident != k || (pair[0] && !pair[0](msg))) {
            continue;
        }
        pair[1](msg);
    }
};

/* add a listener to this log given an identifier, a filter (can be null) and the listener object */
LogController.addListener = function(ident, filter, listener) {
    if (typeof(filter) == 'string') {
        filter = LogController.logLevelAtLeast(filter);
    } else if (filter !== null && typeof(filter) !== 'function') {
        throw new Error("Filter must be a string, a function, or null");
    }
    var entry = [filter, listener];
    entry.ident = ident;
    LogController.listeners[ident] = entry;
};

/* remove a listener from this log */
LogController.removeListener = function(ident) {
    delete LogController.listeners[ident];
};
