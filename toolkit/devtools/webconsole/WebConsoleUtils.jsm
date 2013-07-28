/* -*- Mode: js2; js2-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ConsoleAPIStorage",
                                  "resource://gre/modules/ConsoleAPIStorage.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NetworkHelper",
                                  "resource://gre/modules/devtools/NetworkHelper.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
                                  "resource://gre/modules/PrivateBrowsingUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "gActivityDistributor",
                                   "@mozilla.org/network/http-activity-distributor;1",
                                   "nsIHttpActivityDistributor");

// TODO: Bug 842672 - toolkit/ imports modules from browser/.
// Note that these are only used in JSTermHelpers, see $0 and pprint().
XPCOMUtils.defineLazyModuleGetter(this, "gDevTools",
                                  "resource:///modules/devtools/gDevTools.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "devtools",
                                  "resource://gre/modules/devtools/Loader.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "VariablesView",
                                  "resource:///modules/devtools/VariablesView.jsm");

this.EXPORTED_SYMBOLS = ["WebConsoleUtils", "JSPropertyProvider", "JSTermHelpers",
                         "ConsoleServiceListener", "ConsoleAPIListener",
                         "NetworkResponseListener", "NetworkMonitor",
                         "ConsoleProgressListener"];

// Match the function name from the result of toString() or toSource().
//
// Examples:
// (function foobar(a, b) { ...
// function foobar2(a) { ...
// function() { ...
const REGEX_MATCH_FUNCTION_NAME = /^\(?function\s+([^(\s]+)\s*\(/;

// Match the function arguments from the result of toString() or toSource().
const REGEX_MATCH_FUNCTION_ARGS = /^\(?function\s*[^\s(]*\s*\((.+?)\)/;

this.WebConsoleUtils = {
  /**
   * Convenience function to unwrap a wrapped object.
   *
   * @param aObject the object to unwrap.
   * @return aObject unwrapped.
   */
  unwrap: function WCU_unwrap(aObject)
  {
    try {
      return XPCNativeWrapper.unwrap(aObject);
    }
    catch (ex) {
      return aObject;
    }
  },

  /**
   * Wrap a string in an nsISupportsString object.
   *
   * @param string aString
   * @return nsISupportsString
   */
  supportsString: function WCU_supportsString(aString)
  {
    let str = Cc["@mozilla.org/supports-string;1"].
              createInstance(Ci.nsISupportsString);
    str.data = aString;
    return str;
  },

  /**
   * Clone an object.
   *
   * @param object aObject
   *        The object you want cloned.
   * @param boolean aRecursive
   *        Tells if you want to dig deeper into the object, to clone
   *        recursively.
   * @param function [aFilter]
   *        Optional, filter function, called for every property. Three
   *        arguments are passed: key, value and object. Return true if the
   *        property should be added to the cloned object. Return false to skip
   *        the property.
   * @return object
   *         The cloned object.
   */
  cloneObject: function WCU_cloneObject(aObject, aRecursive, aFilter)
  {
    if (typeof aObject != "object") {
      return aObject;
    }

    let temp;

    if (Array.isArray(aObject)) {
      temp = [];
      Array.forEach(aObject, function(aValue, aIndex) {
        if (!aFilter || aFilter(aIndex, aValue, aObject)) {
          temp.push(aRecursive ? WCU_cloneObject(aValue) : aValue);
        }
      });
    }
    else {
      temp = {};
      for (let key in aObject) {
        let value = aObject[key];
        if (aObject.hasOwnProperty(key) &&
            (!aFilter || aFilter(key, value, aObject))) {
          temp[key] = aRecursive ? WCU_cloneObject(value) : value;
        }
      }
    }

    return temp;
  },

  /**
   * Gets the ID of the inner window of this DOM window.
   *
   * @param nsIDOMWindow aWindow
   * @return integer
   *         Inner ID for the given aWindow.
   */
  getInnerWindowId: function WCU_getInnerWindowId(aWindow)
  {
    return aWindow.QueryInterface(Ci.nsIInterfaceRequestor).
             getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID;
  },

  /**
   * Recursively gather a list of inner window ids given a
   * top level window.
   *
   * @param nsIDOMWindow aWindow
   * @return Array
   *         list of inner window ids.
   */
  getInnerWindowIDsForFrames: function WCU_getInnerWindowIDsForFrames(aWindow)
  {
    let innerWindowID = this.getInnerWindowId(aWindow);
    let ids = [innerWindowID];

    if (aWindow.frames) {
      for (let i = 0; i < aWindow.frames.length; i++) {
        let frame = aWindow.frames[i];
        ids = ids.concat(this.getInnerWindowIDsForFrames(frame));
      }
    }

    return ids;
  },


  /**
   * Gets the ID of the outer window of this DOM window.
   *
   * @param nsIDOMWindow aWindow
   * @return integer
   *         Outer ID for the given aWindow.
   */
  getOuterWindowId: function WCU_getOuterWindowId(aWindow)
  {
    return aWindow.QueryInterface(Ci.nsIInterfaceRequestor).
           getInterface(Ci.nsIDOMWindowUtils).outerWindowID;
  },

  /**
   * Abbreviates the given source URL so that it can be displayed flush-right
   * without being too distracting.
   *
   * @param string aSourceURL
   *        The source URL to shorten.
   * @return string
   *         The abbreviated form of the source URL.
   */
  abbreviateSourceURL: function WCU_abbreviateSourceURL(aSourceURL)
  {
    // Remove any query parameters.
    let hookIndex = aSourceURL.indexOf("?");
    if (hookIndex > -1) {
      aSourceURL = aSourceURL.substring(0, hookIndex);
    }

    // Remove a trailing "/".
    if (aSourceURL[aSourceURL.length - 1] == "/") {
      aSourceURL = aSourceURL.substring(0, aSourceURL.length - 1);
    }

    // Remove all but the last path component.
    let slashIndex = aSourceURL.lastIndexOf("/");
    if (slashIndex > -1) {
      aSourceURL = aSourceURL.substring(slashIndex + 1);
    }

    return aSourceURL;
  },

  /**
   * Tells if the given function is native or not.
   *
   * @param function aFunction
   *        The function you want to check if it is native or not.
   * @return boolean
   *         True if the given function is native, false otherwise.
   */
  isNativeFunction: function WCU_isNativeFunction(aFunction)
  {
    return typeof aFunction == "function" && !("prototype" in aFunction);
  },

  /**
   * Tells if the given property of the provided object is a non-native getter or
   * not.
   *
   * @param object aObject
   *        The object that contains the property.
   * @param string aProp
   *        The property you want to check if it is a getter or not.
   * @return boolean
   *         True if the given property is a getter, false otherwise.
   */
  isNonNativeGetter: function WCU_isNonNativeGetter(aObject, aProp)
  {
    if (typeof aObject != "object") {
      return false;
    }
    let desc = this.getPropertyDescriptor(aObject, aProp);
    return desc && desc.get && !this.isNativeFunction(desc.get);
  },

  /**
   * Get the property descriptor for the given object.
   *
   * @param object aObject
   *        The object that contains the property.
   * @param string aProp
   *        The property you want to get the descriptor for.
   * @return object
   *         Property descriptor.
   */
  getPropertyDescriptor: function WCU_getPropertyDescriptor(aObject, aProp)
  {
    let desc = null;
    while (aObject) {
      try {
        if (desc = Object.getOwnPropertyDescriptor(aObject, aProp)) {
          break;
        }
      }
      catch (ex if (ex.name == "NS_ERROR_XPC_BAD_CONVERT_JS" ||
                    ex.name == "NS_ERROR_XPC_BAD_OP_ON_WN_PROTO" ||
                    ex.name == "TypeError")) {
        // Native getters throw here. See bug 520882.
        // null throws TypeError.
      }
      try {
        aObject = Object.getPrototypeOf(aObject);
      }
      catch (ex if (ex.name == "TypeError")) {
        return desc;
      }
    }
    return desc;
  },

  /**
   * Sort function for object properties.
   *
   * @param object a
   *        Property descriptor.
   * @param object b
   *        Property descriptor.
   * @return integer
   *         -1 if a.name < b.name,
   *         1 if a.name > b.name,
   *         0 otherwise.
   */
  propertiesSort: function WCU_propertiesSort(a, b)
  {
    // Convert the pair.name to a number for later sorting.
    let aNumber = parseFloat(a.name);
    let bNumber = parseFloat(b.name);

    // Sort numbers.
    if (!isNaN(aNumber) && isNaN(bNumber)) {
      return -1;
    }
    else if (isNaN(aNumber) && !isNaN(bNumber)) {
      return 1;
    }
    else if (!isNaN(aNumber) && !isNaN(bNumber)) {
      return aNumber - bNumber;
    }
    // Sort string.
    else if (a.name < b.name) {
      return -1;
    }
    else if (a.name > b.name) {
      return 1;
    }
    else {
      return 0;
    }
  },

  /**
   * Create a grip for the given value. If the value is an object,
   * an object wrapper will be created.
   *
   * @param mixed aValue
   *        The value you want to create a grip for, before sending it to the
   *        client.
   * @param function aObjectWrapper
   *        If the value is an object then the aObjectWrapper function is
   *        invoked to give us an object grip. See this.getObjectGrip().
   * @return mixed
   *         The value grip.
   */
  createValueGrip: function WCU_createValueGrip(aValue, aObjectWrapper)
  {
    let type = typeof(aValue);
    switch (type) {
      case "boolean":
      case "number":
        return aValue;
      case "string":
          return aObjectWrapper(aValue);
      case "object":
      case "function":
        if (aValue) {
          return aObjectWrapper(aValue);
        }
      default:
        if (aValue === null) {
          return { type: "null" };
        }

        if (aValue === undefined) {
          return { type: "undefined" };
        }

        Cu.reportError("Failed to provide a grip for value of " + type + ": " +
                       aValue);
        return null;
    }
  },

  /**
   * Check if the given object is an iterator or a generator.
   *
   * @param object aObject
   *        The object you want to check.
   * @return boolean
   *         True if the given object is an iterator or a generator, otherwise
   *         false is returned.
   */
  isIteratorOrGenerator: function WCU_isIteratorOrGenerator(aObject)
  {
    if (aObject === null) {
      return false;
    }

    if (typeof aObject == "object") {
      if (typeof aObject.__iterator__ == "function" ||
          aObject.constructor && aObject.constructor.name == "Iterator") {
        return true;
      }

      try {
        let str = aObject.toString();
        if (typeof aObject.next == "function" &&
            str.indexOf("[object Generator") == 0) {
          return true;
        }
      }
      catch (ex) {
        // window.history.next throws in the typeof check above.
        return false;
      }
    }

    return false;
  },

  /**
   * Determine if the given request mixes HTTP with HTTPS content.
   *
   * @param string aRequest
   *        Location of the requested content.
   * @param string aLocation
   *        Location of the current page.
   * @return boolean
   *         True if the content is mixed, false if not.
   */
  isMixedHTTPSRequest: function WCU_isMixedHTTPSRequest(aRequest, aLocation)
  {
    try {
      let requestURI = Services.io.newURI(aRequest, null, null);
      let contentURI = Services.io.newURI(aLocation, null, null);
      return (contentURI.scheme == "https" && requestURI.scheme != "https");
    }
    catch (ex) {
      return false;
    }
  },

  /**
   * Helper function to deduce the name of the provided function.
   *
   * @param funtion aFunction
   *        The function whose name will be returned.
   * @return string
   *         Function name.
   */
  getFunctionName: function WCF_getFunctionName(aFunction)
  {
    let name = null;
    if (aFunction.name) {
      name = aFunction.name;
    }
    else {
      let desc;
      try {
        desc = aFunction.getOwnPropertyDescriptor("displayName");
      }
      catch (ex) { }
      if (desc && typeof desc.value == "string") {
        name = desc.value;
      }
    }
    if (!name) {
      try {
        let str = (aFunction.toString() || aFunction.toSource()) + "";
        name = (str.match(REGEX_MATCH_FUNCTION_NAME) || [])[1];
      }
      catch (ex) { }
    }
    return name;
  },

  /**
   * Get the object class name. For example, the |window| object has the Window
   * class name (based on [object Window]).
   *
   * @param object aObject
   *        The object you want to get the class name for.
   * @return string
   *         The object class name.
   */
  getObjectClassName: function WCU_getObjectClassName(aObject)
  {
    if (aObject === null) {
      return "null";
    }
    if (aObject === undefined) {
      return "undefined";
    }

    let type = typeof aObject;
    if (type != "object") {
      // Grip class names should start with an uppercase letter.
      return type.charAt(0).toUpperCase() + type.substr(1);
    }

    let className;

    try {
      className = ((aObject + "").match(/^\[object (\S+)\]$/) || [])[1];
      if (!className) {
        className = ((aObject.constructor + "").match(/^\[object (\S+)\]$/) || [])[1];
      }
      if (!className && typeof aObject.constructor == "function") {
        className = this.getFunctionName(aObject.constructor);
      }
    }
    catch (ex) { }

    return className;
  },

  /**
   * Check if the given value is a grip with an actor.
   *
   * @param mixed aGrip
   *        Value you want to check if it is a grip with an actor.
   * @return boolean
   *         True if the given value is a grip with an actor.
   */
  isActorGrip: function WCU_isActorGrip(aGrip)
  {
    return aGrip && typeof(aGrip) == "object" && aGrip.actor;
  },
};

