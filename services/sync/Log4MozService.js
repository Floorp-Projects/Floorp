/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1
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
 * The Original Code is log4moz
 *
 * The Initial Developer of the Original Code is
 * Michael Johnston
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Michael Johnston <special.michael@gmail.com>
 * Dan Mills <thunder@mozilla.com>
 *
 * ***** END LICENSE BLOCK ***** */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

const MODE_RDONLY   = 0x01;
const MODE_WRONLY   = 0x02;
const MODE_CREATE   = 0x08;
const MODE_APPEND   = 0x10;
const MODE_TRUNCATE = 0x20;

const PERMS_FILE      = 0644;
const PERMS_DIRECTORY = 0755;

// Note: these are also available in the idl
const LEVEL_FATAL  = 70;
const LEVEL_ERROR  = 60;
const LEVEL_WARN   = 50;
const LEVEL_INFO   = 40;
const LEVEL_CONFIG = 30;
const LEVEL_DEBUG  = 20;
const LEVEL_TRACE  = 10;
const LEVEL_ALL    = 0;

const LEVEL_DESC = {
  70: "FATAL ",
  60: "ERROR ",
  50: "WARN  ",
  40: "INFO  ",
  30: "CONFIG",
  20: "DEBUG ",
  10: "TRACE ",
  0:  "ALL   "
};

const ONE_BYTE = 1;
const ONE_KILOBYTE = 1024 * ONE_BYTE;
const ONE_MEGABYTE = 1024 * ONE_KILOBYTE;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

/*
 * LoggingService
 */

function Log4MozService() {
  this._repository = new LoggerRepository();
}
Log4MozService.prototype = {
  classDescription: "Log4moz Logging Service",
  contractID: "@mozilla.org/log4moz/service;1",
  classID: Components.ID("{a60e50d7-90b8-4a12-ad0c-79e6a1896978}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.ILog4MozService,
                                         Ci.nsISupports]),

  get rootLogger() {
    return this._repository.rootLogger;
  },

  getLogger: function LogSvc_getLogger(name) {
    return this._repository.getLogger(name);
  },

  newAppender: function LogSvc_newAppender(kind, formatter) {
    switch (kind) {
    case "dump":
      return new DumpAppender(formatter);
    case "console":
      return new ConsoleAppender(formatter);
    default:
      dump("log4moz: unknown appender kind: " + kind);
      return;
    }
  },

  newFileAppender: function LogSvc_newAppender(kind, file, formatter) {
    switch (kind) {
    case "file":
      return new FileAppender(file, formatter);
    case "rotating":
      // FIXME: hardcoded constants
      return new RotatingFileAppender(file, formatter, ONE_MEGABYTE * 5, 0);
    default:
      dump("log4moz: unknown appender kind: " + kind);
      return;
    }
  },

  newFormatter: function LogSvc_newFormatter(kind) {
    switch (kind) {
    case "basic":
      return new BasicFormatter();
    default:
      dump("log4moz: unknown formatter kind: " + kind);
      return;
    }
  }
};

function NSGetModule(compMgr, fileSpec) {
  return XPCOMUtils.generateModule([Log4MozService]);
}

/*
 * LoggerRepository
 * Implements a hierarchy of Loggers
 */

