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

var EXPORTED_SYMBOLS = ["WebConsoleUtils", "JSPropertyProvider", "JSTermHelpers",
                        "PageErrorListener", "ConsoleAPIListener"];

// Match the function name from the result of toString() or toSource().
//
// Examples:
// (function foobar(a, b) { ...
// function foobar2(a) { ...
// function() { ...
const REGEX_MATCH_FUNCTION_NAME = /^\(?function\s+([^(\s]+)\s*\(/;

// Match the function arguments from the result of toString() or toSource().
const REGEX_MATCH_FUNCTION_ARGS = /^\(?function\s*[^\s(]*\s*\((.+?)\)/;

const TYPES = { OBJECT: 0,
                FUNCTION: 1,
                ARRAY: 2,
                OTHER: 3,
                ITERATOR: 4,
                GETTER: 5,
                GENERATOR: 6,
                STRING: 7
              };

var gObjectId = 0;

var WebConsoleUtils = {
  TYPES: TYPES,

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
   * Gets the window that has the given outer ID.
   *
   * @param integer aOuterId
   * @param nsIDOMWindow [aHintWindow]
   *        Optional, the window object used to QueryInterface to
   *        nsIDOMWindowUtils. If this is not given,
   *        Services.wm.getMostRecentWindow() is used.
   * @return nsIDOMWindow|null
   *         The window object with the given outer ID.
   */
  getWindowByOuterId: function WCU_getWindowByOuterId(aOuterId, aHintWindow)
  {
    let someWindow = aHintWindow || Services.wm.getMostRecentWindow(null);
    let content = null;

    if (someWindow) {
      let windowUtils = someWindow.QueryInterface(Ci.nsIInterfaceRequestor).
                                   getInterface(Ci.nsIDOMWindowUtils);
      content = windowUtils.getOuterWindowWithId(aOuterId);
    }

    return content;
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
   * Format the jsterm execution result based on its type.
   *
   * @param mixed aResult
   *        The evaluation result object you want displayed.
   * @return string
   *         The string that can be displayed.
   */
  formatResult: function WCU_formatResult(aResult)
  {
    let output = "";
    let type = this.getResultType(aResult);

    switch (type) {
      case "string":
        output = this.formatResultString(aResult);
        break;
      case "boolean":
      case "date":
      case "error":
      case "number":
      case "regexp":
        try {
          output = aResult + "";
        }
        catch (ex) {
          output = ex;
        }
        break;
      case "null":
      case "undefined":
        output = type;
        break;
      default:
        try {
          if (aResult.toSource) {
            output = aResult.toSource();
          }
          if (!output || output == "({})") {
            output = aResult + "";
          }
        }
        catch (ex) {
          output = ex;
        }
        break;
    }

    return output + "";
  },

  /**
   * Format a string for output.
   *
   * @param string aString
   *        The string you want to display.
   * @return string
   *         The string that can be displayed.
   */
  formatResultString: function WCU_formatResultString(aString)
  {
    function isControlCode(c) {
      // See http://en.wikipedia.org/wiki/C0_and_C1_control_codes
      // C0 is 0x00-0x1F, C1 is 0x80-0x9F (inclusive).
      // We also include DEL (U+007F) and NBSP (U+00A0), which are not strictly
      // in C1 but border it.
      return (c <= 0x1F) || (0x7F <= c && c <= 0xA0);
    }

    function replaceFn(aMatch, aType, aHex) {
      // Leave control codes escaped, but unescape the rest of the characters.
      let c = parseInt(aHex, 16);
      return isControlCode(c) ? aMatch : String.fromCharCode(c);
    }

    let output = uneval(aString).replace(/\\(x)([0-9a-fA-F]{2})/g, replaceFn)
                 .replace(/\\(u)([0-9a-fA-F]{4})/g, replaceFn);

    return output;
  },

  /**
   * Determine if an object can be inspected or not.
   *
   * @param mixed aObject
   *        The object you want to check if it can be inspected.
   * @return boolean
   *         True if the object is inspectable or false otherwise.
   */
  isObjectInspectable: function WCU_isObjectInspectable(aObject)
  {
    let isEnumerable = false;

    // Skip Iterators and Generators.
    if (this.isIteratorOrGenerator(aObject)) {
      return false;
    }

    try {
      for (let p in aObject) {
        isEnumerable = true;
        break;
      }
    }
    catch (ex) {
      // Proxy objects can lack an enumerable method.
    }

    return isEnumerable && typeof(aObject) != "string";
  },

  /**
   * Determine the type of the jsterm execution result.
   *
   * @param mixed aResult
   *        The evaluation result object you want to check.
   * @return string
   *         Constructor name or type: string, number, boolean, regexp, date,
   *         function, object, null, undefined...
   */
  getResultType: function WCU_getResultType(aResult)
  {
    let type = aResult === null ? "null" : typeof aResult;
    try {
      if (type == "object" && aResult.constructor && aResult.constructor.name) {
        type = aResult.constructor.name + "";
      }
    }
    catch (ex) {
      // Prevent potential exceptions in page-provided objects from taking down
      // the Web Console. If the constructor.name is a getter that throws, or
      // something else bad happens.
    }

    return type.toLowerCase();
  },

  /**
   * Figures out the type of aObject and the string to display as the object
   * value.
   *
   * @see TYPES
   * @param object aObject
   *        The object to operate on.
   * @return object
   *         An object of the form:
   *         {
   *           type: TYPES.OBJECT || TYPES.FUNCTION || ...
   *           display: string for displaying the object
   *         }
   */
  presentableValueFor: function WCU_presentableValueFor(aObject)
  {
    let type = this.getResultType(aObject);
    let presentable;

    switch (type) {
      case "undefined":
      case "null":
        return {
          type: TYPES.OTHER,
          display: type
        };

      case "array":
        return {
          type: TYPES.ARRAY,
          display: "Array"
        };

      case "string":
        return {
          type: TYPES.STRING,
          display: "\"" + aObject + "\""
        };

      case "date":
      case "regexp":
      case "number":
      case "boolean":
        return {
          type: TYPES.OTHER,
          display: aObject.toString()
        };

      case "iterator":
        return {
          type: TYPES.ITERATOR,
          display: "Iterator"
        };

      case "function":
        presentable = aObject.toString();
        return {
          type: TYPES.FUNCTION,
          display: presentable.substring(0, presentable.indexOf(')') + 1)
        };

      default:
        presentable = String(aObject);
        let m = /^\[object (\S+)\]/.exec(presentable);

        try {
          if (typeof aObject == "object" && typeof aObject.next == "function" &&
              m && m[1] == "Generator") {
            return {
              type: TYPES.GENERATOR,
              display: m[1]
            };
          }
        }
        catch (ex) {
          // window.history.next throws in the typeof check above.
          return {
            type: TYPES.OBJECT,
            display: m ? m[1] : "Object"
          };
        }

        if (typeof aObject == "object" &&
            typeof aObject.__iterator__ == "function") {
          return {
            type: TYPES.ITERATOR,
            display: "Iterator"
          };
        }

        return {
          type: TYPES.OBJECT,
          display: m ? m[1] : "Object"
        };
    }
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
   * Get an array that describes the properties of the given object.
   *
   * @param object aObject
   *        The object to get the properties from.
   * @param object aObjectCache
   *        Optional object cache where to store references to properties of
   *        aObject that are inspectable. See this.isObjectInspectable().
   * @return array
   *         An array that describes each property from the given object. Each
   *         array element is an object (a property descriptor). Each property
   *         descriptor has the following properties:
   *         - name - property name
   *         - value - a presentable property value representation (see
   *                   this.presentableValueFor())
   *         - type - value type (see this.presentableValueFor())
   *         - inspectable - tells if the property value holds further
   *                         properties (see this.isObjectInspectable()).
   *         - objectId - optional, available only if aObjectCache is given and
   *         if |inspectable| is true. You can do
   *         aObjectCache[propertyDescriptor.objectId] to get the actual object
   *         referenced by the property of aObject.
   */
  namesAndValuesOf: function WCU_namesAndValuesOf(aObject, aObjectCache)
  {
    let pairs = [];
    let value, presentable;

    let isDOMDocument = aObject instanceof Ci.nsIDOMDocument;
    let deprecated = ["width", "height", "inputEncoding"];

    for (let propName in aObject) {
      // See bug 632275: skip deprecated properties.
      if (isDOMDocument && deprecated.indexOf(propName) > -1) {
        continue;
      }

      // Also skip non-native getters.
      if (this.isNonNativeGetter(aObject, propName)) {
        value = "";
        presentable = {type: TYPES.GETTER, display: "Getter"};
      }
      else {
        try {
          value = aObject[propName];
          presentable = this.presentableValueFor(value);
        }
        catch (ex) {
          continue;
        }
      }

      let pair = {};
      pair.name = propName;
      pair.value = presentable.display;
      pair.inspectable = false;
      pair.type = presentable.type;

      switch (presentable.type) {
        case TYPES.GETTER:
        case TYPES.ITERATOR:
        case TYPES.GENERATOR:
        case TYPES.STRING:
          break;
        default:
          try {
            for (let p in value) {
              pair.inspectable = true;
              break;
            }
          }
          catch (ex) { }
          break;
      }

      // Store the inspectable object.
      if (pair.inspectable && aObjectCache) {
        pair.objectId = ++gObjectId;
        aObjectCache[pair.objectId] = value;
      }

      pairs.push(pair);
    }

    pairs.sort(this.propertiesSort);

    return pairs;
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
   * Inspect the properties of the given object. For each property a descriptor
   * object is created. The descriptor gives you information about the property
   * name, value, type, getter and setter. When the property value references
   * another object you get a wrapper that holds information about that object.
   *
   * @see this.inspectObjectProperty
   * @param object aObject
   *        The object you want to inspect.
   * @param function aObjectWrapper
   *        The function that creates wrappers for property values which
   *        reference other objects. This function must take one argument, the
   *        object to wrap, and it must return an object grip that gives
   *        information about the referenced object.
   * @return array
   *         An array of property descriptors.
   */
  inspectObject: function WCU_inspectObject(aObject, aObjectWrapper)
  {
    let properties = [];
    let isDOMDocument = aObject instanceof Ci.nsIDOMDocument;
    let deprecated = ["width", "height", "inputEncoding"];

    for (let name in aObject) {
      // See bug 632275: skip deprecated properties.
      if (isDOMDocument && deprecated.indexOf(name) > -1) {
        continue;
      }

      properties.push(this.inspectObjectProperty(aObject, name, aObjectWrapper));
    }

    return properties.sort(this.propertiesSort);
  },

  /**
   * A helper method that creates a property descriptor for the provided object,
   * properly formatted for sending in a protocol response.
   *
   * The property value can reference other objects. Since actual objects cannot
   * be sent to the client, we need to send simple object grips - descriptors
   * for those objects. This is why you need to give an object wrapper function
   * that creates object grips.
   *
   * @param string aProperty
   *        Property name for which we have the descriptor.
   * @param object aObject
   *        The object that the descriptor is generated for.
   * @param function aObjectWrapper
   *        This function is given the property value. Whatever the function
   *        returns is used as the representation of the property value.
   * @return object
   *         The property descriptor formatted for sending to the client.
   */
  inspectObjectProperty:
  function WCU_inspectObjectProperty(aObject, aProperty, aObjectWrapper)
  {
    let descriptor = this.getPropertyDescriptor(aObject, aProperty) || {};

    let result = { name: aProperty };
    result.configurable = descriptor.configurable;
    result.enumerable = descriptor.enumerable;
    result.writable = descriptor.writable;
    if (descriptor.value !== undefined) {
      result.value = this.createValueGrip(descriptor.value, aObjectWrapper);
    }
    else if (descriptor.get) {
      if (this.isNativeFunction(descriptor.get)) {
        result.value = this.createValueGrip(aObject[aProperty], aObjectWrapper);
      }
      else {
        result.get = this.createValueGrip(descriptor.get, aObjectWrapper);
        result.set = this.createValueGrip(descriptor.set, aObjectWrapper);
      }
    }

    // There are cases with properties that have no value and no getter. For
    // example window.screen.width.
    if (result.value === undefined && result.get === undefined) {
      result.value = this.createValueGrip(aObject[aProperty], aObjectWrapper);
    }

    return result;
  },

  /**
   * Make an object grip for the given object. An object grip of the simplest
   * form with minimal information about the given object is returned. This
   * method is usually combined with other functions that add further state
   * information and object ID such that, later, the client is able to retrieve
   * more information about the object being represented by this grip.
   *
   * @param object aObject
   *        The object you want to create a grip for.
   * @return object
   *         The object grip.
   */
  getObjectGrip: function WCU_getObjectGrip(aObject)
  {
    let className = null;
    let type = typeof aObject;

    let result = {
      "type": type,
      "className": this.getObjectClassName(aObject),
      "displayString": this.formatResult(aObject),
      "inspectable": this.isObjectInspectable(aObject),
    };

    if (type == "function") {
      result.functionName = this.getFunctionName(aObject);
      result.functionArguments = this.getFunctionArguments(aObject);
    }

    return result;
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
      case "string":
      case "number":
        return aValue;
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
   * Make a string representation for an object actor grip.
   *
   * @param object aGrip
   *        The object grip received from the server.
   * @param boolean [aFormatString=false]
   *        Optional boolean that tells if you want strings to be unevaled or
   *        not.
   * @return string
   *         The object grip converted to a string.
   */
  objectActorGripToString: function WCU_objectActorGripToString(aGrip, aFormatString)
  {
    // Primitives like strings and numbers are not sent as objects.
    // But null and undefined are sent as objects with the type property
    // telling which type of value we have.
    let type = typeof(aGrip);
    if (aGrip && type == "object") {
      return aGrip.displayString || aGrip.className || aGrip.type || type;
    }
    return type == "string" && aFormatString ?
           this.formatResultString(aGrip) : aGrip + "";
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
   * Helper function to deduce the arguments of the provided function.
   *
   * @param funtion aFunction
   *        The function whose name will be returned.
   * @return array
   *         Function arguments.
   */
  getFunctionArguments: function WCF_getFunctionArguments(aFunction)
  {
    let args = [];
    try {
      let str = (aFunction.toString() || aFunction.toSource()) + "";
      let argsString = (str.match(REGEX_MATCH_FUNCTION_ARGS) || [])[1];
      if (argsString) {
        args = argsString.split(/\s*,\s*/);
      }
    }
    catch (ex) { }
    return args;
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
  getObjectClassName: function WCF_getObjectClassName(aObject)
  {
    if (aObject === null) {
      return "null";
    }
    if (aObject === undefined) {
      return "undefined";
    }

    let type = typeof aObject;
    if (type != "object") {
      return type;
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
   * Determine the string to display as a property value in the property panel.
   *
   * @param object aActor
   *        Object actor grip.
   * @return string
   *         Property value as suited for the property panel.
   */
  getPropertyPanelValue: function WCU_getPropertyPanelValue(aActor)
  {
    if (aActor.get) {
      return "Getter";
    }

    let val = aActor.value;
    if (typeof val == "string") {
      return this.formatResultString(val);
    }

    if (typeof val != "object" || !val) {
      return val;
    }

    if (val.type == "function" && val.functionName) {
      return "function " + val.functionName + "(" +
             val.functionArguments.join(", ") + ")";
    }
    if (val.type == "object" && val.className) {
      return val.className;
    }

    return val.displayString || val.type;
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

var JSPropertyProvider = (function _JSPP(WCU) {
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
 *
 * @param string aInputValue
 *        Value that should be completed.
 *
 * @returns null or object
 *          If no completion valued could be computed, null is returned,
 *          otherwise a object with the following form is returned:
 *            {
 *              matches: [ string, string, string ],
 *              matchProp: Last part of the inputValue that was used to find
 *                         the matches-strings.
 *            }
 */
function JSPropertyProvider(aScope, aInputValue)
{
  let obj = WCU.unwrap(aScope);

  // Analyse the aInputValue and find the beginning of the last part that
  // should be completed.
  let beginning = findCompletionBeginning(aInputValue);

  // There was an error analysing the string.
  if (beginning.err) {
    return null;
  }

  // If the current state is not STATE_NORMAL, then we are inside of an string
  // which means that no completion is possible.
  if (beginning.state != STATE_NORMAL) {
    return null;
  }

  let completionPart = aInputValue.substring(beginning.startPos);

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

    // Skip Iterators and Generators.
    if (WCU.isIteratorOrGenerator(obj)) {
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
  let names = {};   // Using an Object to avoid duplicates.

  // We need to go up the prototype chain.
  let ownNames = null;
  while (aObj !== null) {
    ownNames = Object.getOwnPropertyNames(aObj);
    for (let i = 0; i < ownNames.length; i++) {
      // Filtering happens here.
      // If we already have it in, no need to append it.
      if (ownNames[i].indexOf(aOptions.matchProp) != 0 ||
          names[ownNames[i]] == true) {
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
 * The nsIConsoleService listener. This is used to send all the page errors
 * (JavaScript, CSS and more) to the remote Web Console instance.
 *
 * @constructor
 * @param nsIDOMWindow aWindow
 *        The window object for which we are created.
 * @param object aListener
 *        The listener object must have a method: onPageError. This method is
 *        invoked with one argument, the nsIScriptError, whenever a relevant
 *        page error is received.
 */
function PageErrorListener(aWindow, aListener)
{
  this.window = aWindow;
  this.listener = aListener;
}

PageErrorListener.prototype =
{
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIConsoleListener]),

  /**
   * The content window for which we listen to page errors.
   * @type nsIDOMWindow
   */
  window: null,

  /**
   * The listener object which is notified of page errors. It must have
   * a onPageError method which is invoked with one argument: the nsIScriptError.
   * @type object
   */
  listener: null,

  /**
   * Initialize the nsIConsoleService listener.
   */
  init: function PEL_init()
  {
    Services.console.registerListener(this);
  },

  /**
   * The nsIConsoleService observer. This method takes all the script error
   * messages belonging to the current window and sends them to the remote Web
   * Console instance.
   *
   * @param nsIScriptError aScriptError
   *        The script error object coming from the nsIConsoleService.
   */
  observe: function PEL_observe(aScriptError)
  {
    if (!this.window || !this.listener ||
        !(aScriptError instanceof Ci.nsIScriptError) ||
        !aScriptError.outerWindowID) {
      return;
    }

    if (!this.isCategoryAllowed(aScriptError.category)) {
      return;
    }

    let errorWindow =
      WebConsoleUtils.getWindowByOuterId(aScriptError.outerWindowID, this.window);
    if (!errorWindow || errorWindow.top != this.window) {
      return;
    }

    this.listener.onPageError(aScriptError);
  },

  /**
   * Check if the given script error category is allowed to be tracked or not.
   * We ignore chrome-originating errors as we only care about content.
   *
   * @param string aCategory
   *        The nsIScriptError category you want to check.
   * @return boolean
   *         True if the category is allowed to be logged, false otherwise.
   */
  isCategoryAllowed: function PEL_isCategoryAllowed(aCategory)
  {
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
   * Get the cached page errors for the current inner window.
   *
   * @return array
   *         The array of cached messages. Each element is an nsIScriptError
   *         with an added _type property so the remote Web Console instance can
   *         tell the difference between various types of cached messages.
   */
  getCachedMessages: function PEL_getCachedMessages()
  {
    let innerWindowId = WebConsoleUtils.getInnerWindowId(this.window);
    let result = [];
    let errors = {};
    Services.console.getMessageArray(errors, {});

    (errors.value || []).forEach(function(aError) {
      if (!(aError instanceof Ci.nsIScriptError) ||
          aError.innerWindowID != innerWindowId ||
          !this.isCategoryAllowed(aError.category)) {
        return;
      }

      let remoteMessage = WebConsoleUtils.cloneObject(aError);
      result.push(remoteMessage);
    }, this);

    return result;
  },

  /**
   * Remove the nsIConsoleService listener.
   */
  destroy: function PEL_destroy()
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
 *        The window object for which we are created.
 * @param object aOwner
 *        The owner object must have the following methods:
 *        - onConsoleAPICall(). This method is invoked with one argument, the
 *        Console API message that comes from the observer service, whenever
 *        a relevant console API call is received.
 */
function ConsoleAPIListener(aWindow, aOwner)
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
    if (!this.owner || !this.window) {
      return;
    }

    let apiMessage = aMessage.wrappedJSObject;
    let msgWindow = WebConsoleUtils.getWindowByOuterId(apiMessage.ID,
                                                       this.window);
    if (!msgWindow || msgWindow.top != this.window) {
      // Not the same window!
      return;
    }

    this.owner.onConsoleAPICall(apiMessage);
  },

  /**
   * Get the cached messages for the current inner window.
   *
   * @return array
   *         The array of cached messages. Each element is a Console API
   *         prepared to be sent to the remote Web Console instance.
   */
  getCachedMessages: function CAL_getCachedMessages()
  {
    let innerWindowId = WebConsoleUtils.getInnerWindowId(this.window);
    let messages = ConsoleAPIStorage.getEvents(innerWindowId);
    return messages;
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
function JSTermHelpers(aOwner)
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
    let nodes = [];
    let doc = aOwner.window.document;
    let aContext = aContext || doc;

    try {
      let results = doc.evaluate(aXPath, aContext, null,
                                 Ci.nsIDOMXPathResult.ANY_TYPE, null);
      let node;
      while (node = results.iterateNext()) {
        nodes.push(node);
      }
    }
    catch (ex) {
      aOwner.window.console.error(ex.message);
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
      try {
        return aOwner.chromeWindow().InspectorUI.selection;
      }
      catch (ex) {
        aOwner.window.console.error(ex.message);
      }
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
    return Object.keys(WebConsoleUtils.unwrap(aObject));
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
    let arrValues = [];
    let obj = WebConsoleUtils.unwrap(aObject);

    try {
      for (let prop in obj) {
        arrValues.push(obj[prop]);
      }
    }
    catch (ex) {
      aOwner.window.console.error(ex.message);
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
    let obj = WebConsoleUtils.unwrap(aObject);
    if (!WebConsoleUtils.isObjectInspectable(obj)) {
      return aObject;
    }

    aOwner.helperResult = {
      type: "inspectObject",
      input: aOwner.evalInput,
      object: aOwner.createValueGrip(obj),
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
    let getObjectGrip = WebConsoleUtils.getObjectGrip.bind(WebConsoleUtils);
    let obj = WebConsoleUtils.unwrap(aObject);
    let props = WebConsoleUtils.inspectObject(obj, getObjectGrip);
    props.forEach(function(aProp) {
      output.push(aProp.name + ": " +
                  WebConsoleUtils.getPropertyPanelValue(aProp));
    });

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
}
