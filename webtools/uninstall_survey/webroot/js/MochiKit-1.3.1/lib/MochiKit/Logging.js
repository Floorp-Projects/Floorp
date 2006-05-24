/***

MochiKit.Logging 1.3.1

See <http://mochikit.com/> for documentation, downloads, license, etc.

(c) 2005 Bob Ippolito.  All rights Reserved.

***/
if (typeof(dojo) != 'undefined') {
    dojo.provide('MochiKit.Logging');
    dojo.require('MochiKit.Base');
}

if (typeof(JSAN) != 'undefined') {
    JSAN.use("MochiKit.Base", []);
}

try {
    if (typeof(MochiKit.Base) == 'undefined') {
        throw "";
    }
} catch (e) {
    throw "MochiKit.Logging depends on MochiKit.Base!";
}

if (typeof(MochiKit.Logging) == 'undefined') {
    MochiKit.Logging = {};
}

MochiKit.Logging.NAME = "MochiKit.Logging";
MochiKit.Logging.VERSION = "1.3.1";
MochiKit.Logging.__repr__ = function () {
    return "[" + this.NAME + " " + this.VERSION + "]";
};

MochiKit.Logging.toString = function () {
    return this.__repr__();
};


MochiKit.Logging.EXPORT = [
    "LogLevel",
    "LogMessage",
    "Logger",
    "alertListener",
    "logger",
    "log",
    "logError",
    "logDebug",
    "logFatal",
    "logWarning"
];


MochiKit.Logging.EXPORT_OK = [
    "logLevelAtLeast",
    "isLogMessage",
    "compareLogMessage"
];


MochiKit.Logging.LogMessage = function (num, level, info) {
    this.num = num;
    this.level = level;
    this.info = info;
    this.timestamp = new Date();
};

MochiKit.Logging.LogMessage.prototype = {
    repr: function () {
        var m = MochiKit.Base;
        return 'LogMessage(' + 
            m.map(
                m.repr,
                [this.num, this.level, this.info]
            ).join(', ') + ')';
    },
    toString: MochiKit.Base.forwardCall("repr")
};

MochiKit.Base.update(MochiKit.Logging, {
    logLevelAtLeast: function (minLevel) {
        var self = MochiKit.Logging;
        if (typeof(minLevel) == 'string') {
            minLevel = self.LogLevel[minLevel];
        }
        return function (msg) {
            var msgLevel = msg.level;
            if (typeof(msgLevel) == 'string') {
                msgLevel = self.LogLevel[msgLevel];
            }
            return msgLevel >= minLevel;
        };
    },

    isLogMessage: function (/* ... */) {
        var LogMessage = MochiKit.Logging.LogMessage;
        for (var i = 0; i < arguments.length; i++) {
            if (!(arguments[i] instanceof LogMessage)) {
                return false;
            }
        }
        return true;
    },

    compareLogMessage: function (a, b) {
        return MochiKit.Base.compare([a.level, a.info], [b.level, b.info]);
    },

    alertListener: function (msg) {
        alert(
            "num: " + msg.num +
            "\nlevel: " +  msg.level +
            "\ninfo: " + msg.info.join(" ")
        );
    }

});

MochiKit.Logging.Logger = function (/* optional */maxSize) {
    this.counter = 0;
    if (typeof(maxSize) == 'undefined' || maxSize === null) {
        maxSize = -1;
    }
    this.maxSize = maxSize;
    this._messages = [];
    this.listeners = {};
    this.useNativeConsole = false;
};

