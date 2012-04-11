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
 * The Original Code is log4moz.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Michael Johnston <special.michael@gmail.com> (Original Author)
 *   Dan Mills <thunder@mozilla.com>
 *   Philipp von Weitershausen <philipp@weitershausen.de>
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

const EXPORTED_SYMBOLS = ['Log4Moz'];

const {classes: Cc, interfaces: Ci, results: Cr, utils: Cu} = Components;

const ONE_BYTE = 1;
const ONE_KILOBYTE = 1024 * ONE_BYTE;
const ONE_MEGABYTE = 1024 * ONE_KILOBYTE;

const STREAM_SEGMENT_SIZE = 4096;
const PR_UINT32_MAX = 0xffffffff;

Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");

let Log4Moz = {
  Level: {
    Fatal:  70,
    Error:  60,
    Warn:   50,
    Info:   40,
    Config: 30,
    Debug:  20,
    Trace:  10,
    All:    0,
    Desc: {
      70: "FATAL",
      60: "ERROR",
      50: "WARN",
      40: "INFO",
      30: "CONFIG",
      20: "DEBUG",
      10: "TRACE",
      0:  "ALL"
    }
  },

  get repository() {
    delete Log4Moz.repository;
    Log4Moz.repository = new LoggerRepository();
    return Log4Moz.repository;
  },
  set repository(value) {
    delete Log4Moz.repository;
    Log4Moz.repository = value;
  },

  LogMessage: LogMessage,
  Logger: Logger,
  LoggerRepository: LoggerRepository,

  Formatter: Formatter,
  BasicFormatter: BasicFormatter,

  Appender: Appender,
  DumpAppender: DumpAppender,
  ConsoleAppender: ConsoleAppender,
  BlockingStreamAppender: BlockingStreamAppender,
  StorageStreamAppender: StorageStreamAppender,

  // Discouraged due to blocking I/O.
  FileAppender: FileAppender,
  RotatingFileAppender: RotatingFileAppender,

  // Logging helper:
  // let logger = Log4Moz.repository.getLogger("foo");
  // logger.info(Log4Moz.enumerateInterfaces(someObject).join(","));
  enumerateInterfaces: function Log4Moz_enumerateInterfaces(aObject) {
    let interfaces = [];

    for (i in Ci) {
      try {
        aObject.QueryInterface(Ci[i]);
        interfaces.push(i);
      }
      catch(ex) {}
    }

    return interfaces;
  },

  // Logging helper:
  // let logger = Log4Moz.repository.getLogger("foo");
  // logger.info(Log4Moz.enumerateProperties(someObject).join(","));
  enumerateProperties: function Log4Moz_enumerateProps(aObject,
                                                       aExcludeComplexTypes) {
    let properties = [];

    for (p in aObject) {
      try {
        if (aExcludeComplexTypes &&
            (typeof aObject[p] == "object" || typeof aObject[p] == "function"))
          continue;
        properties.push(p + " = " + aObject[p]);
      }
      catch(ex) {
        properties.push(p + " = " + ex);
      }
    }

    return properties;
  }
};


/*
 * LogMessage
 * Encapsulates a single log event's data
 */
function LogMessage(loggerName, level, message){
  this.loggerName = loggerName;
  this.message = message;
  this.level = level;
  this.time = Date.now();
}
LogMessage.prototype = {
  get levelDesc() {
    if (this.level in Log4Moz.Level.Desc)
      return Log4Moz.Level.Desc[this.level];
    return "UNKNOWN";
  },

  toString: function LogMsg_toString(){
    return "LogMessage [" + this.time + " " + this.level + " " +
      this.message + "]";
  }
};

/*
 * Logger
 * Hierarchical version.  Logs to all appenders, assigned or inherited
 */

