/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * The ElementManager manages DOM references and interactions with elements.
 * According to the WebDriver spec (http://code.google.com/p/selenium/wiki/JsonWireProtocol), the
 * server sends the client an element reference, and maintains the map of reference to element.
 * The client uses this reference when querying/interacting with the element, and the 
 * server uses maps this reference to the actual element when it executes the command.
 */

let EXPORTED_SYMBOLS = ["ElementManager", "CLASS_NAME", "SELECTOR", "ID", "NAME", "LINK_TEXT", "PARTIAL_LINK_TEXT", "TAG", "XPATH"];

let uuidGen = Components.classes["@mozilla.org/uuid-generator;1"]
             .getService(Components.interfaces.nsIUUIDGenerator);

let CLASS_NAME = "class name";
let SELECTOR = "css selector";
let ID = "id";
let NAME = "name";
let LINK_TEXT = "link text";
let PARTIAL_LINK_TEXT = "partial link text";
let TAG = "tag name";
let XPATH = "xpath";

function ElementException(msg, num, stack) {
  this.message = msg;
  this.num = num;
  this.stack = stack;
}

/* NOTE: Bug 736592 has been created to replace seenItems with a weakRef map */
function ElementManager(notSupported) {
  this.searchTimeout = 0;
  this.seenItems = {};
  this.timer = Components.classes["@mozilla.org/timer;1"].createInstance(Components.interfaces.nsITimer);
  this.elementStrategies = [CLASS_NAME, SELECTOR, ID, NAME, LINK_TEXT, PARTIAL_LINK_TEXT, TAG, XPATH];
  for (let i = 0; i < notSupported.length; i++) {
    this.elementStrategies.splice(this.elementStrategies.indexOf(notSupported[i]), 1);
  }
}

ElementManager.prototype = {
  /**
   * Reset values
   */
  reset: function EM_clear() {
    this.searchTimeout = 0;
    this.seenItems = {};
  },

  /**
  * Add element to list of seen elements
  *
  * @param nsIDOMElement element
  *        The element to add
  *
  * @return string
  *        Returns the server-assigned reference ID
  */
  addToKnownElements: function EM_addToKnownElements(element) {
    for (let i in this.seenItems) {
      if (this.seenItems[i] == element) {
        return i;
      }
    }
    var id = uuidGen.generateUUID().toString();
    this.seenItems[id] = element;
    return id;
  },
  
  /**
   * Retrieve element from its unique ID
   *
   * @param String id
   *        The DOM reference ID
   * @param nsIDOMWindow win
   *        The window that contains the element
   *
   * @returns nsIDOMElement
   *        Returns the element or throws Exception if not found
   */
  getKnownElement: function EM_getKnownElement(id, win) {
    let el = this.seenItems[id];
    if (!el) {
      throw new ElementException("Element has not been seen before", 17, null);
    }
    el = el;
    if (!(el.ownerDocument == win.document)) {
      throw new ElementException("Stale element reference", 10, null);
    }
    return el;
  },
  
  /**
   * Convert values to primitives that can be transported over the Marionette
   * JSON protocol.
   * 
   * @param object val
   *        object to be wrapped
   *
   * @return object
   *        Returns a JSON primitive or Object
   */
  wrapValue: function EM_wrapValue(val) {
    let result;
    switch(typeof(val)) {
      case "undefined":
        result = null;
        break;
      case "string":
      case "number":
      case "boolean":
        result = val;
        break;
      case "object":
        if (Object.prototype.toString.call(val) == '[object Array]') {
          result = [];
          for (let i in val) {
            result.push(this.wrapValue(val[i]));
          }
        }
        else if (val == null) {
          result = null;
        }
        // nodeType 1 == 'element'
        else if (val.nodeType == 1) {
          for(let i in this.seenItems) {
            if (this.seenItems[i] == val) {
              result = {'ELEMENT': i};
            }
          }
          result = {'ELEMENT': this.addToKnownElements(val)};
        }
        else {
          result = {};
          for (let prop in val) {
            result[prop] = this.wrapValue(val[prop]);
          }
        }
        break;
    }
    return result;
  },
  
  /**
   * Convert any ELEMENT references in 'args' to the actual elements
   *
   * @param object args
   *        Arguments passed in by client
   * @param nsIDOMWindow win
   *        The window that contains the elements
   *
   * @returns object
   *        Returns the objects passed in by the client, with the
   *        reference IDs replaced by the actual elements.
   */
  convertWrappedArguments: function EM_convertWrappedArguments(args, win) {
    let converted;
    switch (typeof(args)) {
      case 'number':
      case 'string':
      case 'boolean':
        converted = args;
        break;
      case 'object':
        if (args == null) {
          converted = null;
        }
        else if (Object.prototype.toString.call(args) == '[object Array]') {
          converted = [];
          for (let i in args) {
            converted.push(this.convertWrappedArguments(args[i], win));
          }
        }
        else if (typeof(args['ELEMENT'] === 'string') &&
                 args.hasOwnProperty('ELEMENT')) {
          converted = this.getKnownElement(args['ELEMENT'],  win);
          if (converted == null)
            throw new ElementException("Unknown element: " + args['ELEMENT'], 500, null);
        }
        else {
          converted = {};
          for (let prop in args) {
            converted[prop] = this.convertWrappedArguments(args[prop], win);
          }
        }
        break;
    }
    return converted;
  },
  
  /*
   * Execute* helpers
   */
  
  /**
   * Return an object with any namedArgs applied to it. Used
   * to let clients use given names when refering to arguments
   * in execute calls, instead of using the arguments list.
   *
   * @param object args
   *        list of arguments being passed in
   *
   * @return object
   *        If '__marionetteArgs' is in args, then
   *        it will return an object with these arguments
   *        as its members.
   */
  applyNamedArgs: function EM_applyNamedArgs(args) {
    namedArgs = {};
    args.forEach(function(arg) {
      if (typeof(arg['__marionetteArgs']) === 'object') {
        for (let prop in arg['__marionetteArgs']) {
          namedArgs[prop] = arg['__marionetteArgs'][prop];
        }
      }
    });
    return namedArgs;
  },
  
  /**
   * Find an element or elements starting at the document root 
   * using the given search strategy. Search
   * will continue until the search timelimit has been reached.
   *
   * @param object values
   *        The 'using' member of values will tell us which search
   *        method to use. The 'value' member tells us the value we
   *        are looking for.
   *        If this object has a 'time' member, this number will be
   *        used to see if we have hit the search timelimit.
   * @param nsIDOMElement rootNode
   *        The document root
   * @param function notify
   *        The notification callback used when we are returning
   * @param boolean all
   *        If true, all found elements will be returned.
   *        If false, only the first element will be returned.
   *
   * @return nsIDOMElement or list of nsIDOMElements
   *        Returns the element(s) by calling the notify function.
   */
  find: function EM_find(values, rootNode, notify, all) {
    let startTime = values.time ? values.time : new Date().getTime();
    if (this.elementStrategies.indexOf(values.using) < 0) {
      throw new ElementException("No such strategy.", 17, null);
    }
    let found = all ? this.findElements(values.using, values.value, rootNode) : this.findElement(values.using, values.value, rootNode);
    if (found) {
      let type = Object.prototype.toString.call(found);
      if ((type == '[object Array]') || (type == '[object HTMLCollection]')) {
        let ids = []
        for (let i = 0 ; i < found.length ; i++) {
          ids.push(this.addToKnownElements(found[i]));
        }
        notify(ids);
      }
      else {
        let id = this.addToKnownElements(found);
        notify(id);
      }
      return;
    } else {
      if (this.searchTimeout == 0 || new Date().getTime() - startTime > this.searchTimeout) {
        throw new ElementException("Unable to locate element: " + values.value, 7, null);
      } else {
        values.time = startTime;
        this.timer.initWithCallback(this.find.bind(this, values, rootNode, notify, all), 100, Components.interfaces.nsITimer.TYPE_ONE_SHOT);
      }
    }
  },
  
  /**
   * Helper method to find. Finds one element using find's criteria
   * 
   * @param string using
   *        String identifying which search method to use
   * @param string value
   *        Value to look for
   * @param nsIDOMElement rootNode
   *        Document root
   *
   * @return nsIDOMElement
   *        Returns found element or throws Exception if not found
   */
  findElement: function EM_findElement(using, value, rootNode) {
    let element;
    switch (using) {
      case ID:
        element = rootNode.getElementById(value);
        break;
      case NAME:
        element = rootNode.getElementsByName(value)[0];
        break;
      case CLASS_NAME:
        element = rootNode.getElementsByClassName(value)[0];
        break;
      case TAG:
        element = rootNode.getElementsByTagName(value)[0];
        break;
      case XPATH:
        element = rootNode.evaluate(value, rootNode, null,
                    Components.interfaces.nsIDOMXPathResult.FIRST_ORDERED_NODE_TYPE, null).
                    singleNodeValue;
        break;
      case LINK_TEXT:
      case PARTIAL_LINK_TEXT:
        let allLinks = rootNode.getElementsByTagName('A');
        for (let i = 0; i < allLinks.length && !element; i++) {
          let text = allLinks[i].text;
          if (PARTIAL_LINK_TEXT == using) {
            if (text.indexOf(value) != -1) {
              element = allLinks[i];
            }
          } else if (text == value) {
            element = allLinks[i];
          }
        }
        break;
      case SELECTOR:
        element = rootNode.querySelector(value);
        break;
      default:
        throw new ElementException("No such strategy", 500, null);
    }
    return element;
  },

  /**
   * Helper method to find. Finds all element using find's criteria
   * 
   * @param string using
   *        String identifying which search method to use
   * @param string value
   *        Value to look for
   * @param nsIDOMElement rootNode
   *        Document root
   *
   * @return nsIDOMElement
   *        Returns found elements or throws Exception if not found
   */
  findElements: function EM_findElements(using, value, rootNode) {
    let elements = [];
    switch (using) {
      case ID:
        value = './/*[@id="' + value + '"]';
      case XPATH:
        values = rootNode.evaluate(value, rootNode, null,
                    Components.interfaces.nsIDOMXPathResult.ORDERED_NODE_ITERATOR_TYPE, null)
        let element = values.iterateNext();
        while (element) {
          elements.push(element);
          element = values.iterateNext();
        }
        break;
      case NAME:
        elements = rootNode.getElementsByName(value);
        break;
      case CLASS_NAME:
        elements = rootNode.getElementsByClassName(value);
        break;
      case TAG:
        elements = rootNode.getElementsByTagName(value);
        break;
      case LINK_TEXT:
      case PARTIAL_LINK_TEXT:
        let allLinks = rootNode.getElementsByTagName('A');
        for (let i = 0; i < allLinks.length; i++) {
          let text = allLinks[i].text;
          if (PARTIAL_LINK_TEXT == using) {
            if (text.indexOf(value) != -1) {
              elements.push(allLinks[i]);
            }
          } else if (text == value) {
            elements.push(allLinks[i]);
          }
        }
        break;
      case SELECTOR:
        elements = rootNode.querySelectorAll(value);
        break;
      default:
        throw new ElementException("No such strategy", 500, null);
    }
    return elements;
  },

  /**
   * Sets the timeout for searching for elements with find element
   * 
   * @param number value
   *        Timeout value in milliseconds
   */
  setSearchTimeout: function EM_setSearchTimeout(value) {
    this.searchTimeout = parseInt(value);
    if(isNaN(this.searchTimeout)){
      throw new ElementException("Not a Number", 500, null);
    }
  },
}