MochiKit.Logging.Logger.prototype = {
    clear: function () {
        this._messages.splice(0, this._messages.length);
    },

    logToConsole: function (msg) {
        if (typeof(window) != "undefined" && window.console
                && window.console.log) {
            // Safari
            window.console.log(msg);
        } else if (typeof(opera) != "undefined" && opera.postError) {
            // Opera
            opera.postError(msg);
        } else if (typeof(printfire) == "function") {
            // FireBug
            printfire(msg);
        }
    },
    
    dispatchListeners: function (msg) {
        for (var k in this.listeners) {
            var pair = this.listeners[k];
            if (pair.ident != k || (pair[0] && !pair[0](msg))) {
                continue;
            }
            pair[1](msg);
        }
    },

    addListener: function (ident, filter, listener) {
        if (typeof(filter) == 'string') {
            filter = MochiKit.Logging.logLevelAtLeast(filter);
        }
        var entry = [filter, listener];
        entry.ident = ident;
        this.listeners[ident] = entry;
    },

    removeListener: function (ident) {
        delete this.listeners[ident];
    },

    baseLog: function (level, message/*, ...*/) {
        var msg = new MochiKit.Logging.LogMessage(
            this.counter,
            level,
            MochiKit.Base.extend(null, arguments, 1)
        );
        this._messages.push(msg);
        this.dispatchListeners(msg);
        if (this.useNativeConsole) {
            this.logToConsole(msg.level + ": " + msg.info.join(" "));
        }
        this.counter += 1;
        while (this.maxSize >= 0 && this._messages.length > this.maxSize) {
            this._messages.shift();
        }
    },

    getMessages: function (howMany) {
        var firstMsg = 0;
        if (!(typeof(howMany) == 'undefined' || howMany === null)) {
            firstMsg = Math.max(0, this._messages.length - howMany);
        }
        return this._messages.slice(firstMsg);
    },

    getMessageText: function (howMany) {
        if (typeof(howMany) == 'undefined' || howMany === null) {
            howMany = 30;
        }
        var messages = this.getMessages(howMany);
        if (messages.length) {
            var lst = map(function (m) {
                return '\n  [' + m.num + '] ' + m.level + ': ' + m.info.join(' '); 
            }, messages);
            lst.unshift('LAST ' + messages.length + ' MESSAGES:');
            return lst.join('');
        }
        return '';
    },

    debuggingBookmarklet: function (inline) {
        if (typeof(MochiKit.LoggingPane) == "undefined") {
            alert(this.getMessageText());
        } else {
            MochiKit.LoggingPane.createLoggingPane(inline || false);
        }
    }
};


MochiKit.Logging.__new__ = function () {
    this.LogLevel = {
        ERROR: 40,
        FATAL: 50,
        WARNING: 30,
        INFO: 20,
        DEBUG: 10
    };

    var m = MochiKit.Base;
    m.registerComparator("LogMessage",
        this.isLogMessage,
        this.compareLogMessage
    );

    var partial = m.partial;

    var Logger = this.Logger;
    var baseLog = Logger.prototype.baseLog;
    m.update(this.Logger.prototype, {
        debug: partial(baseLog, 'DEBUG'),
        log: partial(baseLog, 'INFO'),
        error: partial(baseLog, 'ERROR'),
        fatal: partial(baseLog, 'FATAL'),
        warning: partial(baseLog, 'WARNING')
    });

    // indirectly find logger so it can be replaced
    var self = this;
    var connectLog = function (name) {
        return function () {
            self.logger[name].apply(self.logger, arguments);
        };
    };

    this.log = connectLog('log');
    this.logError = connectLog('error');
    this.logDebug = connectLog('debug');
    this.logFatal = connectLog('fatal');
    this.logWarning = connectLog('warning');
    this.logger = new Logger();
    this.logger.useNativeConsole = true;

    this.EXPORT_TAGS = {
        ":common": this.EXPORT,
        ":all": m.concat(this.EXPORT, this.EXPORT_OK)
    };

    m.nameFunctions(this);

};

if (typeof(printfire) == "undefined" &&
        typeof(document) != "undefined" && document.createEvent &&
        typeof(dispatchEvent) != "undefined") {
    // FireBug really should be less lame about this global function
    printfire  = function () {
        printfire.args = arguments;
        var ev = document.createEvent("Events");
        ev.initEvent("printfire", false, true);
        dispatchEvent(ev);
    };
}

MochiKit.Logging.__new__();

MochiKit.Base._exportSymbols(this, MochiKit.Logging);
