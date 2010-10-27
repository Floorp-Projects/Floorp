// ***** BEGIN LICENSE BLOCK *****
// Version: MPL 1.1/GPL 2.0/LGPL 2.1
//
// The contents of this file are subject to the Mozilla Public License Version
// 1.1 (the "License"); you may not use this file except in compliance with
// the License. You may obtain a copy of the License at
// http://www.mozilla.org/MPL/
//
// Software distributed under the License is distributed on an "AS IS" basis,
// WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
// for the specific language governing rights and limitations under the
// License.
//
// The Original Code is Mozilla Corporation Code.
//
// The Initial Developer of the Original Code is
// Adam Christian.
// Portions created by the Initial Developer are Copyright (C) 2008
// the Initial Developer. All Rights Reserved.
//
// Contributor(s):
//  Adam Christian <adam.christian@gmail.com>
//  Mikeal Rogers <mikeal.rogers@gmail.com>
//  Henrik Skupin <hskupin@mozilla.com>
//
// Alternatively, the contents of this file may be used under the terms of
// either the GNU General Public License Version 2 or later (the "GPL"), or
// the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
// in which case the provisions of the GPL or the LGPL are applicable instead
// of those above. If you wish to allow use of your version of this file only
// under the terms of either the GPL or the LGPL, and not to allow others to
// use your version of this file under the terms of the MPL, indicate your
// decision by deleting the provisions above and replace them with the notice
// and other provisions required by the GPL or the LGPL. If you do not delete
// the provisions above, a recipient may use your version of this file under
// the terms of any one of the MPL, the GPL or the LGPL.
//
// ***** END LICENSE BLOCK *****

var EXPORTED_SYMBOLS = ["MozMillController", "waitForEval", "MozMillAsyncTest",
                        "globalEventRegistry", "sleep"];

var EventUtils = {}; Components.utils.import('resource://mozmill/stdlib/EventUtils.js', EventUtils);

var events = {}; Components.utils.import('resource://mozmill/modules/events.js', events);
var utils = {}; Components.utils.import('resource://mozmill/modules/utils.js', utils);
var elementslib = {}; Components.utils.import('resource://mozmill/modules/elementslib.js', elementslib);
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
var waitForEval = utils.waitForEval;


waitForEvents = function() {}

