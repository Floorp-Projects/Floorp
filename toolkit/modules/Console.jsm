/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Define a 'console' API to roughly match the implementation provided by
 * Firebug.
 * This module helps cases where code is shared between the web and Firefox.
 * See also Browser.jsm for an implementation of other web constants to help
 * sharing code between the web and firefox;
 *
 * The API is only be a rough approximation for 3 reasons:
 * - The Firebug console API is implemented in many places with differences in
 *   the implementations, so there isn't a single reference to adhere to
 * - The Firebug console is a rich display compared with dump(), so there will
 *   be many things that we can't replicate
 * - The primary use of this API is debugging and error logging so the perfect
 *   implementation isn't always required (or even well defined)
 */

this.EXPORTED_SYMBOLS = [ "console", "ConsoleAPI" ];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

var gTimerRegistry = new Map();

/**
 * String utility to ensure that strings are a specified length. Strings
 * that are too long are truncated to the max length and the last char is
 * set to "_". Strings that are too short are padded with spaces.
 *
 * @param {string} aStr
 *        The string to format to the correct length
 * @param {number} aMaxLen
 *        The maximum allowed length of the returned string
 * @param {number} aMinLen (optional)
 *        The minimum allowed length of the returned string. If undefined,
 *        then aMaxLen will be used
 * @param {object} aOptions (optional)
 *        An object allowing format customization. Allowed customizations:
 *          'truncate' - can take the value "start" to truncate strings from
 *             the start as opposed to the end or "center" to truncate
 *             strings in the center.
 *          'align' - takes an alignment when padding is needed for MinLen,
 *             either "start" or "end".  Defaults to "start".
 * @return {string}
 *        The original string formatted to fit the specified lengths
 */
function fmt(aStr, aMaxLen, aMinLen, aOptions) {
  if (aMinLen == null) {
    aMinLen = aMaxLen;
  }
  if (aStr == null) {
    aStr = "";
  }
  if (aStr.length > aMaxLen) {
    if (aOptions && aOptions.truncate == "start") {
      return "_" + aStr.substring(aStr.length - aMaxLen + 1);
    }
    else if (aOptions && aOptions.truncate == "center") {
      let start = aStr.substring(0, (aMaxLen / 2));

      let end = aStr.substring((aStr.length - (aMaxLen / 2)) + 1);
      return start + "_" + end;
    }
    return aStr.substring(0, aMaxLen - 1) + "_";
  }
  if (aStr.length < aMinLen) {
    let padding = Array(aMinLen - aStr.length + 1).join(" ");
    aStr = (aOptions.align === "end") ? padding + aStr : aStr + padding;
  }
  return aStr;
}

/**
 * Utility to extract the constructor name of an object.
 * Object.toString gives: "[object ?????]"; we want the "?????".
 *
 * @param {object} aObj
 *        The object from which to extract the constructor name
 * @return {string}
 *        The constructor name
 */
function getCtorName(aObj) {
  if (aObj === null) {
    return "null";
  }
  if (aObj === undefined) {
    return "undefined";
  }
  if (aObj.constructor && aObj.constructor.name) {
    return aObj.constructor.name;
  }
  // If that fails, use Objects toString which sometimes gives something
  // better than 'Object', and at least defaults to Object if nothing better
  return Object.prototype.toString.call(aObj).slice(8, -1);
}

/**
 * Indicates whether an object is a JS or `Components.Exception` error.
 *
 * @param {object} aThing
          The object to check
 * @return {boolean}
          Is this object an error?
 */
function isError(aThing) {
  return aThing && (
           (typeof aThing.name == "string" &&
            aThing.name.startsWith("NS_ERROR_")) ||
           getCtorName(aThing).endsWith("Error"));
}

/**
 * A single line stringification of an object designed for use by humans
 *
 * @param {any} aThing
 *        The object to be stringified
 * @param {boolean} aAllowNewLines
 * @return {string}
 *        A single line representation of aThing, which will generally be at
 *        most 80 chars long
 */
function stringify(aThing, aAllowNewLines) {
  if (aThing === undefined) {
    return "undefined";
  }

  if (aThing === null) {
    return "null";
  }

  if (isError(aThing)) {
    return "Message: " + aThing;
  }

  if (typeof aThing == "object") {
    let type = getCtorName(aThing);
    if (aThing instanceof Components.interfaces.nsIDOMNode && aThing.tagName) {
      return debugElement(aThing);
    }
    type = (type == "Object" ? "" : type + " ");
    let json;
    try {
      json = JSON.stringify(aThing);
    }
    catch (ex) {
      // Can't use a real ellipsis here, because cmd.exe isn't unicode-enabled
      json = "{" + Object.keys(aThing).join(":..,") + ":.., " + "}";
    }
    return type + json;
  }

  if (typeof aThing == "function") {
    return aThing.toString().replace(/\s+/g, " ");
  }

  let str = aThing.toString();
  if (!aAllowNewLines) {
    str = str.replace(/\n/g, "|");
  }
  return str;
}

