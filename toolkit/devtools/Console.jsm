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

this.EXPORTED_SYMBOLS = [ "console" ];

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

let gTimerRegistry = new Map();

/**
 * String utility to ensure that strings are a specified length. Strings
 * that are too long are truncated to the max length and the last char is
 * set to "_". Strings that are too short are left padded with spaces.
 *
 * @param {string} aStr
 *        The string to format to the correct length
 * @param {number} aMaxLen
 *        The maximum allowed length of the returned string
 * @param {number} aMinLen (optional)
 *        The minimum allowed length of the returned string. If undefined,
 *        then aMaxLen will be used
 * @param {object} aOptions (optional)
 *        An object allowing format customization. The only customization
 *        allowed currently is 'truncate' which can take the value "start" to
 *        truncate strings from the start as opposed to the end.
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
    else {
      return aStr.substring(0, aMaxLen - 1) + "_";
    }
  }
  if (aStr.length < aMinLen) {
    return Array(aMinLen - aStr.length + 1).join(" ") + aStr;
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
 * A single line stringification of an object designed for use by humans
 *
 * @param {any} aThing
 *        The object to be stringified
 * @return {string}
 *        A single line representation of aThing, which will generally be at
 *        most 80 chars long
 */
function stringify(aThing) {
  if (aThing === undefined) {
    return "undefined";
  }

  if (aThing === null) {
    return "null";
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
    return type + fmt(json, 50, 0);
  }

  if (typeof aThing == "function") {
    return fmt(aThing.toString().replace(/\s+/g, " "), 80, 0);
  }

  let str = aThing.toString().replace(/\n/g, "|");
  return fmt(str, 80, 0);
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
      (aElement.className ?
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
    else if (type == "Error") {
      reply += "  " + aThing.message + "\n";
      reply += logProperty("stack", aThing.stack);
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
             fmt(frame.functionName, 75, 75) + "\n";
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
    let args = Array.prototype.slice.call(arguments, 0);
    let frame = getStack(Components.stack.caller, 1)[0];
    sendConsoleAPIMessage(aLevel, frame, args);
    let data = args.map(function(arg) {
      return stringify(arg);
    });
    dump("console." + aLevel + ": " + data.join(", ") + "\n");
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
    dump("console." + aLevel + ": \n");
    let args = Array.prototype.slice.call(arguments, 0);
    let frame = getStack(Components.stack.caller, 1)[0];
    sendConsoleAPIMessage(aLevel, frame, args);
    args.forEach(function(arg) {
      dump(log(arg));
    });
  };
}

/**
 * Send a Console API message. This function will send a console-api-log-event
 * notification through the nsIObserverService.
 *
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
function sendConsoleAPIMessage(aLevel, aFrame, aArgs, aOptions = {})
{
  let consoleEvent = {
    ID: aFrame.filename,
    level: aLevel,
    filename: aFrame.filename,
    lineNumber: aFrame.lineNumber,
    functionName: aFrame.functionName,
    timeStamp: Date.now(),
    arguments: aArgs,
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

  Services.obs.notifyObservers(consoleEvent, "console-api-log-event", null);
}

/**
 * This creates a console object that somewhat replicates Firebug's console
 * object. It currently writes to dump(), but should write to the web
 * console's chrome error section (when it has one)
 */
this.console = {
  debug: createMultiLineDumper("debug"),
  log: createDumper("log"),
  info: createDumper("info"),
  warn: createDumper("warn"),
  error: createMultiLineDumper("error"),

  trace: function Console_trace() {
    let args = Array.prototype.slice.call(arguments, 0);
    let trace = getStack(Components.stack.caller);
    sendConsoleAPIMessage("trace", trace[0], args,
                          { stacktrace: trace });
    dump("console.trace:\n" + formatTrace(trace) + "\n");
  },
  clear: function Console_clear() {},

  dir: createMultiLineDumper("dir"),
  dirxml: createMultiLineDumper("dirxml"),
  group: createDumper("group"),
  groupEnd: createDumper("groupEnd"),

  time: function Console_time() {
    let args = Array.prototype.slice.call(arguments, 0);
    let frame = getStack(Components.stack.caller, 1)[0];
    let timer = startTimer(args[0]);
    sendConsoleAPIMessage("time", frame, args, { timer: timer });
    dump("console.time: '" + timer.name + "' @ " + (new Date()) + "\n");
  },

  timeEnd: function Console_timeEnd() {
    let args = Array.prototype.slice.call(arguments, 0);
    let frame = getStack(Components.stack.caller, 1)[0];
    let timer = stopTimer(args[0]);
    sendConsoleAPIMessage("timeEnd", frame, args, { timer: timer });
    dump("console.timeEnd: '" + timer.name + "' " + timer.duration + "ms\n");
  },
};