waitForEvents.prototype = {
  /**
   *  Initialize list of events for given node
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
    this._menu = new elementslib.Elem(node);
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
    if (menu.localName == "menupopup" &&
        contextElement && contextElement.exists()) {
      this._controller.rightClick(contextElement);
      this._controller.waitFor(function() {
        return menu.state == "open";
      }, "Context menu has been opened.");
    }

    // Run through the entire menu and populate with dynamic entries
    this._buildMenu(this._controller, menu);

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

    return new elementslib.Elem(node);
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
            events.fakeOpenPopup(this._controller.window, popup);
          }
          this._buildMenu(popup);
        }
      }
    }, this);
  }
};

/**
 * Deprecated - Has to be removed with Mozmill 2.0
 */
var MenuTree = function(aWindow, aMenu) {
  var items = aMenu ? aMenu.childNodes : null;

  for each (var node in items) {
    var entry = null;

    switch (node.tagName) {
      case "menu":
        // Fake a click onto the menu to add dynamic entries
        var popup = node.querySelector("menupopup");
        if (popup) {
          if (popup.allowevents) {
            events.fakeOpenPopup(aWindow, popup);
          }
          entry = new MenuTree(aWindow, popup);
        }
        break;
      case "menuitem":
        entry = node;
        break;
      default:
        continue;
    }

    if (entry) {
      var label = node.getAttribute("label");
      this[label] = entry;

      if (node.id)
        this[node.id] = this[label];
    }
  }
};

var MozMillController = function (window) {
  this.window = window;

  this.mozmillModule = {};
  Components.utils.import('resource://mozmill/modules/mozmill.js', this.mozmillModule);

  utils.waitFor(function() {
    return window != null && (window.documentLoaded != undefined);
  }, "controller(): Window could not be initialized.");

  if ( controllerAdditions[window.document.documentElement.getAttribute('windowtype')] != undefined ) {
    this.prototype = new utils.Copy(this.prototype);
    controllerAdditions[window.document.documentElement.getAttribute('windowtype')](this);
    this.windowtype = window.document.documentElement.getAttribute('windowtype');
  }
}

MozMillController.prototype.sleep = utils.sleep;

/**
 * Synthesize a keypress event on the given element
 *
 * @param {ElemBase} aTarget
 *        Element which will receive the keypress event
 * @param {string} aKey
 *        Key to use for synthesizing the keypress event. It can be a simple
 *        character like "k" or a string like "VK_ESCAPE" for command keys
 * @param {object} aModifiers
 *        Information about the modifier keys to send
 *        Elements: accelKey   - Hold down the accelerator key (ctrl/meta)
 *                               [optional - default: false]
 *                  altKey     - Hold down the alt key
 *                              [optional - default: false]
 *                  ctrlKey    - Hold down the ctrl key
 *                               [optional - default: false]
 *                  metaKey    - Hold down the meta key (command key on Mac)
 *                               [optional - default: false]
 *                  shiftKey   - Hold down the shift key
 *                               [optional - default: false]
 * @param {object} aExpectedEvent
 *        Information about the expected event to occur
 *        Elements: target     - Element which should receive the event
 *                               [optional - default: current element]
 *                  type       - Type of the expected key event
 */
MozMillController.prototype.keypress = function(aTarget, aKey, aModifiers, aExpectedEvent) {
  var element = (aTarget == null) ? this.window : aTarget.getNode();
  if (!element) {
    throw new Error("Could not find element " + aTarget.getInfo());
    return false;
  }

  events.triggerKeyEvent(element, 'keypress', aKey, aModifiers, aExpectedEvent);

  frame.events.pass({'function':'Controller.keypress()'});
  return true;
}

/**
 * Synthesize keypress events for each character on the given element
 *
 * @param {ElemBase} aTarget
 *        Element which will receive the type event
 * @param {string} aText
 *        The text to send as single keypress events
 * @param {object} aExpectedEvent
 *        Information about the expected event to occur
 *        Elements: target     - Element which should receive the event
 *                               [optional - default: current element]
 *                  type       - Type of the expected key event
 */
MozMillController.prototype.type = function (aTarget, aText, aExpectedEvent) {
  var element = (aTarget == null) ? this.window : aTarget.getNode();
  if (!element) {
    throw new Error("could not find element " + aTarget.getInfo());
    return false;
  }

  Array.forEach(aText, function(letter) {
    events.triggerKeyEvent(element, 'keypress', letter, {}, aExpectedEvent);
  });

  frame.events.pass({'function':'Controller.type()'});
  return true;
}

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
 * Synthesize a general mouse event on the given element
 *
 * @param {ElemBase} aTarget
 *        Element which will receive the mouse event
 * @param {number} aOffsetX
 *        Relative x offset in the elements bounds to click on
 * @param {number} aOffsetY
 *        Relative y offset in the elements bounds to click on
 * @param {object} aEvent
 *        Information about the event to send
 *        Elements: accelKey   - Hold down the accelerator key (ctrl/meta)
 *                               [optional - default: false]
 *                  altKey     - Hold down the alt key
 *                               [optional - default: false]
 *                  button     - Mouse button to use
 *                               [optional - default: 0]
 *                  clickCount - Number of counts to click
 *                               [optional - default: 1]
 *                  ctrlKey    - Hold down the ctrl key
 *                               [optional - default: false]
 *                  metaKey    - Hold down the meta key (command key on Mac)
 *                               [optional - default: false]
 *                  shiftKey   - Hold down the shift key
 *                               [optional - default: false]
 *                  type       - Type of the mouse event ('click', 'mousedown',
 *                               'mouseup', 'mouseover', 'mouseout')
 *                               [optional - default: 'mousedown' + 'mouseup']
 * @param {object} aExpectedEvent
 *        Information about the expected event to occur
 *        Elements: target     - Element which should receive the event
 *                               [optional - default: current element]
 *                  type       - Type of the expected mouse event
 */
MozMillController.prototype.mouseEvent = function(aTarget, aOffsetX, aOffsetY,
                                                  aEvent, aExpectedEvent) {

  var element = aTarget.getNode();
  if (!element) {
    throw new Error(arguments.callee.name + ": could not find element " +
                    aTarget.getInfo());
  }

  // If no offset is given we will use the center of the element to click on.
  var rect = element.getBoundingClientRect();
  if (isNaN(aOffsetX))
    aOffsetX = rect.width / 2;
  if (isNaN(aOffsetY))
    aOffsetY = rect.height / 2;

  // Scroll element into view otherwise the click will fail
  if (element.scrollIntoView)
    element.scrollIntoView();

  if (aExpectedEvent) {
    // The expected event type has to be set
    if (!aExpectedEvent.type)
      throw new Error(arguments.callee.name + ": Expected event type not specified");

    // If no target has been specified use the specified element
    var target = aExpectedEvent.target ? aExpectedEvent.target.getNode() : element;
    if (!target) {
      throw new Error(arguments.callee.name + ": could not find element " +
                      aExpectedEvent.target.getInfo());
    }

    EventUtils.synthesizeMouseExpectEvent(element, aOffsetX, aOffsetY, aEvent,
                                          target, aExpectedEvent.event,
                                          "controller.mouseEvent()",
                                          element.ownerDocument.defaultView);
  } else {
    EventUtils.synthesizeMouse(element, aOffsetX, aOffsetY, aEvent,
                               element.ownerDocument.defaultView);
  }

  sleep(0);
}

/**
 * Synthesize a mouse click event on the given element
 */
MozMillController.prototype.click = function(elem, left, top, expectedEvent) {
  var element = elem.getNode()

  // Handle menu items differently
  if (element && element.tagName == "menuitem") {
    element.click();
  } else {
    this.mouseEvent(elem, left, top, {}, expectedEvent);
  }

  frame.events.pass({'function':'controller.click()'});
}

/**
 * Synthesize a double click on the given element
 */
MozMillController.prototype.doubleClick = function(elem, left, top, expectedEvent) {
  this.mouseEvent(elem, left, top, {clickCount: 2}, expectedEvent);

  frame.events.pass({'function':'controller.doubleClick()'});
  return true;
}

/**
 * Synthesize a mouse down event on the given element
 */
MozMillController.prototype.mouseDown = function (elem, button, left, top, expectedEvent) {
  this.mouseEvent(elem, left, top, {button: button, type: "mousedown"}, expectedEvent);

  frame.events.pass({'function':'controller.mouseDown()'});
  return true;
};

/**
 * Synthesize a mouse out event on the given element
 */
MozMillController.prototype.mouseOut = function (elem, button, left, top, expectedEvent) {
  this.mouseEvent(elem, left, top, {button: button, type: "mouseout"}, expectedEvent);

  frame.events.pass({'function':'controller.mouseOut()'});
  return true;
};

/**
 * Synthesize a mouse over event on the given element
 */
MozMillController.prototype.mouseOver = function (elem, button, left, top, expectedEvent) {
  this.mouseEvent(elem, left, top, {button: button, type: "mouseover"}, expectedEvent);

  frame.events.pass({'function':'controller.mouseOver()'});
  return true;
};

/**
 * Synthesize a mouse up event on the given element
 */
MozMillController.prototype.mouseUp = function (elem, button, left, top, expectedEvent) {
  this.mouseEvent(elem, left, top, {button: button, type: "mouseup"}, expectedEvent);

  frame.events.pass({'function':'controller.mouseUp()'});
  return true;
};

/**
 * Synthesize a mouse middle click event on the given element
 */
MozMillController.prototype.middleClick = function(elem, left, top, expectedEvent) {
  this.mouseEvent(elem, left, top, {button: 1}, expectedEvent);

  frame.events.pass({'function':'controller.middleClick()'});
  return true;
}

/**
 * Synthesize a mouse right click event on the given element
 */
MozMillController.prototype.rightClick = function(elem, left, top, expectedEvent) {
  this.mouseEvent(elem, left, top, {type : "contextmenu", button: 2 }, expectedEvent);

  frame.events.pass({'function':'controller.rightClick()'});
  return true;
}

/**
 * Synthesize a mouse right click event on the given element (deprecated)
 */
MozMillController.prototype.rightclick = function(){
  frame.log({function:'rightclick - Deprecation Warning', message:'Controller.rightclick should be renamed to Controller.rightClick'});
  this.rightClick.apply(this, arguments);
}

/**
 * Enable/Disable a checkbox depending on the target state
 */
MozMillController.prototype.check = function(el, state) {
  var result = false;
  var element = el.getNode();

  if (!element) {
    throw new Error("could not find element " + el.getInfo());
    return false;
  }

  state = (typeof(state) == "boolean") ? state : false;
  if (state != element.checked) {
    this.click(el);
    this.waitFor(function() {
      return element.checked == state;
    }, "Checkbox " + el.getInfo() + " could not be checked/unchecked", 500);

    result = true;
  }

  frame.events.pass({'function':'Controller.check(' + el.getInfo() + ', state: ' + state + ')'});
  return result;
}

/**
 * Select the given radio button
 */
MozMillController.prototype.radio = function(el)
{
  var element = el.getNode();
  if (!element) {
    throw new Error("could not find element " + el.getInfo());
    return false;
  }

  this.click(el);
  this.waitFor(function() {
    return element.selected == true;
  }, "Radio button " + el.getInfo() + " could not be selected", 500);

  frame.events.pass({'function':'Controller.radio(' + el.getInfo() + ')'});
  return true;
}

MozMillController.prototype.waitFor = function(callback, message, timeout,
                                               interval, thisObject) {
  utils.waitFor(callback, message, timeout, interval, thisObject);

  frame.events.pass({'function':'controller.waitFor()'});
}

MozMillController.prototype.waitForEval = function(expression, timeout, interval, subject) {
  waitFor(function() {
    return eval(expression);
  }, "controller.waitForEval: Timeout exceeded for '" + expression + "'", timeout, interval);

  frame.events.pass({'function':'controller.waitForEval()'});
}

MozMillController.prototype.waitForElement = function(elem, timeout, interval) {
  this.waitFor(function() {
    return elem.exists();
  }, "Timeout exceeded for waitForElement " + elem.getInfo(), timeout, interval);

  frame.events.pass({'function':'Controller.waitForElement()'});
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
  frame.log({'property': 'controller.menus - DEPRECATED',
             'message': 'Use controller.mainMenu instead.'});

  var menubar = this.window.document.querySelector("menubar");
  return new MenuTree(this.window, menubar);
});

