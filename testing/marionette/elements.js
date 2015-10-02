/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var {utils: Cu} = Components;

Cu.import("chrome://marionette/content/error.js");

/**
 * The ElementManager manages DOM references and interactions with elements.
 * According to the WebDriver spec (http://code.google.com/p/selenium/wiki/JsonWireProtocol), the
 * server sends the client an element reference, and maintains the map of reference to element.
 * The client uses this reference when querying/interacting with the element, and the
 * server uses maps this reference to the actual element when it executes the command.
 */

this.EXPORTED_SYMBOLS = [
  "Accessibility",
  "ElementManager",
  "CLASS_NAME",
  "SELECTOR",
  "ID",
  "NAME",
  "LINK_TEXT",
  "PARTIAL_LINK_TEXT",
  "TAG",
  "XPATH",
  "ANON",
  "ANON_ATTRIBUTE"
];

const DOCUMENT_POSITION_DISCONNECTED = 1;

const uuidGen = Components.classes["@mozilla.org/uuid-generator;1"]
    .getService(Components.interfaces.nsIUUIDGenerator);

this.CLASS_NAME = "class name";
this.SELECTOR = "css selector";
this.ID = "id";
this.NAME = "name";
this.LINK_TEXT = "link text";
this.PARTIAL_LINK_TEXT = "partial link text";
this.TAG = "tag name";
this.XPATH = "xpath";
this.ANON= "anon";
this.ANON_ATTRIBUTE = "anon attribute";

this.Accessibility = function Accessibility() {
  // A flag indicating whether the accessibility issue should be logged or cause
  // an exception. Default: log to stdout.
  this.strict = false;
  // An interface for in-process accessibility clients
  // Note: we access it lazily to not enable accessibility when it is not needed
  Object.defineProperty(this, 'accessibleRetrieval', {
    configurable: true,
    get: function() {
      delete this.accessibleRetrieval;
      this.accessibleRetrieval = Components.classes[
        '@mozilla.org/accessibleRetrieval;1'].getService(
          Components.interfaces.nsIAccessibleRetrieval);
      return this.accessibleRetrieval;
    }
  });
};

Accessibility.prototype = {
  /**
   * Accessible object roles that support some action
   * @type Object
   */
  actionableRoles: new Set([
    'pushbutton',
    'checkbutton',
    'combobox',
    'key',
    'link',
    'menuitem',
    'check menu item',
    'radio menu item',
    'option',
    'listbox option',
    'listbox rich option',
    'check rich option',
    'combobox option',
    'radiobutton',
    'rowheader',
    'switch',
    'slider',
    'spinbutton',
    'pagetab',
    'entry',
    'outlineitem'
  ]),

  /**
   * Get an accessible object for a DOM element
   * @param nsIDOMElement element
   * @param Boolean mustHaveAccessible a flag indicating that the element must
   * have an accessible object
   * @return nsIAccessible object for the element
   */
  getAccessibleObject(element, mustHaveAccessible = false) {
    let acc = this.accessibleRetrieval.getAccessibleFor(element);
    if (!acc && mustHaveAccessible) {
      this.handleErrorMessage('Element does not have an accessible object',
        element);
    }
    return acc;
  },

  /**
   * Check if the accessible has a role that supports some action
   * @param nsIAccessible object
   * @return Boolean an indicator of role being actionable
   */
  isActionableRole(accessible) {
    return this.actionableRoles.has(
      this.accessibleRetrieval.getStringRole(accessible.role));
  },

  /**
   * Determine if an accessible has at least one action that it supports
   * @param nsIAccessible object
   * @return Boolean an indicator of supporting at least one accessible action
   */
  hasActionCount(accessible) {
    return accessible.actionCount > 0;
  },

  /**
   * Determine if an accessible has a valid name
   * @param nsIAccessible object
   * @return Boolean an indicator that the element has a non empty valid name
   */
  hasValidName(accessible) {
    return accessible.name && accessible.name.trim();
  },

  /**
   * Check if an accessible has a set hidden attribute
   * @param nsIAccessible object
   * @return Boolean an indicator that the element has a hidden accessible
   * attribute set to true
   */
  hasHiddenAttribute(accessible) {
    let hidden;
    try {
      hidden = accessible.attributes.getStringProperty('hidden');
    } finally {
      // If the property is missing, exception will be thrown.
      return hidden && hidden === 'true';
    }
  },

  /**
   * Verify if an accessible has a given state
   * @param nsIAccessible object
   * @param String stateName name of the state to match
   * @return Boolean accessible has a state
   */
  matchState(accessible, stateName) {
    let stateToMatch = Components.interfaces.nsIAccessibleStates[stateName];
    let state = {};
    accessible.getState(state, {});
    return !!(state.value & stateToMatch);
  },

  /**
   * Check if an accessible is hidden from the user of the accessibility API
   * @param nsIAccessible object
   * @return Boolean an indicator that the element is hidden from the user
   */
  isHidden(accessible) {
    while (accessible) {
      if (this.hasHiddenAttribute(accessible)) {
        return true;
      }
      accessible = accessible.parent;
    }
    return false;
  },

  /**
   * Send an error message or log the error message in the log
   * @param String message
   * @param DOMElement element that caused an error
   */
  handleErrorMessage(message, element) {
    if (!message) {
      return;
    }
    if (element) {
      message += ` -> id: ${element.id}, tagName: ${element.tagName}, className: ${element.className}\n`;
    }
    if (this.strict) {
      throw new ElementNotAccessibleError(message);
    }
    dump(Date.now() + " Marionette: " + message);
  }
};