//////////////////////////////////////////////////////////////////////////
// Localization
//////////////////////////////////////////////////////////////////////////

WebConsoleUtils.l10n = function WCU_l10n(aBundleURI)
{
  this._bundleUri = aBundleURI;
};

WebConsoleUtils.l10n.prototype = {
  _stringBundle: null,

  get stringBundle()
  {
    if (!this._stringBundle) {
      this._stringBundle = Services.strings.createBundle(this._bundleUri);
    }
    return this._stringBundle;
  },

  /**
   * Generates a formatted timestamp string for displaying in console messages.
   *
   * @param integer [aMilliseconds]
   *        Optional, allows you to specify the timestamp in milliseconds since
   *        the UNIX epoch.
   * @return string
   *         The timestamp formatted for display.
   */
  timestampString: function WCU_l10n_timestampString(aMilliseconds)
  {
    let d = new Date(aMilliseconds ? aMilliseconds : null);
    let hours = d.getHours(), minutes = d.getMinutes();
    let seconds = d.getSeconds(), milliseconds = d.getMilliseconds();
    let parameters = [hours, minutes, seconds, milliseconds];
    return this.getFormatStr("timestampFormat", parameters);
  },

  /**
   * Retrieve a localized string.
   *
   * @param string aName
   *        The string name you want from the Web Console string bundle.
   * @return string
   *         The localized string.
   */
  getStr: function WCU_l10n_getStr(aName)
  {
    let result;
    try {
      result = this.stringBundle.GetStringFromName(aName);
    }
    catch (ex) {
      Cu.reportError("Failed to get string: " + aName);
      throw ex;
    }
    return result;
  },

  /**
   * Retrieve a localized string formatted with values coming from the given
   * array.
   *
   * @param string aName
   *        The string name you want from the Web Console string bundle.
   * @param array aArray
   *        The array of values you want in the formatted string.
   * @return string
   *         The formatted local string.
   */
  getFormatStr: function WCU_l10n_getFormatStr(aName, aArray)
  {
    let result;
    try {
      result = this.stringBundle.formatStringFromName(aName, aArray, aArray.length);
    }
    catch (ex) {
      Cu.reportError("Failed to format string: " + aName);
      throw ex;
    }
    return result;
  },
};


//////////////////////////////////////////////////////////////////////////
// JS Completer
//////////////////////////////////////////////////////////////////////////