function Logger(name, repository) {
  if (!repository)
    repository = Log4Moz.repository;
  this._name = name;
  this.children = [];
  this.ownAppenders = [];
  this.appenders = [];
  this._repository = repository;
}
Logger.prototype = {
  get name() {
    return this._name;
  },

  _level: null,
  get level() {
    if (this._level != null)
      return this._level;
    if (this.parent)
      return this.parent.level;
    dump("log4moz warning: root logger configuration error: no level defined\n");
    return Log4Moz.Level.All;
  },
  set level(level) {
    this._level = level;
  },

  _parent: null,
  get parent() this._parent,
  set parent(parent) {
    if (this._parent == parent) {
      return;
    }
    // Remove ourselves from parent's children
    if (this._parent) {
      let index = this._parent.children.indexOf(this);
      if (index != -1) {
        this._parent.children.splice(index, 1);
      }
    }
    this._parent = parent;
    parent.children.push(this);
    this.updateAppenders();
  },

  updateAppenders: function updateAppenders() {
    if (this._parent) {
      let notOwnAppenders = this._parent.appenders.filter(function(appender) {
        return this.ownAppenders.indexOf(appender) == -1;
      }, this);
      this.appenders = notOwnAppenders.concat(this.ownAppenders);
    } else {
      this.appenders = this.ownAppenders.slice();
    }

    // Update children's appenders.
    for (let i = 0; i < this.children.length; i++) {
      this.children[i].updateAppenders();
    }
  },

  addAppender: function Logger_addAppender(appender) {
    if (this.ownAppenders.indexOf(appender) != -1) {
      return;
    }
    this.ownAppenders.push(appender);
    this.updateAppenders();
  },

  removeAppender: function Logger_removeAppender(appender) {
    let index = this.ownAppenders.indexOf(appender);
    if (index == -1) {
      return;
    }
    this.ownAppenders.splice(index, 1);
    this.updateAppenders();
  },

  log: function Logger_log(level, string) {
    if (this.level > level)
      return;

    // Hold off on creating the message object until we actually have
    // an appender that's responsible.
    let message;
    let appenders = this.appenders;
    for (let i = 0; i < appenders.length; i++){
      let appender = appenders[i];
      if (appender.level > level)
        continue;

      if (!message)
        message = new LogMessage(this._name, level, string);

      appender.append(message);
    }
  },

  fatal: function Logger_fatal(string) {
    this.log(Log4Moz.Level.Fatal, string);
  },
  error: function Logger_error(string) {
    this.log(Log4Moz.Level.Error, string);
  },
  warn: function Logger_warn(string) {
    this.log(Log4Moz.Level.Warn, string);
  },
  info: function Logger_info(string) {
    this.log(Log4Moz.Level.Info, string);
  },
  config: function Logger_config(string) {
    this.log(Log4Moz.Level.Config, string);
  },
  debug: function Logger_debug(string) {
    this.log(Log4Moz.Level.Debug, string);
  },
  trace: function Logger_trace(string) {
    this.log(Log4Moz.Level.Trace, string);
  }
};

/*
 * LoggerRepository
 * Implements a hierarchy of Loggers
 */

function LoggerRepository() {}
LoggerRepository.prototype = {
  _loggers: {},

  _rootLogger: null,
  get rootLogger() {
    if (!this._rootLogger) {
      this._rootLogger = new Logger("root", this);
      this._rootLogger.level = Log4Moz.Level.All;
    }
    return this._rootLogger;
  },
  set rootLogger(logger) {
    throw "Cannot change the root logger";
  },

  _updateParents: function LogRep__updateParents(name) {
    let pieces = name.split('.');
    let cur, parent;

    // find the closest parent
    // don't test for the logger name itself, as there's a chance it's already
    // there in this._loggers
    for (let i = 0; i < pieces.length - 1; i++) {
      if (cur)
        cur += '.' + pieces[i];
      else
        cur = pieces[i];
      if (cur in this._loggers)
        parent = cur;
    }

    // if we didn't assign a parent above, there is no parent
    if (!parent)
      this._loggers[name].parent = this.rootLogger;
    else
      this._loggers[name].parent = this._loggers[parent];

    // trigger updates for any possible descendants of this logger
    for (let logger in this._loggers) {
      if (logger != name && logger.indexOf(name) == 0)
        this._updateParents(logger);
    }
  },

  getLogger: function LogRep_getLogger(name) {
    if (name in this._loggers)
      return this._loggers[name];
    this._loggers[name] = new Logger(name, this);
    this._updateParents(name);
    return this._loggers[name];
  }
};

