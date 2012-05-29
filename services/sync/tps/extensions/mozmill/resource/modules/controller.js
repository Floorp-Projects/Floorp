/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["MozMillController", "globalEventRegistry", "sleep"];

var EventUtils = {}; Components.utils.import('resource://mozmill/stdlib/EventUtils.js', EventUtils);

var utils = {}; Components.utils.import('resource://mozmill/modules/utils.js', utils);
var elementslib = {}; Components.utils.import('resource://mozmill/modules/elementslib.js', elementslib);
var mozelement = {}; Components.utils.import('resource://mozmill/modules/mozelement.js', mozelement);
var frame = {}; Components.utils.import('resource://mozmill/modules/frame.js', frame);

var hwindow = Components.classes["@mozilla.org/appshell/appShellService;1"]
                .getService(Components.interfaces.nsIAppShellService)
                .hiddenDOMWindow;
var aConsoleService = Components.classes["@mozilla.org/consoleservice;1"].
     getService(Components.interfaces.nsIConsoleService);

// Declare most used utils functions in the controller namespace
var sleep = utils.sleep;
var assert = utils.assert;
var waitFor = utils.waitFor;

waitForEvents = function() {}

waitForEvents.prototype = {
  /**
   * Initialize list of events for given node
   */
  init : function waitForEvents_init(node, events) {
    if (node.getNode != undefined)
      node = node.getNode();

    this.events = events;
    this.node = node;
    node.firedEvents = {};
    this.registry = {};

    for each(e in events) {
      var listener = function(event) {
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
  wait : function waitForEvents_wait(timeout, interval)
  {
    for (var e in this.registry) {
      utils.waitFor(function() {
        return this.node.firedEvents[e] == true;
      }, "Timeout happened before event '" + ex +"' was fired.", timeout, interval);

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
var Menu = function(controller, menuSelector, document) {
  this._controller = controller;
  this._menu = null;

  document = document || controller.window.document;
  var node = document.querySelector(menuSelector);
  if (node) {
    // We don't unwrap nodes automatically yet (Bug 573185)
    node = node.wrappedJSObject || node;
    this._menu = new mozelement.Elem(node);
  }
  else {
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
  open : function(contextElement) {
    // We have to open the context menu
    var menu = this._menu.getNode();
    if ((menu.localName == "popup" || menu.localName == "menupopup") &&
        contextElement && contextElement.exists()) {
      this._controller.rightClick(contextElement);
      this._controller.waitFor(function() {
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
  close : function() {
    var menu = this._menu.getNode();

    this._controller.keypress(this._menu, "VK_ESCAPE", {});
    this._controller.waitFor(function() {
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
  getItem : function(itemSelector) {
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
  click : function(itemSelector) {
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
  keypress : function(key, modifier) {
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
  select : function(itemSelector, contextElement) {
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
  _buildMenu : function(menu) {
    var items = menu ? menu.childNodes : null;

    Array.forEach(items, function(item) {
      // When we have a menu node, fake a click onto it to populate
      // the sub menu with dynamic entries
      if (item.tagName == "menu") {
        var popup = item.querySelector("menupopup");
        if (popup) {
          if (popup.allowevents) {
            var popupEvent = this._controller.window.document.createEvent("MouseEvent");
            popupEvent.initMouseEvent("popupshowing", true, true, this._controller.window, 0,
                                             0, 0, 0, 0, false, false, false, false, 0, null);
            popup.dispatchEvent(popupEvent);
          }
          this._buildMenu(popup);
        }
      }
    }, this);
  }
};

var MozMillController = function (window) {
  this.window = window;

  this.mozmillModule = {};
  Components.utils.import('resource://mozmill/modules/mozmill.js', this.mozmillModule);

  utils.waitFor(function() {
    return window != null && this.isLoaded();
  }, "controller(): Window could not be initialized.", undefined, undefined, this);

  if ( controllerAdditions[window.document.documentElement.getAttribute('windowtype')] != undefined ) {
    this.prototype = new utils.Copy(this.prototype);
    controllerAdditions[window.document.documentElement.getAttribute('windowtype')](this);
    this.windowtype = window.document.documentElement.getAttribute('windowtype');
  }
}

MozMillController.prototype.sleep = utils.sleep;

// Open the specified url in the current tab
MozMillController.prototype.open = function(url)
{
  switch(this.mozmillModule.Application) {
    case "Firefox":
      this.window.gBrowser.loadURI(url);
      break;
    case "SeaMonkey":
      this.window.getBrowser().loadURI(url);
      break;
    default:
      throw new Error("MozMillController.open not supported.");
  }

  frame.events.pass({'function':'Controller.open()'});
}

/**
 * Take a screenshot of specified node
 *
 * @param {element} node
 *   the window or DOM element to capture
 * @param {string} name
 *   the name of the screenshot used in reporting and as filename
 * @param {boolean} save
 *   if true saves the screenshot as 'name.png' in tempdir, otherwise returns a dataURL
 * @param {element list} highlights
 *   a list of DOM elements to highlight by drawing a red rectangle around them
 */
MozMillController.prototype.screenShot = function _screenShot(node, name, save, highlights) {
  if (!node) {
    throw new Error("node is undefined");
  }

  // Unwrap the node and highlights
  if ("getNode" in node) node = node.getNode();
  if (highlights) {
    for (var i = 0; i < highlights.length; ++i) {
      if ("getNode" in highlights[i]) {
        highlights[i] = highlights[i].getNode();
      }
    }
  }

  // If save is false, a dataURL is used
  // Include both in the report anyway to avoid confusion and make the report easier to parse
  var filepath, dataURL;
  try {
    if (save) {
      filepath = utils.takeScreenshot(node, name, highlights);
    } else {
      dataURL = utils.takeScreenshot(node, undefined, highlights);
    }
  } catch (e) {
    throw new Error("controller.screenShot() failed: " + e);
  }

  // Find the name of the test function
  for (var attr in frame.events.currentModule) {
    if (frame.events.currentModule[attr] == frame.events.currentTest) {
      var testName = attr;
      break;
    }
  }

  // Create a timestamp
  var d = new Date();
  // Report object
  var obj = { "filepath": filepath,
              "dataURL": dataURL,
              "name": name,
              "timestamp": d.toLocaleString(),
              "test_file": frame.events.currentModule.__file__,
              "test_name": testName,
            }
  // Send the screenshot object to python over jsbridge
  this.fireEvent("screenShot", obj);

  frame.events.pass({'function':'controller.screenShot()'});
}

/**
 * Checks if the specified window has been loaded
 *
 * @param {DOMWindow} [window=this.window] Window object to check for loaded state
 */
MozMillController.prototype.isLoaded = function(window) {
  var win = window || this.window;

  return ("mozmillDocumentLoaded" in win) && win.mozmillDocumentLoaded;
};

MozMillController.prototype.waitFor = function(callback, message, timeout,
                                               interval, thisObject) {
  utils.waitFor(callback, message, timeout, interval, thisObject);

  frame.events.pass({'function':'controller.waitFor()'});
}

MozMillController.prototype.__defineGetter__("waitForEvents", function() {
  if (this._waitForEvents == undefined)
    this._waitForEvents = new waitForEvents();
  return this._waitForEvents;
});

/**
 * Wrapper function to create a new instance of a menu
 * @see Menu
 */
MozMillController.prototype.getMenu = function (menuSelector, document) {
  return new Menu(this, menuSelector, document);
};

MozMillController.prototype.__defineGetter__("mainMenu", function() {
  return this.getMenu("menubar");
});

MozMillController.prototype.__defineGetter__("menus", function() {
        throw('controller.menus - DEPRECATED Use controller.mainMenu instead.');

});

MozMillController.prototype.waitForImage = function (elem, timeout, interval) {
  this.waitFor(function() {
    return elem.getNode().complete == true;
  }, "timeout exceeded for waitForImage " + elem.getInfo(), timeout, interval);

  frame.events.pass({'function':'Controller.waitForImage()'});
}

MozMillController.prototype.fireEvent = function (name, obj) {
  if (name == "userShutdown") {
    frame.events.toggleUserShutdown(obj);
  }
  frame.events.fireEvent(name, obj);
}

MozMillController.prototype.startUserShutdown = function (timeout, restart, next, resetProfile) {
  if (restart && resetProfile) {
      throw new Error("You can't have a user-restart and reset the profile; there is a race condition");
  }
  this.fireEvent('userShutdown', {'user': true,
                                  'restart': Boolean(restart),
                                  'next': next,
                                  'resetProfile': Boolean(resetProfile)});
  this.window.setTimeout(this.fireEvent, timeout, 'userShutdown', 0);
}

MozMillController.prototype.restartApplication = function (next, resetProfile)
{
  // restart the application via the python runner
  // - next : name of the next test function to run after restart
  // - resetProfile : whether to reset the profile after restart
  this.fireEvent('userShutdown', {'user': false,
                                  'restart': true,
                                  'next': next,
                                  'resetProfile': Boolean(resetProfile)});
  utils.getMethodInWindows('goQuitApplication')();
}

MozMillController.prototype.stopApplication = function (resetProfile)
{
  // stop the application via the python runner
  // - resetProfile : whether to reset the profile after shutdown
  this.fireEvent('userShutdown', {'user': false,
                                  'restart': false,
                                  'resetProfile': Boolean(resetProfile)});
  utils.getMethodInWindows('goQuitApplication')();
}

//Browser navigation functions
MozMillController.prototype.goBack = function(){
  //this.window.focus();
  this.window.content.history.back();
  frame.events.pass({'function':'Controller.goBack()'});
  return true;
}
MozMillController.prototype.goForward = function(){
  //this.window.focus();
  this.window.content.history.forward();
  frame.events.pass({'function':'Controller.goForward()'});
  return true;
}
MozMillController.prototype.refresh = function(){
  //this.window.focus();
  this.window.content.location.reload(true);
  frame.events.pass({'function':'Controller.refresh()'});
  return true;
}

function logDeprecated(funcName, message) {
   frame.log({'function': funcName + '() - DEPRECATED', 'message': funcName + '() is deprecated' + message});
}

function logDeprecatedAssert(funcName) {
   logDeprecated('controller.' + funcName, '. use the generic `assert` module instead');
}

MozMillController.prototype.assertText = function (el, text) {
  logDeprecatedAssert("assertText");
  //this.window.focus();
  var n = el.getNode();

  if (n && n.innerHTML == text){
    frame.events.pass({'function':'Controller.assertText()'});
    return true;
   }

  throw new Error("could not validate element " + el.getInfo()+" with text "+ text);
  return false;

};

//Assert that a specified node exists
MozMillController.prototype.assertNode = function (el) {
  logDeprecatedAssert("assertNode");

  //this.window.focus();
  var element = el.getNode();
  if (!element){
    throw new Error("could not find element " + el.getInfo());
    return false;
  }
  frame.events.pass({'function':'Controller.assertNode()'});
  return true;
};

// Assert that a specified node doesn't exist
MozMillController.prototype.assertNodeNotExist = function (el) {
  logDeprecatedAssert("assertNodeNotExist");

  //this.window.focus();
  try {
    var element = el.getNode();
  } catch(err){
    frame.events.pass({'function':'Controller.assertNodeNotExist()'});
    return true;
  }

  if (element) {
    throw new Error("Unexpectedly found element " + el.getInfo());
    return false;
  } else {
    frame.events.pass({'function':'Controller.assertNodeNotExist()'});
    return true;
  }
};

//Assert that a form element contains the expected value
MozMillController.prototype.assertValue = function (el, value) {
  logDeprecatedAssert("assertValue");

  //this.window.focus();
  var n = el.getNode();

  if (n && n.value == value){
    frame.events.pass({'function':'Controller.assertValue()'});
    return true;
  }
  throw new Error("could not validate element " + el.getInfo()+" with value "+ value);
  return false;
};

/**
 * Check if the callback function evaluates to true
 */
MozMillController.prototype.assert = function(callback, message, thisObject)
{
  logDeprecatedAssert("assert");
  utils.assert(callback, message, thisObject);

  frame.events.pass({'function': ": controller.assert('" + callback + "')"});
  return true;
}

//Assert that a provided value is selected in a select element
MozMillController.prototype.assertSelected = function (el, value) {
  logDeprecatedAssert("assertSelected");

  //this.window.focus();
  var n = el.getNode();
  var validator = value;

  if (n && n.options[n.selectedIndex].value == validator){
    frame.events.pass({'function':'Controller.assertSelected()'});
    return true;
    }
  throw new Error("could not assert value for element " + el.getInfo()+" with value "+ value);
  return false;
};

//Assert that a provided checkbox is checked
MozMillController.prototype.assertChecked = function (el) {
  logDeprecatedAssert("assertChecked");

  //this.window.focus();
  var element = el.getNode();

  if (element && element.checked == true){
    frame.events.pass({'function':'Controller.assertChecked()'});
    return true;
    }
  throw new Error("assert failed for checked element " + el.getInfo());
  return false;
};

// Assert that a provided checkbox is not checked
MozMillController.prototype.assertNotChecked = function (el) {
  logDeprecatedAssert("assertNotChecked");

  var element = el.getNode();

  if (!element) {
    throw new Error("Could not find element" + el.getInfo());
  }

  if (!element.hasAttribute("checked") || element.checked != true){
    frame.events.pass({'function':'Controller.assertNotChecked()'});
    return true;
    }
  throw new Error("assert failed for not checked element " + el.getInfo());
  return false;
};

/**
 * Assert that an element's javascript property exists or has a particular value
 *
 * if val is undefined, will return true if the property exists.
 * if val is specified, will return true if the property exists and has the correct value
 */
MozMillController.prototype.assertJSProperty = function(el, attrib, val) {
  logDeprecatedAssert("assertJSProperty");

  var element = el.getNode();
  if (!element){
    throw new Error("could not find element " + el.getInfo());
    return false;
  }
  var value = element[attrib];
  var res = (value !== undefined && (val === undefined ? true : String(value) == String(val)));
  if (res) {
    frame.events.pass({'function':'Controller.assertJSProperty("' + el.getInfo() + '") : ' + val});
  } else {
    throw new Error("Controller.assertJSProperty(" + el.getInfo() + ") : " +
                     (val === undefined ? "property '" + attrib + "' doesn't exist" : val + " == " + value));
  }
  return res;
};

/**
 * Assert that an element's javascript property doesn't exist or doesn't have a particular value
 *
 * if val is undefined, will return true if the property doesn't exist.
 * if val is specified, will return true if the property doesn't exist or doesn't have the specified value
 */
MozMillController.prototype.assertNotJSProperty = function(el, attrib, val) {
  logDeprecatedAssert("assertNotJSProperty");

  var element = el.getNode();
  if (!element){
    throw new Error("could not find element " + el.getInfo());
    return false;
  }
  var value = element[attrib];
  var res = (val === undefined ? value === undefined : String(value) != String(val));
  if (res) {
    frame.events.pass({'function':'Controller.assertNotProperty("' + el.getInfo() + '") : ' + val});
  } else {
    throw new Error("Controller.assertNotJSProperty(" + el.getInfo() + ") : " +
                     (val === undefined ? "property '" + attrib + "' exists" : val + " != " + value));
  }
  return res;
};

/**
 * Assert that an element's dom property exists or has a particular value
 *
 * if val is undefined, will return true if the property exists.
 * if val is specified, will return true if the property exists and has the correct value
 */
MozMillController.prototype.assertDOMProperty = function(el, attrib, val) {
  logDeprecatedAssert("assertDOMProperty");

  var element = el.getNode();
  if (!element){
    throw new Error("could not find element " + el.getInfo());
    return false;
  }
  var value, res = element.hasAttribute(attrib);
  if (res && val !== undefined) {
    value = element.getAttribute(attrib);
    res = (String(value) == String(val));
  }

  if (res) {
    frame.events.pass({'function':'Controller.assertDOMProperty("' + el.getInfo() + '") : ' + val});
  } else {
    throw new Error("Controller.assertDOMProperty(" + el.getInfo() + ") : " +
                     (val === undefined ? "property '" + attrib + "' doesn't exist" : val + " == " + value));
  }
  return res;
};

/**
 * Assert that an element's dom property doesn't exist or doesn't have a particular value
 *
 * if val is undefined, will return true if the property doesn't exist.
 * if val is specified, will return true if the property doesn't exist or doesn't have the specified value
 */
MozMillController.prototype.assertNotDOMProperty = function(el, attrib, val) {
  logDeprecatedAssert("assertNotDOMProperty");

  var element = el.getNode();
  if (!element){
    throw new Error("could not find element " + el.getInfo());
    return false;
  }
  var value, res = element.hasAttribute(attrib);
  if (res && val !== undefined) {
    value = element.getAttribute(attrib);
    res = (String(value) == String(val));
  }
  if (!res) {
    frame.events.pass({'function':'Controller.assertNotDOMProperty("' + el.getInfo() + '") : ' + val});
  } else {
    throw new Error("Controller.assertNotDOMProperty(" + el.getInfo() + ") : " +
                     (val == undefined ? "property '" + attrib + "' exists" : val + " == " + value));
  }
  return !res;
};

// deprecated - Use assertNotJSProperty or assertNotDOMProperty instead
MozMillController.prototype.assertProperty = function(el, attrib, val) {
  logDeprecatedAssert("assertProperty");
  return this.assertJSProperty(el, attrib, val);
};

// deprecated - Use assertNotJSProperty or assertNotDOMProperty instead
MozMillController.prototype.assertPropertyNotExist = function(el, attrib) {
  logDeprecatedAssert("assertPropertyNotExist");
  return this.assertNotJSProperty(el, attrib);
};

// Assert that a specified image has actually loaded
// The Safari workaround results in additional requests
// for broken images (in Safari only) but works reliably
MozMillController.prototype.assertImageLoaded = function (el) {
  logDeprecatedAssert("assertImageLoaded");

  //this.window.focus();
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
  // False -- Img failed to load in IE/Safari, or is
  // still trying to load in FF
  if (comp === false) {
    ret = false;
  }
  // True, but image has no size -- image failed to
  // load in FF
  else if (comp === true && img.naturalWidth == 0) {
    ret = false;
  }
  // Otherwise all we can do is assume everything's
  // hunky-dory
  else {
    ret = true;
  }
  if (ret) {
    frame.events.pass({'function':'Controller.assertImageLoaded'});
  } else {
    throw new Error('Controller.assertImageLoaded() failed.')
  }

  return ret;
};

// Drag one element to the top x,y coords of another specified element
MozMillController.prototype.mouseMove = function (doc, start, dest) {
  // if one of these elements couldn't be looked up
  if (typeof start != 'object'){
    throw new Error("received bad coordinates");
    return false;
  }
  if (typeof dest != 'object'){
    throw new Error("received bad coordinates");
    return false;
  }

  var triggerMouseEvent = function(element, clientX, clientY) {
    clientX = clientX ? clientX: 0;
    clientY = clientY ? clientY: 0;

    // make the mouse understand where it is on the screen
    var screenX = element.boxObject.screenX ? element.boxObject.screenX : 0;
    var screenY = element.boxObject.screenY ? element.boxObject.screenY : 0;

    var evt = element.ownerDocument.createEvent('MouseEvents');
    if (evt.initMouseEvent) {
      evt.initMouseEvent('mousemove', true, true, element.ownerDocument.defaultView, 1, screenX, screenY, clientX, clientY)
    }
    else {
      //LOG.warn("element doesn't have initMouseEvent; firing an event which should -- but doesn't -- have other mouse-event related attributes here, as well as controlKeyDown, altKeyDown, shiftKeyDown, metaKeyDown");
      evt.initEvent('mousemove', true, true);
    }
    element.dispatchEvent(evt);
  };

  // Do the initial move to the drag element position
  triggerMouseEvent(doc.body, start[0], start[1]);
  triggerMouseEvent(doc.body, dest[0], dest[1]);
  frame.events.pass({'function':'Controller.mouseMove()'});
  return true;
}

// Drag an element to the specified offset on another element, firing mouse and drag events.
// Returns the captured dropEffect. Adapted from EventUtils' synthesizeDrop()
MozMillController.prototype.dragToElement = function(src, dest, offsetX,
    offsetY, aWindow, dropEffect, dragData) {
  srcElement = src.getNode();
  destElement = dest.getNode();
  aWindow = aWindow || srcElement.ownerDocument.defaultView;
  offsetX = offsetX || 20;
  offsetY = offsetY || 20;

  var dataTransfer;

  var trapDrag = function(event) {
    dataTransfer = event.dataTransfer;
    if(!dragData)
      return;

    for (var i = 0; i < dragData.length; i++) {
      var item = dragData[i];
      for (var j = 0; j < item.length; j++) {
        dataTransfer.mozSetDataAt(item[j].type, item[j].data, i);
      }
    }
    dataTransfer.dropEffect = dropEffect || "move";
    event.preventDefault();
    event.stopPropagation();
  }

  aWindow.addEventListener("dragstart", trapDrag, true);
  EventUtils.synthesizeMouse(srcElement, 2, 2, { type: "mousedown" }, aWindow); // fire mousedown 2 pixels from corner of element
  EventUtils.synthesizeMouse(srcElement, 11, 11, { type: "mousemove" }, aWindow);
  EventUtils.synthesizeMouse(srcElement, offsetX, offsetY, { type: "mousemove" }, aWindow);
  aWindow.removeEventListener("dragstart", trapDrag, true);

  var event = aWindow.document.createEvent("DragEvents");
  event.initDragEvent("dragenter", true, true, aWindow, 0, 0, 0, 0, 0, false, false, false, false, 0, null, dataTransfer);
  destElement.dispatchEvent(event);

  var event = aWindow.document.createEvent("DragEvents");
  event.initDragEvent("dragover", true, true, aWindow, 0, 0, 0, 0, 0, false, false, false, false, 0, null, dataTransfer);
  if (destElement.dispatchEvent(event)) {
    EventUtils.synthesizeMouse(destElement, offsetX, offsetY, { type: "mouseup" }, aWindow);
    return "none";
  }

  event = aWindow.document.createEvent("DragEvents");
  event.initDragEvent("drop", true, true, aWindow, 0, 0, 0, 0, 0, false, false, false, false, 0, null, dataTransfer);
  destElement.dispatchEvent(event);
  EventUtils.synthesizeMouse(destElement, offsetX, offsetY, { type: "mouseup" }, aWindow);

  return dataTransfer.dropEffect;
}

function preferencesAdditions(controller) {
  var mainTabs = controller.window.document.getAnonymousElementByAttribute(controller.window.document.documentElement, 'anonid', 'selector');
  controller.tabs = {};
  for (var i = 0; i < mainTabs.childNodes.length; i++) {
    var node  = mainTabs.childNodes[i];
    var obj = {'button':node}
    controller.tabs[i] = obj;
    var label = node.attributes.item('label').value.replace('pane', '');
    controller.tabs[label] = obj;
  }
  controller.prototype.__defineGetter__("activeTabButton",
    function () {return mainTabs.getElementsByAttribute('selected', true)[0];
  })
}

function Tabs (controller) {
  this.controller = controller;
}
Tabs.prototype.getTab = function(index) {
  return this.controller.window.gBrowser.browsers[index].contentDocument;
}
Tabs.prototype.__defineGetter__("activeTab", function() {
  return this.controller.window.gBrowser.selectedBrowser.contentDocument;
})
Tabs.prototype.selectTab = function(index) {
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
Tabs.prototype.getTabWindow = function(index) {
  return this.findWindow(this.getTab(index));
}
Tabs.prototype.__defineGetter__("activeTabWindow", function () {
  return this.findWindow(this.activeTab);
})
Tabs.prototype.__defineGetter__("length", function () {
  return this.controller.window.gBrowser.browsers.length;
})
Tabs.prototype.__defineGetter__("activeTabIndex", function() {
  return this.controller.window.gBrowser.tabContainer.selectedIndex;
})
Tabs.prototype.selectTabIndex = function(i) {
  this.controller.window.gBrowser.selectTabAtIndex(i);
}

function browserAdditions (controller) {
  controller.tabs = new Tabs(controller);

  controller.waitForPageLoad = function(aDocument, aTimeout, aInterval) {
    var timeout = aTimeout || 30000;
    var owner;

    // If a user tries to do waitForPageLoad(2000), this will assign the
    // interval the first arg which is most likely what they were expecting
    if (typeof(aDocument) == "number"){
      timeout = aDocument;
    }

    // If the document is a tab find the corresponding browser element.
    // Otherwise we have to handle an embedded web page.
    if (aDocument && typeof(aDocument) == "object") {
      owner = this.window.gBrowser.getBrowserForDocument(aDocument);

      if (!owner) {
        // If the document doesn't belong to a tab it will be a
        // HTML element (e.g. iframe) embedded inside a tab.
        // In such a case use the default window of the document.
        owner = aDocument.defaultView;
       }
     }

    // If no owner has been specified, fallback to the selected tab browser
    owner = owner || this.window.gBrowser.selectedBrowser;

    // Wait until the content in the tab has been loaded
    this.waitFor(function() {
        return this.isLoaded(owner);
    }, "controller.waitForPageLoad(): Timeout waiting for page loaded.",
       timeout, aInterval, this);
    frame.events.pass({'function':'controller.waitForPageLoad()'});
  }
}

controllerAdditions = {
  'Browser:Preferences':preferencesAdditions,
  'navigator:browser'  :browserAdditions,
}

/**
 *  DEPRECATION WARNING
 *
 * The following methods have all been DEPRECATED as of Mozmill 2.0
 * Use the MozMillElement object instead (https://developer.mozilla.org/en/Mozmill/Mozmill_Element_Object)
 */
MozMillController.prototype.select = function (elem, index, option, value) {
  return elem.select(index, option, value);
};

MozMillController.prototype.keypress = function(aTarget, aKey, aModifiers, aExpectedEvent) {
  return aTarget.keypress(aKey, aModifiers, aExpectedEvent);
}

MozMillController.prototype.type = function (aTarget, aText, aExpectedEvent) {
  return aTarget.sendKeys(aText, aExpectedEvent);
}

MozMillController.prototype.mouseEvent = function(aTarget, aOffsetX, aOffsetY, aEvent, aExpectedEvent) {
  return aTarget.mouseEvent(aOffsetX, aOffsetY, aEvent, aExpectedEvent);
}

MozMillController.prototype.click = function(elem, left, top, expectedEvent) {
  return elem.click(left, top, expectedEvent);
}

MozMillController.prototype.doubleClick = function(elem, left, top, expectedEvent) {
  return elem.doubleClick(left, top, expectedEvent);
}

MozMillController.prototype.mouseDown = function (elem, button, left, top, expectedEvent) {
  return elem.mouseDown(button, left, top, expectedEvent);
};

MozMillController.prototype.mouseOut = function (elem, button, left, top, expectedEvent) {
  return elem.mouseOut(button, left, top, expectedEvent);
};

MozMillController.prototype.mouseOver = function (elem, button, left, top, expectedEvent) {
  return elem.mouseOver(button, left, top, expectedEvent);
};

MozMillController.prototype.mouseUp = function (elem, button, left, top, expectedEvent) {
  return elem.mouseUp(button, left, top, expectedEvent);
};

MozMillController.prototype.middleClick = function(elem, left, top, expectedEvent) {
  return elem.middleClick(elem, left, top, expectedEvent);
}

MozMillController.prototype.rightClick = function(elem, left, top, expectedEvent) {
  return elem.rightClick(left, top, expectedEvent);
}

MozMillController.prototype.check = function(elem, state) {
  return elem.check(state);
}

MozMillController.prototype.radio = function(elem) {
  return elem.select();
}

MozMillController.prototype.waitThenClick = function (elem, timeout, interval) {
  return elem.waitThenClick(timeout, interval);
}

MozMillController.prototype.waitForElement = function(elem, timeout, interval) {
  return elem.waitForElement(timeout, interval);
}

MozMillController.prototype.waitForElementNotPresent = function(elem, timeout, interval) {
  return elem.waitForElementNotPresent(timeout, interval);
}