this.JSPropertyProvider = (function _JSPP(WCU) {
const STATE_NORMAL = 0;
const STATE_QUOTE = 2;
const STATE_DQUOTE = 3;

const OPEN_BODY = "{[(".split("");
const CLOSE_BODY = "}])".split("");
const OPEN_CLOSE_BODY = {
  "{": "}",
  "[": "]",
  "(": ")",
};

const MAX_COMPLETIONS = 256;

/**
 * Analyses a given string to find the last statement that is interesting for
 * later completion.
 *
 * @param   string aStr
 *          A string to analyse.
 *
 * @returns object
 *          If there was an error in the string detected, then a object like
 *
 *            { err: "ErrorMesssage" }
 *
 *          is returned, otherwise a object like
 *
 *            {
 *              state: STATE_NORMAL|STATE_QUOTE|STATE_DQUOTE,
 *              startPos: index of where the last statement begins
 *            }
 */
function findCompletionBeginning(aStr)
{
  let bodyStack = [];

  let state = STATE_NORMAL;
  let start = 0;
  let c;
  for (let i = 0; i < aStr.length; i++) {
    c = aStr[i];

    switch (state) {
      // Normal JS state.
      case STATE_NORMAL:
        if (c == '"') {
          state = STATE_DQUOTE;
        }
        else if (c == "'") {
          state = STATE_QUOTE;
        }
        else if (c == ";") {
          start = i + 1;
        }
        else if (c == " ") {
          start = i + 1;
        }
        else if (OPEN_BODY.indexOf(c) != -1) {
          bodyStack.push({
            token: c,
            start: start
          });
          start = i + 1;
        }
        else if (CLOSE_BODY.indexOf(c) != -1) {
          var last = bodyStack.pop();
          if (!last || OPEN_CLOSE_BODY[last.token] != c) {
            return {
              err: "syntax error"
            };
          }
          if (c == "}") {
            start = i + 1;
          }
          else {
            start = last.start;
          }
        }
        break;

      // Double quote state > " <
      case STATE_DQUOTE:
        if (c == "\\") {
          i++;
        }
        else if (c == "\n") {
          return {
            err: "unterminated string literal"
          };
        }
        else if (c == '"') {
          state = STATE_NORMAL;
        }
        break;

      // Single quote state > ' <
      case STATE_QUOTE:
        if (c == "\\") {
          i++;
        }
        else if (c == "\n") {
          return {
            err: "unterminated string literal"
          };
        }
        else if (c == "'") {
          state = STATE_NORMAL;
        }
        break;
    }
  }

  return {
    state: state,
    startPos: start
  };
}

/**
 * Provides a list of properties, that are possible matches based on the passed
 * scope and inputValue.
 *
 * @param object aScope
 *        Scope to use for the completion.
 * @param string aInputValue
 *        Value that should be completed.
 * @param number [aCursor=aInputValue.length]
 *        Optional offset in the input where the cursor is located. If this is
 *        omitted then the cursor is assumed to be at the end of the input
 *        value.
 * @returns null or object
 *          If no completion valued could be computed, null is returned,
 *          otherwise a object with the following form is returned:
 *            {
 *              matches: [ string, string, string ],
 *              matchProp: Last part of the inputValue that was used to find
 *                         the matches-strings.
 *            }
 */
function JSPropertyProvider(aScope, aInputValue, aCursor)
{
  if (aCursor === undefined) {
    aCursor = aInputValue.length;
  }

  let inputValue = aInputValue.substring(0, aCursor);
  let obj = WCU.unwrap(aScope);

  // Analyse the inputValue and find the beginning of the last part that
  // should be completed.
  let beginning = findCompletionBeginning(inputValue);

  // There was an error analysing the string.
  if (beginning.err) {
    return null;
  }

  // If the current state is not STATE_NORMAL, then we are inside of an string
  // which means that no completion is possible.
  if (beginning.state != STATE_NORMAL) {
    return null;
  }

  let completionPart = inputValue.substring(beginning.startPos);

  // Don't complete on just an empty string.
  if (completionPart.trim() == "") {
    return null;
  }

  let matches = null;
  let matchProp = "";

  let lastDot = completionPart.lastIndexOf(".");
  if (lastDot > 0 &&
      (completionPart[0] == "'" || completionPart[0] == '"') &&
      completionPart[lastDot - 1] == completionPart[0]) {
    // We are completing a string literal.
    obj = obj.String.prototype;
    matchProp = completionPart.slice(lastDot + 1);

  }
  else {
    // We are completing a variable / a property lookup.

    let properties = completionPart.split(".");
    if (properties.length > 1) {
      matchProp = properties.pop().trimLeft();
      for (let i = 0; i < properties.length; i++) {
        let prop = properties[i].trim();
        if (!prop) {
          return null;
        }

        // If obj is undefined or null (which is what "== null" does),
        // then there is no chance to run completion on it. Exit here.
        if (obj == null) {
          return null;
        }

        // Check if prop is a getter function on obj. Functions can change other
        // stuff so we can't execute them to get the next object. Stop here.
        if (WCU.isNonNativeGetter(obj, prop)) {
          return null;
        }
        try {
          obj = obj[prop];
        }
        catch (ex) {
          return null;
        }
      }
    }
    else {
      matchProp = properties[0].trimLeft();
    }

    // If obj is undefined or null (which is what "== null" does),
    // then there is no chance to run completion on it. Exit here.
    if (obj == null) {
      return null;
    }

    try {
      // Skip Iterators and Generators.
      if (WCU.isIteratorOrGenerator(obj)) {
        return null;
      }
    }
    catch (ex) {
      // The above can throw if |obj| is a dead object.
      // TODO: we should use Cu.isDeadWrapper() - see bug 885800.
      return null;
    }
  }

  let matches = Object.keys(getMatchedProps(obj, {matchProp:matchProp}));

  return {
    matchProp: matchProp,
    matches: matches.sort(),
  };
}

/**
 * Get all accessible properties on this JS value.
 * Filter those properties by name.
 * Take only a certain number of those.
 *
 * @param mixed aObj
 *        JS value whose properties we want to collect.
 *
 * @param object aOptions
 *        Options that the algorithm takes.
 *        - matchProp (string): Filter for properties that match this one.
 *          Defaults to the empty string (which always matches).
 *
 * @return object
 *         Object whose keys are all accessible properties on the object.
 */
function getMatchedProps(aObj, aOptions = {matchProp: ""})
{
  // Argument defaults.
  aOptions.matchProp = aOptions.matchProp || "";

  if (aObj == null) { return {}; }
  try {
    Object.getPrototypeOf(aObj);
  } catch(e) {
    aObj = aObj.constructor.prototype;
  }
  let c = MAX_COMPLETIONS;
  let names = Object.create(null);   // Using an Object to avoid duplicates.

  // We need to go up the prototype chain.
  let ownNames = null;
  while (aObj !== null) {
    ownNames = Object.getOwnPropertyNames(aObj);
    for (let i = 0; i < ownNames.length; i++) {
      // Filtering happens here.
      // If we already have it in, no need to append it.
      if (ownNames[i].indexOf(aOptions.matchProp) != 0 ||
          ownNames[i] in names) {
        continue;
      }
      c--;
      if (c < 0) {
        return names;
      }
      // If it is an array index, we can't take it.
      // This uses a trick: converting a string to a number yields NaN if
      // the operation failed, and NaN is not equal to itself.
      if (+ownNames[i] != +ownNames[i]) {
        names[ownNames[i]] = true;
      }
    }
    aObj = Object.getPrototypeOf(aObj);
  }

  return names;
}


return JSPropertyProvider;
})(WebConsoleUtils);

///////////////////////////////////////////////////////////////////////////////
// The page errors listener
///////////////////////////////////////////////////////////////////////////////

/**
 * The nsIConsoleService listener. This is used to send all of the console
 * messages (JavaScript, CSS and more) to the remote Web Console instance.
 *
 * @constructor
 * @param nsIDOMWindow [aWindow]
 *        Optional - the window object for which we are created. This is used
 *        for filtering out messages that belong to other windows.
 * @param object aListener
 *        The listener object must have one method:
 *        - onConsoleServiceMessage(). This method is invoked with one argument, the
 *        nsIConsoleMessage, whenever a relevant message is received.
 */
this.ConsoleServiceListener = function ConsoleServiceListener(aWindow, aListener)
{
  this.window = aWindow;
  this.listener = aListener;
}

ConsoleServiceListener.prototype =
{
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIConsoleListener]),

  /**
   * The content window for which we listen to page errors.
   * @type nsIDOMWindow
   */
  window: null,

  /**
   * The listener object which is notified of messages from the console service.
   * @type object
   */
  listener: null,

  /**
   * Initialize the nsIConsoleService listener.
   */
  init: function CSL_init()
  {
    Services.console.registerListener(this);
  },

  /**
   * The nsIConsoleService observer. This method takes all the script error
   * messages belonging to the current window and sends them to the remote Web
   * Console instance.
   *
   * @param nsIConsoleMessage aMessage
   *        The message object coming from the nsIConsoleService.
   */
  observe: function CSL_observe(aMessage)
  {
    if (!this.listener) {
      return;
    }

    if (this.window) {
      if (!(aMessage instanceof Ci.nsIScriptError) ||
          !aMessage.outerWindowID ||
          !this.isCategoryAllowed(aMessage.category)) {
        return;
      }

      let errorWindow = Services.wm.getOuterWindowWithId(aMessage.outerWindowID);
      if (!errorWindow || errorWindow.top != this.window) {
        return;
      }
    }

    this.listener.onConsoleServiceMessage(aMessage);
  },

  /**
   * Check if the given message category is allowed to be tracked or not.
   * We ignore chrome-originating errors as we only care about content.
   *
   * @param string aCategory
   *        The message category you want to check.
   * @return boolean
   *         True if the category is allowed to be logged, false otherwise.
   */
  isCategoryAllowed: function CSL_isCategoryAllowed(aCategory)
  {
    if (!aCategory) {
      return false;
    }

    switch (aCategory) {
      case "XPConnect JavaScript":
      case "component javascript":
      case "chrome javascript":
      case "chrome registration":
      case "XBL":
      case "XBL Prototype Handler":
      case "XBL Content Sink":
      case "xbl javascript":
        return false;
    }

    return true;
  },

  /**
   * Get the cached page errors for the current inner window and its (i)frames.
   *
   * @param boolean [aIncludePrivate=false]
   *        Tells if you want to also retrieve messages coming from private
   *        windows. Defaults to false.
   * @return array
   *         The array of cached messages. Each element is an nsIScriptError or
   *         an nsIConsoleMessage
   */
  getCachedMessages: function CSL_getCachedMessages(aIncludePrivate = false)
  {
    let errors = Services.console.getMessageArray() || [];

    // if !this.window, we're in a browser console. Still need to filter
    // private messages.
    if (!this.window) {
      return errors.filter((aError) => {
        if (aError instanceof Ci.nsIScriptError) {
          if (!aIncludePrivate && aError.isFromPrivateWindow) {
            return false;
          }
        }

        return true;
      });
    }

    let ids = WebConsoleUtils.getInnerWindowIDsForFrames(this.window);

    return errors.filter((aError) => {
      if (aError instanceof Ci.nsIScriptError) {
        if (!aIncludePrivate && aError.isFromPrivateWindow) {
          return false;
        }
        if (ids &&
            (ids.indexOf(aError.innerWindowID) == -1 ||
             !this.isCategoryAllowed(aError.category))) {
          return false;
        }
      }
      else if (ids && ids[0]) {
        // If this is not an nsIScriptError and we need to do window-based
        // filtering we skip this message.
        return false;
      }

      return true;
    });
  },

  /**
   * Remove the nsIConsoleService listener.
   */
  destroy: function CSL_destroy()
  {
    Services.console.unregisterListener(this);
    this.listener = this.window = null;
  },
};