/*
 * Formatters
 * These massage a LogMessage into whatever output is desired
 * Only the BasicFormatter is currently implemented
 */

// Abstract formatter
function Formatter() {}
Formatter.prototype = {
  format: function Formatter_format(message) {}
};

// Basic formatter that doesn't do anything fancy
function BasicFormatter(dateFormat) {
  if (dateFormat)
    this.dateFormat = dateFormat;
}
BasicFormatter.prototype = {
  __proto__: Formatter.prototype,

  format: function BF_format(message) {
    return message.time + "\t" + message.loggerName + "\t" + message.levelDesc
           + "\t" + message.message + "\n";
  }
};

/*
 * Appenders
 * These can be attached to Loggers to log to different places
 * Simply subclass and override doAppend to implement a new one
 */

function Appender(formatter) {
  this._name = "Appender";
  this._formatter = formatter? formatter : new BasicFormatter();
}
Appender.prototype = {
  level: Log4Moz.Level.All,

  append: function App_append(message) {
    this.doAppend(this._formatter.format(message));
  },
  toString: function App_toString() {
    return this._name + " [level=" + this._level +
      ", formatter=" + this._formatter + "]";
  },
  doAppend: function App_doAppend(message) {}
};

/*
 * DumpAppender
 * Logs to standard out
 */

function DumpAppender(formatter) {
  this._name = "DumpAppender";
  Appender.call(this, formatter);
}
DumpAppender.prototype = {
  __proto__: Appender.prototype,

  doAppend: function DApp_doAppend(message) {
    dump(message);
  }
};

/*
 * ConsoleAppender
 * Logs to the javascript console
 */

function ConsoleAppender(formatter) {
  this._name = "ConsoleAppender";
  Appender.call(this, formatter);
}
ConsoleAppender.prototype = {
  __proto__: Appender.prototype,

  doAppend: function CApp_doAppend(message) {
    if (message.level > Log4Moz.Level.Warn) {
      Cu.reportError(message);
      return;
    }
    Cc["@mozilla.org/consoleservice;1"].
      getService(Ci.nsIConsoleService).logStringMessage(message);
  }
};

/**
 * Base implementation for stream based appenders.
 *
 * Caution: This writes to the output stream synchronously, thus logging calls
 * block as the data is written to the stream. This can have negligible impact
 * for in-memory streams, but should be taken into account for I/O streams
 * (files, network, etc.)
 */
function BlockingStreamAppender(formatter) {
  this._name = "BlockingStreamAppender";
  Appender.call(this, formatter);
}
BlockingStreamAppender.prototype = {
  __proto__: Appender.prototype,

  _converterStream: null, // holds the nsIConverterOutputStream
  _outputStream: null,    // holds the underlying nsIOutputStream

  /**
   * Output stream to write to.
   *
   * This will automatically open the stream if it doesn't exist yet by
   * calling newOutputStream. The resulting raw stream is wrapped in a
   * nsIConverterOutputStream to ensure text is written as UTF-8.
   */
  get outputStream() {
    if (!this._outputStream) {
      // First create a raw stream. We can bail out early if that fails.
      this._outputStream = this.newOutputStream();
      if (!this._outputStream) {
        return null;
      }

      // Wrap the raw stream in an nsIConverterOutputStream. We can reuse
      // the instance if we already have one.
      if (!this._converterStream) {
        this._converterStream = Cc["@mozilla.org/intl/converter-output-stream;1"]
                                  .createInstance(Ci.nsIConverterOutputStream);
      }
      this._converterStream.init(
        this._outputStream, "UTF-8", STREAM_SEGMENT_SIZE,
        Ci.nsIConverterInputStream.DEFAULT_REPLACEMENT_CHARACTER);
    }
    return this._converterStream;
  },

  newOutputStream: function newOutputStream() {
    throw "Stream-based appenders need to implement newOutputStream()!";
  },

  reset: function reset() {
    if (!this._outputStream) {
      return;
    }
    this.outputStream.close();
    this._outputStream = null;
  },

  doAppend: function doAppend(message) {
    if (!message) {
      return;
    }
    try {
      this.outputStream.writeString(message);
    } catch(ex) {
      if (ex.result == Cr.NS_BASE_STREAM_CLOSED) {
        // The underlying output stream is closed, so let's open a new one
        // and try again.
        this._outputStream = null;
        try {
          this.outputStream.writeString(message);
        } catch (ex) {
          // Ah well, we tried, but something seems to be hosed permanently.
        }
      }
    }
  }
};