/**
 * Create a simple debug representation of a given element.
 *
 * @param {nsIDOMElement} aElement
 *        The element to debug
 * @return {string}
 *        A simple single line representation of aElement
 */
function debugElement(aElement) {
  return "<" + aElement.tagName +
      (aElement.id ? "#" + aElement.id : "") +
      (aElement.className && aElement.className.split ?
          "." + aElement.className.split(" ").join(" .") :
          "") +
      ">";
}

/**
 * A multi line stringification of an object, designed for use by humans
 *
 * @param {any} aThing
 *        The object to be stringified
 * @return {string}
 *        A multi line representation of aThing
 */
function log(aThing) {
  if (aThing === null) {
    return "null\n";
  }

  if (aThing === undefined) {
    return "undefined\n";
  }

  if (typeof aThing == "object") {
    let reply = "";
    let type = getCtorName(aThing);
    if (type == "Map") {
      reply += "Map\n";
      for (let [key, value] of aThing) {
        reply += logProperty(key, value);
      }
    }
    else if (type == "Set") {
      let i = 0;
      reply += "Set\n";
      for (let value of aThing) {
        reply += logProperty('' + i, value);
        i++;
      }
    }
    else if (isError(aThing)) {
      reply += "  Message: " + aThing + "\n";
      if (aThing.stack) {
        reply += "  Stack:\n";
        var frame = aThing.stack;
        while (frame) {
          reply += "    " + frame + "\n";
          frame = frame.caller;
        }
      }
    }
    else if (aThing instanceof Components.interfaces.nsIDOMNode && aThing.tagName) {
      reply += "  " + debugElement(aThing) + "\n";
    }
    else {
      let keys = Object.getOwnPropertyNames(aThing);
      if (keys.length > 0) {
        reply += type + "\n";
        keys.forEach(function(aProp) {
          reply += logProperty(aProp, aThing[aProp]);
        });
      }
      else {
        reply += type + "\n";
        let root = aThing;
        let logged = [];
        while (root != null) {
          let properties = Object.keys(root);
          properties.sort();
          properties.forEach(function(property) {
            if (!(property in logged)) {
              logged[property] = property;
              reply += logProperty(property, aThing[property]);
            }
          });

          root = Object.getPrototypeOf(root);
          if (root != null) {
            reply += '  - prototype ' + getCtorName(root) + '\n';
          }
        }
      }
    }

    return reply;
  }

  return "  " + aThing.toString() + "\n";
}

/**
 * Helper for log() which converts a property/value pair into an output
 * string
 *
 * @param {string} aProp
 *        The name of the property to include in the output string
 * @param {object} aValue
 *        Value assigned to aProp to be converted to a single line string
 * @return {string}
 *        Multi line output string describing the property/value pair
 */
function logProperty(aProp, aValue) {
  let reply = "";
  if (aProp == "stack" && typeof value == "string") {
    let trace = parseStack(aValue);
    reply += formatTrace(trace);
  }
  else {
    reply += "    - " + aProp + " = " + stringify(aValue) + "\n";
  }
  return reply;
}

const LOG_LEVELS = {
  "all": Number.MIN_VALUE,
  "debug": 2,
  "log": 3,
  "info": 3,
  "clear": 3,
  "trace": 3,
  "timeEnd": 3,
  "time": 3,
  "group": 3,
  "groupEnd": 3,
  "profile": 3,
  "profileEnd": 3,
  "dir": 3,
  "dirxml": 3,
  "warn": 4,
  "error": 5,
  "off": Number.MAX_VALUE,
};

/**
 * Helper to tell if a console message of `aLevel` type
 * should be logged in stdout and sent to consoles given
 * the current maximum log level being defined in `console.maxLogLevel`
 *
 * @param {string} aLevel
 *        Console message log level
 * @param {string} aMaxLevel {string}
 *        String identifier (See LOG_LEVELS for possible
 *        values) that allows to filter which messages
 *        are logged based on their log level
 * @return {boolean}
 *        Should this message be logged or not?
 */