///////////////////////////////////////////////////////////////////////////////
// The window.console API observer
///////////////////////////////////////////////////////////////////////////////

/**
 * The window.console API observer. This allows the window.console API messages
 * to be sent to the remote Web Console instance.
 *
 * @constructor
 * @param nsIDOMWindow aWindow
 *        Optional - the window object for which we are created. This is used
 *        for filtering out messages that belong to other windows.
 * @param object aOwner
 *        The owner object must have the following methods:
 *        - onConsoleAPICall(). This method is invoked with one argument, the
 *        Console API message that comes from the observer service, whenever
 *        a relevant console API call is received.
 */
this.ConsoleAPIListener = function ConsoleAPIListener(aWindow, aOwner)
{
  this.window = aWindow;
  this.owner = aOwner;
}

ConsoleAPIListener.prototype =
{
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  /**
   * The content window for which we listen to window.console API calls.
   * @type nsIDOMWindow
   */
  window: null,

  /**
   * The owner object which is notified of window.console API calls. It must
   * have a onConsoleAPICall method which is invoked with one argument: the
   * console API call object that comes from the observer service.
   *
   * @type object
   * @see WebConsoleActor
   */
  owner: null,

  /**
   * Initialize the window.console API observer.
   */
  init: function CAL_init()
  {
    // Note that the observer is process-wide. We will filter the messages as
    // needed, see CAL_observe().
    Services.obs.addObserver(this, "console-api-log-event", false);
  },

  /**
   * The console API message observer. When messages are received from the
   * observer service we forward them to the remote Web Console instance.
   *
   * @param object aMessage
   *        The message object receives from the observer service.
   * @param string aTopic
   *        The message topic received from the observer service.
   */
  observe: function CAL_observe(aMessage, aTopic)
  {
    if (!this.owner) {
      return;
    }

    let apiMessage = aMessage.wrappedJSObject;
    if (this.window) {
      let msgWindow = Services.wm.getOuterWindowWithId(apiMessage.ID);
      if (!msgWindow || msgWindow.top != this.window) {
        // Not the same window!
        return;
      }
    }

    this.owner.onConsoleAPICall(apiMessage);
  },

  /**
   * Get the cached messages for the current inner window and its (i)frames.
   *
   * @param boolean [aIncludePrivate=false]
   *        Tells if you want to also retrieve messages coming from private
   *        windows. Defaults to false.
   * @return array
   *         The array of cached messages.
   */
  getCachedMessages: function CAL_getCachedMessages(aIncludePrivate = false)
  {
    let messages = [];

    // if !this.window, we're in a browser console. Retrieve all events
    // for filtering based on privacy.
    if (!this.window) {
      messages = ConsoleAPIStorage.getEvents();
    } else {
      let ids = WebConsoleUtils.getInnerWindowIDsForFrames(this.window);
      ids.forEach((id) => {
        messages = messages.concat(ConsoleAPIStorage.getEvents(id));
      });
    }

    if (aIncludePrivate) {
      return messages;
    }

    return messages.filter((m) => !m.private);
  },

  /**
   * Destroy the console API listener.
   */
  destroy: function CAL_destroy()
  {
    Services.obs.removeObserver(this, "console-api-log-event");
    this.window = this.owner = null;
  },
};



/**
 * JSTerm helper functions.
 *
 * Defines a set of functions ("helper functions") that are available from the
 * Web Console but not from the web page.
 *
 * A list of helper functions used by Firebug can be found here:
 *   http://getfirebug.com/wiki/index.php/Command_Line_API
 *
 * @param object aOwner
 *        The owning object.
 */
this.JSTermHelpers = function JSTermHelpers(aOwner)
{
  /**
   * Find a node by ID.
   *
   * @param string aId
   *        The ID of the element you want.
   * @return nsIDOMNode or null
   *         The result of calling document.querySelector(aSelector).
   */
  aOwner.sandbox.$ = function JSTH_$(aSelector)
  {
    return aOwner.window.document.querySelector(aSelector);
  };

  /**
   * Find the nodes matching a CSS selector.
   *
   * @param string aSelector
   *        A string that is passed to window.document.querySelectorAll.
   * @return nsIDOMNodeList
   *         Returns the result of document.querySelectorAll(aSelector).
   */
  aOwner.sandbox.$$ = function JSTH_$$(aSelector)
  {
    return aOwner.window.document.querySelectorAll(aSelector);
  };

  /**
   * Runs an xPath query and returns all matched nodes.
   *
   * @param string aXPath
   *        xPath search query to execute.
   * @param [optional] nsIDOMNode aContext
   *        Context to run the xPath query on. Uses window.document if not set.
   * @return array of nsIDOMNode
   */
  aOwner.sandbox.$x = function JSTH_$x(aXPath, aContext)
  {
    let nodes = new aOwner.window.wrappedJSObject.Array();
    let doc = aOwner.window.document;
    let aContext = aContext || doc;

    let results = doc.evaluate(aXPath, aContext, null,
                               Ci.nsIDOMXPathResult.ANY_TYPE, null);
    let node;
    while (node = results.iterateNext()) {
      nodes.push(node);
    }

    return nodes;
  };

  /**
   * Returns the currently selected object in the highlighter.
   *
   * TODO: this implementation crosses the client/server boundaries! This is not
   * usable within a remote browser. To implement this feature correctly we need
   * support for remote inspection capabilities within the Inspector as well.
   * See bug 787975.
   *
   * @return nsIDOMElement|null
   *         The DOM element currently selected in the highlighter.
   */
   Object.defineProperty(aOwner.sandbox, "$0", {
    get: function() {
      let window = aOwner.chromeWindow();
      if (!window) {
        return null;
      }

      let target = null;
      try {
        target = devtools.TargetFactory.forTab(window.gBrowser.selectedTab);
      }
      catch (ex) {
        // If we report this exception the user will get it in the Browser
        // Console every time when she evaluates any string.
      }

      if (!target) {
        return null;
      }

      let toolbox = gDevTools.getToolbox(target);
      let panel = toolbox ? toolbox.getPanel("inspector") : null;
      let node = panel ? panel.selection.node : null;

      return node ? aOwner.makeDebuggeeValue(node) : null;
    },
    enumerable: true,
    configurable: false
  });

  /**
   * Clears the output of the JSTerm.
   */
  aOwner.sandbox.clear = function JSTH_clear()
  {
    aOwner.helperResult = {
      type: "clearOutput",
    };
  };

  /**
   * Returns the result of Object.keys(aObject).
   *
   * @param object aObject
   *        Object to return the property names from.
   * @return array of strings
   */
  aOwner.sandbox.keys = function JSTH_keys(aObject)
  {
    return aOwner.window.wrappedJSObject.Object.keys(WebConsoleUtils.unwrap(aObject));
  };

  /**
   * Returns the values of all properties on aObject.
   *
   * @param object aObject
   *        Object to display the values from.
   * @return array of string
   */
  aOwner.sandbox.values = function JSTH_values(aObject)
  {
    let arrValues = new aOwner.window.wrappedJSObject.Array();
    let obj = WebConsoleUtils.unwrap(aObject);

    for (let prop in obj) {
      arrValues.push(obj[prop]);
    }

    return arrValues;
  };

  /**
   * Opens a help window in MDN.
   */
  aOwner.sandbox.help = function JSTH_help()
  {
    aOwner.helperResult = { type: "help" };
  };

  /**
   * Inspects the passed aObject. This is done by opening the PropertyPanel.
   *
   * @param object aObject
   *        Object to inspect.
   */
  aOwner.sandbox.inspect = function JSTH_inspect(aObject)
  {
    let dbgObj = aOwner.makeDebuggeeValue(aObject);
    let grip = aOwner.createValueGrip(dbgObj);
    aOwner.helperResult = {
      type: "inspectObject",
      input: aOwner.evalInput,
      object: grip,
    };
  };

  /**
   * Prints aObject to the output.
   *
   * @param object aObject
   *        Object to print to the output.
   * @return string
   */
  aOwner.sandbox.pprint = function JSTH_pprint(aObject)
  {
    if (aObject === null || aObject === undefined || aObject === true ||
        aObject === false) {
      aOwner.helperResult = {
        type: "error",
        message: "helperFuncUnsupportedTypeError",
      };
      return;
    }

    aOwner.helperResult = { rawOutput: true };

    if (typeof aObject == "function") {
      return aObject + "\n";
    }

    let output = [];

    let obj = WebConsoleUtils.unwrap(aObject);
    for (let name in obj) {
      let desc = WebConsoleUtils.getPropertyDescriptor(obj, name) || {};
      if (desc.get || desc.set) {
        // TODO: Bug 842672 - toolkit/ imports modules from browser/.
        let getGrip = VariablesView.getGrip(desc.get);
        let setGrip = VariablesView.getGrip(desc.set);
        let getString = VariablesView.getString(getGrip);
        let setString = VariablesView.getString(setGrip);
        output.push(name + ":", "  get: " + getString, "  set: " + setString);
      }
      else {
        let valueGrip = VariablesView.getGrip(obj[name]);
        let valueString = VariablesView.getString(valueGrip);
        output.push(name + ": " + valueString);
      }
    }

    return "  " + output.join("\n  ");
  };

  /**
   * Print a string to the output, as-is.
   *
   * @param string aString
   *        A string you want to output.
   * @return void
   */
  aOwner.sandbox.print = function JSTH_print(aString)
  {
    aOwner.helperResult = { rawOutput: true };
    return String(aString);
  };
};


