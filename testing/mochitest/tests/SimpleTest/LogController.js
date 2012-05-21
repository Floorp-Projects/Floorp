/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