function shouldLog(aLevel, aMaxLevel) {
  return LOG_LEVELS[aMaxLevel] <= LOG_LEVELS[aLevel];
}

/**
 * Parse a stack trace, returning an array of stack frame objects, where
 * each has filename/lineNumber/functionName members
 *
 * @param {string} aStack
 *        The serialized stack trace
 * @return {object[]}
 *        Array of { file: "...", line: NNN, call: "..." } objects
 */
function parseStack(aStack) {
  let trace = [];
  aStack.split("\n").forEach(function(line) {
    if (!line) {
      return;
    }
    let at = line.lastIndexOf("@");
    let posn = line.substring(at + 1);
    trace.push({
      filename: posn.split(":")[0],
      lineNumber: posn.split(":")[1],
      functionName: line.substring(0, at)
    });
  });
  return trace;
}

/**
 * Format a frame coming from Components.stack such that it can be used by the
 * Browser Console, via console-api-log-event notifications.
 *
 * @param {object} aFrame
 *        The stack frame from which to begin the walk.
 * @param {number=0} aMaxDepth
 *        Maximum stack trace depth. Default is 0 - no depth limit.
 * @return {object[]}
 *         An array of {filename, lineNumber, functionName, language} objects.
 *         These objects follow the same format as other console-api-log-event
 *         messages.
 */
function getStack(aFrame, aMaxDepth = 0) {
  if (!aFrame) {
    aFrame = Components.stack.caller;
  }
  let trace = [];
  while (aFrame) {
    trace.push({
      filename: aFrame.filename,
      lineNumber: aFrame.lineNumber,
      functionName: aFrame.name,
      language: aFrame.language,
    });
    if (aMaxDepth == trace.length) {
      break;
    }
    aFrame = aFrame.caller;
  }
  return trace;
}

/**
 * Take the output from parseStack() and convert it to nice readable
 * output
 *
 * @param {object[]} aTrace
 *        Array of trace objects as created by parseStack()
 * @return {string} Multi line report of the stack trace
 */
function formatTrace(aTrace) {
  let reply = "";
  aTrace.forEach(function(frame) {
    reply += fmt(frame.filename, 20, 20, { truncate: "start" }) + " " +
             fmt(frame.lineNumber, 5, 5) + " " +
             fmt(frame.functionName, 75, 0, { truncate: "center" }) + "\n";
  });
  return reply;
}

/**
 * Create a new timer by recording the current time under the specified name.
 *
 * @param {string} aName
 *        The name of the timer.
 * @param {number} [aTimestamp=Date.now()]
 *        Optional timestamp that tells when the timer was originally started.
 * @return {object}
 *         The name property holds the timer name and the started property
 *         holds the time the timer was started. In case of error, it returns
 *         an object with the single property "error" that contains the key
 *         for retrieving the localized error message.
 */
function startTimer(aName, aTimestamp) {
  let key = aName.toString();
  if (!gTimerRegistry.has(key)) {
    gTimerRegistry.set(key, aTimestamp || Date.now());
  }
  return { name: aName, started: gTimerRegistry.get(key) };
}

/**
 * Stop the timer with the specified name and retrieve the elapsed time.
 *
 * @param {string} aName
 *        The name of the timer.
 * @param {number} [aTimestamp=Date.now()]
 *        Optional timestamp that tells when the timer was originally stopped.
 * @return {object}
 *         The name property holds the timer name and the duration property
 *         holds the number of milliseconds since the timer was started.
 */
function stopTimer(aName, aTimestamp) {
  let key = aName.toString();
  let duration = (aTimestamp || Date.now()) - gTimerRegistry.get(key);
  gTimerRegistry.delete(key);
  return { name: aName, duration: duration };
}

/**
 * Dump a new message header to stdout by taking care of adding an eventual
 * prefix
 *
 * @param {object} aConsole
 *        ConsoleAPI instance
 * @param {string} aLevel
 *        The string identifier for the message log level
 * @param {string} aMessage
 *        The string message to print to stdout
 */
function dumpMessage(aConsole, aLevel, aMessage) {
  aConsole.dump(
    "console." + aLevel + ": " +
    (aConsole.prefix ? aConsole.prefix + ": " : "")  +
    aMessage + "\n"
  );
}

/**
 * Create a function which will output a concise level of output when used
 * as a logging function
 *
 * @param {string} aLevel
 *        A prefix to all output generated from this function detailing the
 *        level at which output occurred
 * @return {function}
 *        A logging function
 * @see createMultiLineDumper()
 */