function LoggerRepository() {}
LoggerRepository.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.ILoggerRepository, Ci.nsISupports]),

  _loggers: {},

  _rootLogger: null,
  get rootLogger() {
    if (!this._rootLogger) {
      this._rootLogger = new Logger("root", this);
      this._rootLogger.level = LEVEL_ALL;
    }
    return this._rootLogger;
  },
  // FIXME: need to update all parent values if we do this
  //set rootLogger(logger) {
  //  this._rootLogger = logger;
  //},

  _updateParents: function LogRep__updateParents(name) {
    let pieces = name.split('.');
    let cur, parent;

    // find the closest parent
    for (let i = 0; i < pieces.length; i++) {
      if (cur)
        cur += '.' + pieces[i];
      else
        cur = pieces[i];
      if (cur in this._loggers)
        parent = cur;
    }

    // if they are the same it has no parent
    if (parent == name)
      this._loggers[name].parent = this.rootLogger;
    else
      this._loggers[name].parent = this._loggers[parent];

    // trigger updates for any possible descendants of this logger
    for (let logger in this._loggers) {
      if (logger != name && name.indexOf(logger) == 0)
        this._updateParents(logger);
    }
  },

  getLogger: function LogRep_getLogger(name) {
    if (!(name in this._loggers)) {
      this._loggers[name] = new Logger(name, this);
      this._updateParents(name);
    }
    return this._loggers[name];
  }
}

/*
 * Logger
 * Hierarchical version.  Logs to all appenders, assigned or inherited
 */