this.ElementManager = function ElementManager(notSupported) {
  this.seenItems = {};
  this.timer = Components.classes["@mozilla.org/timer;1"].createInstance(Components.interfaces.nsITimer);
  this.elementKey = 'ELEMENT';
  this.w3cElementKey = 'element-6066-11e4-a52e-4f735466cecf';
  this.elementStrategies = [CLASS_NAME, SELECTOR, ID, NAME, LINK_TEXT, PARTIAL_LINK_TEXT, TAG, XPATH, ANON, ANON_ATTRIBUTE];
  for (let i = 0; i < notSupported.length; i++) {
    this.elementStrategies.splice(this.elementStrategies.indexOf(notSupported[i]), 1);
  }
}

ElementManager.prototype = {
  /**
   * Reset values
   */
  reset: function EM_clear() {
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
      let foundEl = null;
      try {
        foundEl = this.seenItems[i].get();
      } catch (e) {}
      if (foundEl) {
        if (XPCNativeWrapper(foundEl) == XPCNativeWrapper(element)) {
          return i;
        }
      } else {
        // cleanup reference to GC'd element
        delete this.seenItems[i];
      }
    }
    let uuid = uuidGen.generateUUID().toString();
    let id = uuid.substring(1, uuid.length - 1);
    this.seenItems[id] = Components.utils.getWeakReference(element);
    return id;
  },

  /**
   * Retrieve element from its unique ID
   *
   * @param String id
   *        The DOM reference ID
   * @param nsIDOMWindow, ShadowRoot container
   *        The window and an optional shadow root that contains the element
   *
   * @returns nsIDOMElement
   *        Returns the element or throws Exception if not found
   */
  getKnownElement: function EM_getKnownElement(id, container) {
    let el = this.seenItems[id];
    if (!el) {
      throw new JavaScriptError("Element has not been seen before. Id given was " + id);
    }
    try {
      el = el.get();
    }
    catch(e) {
      el = null;
      delete this.seenItems[id];
    }
    // use XPCNativeWrapper to compare elements; see bug 834266
    let wrappedFrame = XPCNativeWrapper(container.frame);
    let wrappedShadowRoot;
    if (container.shadowRoot) {
      wrappedShadowRoot = XPCNativeWrapper(container.shadowRoot);
    }

    if (!el ||
        !(XPCNativeWrapper(el).ownerDocument == wrappedFrame.document) ||
        this.isDisconnected(XPCNativeWrapper(el), wrappedShadowRoot,
          wrappedFrame)) {
      throw new StaleElementReferenceError(
          "The element reference is stale. Either the element " +
          "is no longer attached to the DOM or the page has been refreshed.");
    }
    return el;
  },

  /**
   * Check if the element is detached from the current frame as well as the
   * optional shadow root (when inside a Shadow DOM context).
   * @param nsIDOMElement el
   *        element to be checked
   * @param ShadowRoot shadowRoot
   *        an optional shadow root containing an element
   * @param nsIDOMWindow frame
   *        window that contains the element or the current host of the shadow
   *        root.
   * @return {Boolean} a flag indicating that the element is disconnected
   */
  isDisconnected: function EM_isDisconnected(el, shadowRoot, frame) {
    if (shadowRoot && frame.ShadowRoot) {
      if (el.compareDocumentPosition(shadowRoot) &
        DOCUMENT_POSITION_DISCONNECTED) {
        return true;
      }
      // Looking for next possible ShadowRoot ancestor
      let parent = shadowRoot.host;
      while (parent && !(parent instanceof frame.ShadowRoot)) {
        parent = parent.parentNode;
      }
      return this.isDisconnected(shadowRoot.host, parent, frame);
    } else {
      return el.compareDocumentPosition(frame.document.documentElement) &
        DOCUMENT_POSITION_DISCONNECTED;
    }
  },

  /**
   * Convert values to primitives that can be transported over the
   * Marionette protocol.
   *
   * This function implements the marshaling algorithm defined in the
   * WebDriver specification:
   *
   *     https://dvcs.w3.org/hg/webdriver/raw-file/tip/webdriver-spec.html#synchronous-javascript-execution
   *
   * @param object val
   *        object to be marshaled
   *
   * @return object
   *         Returns a JSON primitive or Object
   */
  wrapValue: function EM_wrapValue(val) {
    let result = null;

    switch (typeof(val)) {
      case "undefined":
        result = null;
        break;

      case "string":
      case "number":
      case "boolean":
        result = val;
        break;

      case "object":
        let type = Object.prototype.toString.call(val);
        if (type == "[object Array]" ||
            type == "[object NodeList]") {
          result = [];
          for (let i = 0; i < val.length; ++i) {
            result.push(this.wrapValue(val[i]));

          }
        }
        else if (val == null) {
          result = null;
        }
        else if (val.nodeType == 1) {
          let elementId = this.addToKnownElements(val);
          result = {[this.elementKey]: elementId, [this.w3cElementKey]: elementId};
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
   * @param nsIDOMWindow, ShadowRoot container
   *        The window and an optional shadow root that contains the element
   *
   * @returns object
   *        Returns the objects passed in by the client, with the
   *        reference IDs replaced by the actual elements.
   */
  convertWrappedArguments: function EM_convertWrappedArguments(args, container) {
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
            converted.push(this.convertWrappedArguments(args[i], container));
          }
        }
        else if (((typeof(args[this.elementKey]) === 'string') && args.hasOwnProperty(this.elementKey)) ||
                 ((typeof(args[this.w3cElementKey]) === 'string') &&
                     args.hasOwnProperty(this.w3cElementKey))) {
          let elementUniqueIdentifier = args[this.w3cElementKey] ? args[this.w3cElementKey] : args[this.elementKey];
          converted = this.getKnownElement(elementUniqueIdentifier, container);
          if (converted == null) {
            throw new WebDriverError(`Unknown element: ${elementUniqueIdentifier}`);
          }
        }
        else {
          converted = {};
          for (let prop in args) {
            converted[prop] = this.convertWrappedArguments(args[prop], container);
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
      if (arg && typeof(arg['__marionetteArgs']) === 'object') {
        for (let prop in arg['__marionetteArgs']) {
          namedArgs[prop] = arg['__marionetteArgs'][prop];
        }
      }
    });
    return namedArgs;
  },

  /**
   * Find an element or elements starting at the document root or
   * given node, using the given search strategy. Search
   * will continue until the search timelimit has been reached.
   *
   * @param nsIDOMWindow, ShadowRoot container
   *        The window and an optional shadow root that contains the element
   * @param object values
   *        The 'using' member of values will tell us which search
   *        method to use. The 'value' member tells us the value we
   *        are looking for.
   *        If this object has an 'element' member, this will be used
   *        as the start node instead of the document root
   *        If this object has a 'time' member, this number will be
   *        used to see if we have hit the search timelimit.
   * @param boolean all
   *        If true, all found elements will be returned.
   *        If false, only the first element will be returned.
   * @param function on_success
   *        Callback used when operating is successful.
   * @param function on_error
   *        Callback to invoke when an error occurs.
   *
   * @return nsIDOMElement or list of nsIDOMElements
   *        Returns the element(s) by calling the on_success function.
   */
  find: function EM_find(container, values, searchTimeout, all, on_success, on_error, command_id) {
    let startTime = values.time ? values.time : new Date().getTime();
    let rootNode = container.shadowRoot || container.frame.document;
    let startNode = (values.element != undefined) ?
                    this.getKnownElement(values.element, container) : rootNode;
    if (this.elementStrategies.indexOf(values.using) < 0) {
      throw new InvalidSelectorError("No such strategy: " + values.using);
    }
    let found = all ? this.findElements(values.using, values.value, rootNode, startNode) :
                      this.findElement(values.using, values.value, rootNode, startNode);
    let type = Object.prototype.toString.call(found);
    let isArrayLike = ((type == '[object Array]') || (type == '[object HTMLCollection]') || (type == '[object NodeList]'));
    if (found == null || (isArrayLike && found.length <= 0)) {
      if (!searchTimeout || new Date().getTime() - startTime > searchTimeout) {
        if (all) {
          on_success([], command_id); // findElements should return empty list
        } else {
          // Format message depending on strategy if necessary
          let message = "Unable to locate element: " + values.value;
          if (values.using == ANON) {
            message = "Unable to locate anonymous children";
          } else if (values.using == ANON_ATTRIBUTE) {
            message = "Unable to locate anonymous element: " + JSON.stringify(values.value);
          }
          on_error(new NoSuchElementError(message), command_id);
        }
      } else {
        values.time = startTime;
        this.timer.initWithCallback(this.find.bind(this, container, values,
                                                   searchTimeout, all,
                                                   on_success, on_error,
                                                   command_id),
                                    100,
                                    Components.interfaces.nsITimer.TYPE_ONE_SHOT);
      }
    } else {
      if (isArrayLike) {
        let ids = []
        for (let i = 0 ; i < found.length ; i++) {
          let foundElement = this.addToKnownElements(found[i]);
          let returnElement = {
            [this.elementKey] : foundElement,
            [this.w3cElementKey] : foundElement,
          };
          ids.push(returnElement);
        }
        on_success(ids, command_id);
      } else {
        let id = this.addToKnownElements(found);
        on_success({[this.elementKey]: id, [this.w3cElementKey]:id}, command_id);
      }
    }
  },

  /**
   * Find a value by XPATH
   *
   * @param nsIDOMElement root
   *        Document root
   * @param string value
   *        XPATH search string
   * @param nsIDOMElement node
   *        start node
   *
   * @return nsIDOMElement
   *        returns the found element
   */
  findByXPath: function EM_findByXPath(root, value, node) {
    return root.evaluate(value, node, null,
            Components.interfaces.nsIDOMXPathResult.FIRST_ORDERED_NODE_TYPE, null).singleNodeValue;
  },

  /**
   * Find values by XPATH
   *
   * @param nsIDOMElement root
   *        Document root
   * @param string value
   *        XPATH search string
   * @param nsIDOMElement node
   *        start node
   *
   * @return object
   *        returns a list of found nsIDOMElements
   */
  findByXPathAll: function EM_findByXPathAll(root, value, node) {
    let values = root.evaluate(value, node, null,
                      Components.interfaces.nsIDOMXPathResult.ORDERED_NODE_ITERATOR_TYPE, null);
    let elements = [];
    let element = values.iterateNext();
    while (element) {
      elements.push(element);
      element = values.iterateNext();
    }
    return elements;
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
   * @param nsIDOMElement startNode
   *        Node from which we start searching
   *
   * @return nsIDOMElement
   *        Returns found element or throws Exception if not found
   */
  findElement: function EM_findElement(using, value, rootNode, startNode) {
    let element;
    switch (using) {
      case ID:
        element = startNode.getElementById ?
                  startNode.getElementById(value) :
                  this.findByXPath(rootNode, './/*[@id="' + value + '"]', startNode);
        break;
      case NAME:
        element = startNode.getElementsByName ?
                  startNode.getElementsByName(value)[0] :
                  this.findByXPath(rootNode, './/*[@name="' + value + '"]', startNode);
        break;
      case CLASS_NAME:
        element = startNode.getElementsByClassName(value)[0]; //works for >=FF3
        break;
      case TAG:
        element = startNode.getElementsByTagName(value)[0]; //works for all elements
        break;
      case XPATH:
        element = this.findByXPath(rootNode, value, startNode);
        break;
      case LINK_TEXT:
      case PARTIAL_LINK_TEXT:
        let allLinks = startNode.getElementsByTagName('A');
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
        element = startNode.querySelector(value);
        break;
      case ANON:
        element = rootNode.getAnonymousNodes(startNode);
        if (element != null) {
          element = element[0];
        }
        break;
      case ANON_ATTRIBUTE:
        let attr = Object.keys(value)[0];
        element = rootNode.getAnonymousElementByAttribute(startNode, attr, value[attr]);
        break;
      default:
        throw new InvalidSelectorError("No such strategy: " + using);
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
   * @param nsIDOMElement startNode
   *        Node from which we start searching
   *
   * @return nsIDOMElement
   *        Returns found elements or throws Exception if not found
   */
  findElements: function EM_findElements(using, value, rootNode, startNode) {
    let elements = [];
    switch (using) {
      case ID:
        value = './/*[@id="' + value + '"]';
      case XPATH:
        elements = this.findByXPathAll(rootNode, value, startNode);
        break;
      case NAME:
        elements = startNode.getElementsByName ?
                   startNode.getElementsByName(value) :
                   this.findByXPathAll(rootNode, './/*[@name="' + value + '"]', startNode);
        break;
      case CLASS_NAME:
        elements = startNode.getElementsByClassName(value);
        break;
      case TAG:
        elements = startNode.getElementsByTagName(value);
        break;
      case LINK_TEXT:
      case PARTIAL_LINK_TEXT:
        let allLinks = startNode.getElementsByTagName('A');
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
        elements = Array.slice(startNode.querySelectorAll(value));
        break;
      case ANON:
        elements = rootNode.getAnonymousNodes(startNode) || [];
        break;
      case ANON_ATTRIBUTE:
        let attr = Object.keys(value)[0];
        let el = rootNode.getAnonymousElementByAttribute(startNode, attr, value[attr]);
        if (el != null) {
          elements = [el];
        }
        break;
      default:
        throw new InvalidSelectorError("No such strategy: " + using);
    }
    return elements;
  },
}