function createDumper(aLevel) {
  return function() {
    if (!shouldLog(aLevel, this.maxLogLevel)) {
      return;
    }
    let args = Array.prototype.slice.call(arguments, 0);
    let frame = getStack(Components.stack.caller, 1)[0];
    sendConsoleAPIMessage(this, aLevel, frame, args);
    let data = args.map(function(arg) {
      return stringify(arg, true);
    });
    dumpMessage(this, aLevel, data.join(" "));
  };
}

/**
 * Create a function which will output more detailed level of output when
 * used as a logging function
 *
 * @param {string} aLevel
 *        A prefix to all output generated from this function detailing the
 *        level at which output occurred
 * @return {function}
 *        A logging function
 * @see createDumper()
 */
function createMultiLineDumper(aLevel) {
  return function() {
    if (!shouldLog(aLevel, this.maxLogLevel)) {
      return;
    }
    dumpMessage(this, aLevel, "");
    let args = Array.prototype.slice.call(arguments, 0);
    let frame = getStack(Components.stack.caller, 1)[0];
    sendConsoleAPIMessage(this, aLevel, frame, args);
    args.forEach(function(arg) {
      this.dump(log(arg));
    }, this);
  };
}

/**
 * Send a Console API message. This function will send a console-api-log-event
 * notification through the nsIObserverService.
 *
 * @param {object} aConsole
 *        The instance of ConsoleAPI performing the logging.
 * @param {string} aLevel
 *        Message severity level. This is usually the name of the console method
 *        that was called.
 * @param {object} aFrame
 *        The youngest stack frame coming from Components.stack, as formatted by
 *        getStack().
 * @param {array} aArgs
 *        The arguments given to the console method.
 * @param {object} aOptions
 *        Object properties depend on the console method that was invoked:
 *        - timer: for time() and timeEnd(). Holds the timer information.
 *        - groupName: for group(), groupCollapsed() and groupEnd().
 *        - stacktrace: for trace(). Holds the array of stack frames as given by
 *        getStack().
 */
function sendConsoleAPIMessage(aConsole, aLevel, aFrame, aArgs, aOptions = {})
{
  let consoleEvent = {
    ID: "jsm",
    innerID: aConsole.innerID || aFrame.filename,
    consoleID: aConsole.consoleID,
    level: aLevel,
    filename: aFrame.filename,
    lineNumber: aFrame.lineNumber,
    functionName: aFrame.functionName,
    timeStamp: Date.now(),
    arguments: aArgs,
    prefix: aConsole.prefix,
  };

  consoleEvent.wrappedJSObject = consoleEvent;

  switch (aLevel) {
    case "trace":
      consoleEvent.stacktrace = aOptions.stacktrace;
      break;
    case "time":
    case "timeEnd":
      consoleEvent.timer = aOptions.timer;
      break;
    case "group":
    case "groupCollapsed":
    case "groupEnd":
      try {
        consoleEvent.groupName = Array.prototype.join.call(aArgs, " ");
      }
      catch (ex) {
        Cu.reportError(ex);
        Cu.reportError(ex.stack);
        return;
      }
      break;
  }

  let ConsoleAPIStorage = Cc["@mozilla.org/consoleAPI-storage;1"]
                            .getService(Ci.nsIConsoleAPIStorage);
  if (ConsoleAPIStorage) {
    ConsoleAPIStorage.recordEvent("jsm", null, consoleEvent);
  }
}

/**
 * This creates a console object that somewhat replicates Firebug's console
 * object
 *
 * @param {object} aConsoleOptions
 *        Optional dictionary with a set of runtime console options:
 *        - prefix {string} : An optional prefix string to be printed before
 *                            the actual logged message
 *        - maxLogLevel {string} : String identifier (See LOG_LEVELS for
 *                            possible values) that allows to filter which
 *                            messages are logged based on their log level.
 *                            If falsy value, all messages will be logged.
 *                            If wrong value that doesn't match any key of
 *                            LOG_LEVELS, no message will be logged
 *        - maxLogLevelPref {string} : String pref name which contains the
 *                            level to use for maxLogLevel. If the pref doesn't
 *                            exist or gets removed, the maxLogLevel will default
 *                            to the value passed to this constructor (or "all"
 *                            if it wasn't specified).
 *        - dump {function} : An optional function to intercept all strings
 *                            written to stdout
 *        - innerID {string}: An ID representing the source of the message.
 *                            Normally the inner ID of a DOM window.
 *        - consoleID {string} : String identified for the console, this will
 *                            be passed through the console notifications
 * @return {object}
 *        A console API instance object
 */
