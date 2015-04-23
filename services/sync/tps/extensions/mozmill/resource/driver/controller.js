/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["MozMillController", "globalEventRegistry",
                        "sleep", "windowMap"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

var EventUtils = {}; Cu.import('resource://mozmill/stdlib/EventUtils.js', EventUtils);

var assertions = {}; Cu.import('resource://mozmill/modules/assertions.js', assertions);
var broker = {}; Cu.import('resource://mozmill/driver/msgbroker.js', broker);
var elementslib = {}; Cu.import('resource://mozmill/driver/elementslib.js', elementslib);
var errors = {}; Cu.import('resource://mozmill/modules/errors.js', errors);
var mozelement = {}; Cu.import('resource://mozmill/driver/mozelement.js', mozelement);
var utils = {}; Cu.import('resource://mozmill/stdlib/utils.js', utils);
var windows = {}; Cu.import('resource://mozmill/modules/windows.js', windows);

// Declare most used utils functions in the controller namespace
var assert = new assertions.Assert();
var waitFor = assert.waitFor;

var sleep = utils.sleep;

// For Mozmill 1.5 backward compatibility
var windowMap = windows.map;

waitForEvents = function () {
}

waitForEvents.prototype = {
  /**
   * Initialize list of events for given node
   */
  init: function waitForEvents_init(node, events) {
    if (node.getNode != undefined)
      node = node.getNode();

    this.events = events;
    this.node = node;
    node.firedEvents = {};
    this.registry = {};

    for each (var e in events) {
      var listener = function (event) {
        this.firedEvents[event.type] = true;
      }

      this.registry[e] = listener;
      this.registry[e].result = false;
      this.node.addEventListener(e, this.registry[e], true);
    }
  },

  /**
   * Wait until all assigned events have been fired
   */
  wait: function waitForEvents_wait(timeout, interval) {
    for (var e in this.registry) {
      assert.waitFor(function () {
        return this.node.firedEvents[e] == true;
      }, "waitForEvents.wait(): Event '" + ex + "' has been fired.", timeout, interval);

      this.node.removeEventListener(e, this.registry[e], true);
    }
  }
}

/**
 * Class to handle menus and context menus
 *
 * @constructor
 * @param {MozMillController} controller
 *        Mozmill controller of the window under test
 * @param {string} menuSelector
 *        jQuery like selector string of the element
 * @param {object} document
 *        Document to use for finding the menu
 *        [optional - default: aController.window.document]
 */
var Menu = function (controller, menuSelector, document) {
  this._controller = controller;
  this._menu = null;

  document = document || controller.window.document;
  var node = document.querySelector(menuSelector);
  if (node) {
    // We don't unwrap nodes automatically yet (Bug 573185)
    node = node.wrappedJSObject || node;
    this._menu = new mozelement.Elem(node);
  } else {
    throw new Error("Menu element '" + menuSelector + "' not found.");
  }
}

Menu.prototype = {

  /**
   * Open and populate the menu
   *
   * @param {ElemBase} contextElement
   *        Element whose context menu has to be opened
   * @returns {Menu} The Menu instance
   */
  open: function Menu_open(contextElement) {
    // We have to open the context menu
    var menu = this._menu.getNode();
    if ((menu.localName == "popup" || menu.localName == "menupopup") &&
        contextElement && contextElement.exists()) {
      this._controller.rightClick(contextElement);
      assert.waitFor(function () {
        return menu.state == "open";
      }, "Context menu has been opened.");
    }

    // Run through the entire menu and populate with dynamic entries
    this._buildMenu(menu);

    return this;
  },

  /**
   * Close the menu
   *
   * @returns {Menu} The Menu instance
   */
  close: function Menu_close() {
    var menu = this._menu.getNode();

    this._controller.keypress(this._menu, "VK_ESCAPE", {});
    assert.waitFor(function () {
      return menu.state == "closed";
    }, "Context menu has been closed.");

    return this;
  },

  /**
   * Retrieve the specified menu entry
   *
   * @param {string} itemSelector
   *        jQuery like selector string of the menu item
   * @returns {ElemBase} Menu element
   * @throws Error If menu element has not been found
   */
  getItem: function Menu_getItem(itemSelector) {
    // Run through the entire menu and populate with dynamic entries
    this._buildMenu(this._menu.getNode());

    var node = this._menu.getNode().querySelector(itemSelector);

    if (!node) {
      throw new Error("Menu entry '" + itemSelector + "' not found.");
    }

    return new mozelement.Elem(node);
  },

  /**
   * Click the specified menu entry
   *
   * @param {string} itemSelector
   *        jQuery like selector string of the menu item
   *
   * @returns {Menu} The Menu instance
   */
  click: function Menu_click(itemSelector) {
    this._controller.click(this.getItem(itemSelector));

    return this;
  },

  /**
   * Synthesize a keypress against the menu
   *
   * @param {string} key
   *        Key to press
   * @param {object} modifier
   *        Key modifiers
   * @see MozMillController#keypress
   *
   * @returns {Menu} The Menu instance
   */
  keypress: function Menu_keypress(key, modifier) {
    this._controller.keypress(this._menu, key, modifier);

    return this;
  },

  /**
   * Opens the context menu, click the specified entry and
   * make sure that the menu has been closed.
   *
   * @param {string} itemSelector
   *        jQuery like selector string of the element
   * @param {ElemBase} contextElement
   *        Element whose context menu has to be opened
   *
   * @returns {Menu} The Menu instance
   */
  select: function Menu_select(itemSelector, contextElement) {
    this.open(contextElement);
    this.click(itemSelector);
    this.close();
  },

  /**
   * Recursive function which iterates through all menu elements and
   * populates the menus with dynamic menu entries.
   *
   * @param {node} menu
   *        Top menu node whose elements have to be populated
   */
  _buildMenu: function Menu__buildMenu(menu) {
    var items = menu ? menu.childNodes : null;

    Array.forEach(items, function (item) {
      // When we have a menu node, fake a click onto it to populate
      // the sub menu with dynamic entries
      if (item.tagName == "menu") {
        var popup = item.querySelector("menupopup");

        if (popup) {
          var popupEvent = this._controller.window.document.createEvent("MouseEvent");
          popupEvent.initMouseEvent("popupshowing", true, true,
                                    this._controller.window, 0, 0, 0, 0, 0,
                                    false, false, false, false, 0, null);
          popup.dispatchEvent(popupEvent);

          this._buildMenu(popup);
        }
      }
    }, this);
  }
};

var MozMillController = function (window) {
  this.window = window;

  this.mozmillModule = {};
  Cu.import('resource://mozmill/driver/mozmill.js', this.mozmillModule);

  var self = this;
  assert.waitFor(function () {
    return window != null && self.isLoaded();
  }, "controller(): Window has been initialized.");

  // Ensure to focus the window which will move it virtually into the foreground
  // when focusmanager.testmode is set enabled.
  this.window.focus();

  var windowType = window.document.documentElement.getAttribute('windowtype');
  if (controllerAdditions[windowType] != undefined ) {
    this.prototype = new utils.Copy(this.prototype);
    controllerAdditions[windowType](this);
    this.windowtype = windowType;
  }
}

/**
 * Returns the global browser object of the window
 *
 * @returns {Object} The browser object
 */
MozMillController.prototype.__defineGetter__("browserObject", function () {
  return utils.getBrowserObject(this.window);
});

// constructs a MozMillElement from the controller's window
MozMillController.prototype.__defineGetter__("rootElement", function () {
  if (this._rootElement == undefined) {
    let docElement = this.window.document.documentElement;
    this._rootElement = new mozelement.MozMillElement("Elem", docElement);
  }

  return this._rootElement;
});

MozMillController.prototype.sleep = utils.sleep;
MozMillController.prototype.waitFor = assert.waitFor;

// Open the specified url in the current tab
MozMillController.prototype.open = function (url) {
  switch (this.mozmillModule.Application) {
    case "Firefox":
      // Stop a running page load to not overlap requests
      if (this.browserObject.selectedBrowser) {
        this.browserObject.selectedBrowser.stop();
      }

      this.browserObject.loadURI(url);
      break;

    default:
      throw new Error("MozMillController.open not supported.");
  }

  broker.pass({'function':'Controller.open()'});
}

/**
 * Take a screenshot of specified node
 *
 * @param {Element} node
 *        The window or DOM element to capture
 * @param {String} name
 *        The name of the screenshot used in reporting and as filename
 * @param {Boolean} save
 *        If true saves the screenshot as 'name.jpg' in tempdir,
 *        otherwise returns a dataURL
 * @param {Element[]} highlights
 *        A list of DOM elements to highlight by drawing a red rectangle around them
 *
 * @returns {Object} Object which contains properties like filename, dataURL,
 *          name and timestamp of the screenshot
 */
MozMillController.prototype.screenshot = function (node, name, save, highlights) {
  if (!node) {
    throw new Error("node is undefined");
  }

  // Unwrap the node and highlights
  if ("getNode" in node) {
    node = node.getNode();
  }

  if (highlights) {
    for (var i = 0; i < highlights.length; ++i) {
      if ("getNode" in highlights[i]) {
        highlights[i] = highlights[i].getNode();
      }
    }
  }

  // If save is false, a dataURL is used
  // Include both in the report anyway to avoid confusion and make the report easier to parse
  var screenshot = {"filename": undefined,
                    "dataURL": utils.takeScreenshot(node, highlights),
                    "name": name,
                    "timestamp": new Date().toLocaleString()};

  if (!save) {
    return screenshot;
  }

  // Save the screenshot to disk

  let {filename, failure} = utils.saveDataURL(screenshot.dataURL, name);
  screenshot.filename = filename;
  screenshot.failure = failure;

  if (failure) {
    broker.log({'function': 'controller.screenshot()',
                'message': 'Error writing to file: ' + screenshot.filename});
  } else {
    // Send the screenshot object to python over jsbridge
    broker.sendMessage("screenshot", screenshot);
    broker.pass({'function': 'controller.screenshot()'});
  }

  return screenshot;
}

/**
 * Checks if the specified window has been loaded
 *
 * @param {DOMWindow} [aWindow=this.window] Window object to check for loaded state
 */
MozMillController.prototype.isLoaded = function (aWindow) {
  var win = aWindow || this.window;

  return windows.map.getValue(utils.getWindowId(win), "loaded") || false;
};

MozMillController.prototype.__defineGetter__("waitForEvents", function () {
  if (this._waitForEvents == undefined) {
    this._waitForEvents = new waitForEvents();
  }

  return this._waitForEvents;
});

/**
 * Wrapper function to create a new instance of a menu
 * @see Menu
 */
MozMillController.prototype.getMenu = function (menuSelector, document) {
  return new Menu(this, menuSelector, document);
};

MozMillController.prototype.__defineGetter__("mainMenu", function () {
  return this.getMenu("menubar");
});

MozMillController.prototype.__defineGetter__("menus", function () {
  logDeprecated('controller.menus', 'Use controller.mainMenu instead');
});

MozMillController.prototype.waitForImage = function (aElement, timeout, interval) {
  this.waitFor(function () {
    return aElement.getNode().complete == true;
  }, "timeout exceeded for waitForImage " + aElement.getInfo(), timeout, interval);

  broker.pass({'function':'Controller.waitForImage()'});
}

MozMillController.prototype.startUserShutdown = function (timeout, restart, next, resetProfile) {
  if (restart && resetProfile) {
    throw new Error("You can't have a user-restart and reset the profile; there is a race condition");
  }

  let shutdownObj = {
    'user': true,
    'restart': Boolean(restart),
    'next': next,
    'resetProfile': Boolean(resetProfile),
    'timeout': timeout
  };

  broker.sendMessage('shutdown', shutdownObj);
}

/**
 * Restart the application
 *
 * @param {string} aNext
 *        Name of the next test function to run after restart
 * @param {boolean} [aFlags=undefined]
 *        Additional flags how to handle the shutdown or restart. The attributes
 *        eRestarti386 (0x20) and eRestartx86_64 (0x30) have not been documented yet.
 * @see https://developer.mozilla.org/nsIAppStartup#Attributes
 */
MozMillController.prototype.restartApplication = function (aNext, aFlags) {
  var flags = Ci.nsIAppStartup.eAttemptQuit | Ci.nsIAppStartup.eRestart;

  if (aFlags) {
    flags |= aFlags;
  }

  broker.sendMessage('shutdown', {'user': false,
                                  'restart': true,
                                  'flags': flags,
                                  'next': aNext,
                                  'timeout': 0 });

  // We have to ensure to stop the test from continuing until the application is
  // shutting down. The only way to do that is by throwing an exception.
  throw new errors.ApplicationQuitError();
}

/**
 * Stop the application
 *
 * @param {boolean} [aResetProfile=false]
 *        Whether to reset the profile during restart
 * @param {boolean} [aFlags=undefined]
 *        Additional flags how to handle the shutdown or restart. The attributes
 *        eRestarti386 and eRestartx86_64 have not been documented yet.
 * @see https://developer.mozilla.org/nsIAppStartup#Attributes
 */
MozMillController.prototype.stopApplication = function (aResetProfile, aFlags) {
  var flags = Ci.nsIAppStartup.eAttemptQuit;

  if (aFlags) {
    flags |= aFlags;
  }

  broker.sendMessage('shutdown', {'user': false,
                                  'restart': false,
                                  'flags': flags,
                                  'resetProfile': aResetProfile,
                                  'timeout': 0 });

  // We have to ensure to stop the test from continuing until the application is
  // shutting down. The only way to do that is by throwing an exception.
  throw new errors.ApplicationQuitError();
}

//Browser navigation functions
MozMillController.prototype.goBack = function () {
  this.window.content.history.back();
  broker.pass({'function':'Controller.goBack()'});

  return true;
}

MozMillController.prototype.goForward = function () {
  this.window.content.history.forward();
  broker.pass({'function':'Controller.goForward()'});

  return true;
}

MozMillController.prototype.refresh = function () {
  this.window.content.location.reload(true);
  broker.pass({'function':'Controller.refresh()'});

  return true;
}

function logDeprecated(funcName, message) {
  broker.log({'function': funcName + '() - DEPRECATED',
              'message': funcName + '() is deprecated. ' + message});
}

function logDeprecatedAssert(funcName) {
   logDeprecated('controller.' + funcName,
                 '. Use the generic `assertion` module instead.');
}

MozMillController.prototype.assertText = function (el, text) {
  logDeprecatedAssert("assertText");

  var n = el.getNode();

  if (n && n.innerHTML == text) {
    broker.pass({'function': 'Controller.assertText()'});
  } else {
    throw new Error("could not validate element " + el.getInfo() +
                    " with text "+ text);
  }

  return true;
};

/**
 * Assert that a specified node exists
 */
MozMillController.prototype.assertNode = function (el) {
  logDeprecatedAssert("assertNode");

  //this.window.focus();
  var element = el.getNode();
  if (!element) {
    throw new Error("could not find element " + el.getInfo());
  }

  broker.pass({'function': 'Controller.assertNode()'});
  return true;
};

/**
 * Assert that a specified node doesn't exist
 */
MozMillController.prototype.assertNodeNotExist = function (el) {
  logDeprecatedAssert("assertNodeNotExist");

  try {
    var element = el.getNode();
  } catch (e) {
    broker.pass({'function': 'Controller.assertNodeNotExist()'});
  }

  if (element) {
    throw new Error("Unexpectedly found element " + el.getInfo());
  } else {
    broker.pass({'function':'Controller.assertNodeNotExist()'});
  }

  return true;
};

/**
 * Assert that a form element contains the expected value
 */
MozMillController.prototype.assertValue = function (el, value) {
  logDeprecatedAssert("assertValue");

  var n = el.getNode();

  if (n && n.value == value) {
    broker.pass({'function': 'Controller.assertValue()'});
  } else {
    throw new Error("could not validate element " + el.getInfo() +
                    " with value " + value);
  }

  return false;
};

/**
 * Check if the callback function evaluates to true
 */
MozMillController.prototype.assert = function (callback, message, thisObject) {
  logDeprecatedAssert("assert");

  utils.assert(callback, message, thisObject);
  broker.pass({'function': ": controller.assert('" + callback + "')"});

  return true;
}

/**
 * Assert that a provided value is selected in a select element
 */
MozMillController.prototype.assertSelected = function (el, value) {
  logDeprecatedAssert("assertSelected");

  var n = el.getNode();
  var validator = value;

  if (n && n.options[n.selectedIndex].value == validator) {
    broker.pass({'function':'Controller.assertSelected()'});
  } else {
    throw new Error("could not assert value for element " + el.getInfo() +
                    " with value " + value);
  }

  return true;
};

/**
 * Assert that a provided checkbox is checked
 */
MozMillController.prototype.assertChecked = function (el) {
  logDeprecatedAssert("assertChecked");

  var element = el.getNode();

  if (element && element.checked == true) {
    broker.pass({'function':'Controller.assertChecked()'});
  } else {
    throw new Error("assert failed for checked element " + el.getInfo());
  }

  return true;
};

/**
 * Assert that a provided checkbox is not checked
 */
MozMillController.prototype.assertNotChecked = function (el) {
  logDeprecatedAssert("assertNotChecked");

  var element = el.getNode();

  if (!element) {
    throw new Error("Could not find element" + el.getInfo());
  }

  if (!element.hasAttribute("checked") || element.checked != true) {
    broker.pass({'function': 'Controller.assertNotChecked()'});
  } else {
    throw new Error("assert failed for not checked element " + el.getInfo());
  }

  return true;
};

/**
 * Assert that an element's javascript property exists or has a particular value
 *
 * if val is undefined, will return true if the property exists.
 * if val is specified, will return true if the property exists and has the correct value
 */
MozMillController.prototype.assertJSProperty = function (el, attrib, val) {
  logDeprecatedAssert("assertJSProperty");

  var element = el.getNode();

  if (!element){
    throw new Error("could not find element " + el.getInfo());
  }

  var value = element[attrib];
  var res = (value !== undefined && (val === undefined ? true :
                                                         String(value) == String(val)));
  if (res) {
    broker.pass({'function':'Controller.assertJSProperty("' + el.getInfo() + '") : ' + val});
  } else {
    throw new Error("Controller.assertJSProperty(" + el.getInfo() + ") : " +
                    (val === undefined ? "property '" + attrib +
                    "' doesn't exist" : val + " == " + value));
  }

  return true;
};

/**
 * Assert that an element's javascript property doesn't exist or doesn't have a particular value
 *
 * if val is undefined, will return true if the property doesn't exist.
 * if val is specified, will return true if the property doesn't exist or doesn't have the specified value
 */
MozMillController.prototype.assertNotJSProperty = function (el, attrib, val) {
  logDeprecatedAssert("assertNotJSProperty");

  var element = el.getNode();

  if (!element){
    throw new Error("could not find element " + el.getInfo());
  }

  var value = element[attrib];
  var res = (val === undefined ? value === undefined : String(value) != String(val));
  if (res) {
    broker.pass({'function':'Controller.assertNotProperty("' + el.getInfo() + '") : ' + val});
  } else {
    throw new Error("Controller.assertNotJSProperty(" + el.getInfo() + ") : " +
                    (val === undefined ? "property '" + attrib +
                    "' exists" : val + " != " + value));
  }

  return true;
};

/**
 * Assert that an element's dom property exists or has a particular value
 *
 * if val is undefined, will return true if the property exists.
 * if val is specified, will return true if the property exists and has the correct value
 */
MozMillController.prototype.assertDOMProperty = function (el, attrib, val) {
  logDeprecatedAssert("assertDOMProperty");

  var element = el.getNode();

  if (!element){
    throw new Error("could not find element " + el.getInfo());
  }

  var value, res = element.hasAttribute(attrib);
  if (res && val !== undefined) {
    value = element.getAttribute(attrib);
    res = (String(value) == String(val));
  }

  if (res) {
    broker.pass({'function':'Controller.assertDOMProperty("' + el.getInfo() + '") : ' + val});
  } else {
    throw new Error("Controller.assertDOMProperty(" + el.getInfo() + ") : " +
                    (val === undefined ? "property '" + attrib +
                    "' doesn't exist" : val + " == " + value));
  }

  return true;
};

/**
 * Assert that an element's dom property doesn't exist or doesn't have a particular value
 *
 * if val is undefined, will return true if the property doesn't exist.
 * if val is specified, will return true if the property doesn't exist or doesn't have the specified value
 */
MozMillController.prototype.assertNotDOMProperty = function (el, attrib, val) {
  logDeprecatedAssert("assertNotDOMProperty");

  var element = el.getNode();

  if (!element) {
    throw new Error("could not find element " + el.getInfo());
  }

  var value, res = element.hasAttribute(attrib);
  if (res && val !== undefined) {
    value = element.getAttribute(attrib);
    res = (String(value) == String(val));
  }

  if (!res) {
    broker.pass({'function':'Controller.assertNotDOMProperty("' + el.getInfo() + '") : ' + val});
  } else {
    throw new Error("Controller.assertNotDOMProperty(" + el.getInfo() + ") : " +
                    (val == undefined ? "property '" + attrib +
                    "' exists" : val + " == " + value));
  }

  return true;
};

/**
 * Assert that a specified image has actually loaded. The Safari workaround results
 * in additional requests for broken images (in Safari only) but works reliably
 */
MozMillController.prototype.assertImageLoaded = function (el) {
  logDeprecatedAssert("assertImageLoaded");

  var img = el.getNode();

  if (!img || img.tagName != 'IMG') {
    throw new Error('Controller.assertImageLoaded() failed.')
    return false;
  }

  var comp = img.complete;
  var ret = null; // Return value

  // Workaround for Safari -- it only supports the
  // complete attrib on script-created images
  if (typeof comp == 'undefined') {
    test = new Image();
    // If the original image was successfully loaded,
    // src for new one should be pulled from cache
    test.src = img.src;
    comp = test.complete;
  }

  // Check the complete attrib. Note the strict
  // equality check -- we don't want undefined, null, etc.
  // --------------------------
  if (comp === false) {
    // False -- Img failed to load in IE/Safari, or is
    // still trying to load in FF
    ret = false;
  } else if (comp === true && img.naturalWidth == 0) {
    // True, but image has no size -- image failed to
    // load in FF
    ret = false;
  } else {
    // Otherwise all we can do is assume everything's
    // hunky-dory
   ret = true;
  }

  if (ret) {
    broker.pass({'function':'Controller.assertImageLoaded'});
  } else {
    throw new Error('Controller.assertImageLoaded() failed.')
  }

  return true;
};

/**
 * Drag one element to the top x,y coords of another specified element
 */
MozMillController.prototype.mouseMove = function (doc, start, dest) {
  // if one of these elements couldn't be looked up
  if (typeof start != 'object'){
    throw new Error("received bad coordinates");
  }

  if (typeof dest != 'object'){
    throw new Error("received bad coordinates");
  }

  var triggerMouseEvent = function (element, clientX, clientY) {
    clientX = clientX ? clientX: 0;
    clientY = clientY ? clientY: 0;

    // make the mouse understand where it is on the screen
    var screenX = element.boxObject.screenX ? element.boxObject.screenX : 0;
    var screenY = element.boxObject.screenY ? element.boxObject.screenY : 0;

    var evt = element.ownerDocument.createEvent('MouseEvents');
    if (evt.initMouseEvent) {
      evt.initMouseEvent('mousemove', true, true, element.ownerDocument.defaultView,
                         1, screenX, screenY, clientX, clientY);
    } else {
      evt.initEvent('mousemove', true, true);
    }

    element.dispatchEvent(evt);
  };

  // Do the initial move to the drag element position
  triggerMouseEvent(doc.body, start[0], start[1]);
  triggerMouseEvent(doc.body, dest[0], dest[1]);

  broker.pass({'function':'Controller.mouseMove()'});
  return true;
}

/**
 * Drag an element to the specified offset on another element, firing mouse and
 * drag events. Adapted from ChromeUtils.js synthesizeDrop()
 *
 * @deprecated Use the MozMillElement object
 *
 * @param {MozElement} aSrc
 *        Source element to be dragged
 * @param {MozElement} aDest
 *        Destination element over which the drop occurs
 * @param {Number} [aOffsetX=element.width/2]
 *        Relative x offset for dropping on the aDest element
 * @param {Number} [aOffsetY=element.height/2]
 *        Relative y offset for dropping on the aDest element
 * @param {DOMWindow} [aSourceWindow=this.element.ownerDocument.defaultView]
 *        Custom source Window to be used.
 * @param {String} [aDropEffect="move"]
 *        Effect used for the drop event
 * @param {Object[]} [aDragData]
 *        An array holding custom drag data to be used during the drag event
 *        Format: [{ type: "text/plain", "Text to drag"}, ...]
 *
 * @returns {String} the captured dropEffect
 */
MozMillController.prototype.dragToElement = function (aSrc, aDest, aOffsetX,
                                                      aOffsetY, aSourceWindow,
                                                      aDropEffect, aDragData) {
  logDeprecated("controller.dragToElement", "Use the MozMillElement object.");
  return aSrc.dragToElement(aDest, aOffsetX, aOffsetY, aSourceWindow, null,
                            aDropEffect, aDragData);
};

function Tabs(controller) {
  this.controller = controller;
}

Tabs.prototype.getTab = function (index) {
  return this.controller.browserObject.browsers[index].contentDocument;
}

Tabs.prototype.__defineGetter__("activeTab", function () {
  return this.controller.browserObject.selectedBrowser.contentDocument;
});

Tabs.prototype.selectTab = function (index) {
  // GO in to tab manager and grab the tab by index and call focus.
}

Tabs.prototype.findWindow = function (doc) {
  for (var i = 0; i <= (this.controller.window.frames.length - 1); i++) {
    if (this.controller.window.frames[i].document == doc) {
      return this.controller.window.frames[i];
    }
  }

  throw new Error("Cannot find window for document. Doc title == " + doc.title);
}

Tabs.prototype.getTabWindow = function (index) {
  return this.findWindow(this.getTab(index));
}

Tabs.prototype.__defineGetter__("activeTabWindow", function () {
  return this.findWindow(this.activeTab);
});

Tabs.prototype.__defineGetter__("length", function () {
  return this.controller.browserObject.browsers.length;
});

Tabs.prototype.__defineGetter__("activeTabIndex", function () {
  var browser = this.controller.browserObject;
  return browser.tabContainer.selectedIndex;
});

Tabs.prototype.selectTabIndex = function (aIndex) {
  var browser = this.controller.browserObject;
  browser.selectTabAtIndex(aIndex);
}

function browserAdditions (controller) {
  controller.tabs = new Tabs(controller);

  controller.waitForPageLoad = function (aDocument, aTimeout, aInterval) {
    var timeout = aTimeout || 30000;
    var win = null;
    var timed_out = false;

    // If a user tries to do waitForPageLoad(2000), this will assign the
    // interval the first arg which is most likely what they were expecting
    if (typeof(aDocument) == "number"){
      timeout = aDocument;
    }

    // If we have a real document use its default view
    if (aDocument && (typeof(aDocument) === "object") &&
        "defaultView" in aDocument)
      win = aDocument.defaultView;

    // If no document has been specified, fallback to the default view of the
    // currently selected tab browser
    win = win || this.browserObject.selectedBrowser.contentWindow;

    // Wait until the content in the tab has been loaded
    try {
      this.waitFor(function () {
        return windows.map.hasPageLoaded(utils.getWindowId(win));
      }, "Timeout", timeout, aInterval);
    }
    catch (ex if ex instanceof errors.TimeoutError) {
      timed_out = true;
    }
    finally {
      state = 'URI=' + win.document.location.href +
              ', readyState=' + win.document.readyState;
      message = "controller.waitForPageLoad(" + state + ")";

      if (timed_out) {
        throw new errors.AssertionError(message);
      }

      broker.pass({'function': message});
    }
  }
}

var controllerAdditions = {
  'navigator:browser'  :browserAdditions
};

/**
 *  DEPRECATION WARNING
 *
 * The following methods have all been DEPRECATED as of Mozmill 2.0
 */
MozMillController.prototype.assertProperty = function (el, attrib, val) {
  logDeprecatedAssert("assertProperty");

  return this.assertJSProperty(el, attrib, val);
};

MozMillController.prototype.assertPropertyNotExist = function (el, attrib) {
  logDeprecatedAssert("assertPropertyNotExist");
  return this.assertNotJSProperty(el, attrib);
};

/**
 *  DEPRECATION WARNING
 *
 * The following methods have all been DEPRECATED as of Mozmill 2.0
 * Use the MozMillElement object instead (https://developer.mozilla.org/en/Mozmill/Mozmill_Element_Object)
 */
MozMillController.prototype.select = function (aElement, index, option, value) {
  logDeprecated("controller.select", "Use the MozMillElement object.");

  return aElement.select(index, option, value);
};

MozMillController.prototype.keypress = function (aElement, aKey, aModifiers, aExpectedEvent) {
  logDeprecated("controller.keypress", "Use the MozMillElement object.");

  if (!aElement) {
    aElement = new mozelement.MozMillElement("Elem", this.window);
  }

  return aElement.keypress(aKey, aModifiers, aExpectedEvent);
}

MozMillController.prototype.type = function (aElement, aText, aExpectedEvent) {
  logDeprecated("controller.type", "Use the MozMillElement object.");

  if (!aElement) {
    aElement = new mozelement.MozMillElement("Elem", this.window);
  }

  var that = this;
  var retval = true;
  Array.forEach(aText, function (letter) {
    if (!that.keypress(aElement, letter, {}, aExpectedEvent)) {
      retval = false; }
  });

  return retval;
}

MozMillController.prototype.mouseEvent = function (aElement, aOffsetX, aOffsetY, aEvent, aExpectedEvent) {
  logDeprecated("controller.mouseEvent", "Use the MozMillElement object.");

  return aElement.mouseEvent(aOffsetX, aOffsetY, aEvent, aExpectedEvent);
}

MozMillController.prototype.click = function (aElement, left, top, expectedEvent) {
  logDeprecated("controller.click", "Use the MozMillElement object.");

  return aElement.click(left, top, expectedEvent);
}

MozMillController.prototype.doubleClick = function (aElement, left, top, expectedEvent) {
  logDeprecated("controller.doubleClick", "Use the MozMillElement object.");

  return aElement.doubleClick(left, top, expectedEvent);
}

MozMillController.prototype.mouseDown = function (aElement, button, left, top, expectedEvent) {
  logDeprecated("controller.mouseDown", "Use the MozMillElement object.");

  return aElement.mouseDown(button, left, top, expectedEvent);
};

MozMillController.prototype.mouseOut = function (aElement, button, left, top, expectedEvent) {
  logDeprecated("controller.mouseOut", "Use the MozMillElement object.");

  return aElement.mouseOut(button, left, top, expectedEvent);
};

MozMillController.prototype.mouseOver = function (aElement, button, left, top, expectedEvent) {
  logDeprecated("controller.mouseOver", "Use the MozMillElement object.");

  return aElement.mouseOver(button, left, top, expectedEvent);
};

MozMillController.prototype.mouseUp = function (aElement, button, left, top, expectedEvent) {
  logDeprecated("controller.mouseUp", "Use the MozMillElement object.");

  return aElement.mouseUp(button, left, top, expectedEvent);
};

MozMillController.prototype.middleClick = function (aElement, left, top, expectedEvent) {
  logDeprecated("controller.middleClick", "Use the MozMillElement object.");

  return aElement.middleClick(aElement, left, top, expectedEvent);
}

MozMillController.prototype.rightClick = function (aElement, left, top, expectedEvent) {
  logDeprecated("controller.rightClick", "Use the MozMillElement object.");

  return aElement.rightClick(left, top, expectedEvent);
}

MozMillController.prototype.check = function (aElement, state) {
  logDeprecated("controller.check", "Use the MozMillElement object.");

  return aElement.check(state);
}

MozMillController.prototype.radio = function (aElement) {
  logDeprecated("controller.radio", "Use the MozMillElement object.");

  return aElement.select();
}

MozMillController.prototype.waitThenClick = function (aElement, timeout, interval) {
  logDeprecated("controller.waitThenClick", "Use the MozMillElement object.");

  return aElement.waitThenClick(timeout, interval);
}

MozMillController.prototype.waitForElement = function (aElement, timeout, interval) {
  logDeprecated("controller.waitForElement", "Use the MozMillElement object.");

  return aElement.waitForElement(timeout, interval);
}

MozMillController.prototype.waitForElementNotPresent = function (aElement, timeout, interval) {
  logDeprecated("controller.waitForElementNotPresent", "Use the MozMillElement object.");

  return aElement.waitForElementNotPresent(timeout, interval);
}