function Logger(name, repository) {
  this._name = name;
  this._repository = repository;
  this._appenders = [];
}
Logger.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.ILogger, Ci.nsISupports]),

  parent: null,

  _level: -1,
  get level() {
    if (this._level >= 0)
      return this._level;
    if (this.parent)
      return this.parent.level;
    dump("log4moz warning: root logger configuration error: no level defined\n");
    return LEVEL_ALL;
  },
  set level(level) {
    this._level = level;
  },

  _appenders: null,
  get appenders() {
    if (!this.parent)
      return this._appenders;
    return this._appenders.concat(this.parent.appenders);
  },

  addAppender: function Logger_addAppender(appender) {
    for (let i = 0; i < this._appenders.length; i++) {
      if (this._appenders[i] == appender)
        return;
    }
    this._appenders.push(appender);
  },

  log: function Logger_log(message) {
    if (this.level > message.level)
      return;
    let appenders = this.appenders;
    for (let i = 0; i < appenders.length; i++){
      appenders[i].append(message);
    }
  },

  fatal: function Logger_fatal(string) {
    this.log(new LogMessage(LEVEL_FATAL, string));
  },
  error: function Logger_error(string) {
    this.log(new LogMessage(LEVEL_ERROR, string));
  },
  warning: function Logger_warning(string) {
    this.log(new LogMessage(LEVEL_WARN, string));
  },
  info: function Logger_info(string) {
    this.log(new LogMessage(LEVEL_INFO, string));
  },
  config: function Logger_config(string) {
    this.log(new LogMessage(LEVEL_CONFIG, string));
  },
  debug: function Logger_debug(string) {
    this.log(new LogMessage(LEVEL_DEBUG, string));
  },
  trace: function Logger_trace(string) {
    this.log(new LogMessage(LEVEL_TRACE, string));
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
  QueryInterface: XPCOMUtils.generateQI([Ci.IAppender, Ci.nsISupports]),

  _level: LEVEL_ALL,
  get level() { return this._level; },
  set level(level) { this._level = level; },

  append: function App_append(message) {
    if(this._level <= message.level)
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
  this._formatter = formatter;
  this._level = LEVEL_ALL;
}
DumpAppender.prototype = new Appender();
DumpAppender.prototype.doAppend = function DApp_doAppend(message) {
  dump(message);
};

/*
 * ConsoleAppender
 * Logs to the javascript console
 */

function ConsoleAppender(formatter) {
  this._name = "ConsoleAppender";
  this._formatter = formatter;
  this._level = LEVEL_ALL;
}
ConsoleAppender.prototype = new Appender();
ConsoleAppender.prototype.doAppend = function CApp_doAppend(message) {
  //error or normal?    
  if(message.level > LEVEL_WARN){
    Cu.reportError(message);
  } else {
    //get the js console
    var consoleService = Components.classes["@mozilla.org/consoleservice;1"].
    getService(Ci.nsIConsoleService);
    consoleService.logStringMessage(message);
  }
};

/*
 * FileAppender
 * Logs to a file
 */

function FileAppender(file, formatter) {
  this._name = "FileAppender";
  this._file = file; // nsIFile
  this._formatter = formatter;
  this._level = LEVEL_ALL;
  this.__fos = null;
}
FileAppender.prototype = new Appender();
FileAppender.prototype._fos = function FApp__fos_get() {
  if (!this.__fos)
    this.openStream();
  return this.__fos;
};
FileAppender.prototype.openStream = function FApp_openStream() {
  this.__fos = Cc["@mozilla.org/network/file-output-stream;1"].
    createInstance(Ci.nsIFileOutputStream);
  let flags = MODE_WRONLY | MODE_CREATE | MODE_APPEND;
  this.__fos.init(this._file, flags, PERMS_FILE, 0);
};
FileAppender.prototype.closeStream = function FApp_closeStream() {
  if (!this.__fos)
    return;
  try {
    this.__fos.close();
    this.__fos = null;
  } catch(e) {
    dump("Failed to close file output stream\n" + e);
  }
};
FileAppender.prototype.doAppend = function FApp_doAppend(message) {
  if (message === null || message.length <= 0)
    return;
  try {
    this._fos().write(message, message.length);
  } catch(e) {
    dump("Error writing file:\n" + e);
  }
};

/*
 * RotatingFileAppender
 * Similar to FileAppender, but rotates logs when they become too large
 */

function RotatingFileAppender(file, formatter, maxSize, maxBackups) {
  this._name = "RotatingFileAppender";
  this._file = file; // nsIFile
  this._formatter = formatter;
  this._level = LEVEL_ALL;
  this._maxSize = maxSize;
  this._maxBackups = maxBackups;
}
RotatingFileAppender.prototype = new FileAppender();
RotatingFileAppender.prototype.doAppend = function RFApp_doAppend(message) {
  if (message === null || message.length <= 0)
    return;
  try {
    this.rotateLogs();
    this._fos().write(message, message.length);
  } catch(e) {
    dump("Error writing file:\n" + e);
  }
};
RotatingFileAppender.prototype.rotateLogs = function RFApp_rotateLogs() {
  if(this._file.exists() &&
     this._file.fileSize < this._maxSize)
    return;

  this.closeStream();

  for (let i = this.maxBackups - 1; i > 0; i--){
    let backup = this._file.parent.clone();
    backup.append(this._file.leafName + "." + i);
    if (backup.exists())
      backup.moveTo(this._file.parent, this._file.leafName + "." + (i + 1));
  }

  let cur = this._file.clone();
  if (cur.exists())
    cur.moveTo(cur.parent, cur.leafName + ".1");

  // this._file still points to the same file
};

/*
 * Formatters
 * These massage a LogMessage into whatever output is desired
 * Only the BasicFormatter is currently implemented
 */

// FIXME: should allow for formatting the whole string, not just the date
function BasicFormatter(dateFormat) {
  if (dateFormat)
    this.dateFormat = dateFormat;
}
BasicFormatter.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.IFormatter, Ci.nsISupports]),

  _dateFormat: null,

  get dateFormat() {
    if (!this._dateFormat)
      this._dateFormat = "%Y-%m-%d %H:%M:%S";
    return this._dateFormat;
  },
  set dateFormat(format) {
    this._dateFormat = format;
  },
  format: function BF_format(message) {
    // FIXME: message.date.toLocaleFormat(this.dateFormat) + " " +
           
    return message.levelDesc + " " + message.message + "\n";
  }
};

/*
 * LogMessage
 * Encapsulates a single log event's data
 */
function LogMessage(level, message){
  this.message = message;
  this.level = level;
  this.date = new Date();
}
LogMessage.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.ILogMessage, Ci.nsISupports]),

  get levelDesc() {
    if (this.level in LEVEL_DESC)
      return LEVEL_DESC[this.level];
    return "UNKNOWN";
  },
  toString: function LogMsg_toString(){
    return "LogMessage [" + this.date + " " + this.level + " " +
      this.message + "]";
  }
};