function ConsoleAPI(aConsoleOptions = {}) {
  // Normalize console options to set default values
  // in order to avoid runtime checks on each console method call.
  this.dump = aConsoleOptions.dump || dump;
  this.prefix = aConsoleOptions.prefix || "";
  this.maxLogLevel = aConsoleOptions.maxLogLevel;
  this.innerID = aConsoleOptions.innerID || null;
  this.consoleID = aConsoleOptions.consoleID || "";

  // Setup maxLogLevelPref watching
  let updateMaxLogLevel = () => {
    if (Services.prefs.getPrefType(aConsoleOptions.maxLogLevelPref) == Services.prefs.PREF_STRING) {
      this._maxLogLevel = Services.prefs.getCharPref(aConsoleOptions.maxLogLevelPref).toLowerCase();
    } else {
      this._maxLogLevel = this._maxExplicitLogLevel;
    }
  };

  if (aConsoleOptions.maxLogLevelPref) {
    updateMaxLogLevel();
    Services.prefs.addObserver(aConsoleOptions.maxLogLevelPref, updateMaxLogLevel, false);
  }

  // Bind all the functions to this object.
  for (let prop in this) {
    if (typeof(this[prop]) === "function") {
      this[prop] = this[prop].bind(this);
    }
  }
}

ConsoleAPI.prototype = {
  /**
   * The last log level that was specified via the constructor or setter. This
   * is used as a fallback if the pref doesn't exist or is removed.
   */
  _maxExplicitLogLevel: null,
  /**
   * The current log level via all methods of setting (pref or via the API).
   */
  _maxLogLevel: null,
  debug: createMultiLineDumper("debug"),
  log: createDumper("log"),
  info: createDumper("info"),
  warn: createDumper("warn"),
  error: createMultiLineDumper("error"),
  exception: createMultiLineDumper("error"),

  trace: function Console_trace() {
    if (!shouldLog("trace", this.maxLogLevel)) {
      return;
    }
    let args = Array.prototype.slice.call(arguments, 0);
    let trace = getStack(Components.stack.caller);
    sendConsoleAPIMessage(this, "trace", trace[0], args,
                          { stacktrace: trace });
    dumpMessage(this, "trace", "\n" + formatTrace(trace));
  },
  clear: function Console_clear() {},

  dir: createMultiLineDumper("dir"),
  dirxml: createMultiLineDumper("dirxml"),
  group: createDumper("group"),
  groupEnd: createDumper("groupEnd"),

  time: function Console_time() {
    if (!shouldLog("time", this.maxLogLevel)) {
      return;
    }
    let args = Array.prototype.slice.call(arguments, 0);
    let frame = getStack(Components.stack.caller, 1)[0];
    let timer = startTimer(args[0]);
    sendConsoleAPIMessage(this, "time", frame, args, { timer: timer });
    dumpMessage(this, "time",
                "'" + timer.name + "' @ " + (new Date()));
  },

  timeEnd: function Console_timeEnd() {
    if (!shouldLog("timeEnd", this.maxLogLevel)) {
      return;
    }
    let args = Array.prototype.slice.call(arguments, 0);
    let frame = getStack(Components.stack.caller, 1)[0];
    let timer = stopTimer(args[0]);
    sendConsoleAPIMessage(this, "timeEnd", frame, args, { timer: timer });
    dumpMessage(this, "timeEnd",
                "'" + timer.name + "' " + timer.duration + "ms");
  },

  profile(profileName) {
    if (!shouldLog("profile", this.maxLogLevel)) {
      return;
    }
    Services.obs.notifyObservers({
      wrappedJSObject: {
        action: "profile",
        arguments: [ profileName ]
      }
    }, "console-api-profiler", null);
    dumpMessage(this, "profile", `'${profileName}'`);
  },

  profileEnd(profileName) {
    if (!shouldLog("profileEnd", this.maxLogLevel)) {
      return;
    }
    Services.obs.notifyObservers({
      wrappedJSObject: {
        action: "profileEnd",
        arguments: [ profileName ]
      }
    }, "console-api-profiler", null);
    dumpMessage(this, "profileEnd", `'${profileName}'`);
  },

  get maxLogLevel() {
    return this._maxLogLevel || "all";
  },

  set maxLogLevel(aValue) {
    this._maxLogLevel = this._maxExplicitLogLevel = aValue;
  },
};

this.console = new ConsoleAPI();
this.ConsoleAPI = ConsoleAPI;