MozMillController.prototype.waitForImage = function (elem, timeout, interval) {
  this.waitFor(function() {
    return elem.getNode().complete == true;
  }, "timeout exceeded for waitForImage " + elem.getInfo(), timeout, interval);

  frame.events.pass({'function':'Controller.waitForImage()'});
}

MozMillController.prototype.waitThenClick = function (elem, timeout, interval) {
  this.waitForElement(elem, timeout, interval);
  this.click(elem);
}

MozMillController.prototype.fireEvent = function (name, obj) {
  if (name == "userShutdown") {
    frame.events.toggleUserShutdown();
  }
  frame.events.fireEvent(name, obj);
}

MozMillController.prototype.startUserShutdown = function (timeout, restart) {
  // 0 = default shutdown, 1 = user shutdown, 2 = user restart
  this.fireEvent('userShutdown', restart ? 2 : 1);
  this.window.setTimeout(this.fireEvent, timeout, 'userShutdown', 0);
}

/* Select the specified option and trigger the relevant events of the element.*/
MozMillController.prototype.select = function (el, indx, option, value) {
  element = el.getNode();
  if (!element){
    throw new Error("Could not find element " + el.getInfo());
    return false;
  }

  //if we have a select drop down
  if (element.localName.toLowerCase() == "select"){
    var item = null;

    // The selected item should be set via its index
    if (indx != undefined) {
      // Resetting a menulist has to be handled separately
      if (indx == -1) {
        events.triggerEvent(element, 'focus', false);
        element.selectedIndex = indx;
        events.triggerEvent(element, 'change', true);

     frame.events.pass({'function':'Controller.select()'});
     return true;
      } else {
        item = element.options.item(indx);
    }
    } else {
      for (var i = 0; i < element.options.length; i++) {
        var entry = element.options.item(i);
        if (option != undefined && entry.innerHTML == option ||
            value != undefined && entry.value == value) {
          item = entry;
         break;
       }
     }
           }

    // Click the item
    try {
      // EventUtils.synthesizeMouse doesn't work.
      events.triggerEvent(element, 'focus', false);
      item.selected = true;
           events.triggerEvent(element, 'change', true);

      frame.events.pass({'function':'Controller.select()'});
      return true;
    } catch (ex) {
      throw new Error("No item selected for element " + el.getInfo());
     return false;
   }
  }
  //if we have a xul menulist select accordingly
  else if (element.localName.toLowerCase() == "menulist"){
    var item = null;

    if (indx != undefined) {
      if (indx == -1) {
        events.triggerEvent(element, 'focus', false);
      element.selectedIndex = indx;
        events.triggerEvent(element, 'change', true);

        frame.events.pass({'function':'Controller.select()'});
        return true;
      } else {
        item = element.getItemAtIndex(indx);
      }
    } else {
      for (var i = 0; i < element.itemCount; i++) {
        var entry = element.getItemAtIndex(i);
        if (option != undefined && entry.label == option ||
            value != undefined && entry.value == value) {
          item = entry;
          break;
    }
    }
  }

    // Click the item
    try {
      EventUtils.synthesizeMouse(element, 1, 1, {}, item.ownerDocument.defaultView);
      this.sleep(0);

      // Scroll down until item is visible
      for (var i = s = element.selectedIndex; i <= element.itemCount + s; ++i) {
        var entry = element.getItemAtIndex((i + 1) % element.itemCount);
        EventUtils.synthesizeKey("VK_DOWN", {}, element.ownerDocument.defaultView);
        if (entry.label == item.label) {
          break;
        }
        else if (entry.label == "") i += 1;
      }

      EventUtils.synthesizeMouse(item, 1, 1, {}, item.ownerDocument.defaultView);
      this.sleep(0);

   frame.events.pass({'function':'Controller.select()'});
   return true;
    } catch (ex) {
      throw new Error('No item selected for element ' + el.getInfo());
      return false;
    }
  }
};

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