(function(_global, WCU) {
///////////////////////////////////////////////////////////////////////////////
// Network logging
///////////////////////////////////////////////////////////////////////////////

// The maximum uint32 value.
const PR_UINT32_MAX = 4294967295;

// HTTP status codes.
const HTTP_MOVED_PERMANENTLY = 301;
const HTTP_FOUND = 302;
const HTTP_SEE_OTHER = 303;
const HTTP_TEMPORARY_REDIRECT = 307;

// The maximum number of bytes a NetworkResponseListener can hold.
const RESPONSE_BODY_LIMIT = 1048576; // 1 MB

/**
 * The network response listener implements the nsIStreamListener and
 * nsIRequestObserver interfaces. This is used within the NetworkMonitor feature
 * to get the response body of the request.
 *
 * The code is mostly based on code listings from:
 *
 *   http://www.softwareishard.com/blog/firebug/
 *      nsitraceablechannel-intercept-http-traffic/
 *
 * @constructor
 * @param object aOwner
 *        The response listener owner. This object needs to hold the
 *        |openResponses| object.
 * @param object aHttpActivity
 *        HttpActivity object associated with this request. See NetworkMonitor
 *        for more information.
 */
function NetworkResponseListener(aOwner, aHttpActivity)
{
  this.owner = aOwner;
  this.receivedData = "";
  this.httpActivity = aHttpActivity;
  this.bodySize = 0;
}

NetworkResponseListener.prototype = {
  QueryInterface:
    XPCOMUtils.generateQI([Ci.nsIStreamListener, Ci.nsIInputStreamCallback,
                           Ci.nsIRequestObserver, Ci.nsISupports]),

  /**
   * This NetworkResponseListener tracks the NetworkMonitor.openResponses object
   * to find the associated uncached headers.
   * @private
   */
  _foundOpenResponse: false,

  /**
   * The response listener owner.
   */
  owner: null,

  /**
   * The response will be written into the outputStream of this nsIPipe.
   * Both ends of the pipe must be blocking.
   */
  sink: null,

  /**
   * The HttpActivity object associated with this response.
   */
  httpActivity: null,

  /**
   * Stores the received data as a string.
   */
  receivedData: null,

  /**
   * The network response body size.
   */
  bodySize: null,

  /**
   * The nsIRequest we are started for.
   */
  request: null,

  /**
   * Set the async listener for the given nsIAsyncInputStream. This allows us to
   * wait asynchronously for any data coming from the stream.
   *
   * @param nsIAsyncInputStream aStream
   *        The input stream from where we are waiting for data to come in.
   * @param nsIInputStreamCallback aListener
   *        The input stream callback you want. This is an object that must have
   *        the onInputStreamReady() method. If the argument is null, then the
   *        current callback is removed.
   * @return void
   */
  setAsyncListener: function NRL_setAsyncListener(aStream, aListener)
  {
    // Asynchronously wait for the stream to be readable or closed.
    aStream.asyncWait(aListener, 0, 0, Services.tm.mainThread);
  },

  /**
   * Stores the received data, if request/response body logging is enabled. It
   * also does limit the number of stored bytes, based on the
   * RESPONSE_BODY_LIMIT constant.
   *
   * Learn more about nsIStreamListener at:
   * https://developer.mozilla.org/en/XPCOM_Interface_Reference/nsIStreamListener
   *
   * @param nsIRequest aRequest
   * @param nsISupports aContext
   * @param nsIInputStream aInputStream
   * @param unsigned long aOffset
   * @param unsigned long aCount
   */
  onDataAvailable:
  function NRL_onDataAvailable(aRequest, aContext, aInputStream, aOffset, aCount)
  {
    this._findOpenResponse();
    let data = NetUtil.readInputStreamToString(aInputStream, aCount);

    this.bodySize += aCount;

    if (!this.httpActivity.discardResponseBody &&
        this.receivedData.length < RESPONSE_BODY_LIMIT) {
      this.receivedData += NetworkHelper.
                           convertToUnicode(data, aRequest.contentCharset);
    }
  },

  /**
   * See documentation at
   * https://developer.mozilla.org/En/NsIRequestObserver
   *
   * @param nsIRequest aRequest
   * @param nsISupports aContext
   */
  onStartRequest: function NRL_onStartRequest(aRequest)
  {
    this.request = aRequest;
    this._findOpenResponse();
    // Asynchronously wait for the data coming from the request.
    this.setAsyncListener(this.sink.inputStream, this);
  },

  /**
   * Handle the onStopRequest by closing the sink output stream.
   *
   * For more documentation about nsIRequestObserver go to:
   * https://developer.mozilla.org/En/NsIRequestObserver
   */
  onStopRequest: function NRL_onStopRequest()
  {
    this._findOpenResponse();
    this.sink.outputStream.close();
  },

  /**
   * Find the open response object associated to the current request. The
   * NetworkMonitor._httpResponseExaminer() method saves the response headers in
   * NetworkMonitor.openResponses. This method takes the data from the open
   * response object and puts it into the HTTP activity object, then sends it to
   * the remote Web Console instance.
   *
   * @private
   */
  _findOpenResponse: function NRL__findOpenResponse()
  {
    if (!this.owner || this._foundOpenResponse) {
      return;
    }

    let openResponse = null;

    for each (let item in this.owner.openResponses) {
      if (item.channel === this.httpActivity.channel) {
        openResponse = item;
        break;
      }
    }

    if (!openResponse) {
      return;
    }
    this._foundOpenResponse = true;

    delete this.owner.openResponses[openResponse.id];

    this.httpActivity.owner.addResponseHeaders(openResponse.headers);
    this.httpActivity.owner.addResponseCookies(openResponse.cookies);
  },

  /**
   * Clean up the response listener once the response input stream is closed.
   * This is called from onStopRequest() or from onInputStreamReady() when the
   * stream is closed.
   * @return void
   */
  onStreamClose: function NRL_onStreamClose()
  {
    if (!this.httpActivity) {
      return;
    }
    // Remove our listener from the request input stream.
    this.setAsyncListener(this.sink.inputStream, null);

    this._findOpenResponse();

    if (!this.httpActivity.discardResponseBody && this.receivedData.length) {
      this._onComplete(this.receivedData);
    }
    else if (!this.httpActivity.discardResponseBody &&
             this.httpActivity.responseStatus == 304) {
      // Response is cached, so we load it from cache.
      let charset = this.request.contentCharset || this.httpActivity.charset;
      NetworkHelper.loadFromCache(this.httpActivity.url, charset,
                                  this._onComplete.bind(this));
    }
    else {
      this._onComplete();
    }
  },

  /**
   * Handler for when the response completes. This function cleans up the
   * response listener.
   *
   * @param string [aData]
   *        Optional, the received data coming from the response listener or
   *        from the cache.
   */
  _onComplete: function NRL__onComplete(aData)
  {
    let response = {
      mimeType: "",
      text: aData || "",
    };

    response.size = response.text.length;

    try {
      response.mimeType = this.request.contentType;
    }
    catch (ex) { }

    if (!response.mimeType || !NetworkHelper.isTextMimeType(response.mimeType)) {
      response.encoding = "base64";
      response.text = btoa(response.text);
    }

    if (response.mimeType && this.request.contentCharset) {
      response.mimeType += "; charset=" + this.request.contentCharset;
    }

    this.receivedData = "";

    this.httpActivity.owner.
      addResponseContent(response, this.httpActivity.discardResponseBody);

    this.httpActivity.channel = null;
    this.httpActivity.owner = null;
    this.httpActivity = null;
    this.sink = null;
    this.inputStream = null;
    this.request = null;
    this.owner = null;
  },

  /**
   * The nsIInputStreamCallback for when the request input stream is ready -
   * either it has more data or it is closed.
   *
   * @param nsIAsyncInputStream aStream
   *        The sink input stream from which data is coming.
   * @returns void
   */
  onInputStreamReady: function NRL_onInputStreamReady(aStream)
  {
    if (!(aStream instanceof Ci.nsIAsyncInputStream) || !this.httpActivity) {
      return;
    }

    let available = -1;
    try {
      // This may throw if the stream is closed normally or due to an error.
      available = aStream.available();
    }
    catch (ex) { }

    if (available != -1) {
      if (available != 0) {
        // Note that passing 0 as the offset here is wrong, but the
        // onDataAvailable() method does not use the offset, so it does not
        // matter.
        this.onDataAvailable(this.request, null, aStream, 0, available);
      }
      this.setAsyncListener(aStream, this);
    }
    else {
      this.onStreamClose();
    }
  },
};

/**
 * The network monitor uses the nsIHttpActivityDistributor to monitor network
 * requests. The nsIObserverService is also used for monitoring
 * http-on-examine-response notifications. All network request information is
 * routed to the remote Web Console.
 *
 * @constructor
 * @param nsIDOMWindow aWindow
 *        Optional, the window that we monitor network requests for. If no
 *        window is given, all browser network requests are logged.
 * @param object aOwner
 *        The network monitor owner. This object needs to hold:
 *        - onNetworkEvent(aRequestInfo, aChannel). This method is invoked once for
 *        every new network request and it is given two arguments: the initial network
 *        request information, and the channel. onNetworkEvent() must return an object
 *        which holds several add*() methods which are used to add further network
 *        request/response information.
 *        - saveRequestAndResponseBodies property which tells if you want to log
 *        request and response bodies.
 */
function NetworkMonitor(aWindow, aOwner)
{
  this.window = aWindow;
  this.owner = aOwner;
  this.openRequests = {};
  this.openResponses = {};
  this._httpResponseExaminer = this._httpResponseExaminer.bind(this);
}

NetworkMonitor.prototype = {
  httpTransactionCodes: {
    0x5001: "REQUEST_HEADER",
    0x5002: "REQUEST_BODY_SENT",
    0x5003: "RESPONSE_START",
    0x5004: "RESPONSE_HEADER",
    0x5005: "RESPONSE_COMPLETE",
    0x5006: "TRANSACTION_CLOSE",

    0x804b0003: "STATUS_RESOLVING",
    0x804b000b: "STATUS_RESOLVED",
    0x804b0007: "STATUS_CONNECTING_TO",
    0x804b0004: "STATUS_CONNECTED_TO",
    0x804b0005: "STATUS_SENDING_TO",
    0x804b000a: "STATUS_WAITING_FOR",
    0x804b0006: "STATUS_RECEIVING_FROM"
  },

  // Network response bodies are piped through a buffer of the given size (in
  // bytes).
  responsePipeSegmentSize: null,

  owner: null,

  /**
   * Whether to save the bodies of network requests and responses. Disabled by
   * default to save memory.
   */
  get saveRequestAndResponseBodies()
    this.owner && this.owner.saveRequestAndResponseBodies,

  /**
   * Object that holds the HTTP activity objects for ongoing requests.
   */
  openRequests: null,

  /**
   * Object that holds response headers coming from this._httpResponseExaminer.
   */
  openResponses: null,

  /**
   * The network monitor initializer.
   */
  init: function NM_init()
  {
    this.responsePipeSegmentSize = Services.prefs
                                   .getIntPref("network.buffer.cache.size");

    gActivityDistributor.addObserver(this);

    if (Services.appinfo.processType != Ci.nsIXULRuntime.PROCESS_TYPE_CONTENT) {
      Services.obs.addObserver(this._httpResponseExaminer,
                               "http-on-examine-response", false);
    }
  },

  /**
   * Observe notifications for the http-on-examine-response topic, coming from
   * the nsIObserverService.
   *
   * @private
   * @param nsIHttpChannel aSubject
   * @param string aTopic
   * @returns void
   */
  _httpResponseExaminer: function NM__httpResponseExaminer(aSubject, aTopic)
  {
    // The httpResponseExaminer is used to retrieve the uncached response
    // headers. The data retrieved is stored in openResponses. The
    // NetworkResponseListener is responsible with updating the httpActivity
    // object with the data from the new object in openResponses.

    if (!this.owner || aTopic != "http-on-examine-response" ||
        !(aSubject instanceof Ci.nsIHttpChannel)) {
      return;
    }

    let channel = aSubject.QueryInterface(Ci.nsIHttpChannel);

    if (this.window) {
      // Try to get the source window of the request.
      let win = NetworkHelper.getWindowForRequest(channel);
      if (!win || win.top !== this.window) {
        return;
      }
    }

    let response = {
      id: gSequenceId(),
      channel: channel,
      headers: [],
      cookies: [],
    };

    let setCookieHeader = null;

    channel.visitResponseHeaders({
      visitHeader: function NM__visitHeader(aName, aValue) {
        let lowerName = aName.toLowerCase();
        if (lowerName == "set-cookie") {
          setCookieHeader = aValue;
        }
        response.headers.push({ name: aName, value: aValue });
      }
    });

    if (!response.headers.length) {
      return; // No need to continue.
    }

    if (setCookieHeader) {
      response.cookies = NetworkHelper.parseSetCookieHeader(setCookieHeader);
    }

    // Determine the HTTP version.
    let httpVersionMaj = {};
    let httpVersionMin = {};

    channel.QueryInterface(Ci.nsIHttpChannelInternal);
    channel.getResponseVersion(httpVersionMaj, httpVersionMin);

    response.status = channel.responseStatus;
    response.statusText = channel.responseStatusText;
    response.httpVersion = "HTTP/" + httpVersionMaj.value + "." +
                                     httpVersionMin.value;

    this.openResponses[response.id] = response;
  },

  /**
   * Begin observing HTTP traffic that originates inside the current tab.
   *
   * @see https://developer.mozilla.org/en/XPCOM_Interface_Reference/nsIHttpActivityObserver
   *
   * @param nsIHttpChannel aChannel
   * @param number aActivityType
   * @param number aActivitySubtype
   * @param number aTimestamp
   * @param number aExtraSizeData
   * @param string aExtraStringData
   */
  observeActivity:
  function NM_observeActivity(aChannel, aActivityType, aActivitySubtype,
                              aTimestamp, aExtraSizeData, aExtraStringData)
  {
    if (!this.owner ||
        aActivityType != gActivityDistributor.ACTIVITY_TYPE_HTTP_TRANSACTION &&
        aActivityType != gActivityDistributor.ACTIVITY_TYPE_SOCKET_TRANSPORT) {
      return;
    }

    if (!(aChannel instanceof Ci.nsIHttpChannel)) {
      return;
    }

    aChannel = aChannel.QueryInterface(Ci.nsIHttpChannel);

    if (aActivitySubtype ==
        gActivityDistributor.ACTIVITY_SUBTYPE_REQUEST_HEADER) {
      this._onRequestHeader(aChannel, aTimestamp, aExtraStringData);
      return;
    }

    // Iterate over all currently ongoing requests. If aChannel can't
    // be found within them, then exit this function.
    let httpActivity = null;
    for each (let item in this.openRequests) {
      if (item.channel === aChannel) {
        httpActivity = item;
        break;
      }
    }

    if (!httpActivity) {
      return;
    }

    let transCodes = this.httpTransactionCodes;

    // Store the time information for this activity subtype.
    if (aActivitySubtype in transCodes) {
      let stage = transCodes[aActivitySubtype];
      if (stage in httpActivity.timings) {
        httpActivity.timings[stage].last = aTimestamp;
      }
      else {
        httpActivity.timings[stage] = {
          first: aTimestamp,
          last: aTimestamp,
        };
      }
    }

    switch (aActivitySubtype) {
      case gActivityDistributor.ACTIVITY_SUBTYPE_REQUEST_BODY_SENT:
        this._onRequestBodySent(httpActivity);
        break;
      case gActivityDistributor.ACTIVITY_SUBTYPE_RESPONSE_HEADER:
        this._onResponseHeader(httpActivity, aExtraStringData);
        break;
      case gActivityDistributor.ACTIVITY_SUBTYPE_TRANSACTION_CLOSE:
        this._onTransactionClose(httpActivity);
        break;
      default:
        break;
    }
  },

  /**
   * Handler for ACTIVITY_SUBTYPE_REQUEST_HEADER. When a request starts the
   * headers are sent to the server. This method creates the |httpActivity|
   * object where we store the request and response information that is
   * collected through its lifetime.
   *
   * @private
   * @param nsIHttpChannel aChannel
   * @param number aTimestamp
   * @param string aExtraStringData
   * @return void
   */
  _onRequestHeader:
  function NM__onRequestHeader(aChannel, aTimestamp, aExtraStringData)
  {
    let win = NetworkHelper.getWindowForRequest(aChannel);

    // Try to get the source window of the request.
    if (this.window && (!win || win.top !== this.window)) {
      return;
    }

    let httpActivity = this.createActivityObject(aChannel);

    // see NM__onRequestBodySent()
    httpActivity.charset = win ? win.document.characterSet : null;
    httpActivity.private = win ? PrivateBrowsingUtils.isWindowPrivate(win) : false;

    httpActivity.timings.REQUEST_HEADER = {
      first: aTimestamp,
      last: aTimestamp
    };

    let httpVersionMaj = {};
    let httpVersionMin = {};
    let event = {};
    event.startedDateTime = new Date(Math.round(aTimestamp / 1000)).toISOString();
    event.headersSize = aExtraStringData.length;
    event.method = aChannel.requestMethod;
    event.url = aChannel.URI.spec;
    event.private = httpActivity.private;

    // Determine if this is an XHR request.
    try {
      let callbacks = aChannel.notificationCallbacks;
      let xhrRequest = callbacks ? callbacks.getInterface(Ci.nsIXMLHttpRequest) : null;
      httpActivity.isXHR = event.isXHR = !!xhrRequest;
    } catch (e) {
      httpActivity.isXHR = event.isXHR = false;
    }

    // Determine the HTTP version.
    aChannel.QueryInterface(Ci.nsIHttpChannelInternal);
    aChannel.getRequestVersion(httpVersionMaj, httpVersionMin);

    event.httpVersion = "HTTP/" + httpVersionMaj.value + "." +
                                  httpVersionMin.value;

    event.discardRequestBody = !this.saveRequestAndResponseBodies;
    event.discardResponseBody = !this.saveRequestAndResponseBodies;

    let headers = [];
    let cookies = [];
    let cookieHeader = null;

    // Copy the request header data.
    aChannel.visitRequestHeaders({
      visitHeader: function NM__visitHeader(aName, aValue)
      {
        if (aName == "Cookie") {
          cookieHeader = aValue;
        }
        headers.push({ name: aName, value: aValue });
      }
    });

    if (cookieHeader) {
      cookies = NetworkHelper.parseCookieHeader(cookieHeader);
    }

    httpActivity.owner = this.owner.onNetworkEvent(event, aChannel);

    this._setupResponseListener(httpActivity);

    this.openRequests[httpActivity.id] = httpActivity;

    httpActivity.owner.addRequestHeaders(headers);
    httpActivity.owner.addRequestCookies(cookies);
  },

  /**
   * Create the empty HTTP activity object. This object is used for storing all
   * the request and response information.
   *
   * This is a HAR-like object. Conformance to the spec is not guaranteed at
   * this point.
   *
   * TODO: Bug 708717 - Add support for network log export to HAR
   *
   * @see http://www.softwareishard.com/blog/har-12-spec
   * @param nsIHttpChannel aChannel
   *        The HTTP channel for which the HTTP activity object is created.
   * @return object
   *         The new HTTP activity object.
   */
  createActivityObject: function NM_createActivityObject(aChannel)
  {
    return {
      id: gSequenceId(),
      channel: aChannel,
      charset: null, // see NM__onRequestHeader()
      url: aChannel.URI.spec,
      discardRequestBody: !this.saveRequestAndResponseBodies,
      discardResponseBody: !this.saveRequestAndResponseBodies,
      timings: {}, // internal timing information, see NM_observeActivity()
      responseStatus: null, // see NM__onResponseHeader()
      owner: null, // the activity owner which is notified when changes happen
    };
  },

  /**
   * Setup the network response listener for the given HTTP activity. The
   * NetworkResponseListener is responsible for storing the response body.
   *
   * @private
   * @param object aHttpActivity
   *        The HTTP activity object we are tracking.
   */
  _setupResponseListener: function NM__setupResponseListener(aHttpActivity)
  {
    let channel = aHttpActivity.channel;
    channel.QueryInterface(Ci.nsITraceableChannel);

    // The response will be written into the outputStream of this pipe.
    // This allows us to buffer the data we are receiving and read it
    // asynchronously.
    // Both ends of the pipe must be blocking.
    let sink = Cc["@mozilla.org/pipe;1"].createInstance(Ci.nsIPipe);

    // The streams need to be blocking because this is required by the
    // stream tee.
    sink.init(false, false, this.responsePipeSegmentSize, PR_UINT32_MAX, null);

    // Add listener for the response body.
    let newListener = new NetworkResponseListener(this, aHttpActivity);

    // Remember the input stream, so it isn't released by GC.
    newListener.inputStream = sink.inputStream;
    newListener.sink = sink;

    let tee = Cc["@mozilla.org/network/stream-listener-tee;1"].
              createInstance(Ci.nsIStreamListenerTee);

    let originalListener = channel.setNewListener(tee);

    tee.init(originalListener, sink.outputStream, newListener);
  },

  /**
   * Handler for ACTIVITY_SUBTYPE_REQUEST_BODY_SENT. The request body is logged
   * here.
   *
   * @private
   * @param object aHttpActivity
   *        The HTTP activity object we are working with.
   */
  _onRequestBodySent: function NM__onRequestBodySent(aHttpActivity)
  {
    if (aHttpActivity.discardRequestBody) {
      return;
    }

    let sentBody = NetworkHelper.
                   readPostTextFromRequest(aHttpActivity.channel,
                                           aHttpActivity.charset);

    if (!sentBody && this.window &&
        aHttpActivity.url == this.window.location.href) {
      // If the request URL is the same as the current page URL, then
      // we can try to get the posted text from the page directly.
      // This check is necessary as otherwise the
      //   NetworkHelper.readPostTextFromPageViaWebNav()
      // function is called for image requests as well but these
      // are not web pages and as such don't store the posted text
      // in the cache of the webpage.
      let webNav = this.window.QueryInterface(Ci.nsIInterfaceRequestor).
                   getInterface(Ci.nsIWebNavigation);
      sentBody = NetworkHelper.
                 readPostTextFromPageViaWebNav(webNav, aHttpActivity.charset);
    }

    if (sentBody) {
      aHttpActivity.owner.addRequestPostData({ text: sentBody });
    }
  },

  /**
   * Handler for ACTIVITY_SUBTYPE_RESPONSE_HEADER. This method stores
   * information about the response headers.
   *
   * @private
   * @param object aHttpActivity
   *        The HTTP activity object we are working with.
   * @param string aExtraStringData
   *        The uncached response headers.
   */
  _onResponseHeader:
  function NM__onResponseHeader(aHttpActivity, aExtraStringData)
  {
    // aExtraStringData contains the uncached response headers. The first line
    // contains the response status (e.g. HTTP/1.1 200 OK).
    //
    // Note: The response header is not saved here. Calling the
    // channel.visitResponseHeaders() methood at this point sometimes causes an
    // NS_ERROR_NOT_AVAILABLE exception.
    //
    // We could parse aExtraStringData to get the headers and their values, but
    // that is not trivial to do in an accurate manner. Hence, we save the
    // response headers in this._httpResponseExaminer().

    let headers = aExtraStringData.split(/\r\n|\n|\r/);
    let statusLine = headers.shift();
    let statusLineArray = statusLine.split(" ");

    let response = {};
    response.httpVersion = statusLineArray.shift();
    response.status = statusLineArray.shift();
    response.statusText = statusLineArray.join(" ");
    response.headersSize = aExtraStringData.length;

    aHttpActivity.responseStatus = response.status;

    // Discard the response body for known response statuses.
    switch (parseInt(response.status)) {
      case HTTP_MOVED_PERMANENTLY:
      case HTTP_FOUND:
      case HTTP_SEE_OTHER:
      case HTTP_TEMPORARY_REDIRECT:
        aHttpActivity.discardResponseBody = true;
        break;
    }

    response.discardResponseBody = aHttpActivity.discardResponseBody;

    aHttpActivity.owner.addResponseStart(response);
  },

  /**
   * Handler for ACTIVITY_SUBTYPE_TRANSACTION_CLOSE. This method updates the HAR
   * timing information on the HTTP activity object and clears the request
   * from the list of known open requests.
   *
   * @private
   * @param object aHttpActivity
   *        The HTTP activity object we work with.
   */
  _onTransactionClose: function NM__onTransactionClose(aHttpActivity)
  {
    let result = this._setupHarTimings(aHttpActivity);
    aHttpActivity.owner.addEventTimings(result.total, result.timings);
    delete this.openRequests[aHttpActivity.id];
  },

  /**
   * Update the HTTP activity object to include timing information as in the HAR
   * spec. The HTTP activity object holds the raw timing information in
   * |timings| - these are timings stored for each activity notification. The
   * HAR timing information is constructed based on these lower level data.
   *
   * @param object aHttpActivity
   *        The HTTP activity object we are working with.
   * @return object
   *         This object holds two properties:
   *         - total - the total time for all of the request and response.
   *         - timings - the HAR timings object.
   */
  _setupHarTimings: function NM__setupHarTimings(aHttpActivity)
  {
    let timings = aHttpActivity.timings;
    let harTimings = {};

    // Not clear how we can determine "blocked" time.
    harTimings.blocked = -1;

    // DNS timing information is available only in when the DNS record is not
    // cached.
    harTimings.dns = timings.STATUS_RESOLVING && timings.STATUS_RESOLVED ?
                     timings.STATUS_RESOLVED.last -
                     timings.STATUS_RESOLVING.first : -1;

    if (timings.STATUS_CONNECTING_TO && timings.STATUS_CONNECTED_TO) {
      harTimings.connect = timings.STATUS_CONNECTED_TO.last -
                           timings.STATUS_CONNECTING_TO.first;
    }
    else if (timings.STATUS_SENDING_TO) {
      harTimings.connect = timings.STATUS_SENDING_TO.first -
                           timings.REQUEST_HEADER.first;
    }
    else {
      harTimings.connect = -1;
    }

    if ((timings.STATUS_WAITING_FOR || timings.STATUS_RECEIVING_FROM) &&
        (timings.STATUS_CONNECTED_TO || timings.STATUS_SENDING_TO)) {
      harTimings.send = (timings.STATUS_WAITING_FOR ||
                         timings.STATUS_RECEIVING_FROM).first -
                        (timings.STATUS_CONNECTED_TO ||
                         timings.STATUS_SENDING_TO).last;
    }
    else {
      harTimings.send = -1;
    }

    if (timings.RESPONSE_START) {
      harTimings.wait = timings.RESPONSE_START.first -
                        (timings.REQUEST_BODY_SENT ||
                         timings.STATUS_SENDING_TO).last;
    }
    else {
      harTimings.wait = -1;
    }

    if (timings.RESPONSE_START && timings.RESPONSE_COMPLETE) {
      harTimings.receive = timings.RESPONSE_COMPLETE.last -
                           timings.RESPONSE_START.first;
    }
    else {
      harTimings.receive = -1;
    }

    let totalTime = 0;
    for (let timing in harTimings) {
      let time = Math.max(Math.round(harTimings[timing] / 1000), -1);
      harTimings[timing] = time;
      if (time > -1) {
        totalTime += time;
      }
    }

    return {
      total: totalTime,
      timings: harTimings,
    };
  },

  /**
   * Suspend Web Console activity. This is called when all Web Consoles are
   * closed.
   */
  destroy: function NM_destroy()
  {
    if (Services.appinfo.processType != Ci.nsIXULRuntime.PROCESS_TYPE_CONTENT) {
      Services.obs.removeObserver(this._httpResponseExaminer,
                                  "http-on-examine-response");
    }

    gActivityDistributor.removeObserver(this);

    this.openRequests = {};
    this.openResponses = {};
    this.owner = null;
    this.window = null;
  },
};

_global.NetworkMonitor = NetworkMonitor;
_global.NetworkResponseListener = NetworkResponseListener;
})(this, WebConsoleUtils);