/**
 * Append to an nsIStorageStream
 *
 * This writes logging output to an in-memory stream which can later be read
 * back as an nsIInputStream. It can be used to avoid expensive I/O operations
 * during logging. Instead, one can periodically consume the input stream and
 * e.g. write it to disk asynchronously.
 */
function StorageStreamAppender(formatter) {
  this._name = "StorageStreamAppender";
  BlockingStreamAppender.call(this, formatter);
}
StorageStreamAppender.prototype = {
  __proto__: BlockingStreamAppender.prototype,

  _ss: null,
  newOutputStream: function newOutputStream() {
    let ss = this._ss = Cc["@mozilla.org/storagestream;1"]
                          .createInstance(Ci.nsIStorageStream);
    ss.init(STREAM_SEGMENT_SIZE, PR_UINT32_MAX, null);
    return ss.getOutputStream(0);
  },

  getInputStream: function getInputStream() {
    if (!this._ss) {
      return null;
    }
    return this._ss.newInputStream(0);
  },

  reset: function reset() {
    BlockingStreamAppender.prototype.reset.call(this);
    this._ss = null;
  }
};

/**
 * File appender (discouraged)
 *
 * Writes otuput to a file using a regular nsIFileOutputStream (as opposed
 * to nsISafeFileOutputStream, since immediate durability is typically not
 * needed for logs.) Note that I/O operations block the logging caller.
 */
function FileAppender(file, formatter) {
  this._name = "FileAppender";
  this._file = file; // nsIFile
  BlockingStreamAppender.call(this, formatter);
}
FileAppender.prototype = {
  __proto__: BlockingStreamAppender.prototype,

  newOutputStream: function newOutputStream() {
    try {
      return FileUtils.openFileOutputStream(this._file);
    } catch(e) {
      return null;
    }
  },

  reset: function reset() {
    BlockingStreamAppender.prototype.reset.call(this);
    try {
      this._file.remove(false);
    } catch (e) {
      // File didn't exist in the first place, or we're on Windows. Meh.
    }
  }
};

/**
 * Rotating file appender (discouraged)
 *
 * Similar to FileAppender, but rotates logs when they become too large.
 */
function RotatingFileAppender(file, formatter, maxSize, maxBackups) {
  if (maxSize === undefined)
    maxSize = ONE_MEGABYTE * 2;

  if (maxBackups === undefined)
    maxBackups = 0;

  this._name = "RotatingFileAppender";
  FileAppender.call(this, file, formatter);
  this._maxSize = maxSize;
  this._maxBackups = maxBackups;
}
RotatingFileAppender.prototype = {
  __proto__: FileAppender.prototype,

  doAppend: function doAppend(message) {
    FileAppender.prototype.doAppend.call(this, message);
    try {
      this.rotateLogs();
    } catch(e) {
      dump("Error writing file:" + e + "\n");
    }
  },

  rotateLogs: function rotateLogs() {
    if (this._file.exists() && this._file.fileSize < this._maxSize) {
      return;
    }

    BlockingStreamAppender.prototype.reset.call(this);

    for (let i = this.maxBackups - 1; i > 0; i--) {
      let backup = this._file.parent.clone();
      backup.append(this._file.leafName + "." + i);
      if (backup.exists()) {
        backup.moveTo(this._file.parent, this._file.leafName + "." + (i + 1));
      }
    }

    let cur = this._file.clone();
    if (cur.exists()) {
      cur.moveTo(cur.parent, cur.leafName + ".1");
    }

    // Note: this._file still points to the same file
  }
};