MozMillController.prototype.assertText = function (el, text) {
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
  utils.assert(callback, message, thisObject);

  frame.events.pass({'function': ": controller.assert('" + callback + "')"});
  return true;
}

// Assert that the result of a Javascript expression is true
MozMillController.prototype.assertJS = function(expression, subject) {
  assert(function() {
    return eval(expression)
  }, "controller.assertJS: Failed for '" + expression + "'");

  frame.events.pass({'function': "controller.assertJS('" + expression + "')"});
  return true;
}

//Assert that a provided value is selected in a select element
MozMillController.prototype.assertSelected = function (el, value) {
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
  frame.log({'function':'controller.assertProperty() - DEPRECATED', 
                      'message':'assertProperty(el, attrib, val) is deprecated. Use assertJSProperty(el, attrib, val) or assertDOMProperty(el, attrib, val) instead'});
  return this.assertJSProperty(el, attrib, val);
};

// deprecated - Use assertNotJSProperty or assertNotDOMProperty instead
MozMillController.prototype.assertPropertyNotExist = function(el, attrib) {
  frame.log({'function':'controller.assertPropertyNotExist() - DEPRECATED',
                   'message':'assertPropertyNotExist(el, attrib) is deprecated. Use assertNotJSProperty(el, attrib) or assertNotDOMProperty(el, attrib) instead'});
  return this.assertNotJSProperty(el, attrib);
};