/**
 * A WebProgressListener that listens for location changes.
 *
 * This progress listener is used to track file loads and other kinds of
 * location changes.
 *
 * @constructor
 * @param object aWindow
 *        The window for which we need to track location changes.
 * @param object aOwner
 *        The listener owner which needs to implement two methods:
 *        - onFileActivity(aFileURI)
 *        - onLocationChange(aState, aTabURI, aPageTitle)
 */
this.ConsoleProgressListener =
 function ConsoleProgressListener(aWindow, aOwner)
{
  this.window = aWindow;
  this.owner = aOwner;
}

ConsoleProgressListener.prototype = {
  /**
   * Constant used for startMonitor()/stopMonitor() that tells you want to
   * monitor file loads.
   */
  MONITOR_FILE_ACTIVITY: 1,

  /**
   * Constant used for startMonitor()/stopMonitor() that tells you want to
   * monitor page location changes.
   */
  MONITOR_LOCATION_CHANGE: 2,

  /**
   * Tells if you want to monitor file activity.
   * @private
   * @type boolean
   */
  _fileActivity: false,

  /**
   * Tells if you want to monitor location changes.
   * @private
   * @type boolean
   */
  _locationChange: false,

  /**
   * Tells if the console progress listener is initialized or not.
   * @private
   * @type boolean
   */
  _initialized: false,

  _webProgress: null,

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                         Ci.nsISupportsWeakReference]),

  /**
   * Initialize the ConsoleProgressListener.
   * @private
   */
  _init: function CPL__init()
  {
    if (this._initialized) {
      return;
    }

    this._webProgress = this.window.QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIWebNavigation)
                        .QueryInterface(Ci.nsIWebProgress);
    this._webProgress.addProgressListener(this,
                                          Ci.nsIWebProgress.NOTIFY_STATE_ALL);

    this._initialized = true;
  },

  /**
   * Start a monitor/tracker related to the current nsIWebProgressListener
   * instance.
   *
   * @param number aMonitor
   *        Tells what you want to track. Available constants:
   *        - this.MONITOR_FILE_ACTIVITY
   *          Track file loads.
   *        - this.MONITOR_LOCATION_CHANGE
   *          Track location changes for the top window.
   */
  startMonitor: function CPL_startMonitor(aMonitor)
  {
    switch (aMonitor) {
      case this.MONITOR_FILE_ACTIVITY:
        this._fileActivity = true;
        break;
      case this.MONITOR_LOCATION_CHANGE:
        this._locationChange = true;
        break;
      default:
        throw new Error("ConsoleProgressListener: unknown monitor type " +
                        aMonitor + "!");
    }
    this._init();
  },

  /**
   * Stop a monitor.
   *
   * @param number aMonitor
   *        Tells what you want to stop tracking. See this.startMonitor() for
   *        the list of constants.
   */
  stopMonitor: function CPL_stopMonitor(aMonitor)
  {
    switch (aMonitor) {
      case this.MONITOR_FILE_ACTIVITY:
        this._fileActivity = false;
        break;
      case this.MONITOR_LOCATION_CHANGE:
        this._locationChange = false;
        break;
      default:
        throw new Error("ConsoleProgressListener: unknown monitor type " +
                        aMonitor + "!");
    }

    if (!this._fileActivity && !this._locationChange) {
      this.destroy();
    }
  },

  onStateChange:
  function CPL_onStateChange(aProgress, aRequest, aState, aStatus)
  {
    if (!this.owner) {
      return;
    }

    if (this._fileActivity) {
      this._checkFileActivity(aProgress, aRequest, aState, aStatus);
    }

    if (this._locationChange) {
      this._checkLocationChange(aProgress, aRequest, aState, aStatus);
    }
  },

  /**
   * Check if there is any file load, given the arguments of
   * nsIWebProgressListener.onStateChange. If the state change tells that a file
   * URI has been loaded, then the remote Web Console instance is notified.
   * @private
   */
  _checkFileActivity:
  function CPL__checkFileActivity(aProgress, aRequest, aState, aStatus)
  {
    if (!(aState & Ci.nsIWebProgressListener.STATE_START)) {
      return;
    }

    let uri = null;
    if (aRequest instanceof Ci.imgIRequest) {
      let imgIRequest = aRequest.QueryInterface(Ci.imgIRequest);
      uri = imgIRequest.URI;
    }
    else if (aRequest instanceof Ci.nsIChannel) {
      let nsIChannel = aRequest.QueryInterface(Ci.nsIChannel);
      uri = nsIChannel.URI;
    }

    if (!uri || !uri.schemeIs("file") && !uri.schemeIs("ftp")) {
      return;
    }

    this.owner.onFileActivity(uri.spec);
  },

  /**
   * Check if the current window.top location is changing, given the arguments
   * of nsIWebProgressListener.onStateChange. If that is the case, the remote
   * Web Console instance is notified.
   * @private
   */
  _checkLocationChange:
  function CPL__checkLocationChange(aProgress, aRequest, aState, aStatus)
  {
    let isStart = aState & Ci.nsIWebProgressListener.STATE_START;
    let isStop = aState & Ci.nsIWebProgressListener.STATE_STOP;
    let isNetwork = aState & Ci.nsIWebProgressListener.STATE_IS_NETWORK;
    let isWindow = aState & Ci.nsIWebProgressListener.STATE_IS_WINDOW;

    // Skip non-interesting states.
    if (!isNetwork || !isWindow || aProgress.DOMWindow != this.window) {
      return;
    }

    if (isStart && aRequest instanceof Ci.nsIChannel) {
      this.owner.onLocationChange("start", aRequest.URI.spec, "");
    }
    else if (isStop) {
      this.owner.onLocationChange("stop", this.window.location.href,
                                  this.window.document.title);
    }
  },

  onLocationChange: function() {},
  onStatusChange: function() {},
  onProgressChange: function() {},
  onSecurityChange: function() {},

  /**
   * Destroy the ConsoleProgressListener.
   */
  destroy: function CPL_destroy()
  {
    if (!this._initialized) {
      return;
    }

    this._initialized = false;
    this._fileActivity = false;
    this._locationChange = false;

    try {
      this._webProgress.removeProgressListener(this);
    }
    catch (ex) {
      // This can throw during browser shutdown.
    }

    this._webProgress = null;
    this.window = null;
    this.owner = null;
  },
};

function gSequenceId()
{
  return gSequenceId.n++;
}
gSequenceId.n = 0;