// Assert that a specified image has actually loaded
// The Safari workaround results in additional requests
// for broken images (in Safari only) but works reliably
MozMillController.prototype.assertImageLoaded = function (el) {
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

//Drag one eleent to the top x,y coords of another specified element
MozMillController.prototype.mouseMove = function (doc, start, dest) {
  //if one of these elements couldn't be looked up
  if (typeof start != 'object'){
    throw new Error("received bad coordinates");
    return false;
  }
  if (typeof dest != 'object'){
    throw new Error("received bad coordinates");
    return false;
  }

  //Do the initial move to the drag element position
  events.triggerMouseEvent(doc.body, 'mousemove', true, start[0], start[1]);
  events.triggerMouseEvent(doc.body, 'mousemove', true, dest[0], dest[1]);
  frame.events.pass({'function':'Controller.mouseMove()'});
  return true;
}

//Drag one eleent to the top x,y coords of another specified element
MozMillController.prototype.dragDropElemToElem = function (dstart, ddest) {
  //Get the drag and dest
  var drag = dstart.getNode();
  var dest = ddest.getNode();

  //if one of these elements couldn't be looked up
  if (!drag){
    throw new Error("could not find element " + drag.getInfo());
    return false;
  }
  if (!dest){
    throw new Error("could not find element " + dest.getInfo());
    return false;
  }

  var dragCoords = null;
  var destCoords = null;

  dragCoords = drag.getBoundingClientRect();
  destCoords = dest.getBoundingClientRect();

  //Do the initial move to the drag element position
  events.triggerMouseEvent(drag.ownerDocument.body, 'mousemove', true, dragCoords.left, dragCoords.top);
  events.triggerMouseEvent(drag, 'mousedown', true, dragCoords.left, dragCoords.top); //do the mousedown
  events.triggerMouseEvent(drag.ownerDocument.body, 'mousemove', true, destCoords.left, destCoords.top);
  events.triggerMouseEvent(dest, 'mouseup', true, destCoords.left, destCoords.top);
  events.triggerMouseEvent(dest, 'click', true, destCoords.left, destCoords.top);
  frame.events.pass({'function':'Controller.dragDropElemToElem()'});
  return true;
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

  controller.waitForPageLoad = function(aTabDocument, aTimeout, aInterval) {
    var timeout = aTimeout || 30000;
    var tab = null;

    // If a user tries to do waitForPageLoad(2000), this will assign the
    // interval the first arg which is most likely what they were expecting
    if (typeof(aTabDocument) == "number"){
      timeout = aTabDocument;
    }

    // Find the browser element for the given aTabDocument
    if (aTabDocument && typeof(aTabDocument) == "object") {
      for each (var browser in this.window.gBrowser.browsers) {
        if (browser.contentDocument == aTabDocument) {
          tab = browser;
          break;
        }
      }

      if (!tab) {
        throw new Error("controller.waitForPageLoad(): Specified tab hasn't been found.");
      }
    }

    // If tab hasn't been specified, fallback to selected browser
    tab = this.window.gBrowser.selectedBrowser;

    // Wait until the content in the tab has been loaded
    this.waitFor(function() {
      return tab.contentDocument.documentLoaded;
    }, "controller.waitForPageLoad(): Timeout waiting for page loaded.", timeout, aInterval);

    frame.events.pass({'function':'controller.waitForPageLoad()'});
  }
}

controllerAdditions = {
  'Browser:Preferences':preferencesAdditions,
  'navigator:browser'  :browserAdditions,
}

var withs = {}; Components.utils.import('resource://mozmill/stdlib/withs.js', withs);

MozMillAsyncTest = function (timeout) {
  if (timeout == undefined) {
    this.timeout = 6000;
  } else {
    this.timeout = timeout;
  }
  this._done = false;
  this._mozmillasynctest = true;
}

MozMillAsyncTest.prototype.run = function () {
  for (var i in this) {
    if (withs.startsWith(i, 'test') && typeof(this[i]) == 'function') {
      this[i]();
    }
  }

  utils.waitFor(function() {
    return this._done == true;
  }, "MozMillAsyncTest timed out. Done is " + this._done, 500, 100); 

  return true;
}

MozMillAsyncTest.prototype.finish = function () {
  this._done = true;
}
