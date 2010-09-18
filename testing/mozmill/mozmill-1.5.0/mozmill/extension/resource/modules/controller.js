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
      }, timeout, interval, "Timeout happened before event '" + ex +"' was fired.");
  
      this.node.removeEventListener(e, this.registry[e], true);
    }
  }
}

/**
 * Dynamically create hierarchy of available menu entries
 *
 * @param object aWindow
 *               Browser window to use
 * @param object aElements
 *               Array of menu or menuitem elements
 */
var Menu = function (aWindow, aElements) {
  for each (var node in aElements) {
    var entry = null;

    switch (node.tagName) {
      case "menu":
        var popup = node.getElementsByTagName("menupopup")[0];

        // Fake a click onto the menu to add dynamic entries
        if (popup) {
          if (popup.allowevents) {
            events.fakeOpenPopup(aWindow, popup);
          }
          entry = new Menu(aWindow, popup.childNodes);
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
  }, 5000, 100, "controller(): Window could not be initialized.");

  if ( controllerAdditions[window.document.documentElement.getAttribute('windowtype')] != undefined ) {
    this.prototype = new utils.Copy(this.prototype);
    controllerAdditions[window.document.documentElement.getAttribute('windowtype')](this);
    this.windowtype = window.document.documentElement.getAttribute('windowtype');
  }
}

MozMillController.prototype.sleep = utils.sleep;

MozMillController.prototype.keypress = function(el, aKey, modifiers) {
  var element = (el == null) ? this.window : el.getNode();
  if (!element) {
    throw new Error("could not find element " + el.getInfo());
    return false;
  }

  events.triggerKeyEvent(element, 'keypress', aKey, modifiers);

  frame.events.pass({'function':'Controller.keypress()'});
  return true;
}

MozMillController.prototype.type = function (el, text) {
  var element = (el == null) ? this.window : el.getNode();
  if (!element) {
    throw new Error("could not find element " + el.getInfo());
    return false;
  }

  for (var indx = 0; indx < text.length; indx++) {
    events.triggerKeyEvent(element, 'keypress', text.charAt(indx), {});
  }

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

MozMillController.prototype.click = function(elem, left, top)
{
  var element = elem.getNode();
  if (!element){
    throw new Error("could not find element " + elem.getInfo());
    return false;
  }

  if (element.tagName == "menuitem") {
    element.click();
    return true;
  }

  if (isNaN(left)) { left = 1; }
  if (isNaN(top)) { top = 1; }

  // Scroll element into view otherwise the click will fail
  if (element.scrollIntoView)
    element.scrollIntoView();

  EventUtils.synthesizeMouse(element, left, top, {}, element.ownerDocument.defaultView);
  this.sleep(0);

  frame.events.pass({'function':'Controller.click()'});
  return true;
}

MozMillController.prototype.doubleClick = function(elem, left, top)
{
  var element = elem.getNode();
  if (!element) {
    throw new Error("could not find element " + elem.getInfo());
    return false;
  }

  if (isNaN(left)) { left = 1; }
  if (isNaN(top)) { top = 1; }

  // Scroll element into view before initiating a double click
  if (element.scrollIntoView)
    element.scrollIntoView();

  EventUtils.synthesizeMouse(element, left, top, {clickCount: 2},
                             element.ownerDocument.defaultView);
  this.sleep(0);

  frame.events.pass({'function':'Controller.doubleClick()'});
  return true;
}

MozMillController.prototype.mouseDown = function (elem, button, left, top)
{
  var element = elem.getNode();
  if (!element) {
    throw new Error("could not find element " + elem.getInfo());
    return false;
  }

  if (isNaN(left)) { left = 1; }
  if (isNaN(top)) { top = 1; }

  // Scroll element into view before initiating a mouse down
  if (element.scrollIntoView)
    element.scrollIntoView();

  EventUtils.synthesizeMouse(element, left, top, {button: button, type: "mousedown"},
                             element.ownerDocument.defaultView);
  this.sleep(0);

  frame.events.pass({'function':'Controller.mouseDown()'});
  return true;
};

MozMillController.prototype.mouseOut = function (elem, button, left, top)
{
  var element = elem.getNode();
  if (!element) {
    throw new Error("could not find element " + elem.getInfo());
    return false;
  }

  if (isNaN(left)) { left = 1; }
  if (isNaN(top)) { top = 1; }

  // Scroll element into view before initiating a mouse down
  if (element.scrollIntoView)
    element.scrollIntoView();

  EventUtils.synthesizeMouse(element, left, top, {button: button, type: "mouseout"},
                             element.ownerDocument.defaultView);
  this.sleep(0);

  frame.events.pass({'function':'Controller.mouseOut()'});
  return true;
};

MozMillController.prototype.mouseOver = function (elem, button, left, top)
{
  var element = elem.getNode();
  if (!element) {
    throw new Error("could not find element " + elem.getInfo());
    return false;
  }

  if (isNaN(left)) { left = 1; }
  if (isNaN(top)) { top = 1; }

  // Scroll element into view before initiating a mouse down
  if (element.scrollIntoView)
    element.scrollIntoView();

  EventUtils.synthesizeMouse(element, left, top, {button: button, type: "mouseover"},
                             element.ownerDocument.defaultView);
  this.sleep(0);

  frame.events.pass({'function':'Controller.mouseOver()'});
  return true;
};

MozMillController.prototype.mouseUp = function (elem, button, left, top)
{
  var element = elem.getNode();
  if (!element) {
    throw new Error("could not find element " + elem.getInfo());
    return false;
  }

  if (isNaN(left)) { left = 1; }
  if (isNaN(top)) { top = 1; }

  // Scroll element into view before initiating a mouse down
  if (element.scrollIntoView)
    element.scrollIntoView();

  EventUtils.synthesizeMouse(element, left, top, {button: button, type: "mouseup"},
                             element.ownerDocument.defaultView);
  this.sleep(0);

  frame.events.pass({'function':'Controller.mouseUp()'});
  return true;
};

MozMillController.prototype.middleClick = function(elem, left, top)
{
  var element = elem.getNode();
    if (!element){
      throw new Error("could not find element " + el.getInfo());
      return false;
    }

    if (isNaN(left)) { left = 1; }
    if (isNaN(top)) { top = 1; }

  // Scroll element into view before initiating a right click
  if (element.scrollIntoView)
    element.scrollIntoView();

  EventUtils.synthesizeMouse(element, left, top,
                             { button: 1 },
                             element.ownerDocument.defaultView);
  this.sleep(0);

  frame.events.pass({'function':'Controller.middleClick()'});
    return true;
}

MozMillController.prototype.rightClick = function(elem, left, top)
{
  var element = elem.getNode();
    if (!element){
      throw new Error("could not find element " + el.getInfo());
      return false;
    }

    if (isNaN(left)) { left = 1; }
    if (isNaN(top)) { top = 1; }

  // Scroll element into view before initiating a right click
  if (element.scrollIntoView)
    element.scrollIntoView();

  EventUtils.synthesizeMouse(element, left, top,
                             { type : "contextmenu", button: 2 },
                             element.ownerDocument.defaultView);
  this.sleep(0);

  frame.events.pass({'function':'Controller.rightClick()'});
    return true;
}

//show a deprecation warning for rightclick to update to rightClick
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
    }, 500, 100, "Checkbox " + el.getInfo() + " could not be checked/unchecked");

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
  }, 500, 100, "Radio button " + el.getInfo() + " could not be selected");

  frame.events.pass({'function':'Controller.radio(' + el.getInfo() + ')'});
  return true;
}

MozMillController.prototype.waitFor = function(callback, timeout, interval, message) {
  utils.waitFor(callback, timeout, interval, message);

  frame.events.pass({'function':'controller.waitFor()'});
}

MozMillController.prototype.waitForEval = function(expression, timeout, interval, subject) {
  waitFor(function() {
    return eval(expression);
  }, timeout, interval, "controller.waitForEval: Timeout exceeded for '" + expression + "'");

  frame.events.pass({'function':'controller.waitForEval()'});
}

MozMillController.prototype.waitForElement = function(elem, timeout, interval) {
  this.waitFor(function() {
    return elem.exists();
  }, timeout, interval, "Timeout exceeded for waitForElement " + elem.getInfo());

  frame.events.pass({'function':'Controller.waitForElement()'});
}

MozMillController.prototype.__defineGetter__("waitForEvents", function() {
  if (this._waitForEvents == undefined)
    this._waitForEvents = new waitForEvents();
  return this._waitForEvents;
});

MozMillController.prototype.__defineGetter__("menus", function() {
  var menu = null;

  var menubar = this.window.document.getElementsByTagName('menubar');
  if(menubar && menubar.length > 0) 
    menu = new Menu(this.window, menubar[0].childNodes);

  return menu;
});

MozMillController.prototype.waitForImage = function (elem, timeout, interval) {
  this.waitFor(function() {
    return elem.getNode().complete == true;
  }, timeout, interval, "timeout exceeded for waitForImage " + elem.getInfo());

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
MozMillController.prototype.assert = function(callback, message)
{
  utils.assert(callback, message);

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
    var self = {loaded: false};
    var tab = null;

    // If a user tries to do waitForPageLoad(2000), this will assign the
    // interval the first arg which is most likely what they were expecting
    if (typeof(aTabDocument) == "number"){
      aTimeout = aTabDocument;
    }

    // Find the browser element for the given aTabDocument
    if (typeof(aTabDocument) == "object") {
      for each (var browser in this.window.gBrowser) {
        if (browser && browser.contentDocument === aTabDocument) {
          tab = browser;
          break;
        }
      }
    }

    // Fallback to selected browser
    tab = tab || this.window.gBrowser.selectedBrowser;

    // If the page has already been loaded we have to return earlier. Otherwise
    // we end up in a timeout when waiting for the DOMContentLoaded event
    // Note: It will not fix the problem for error pages
    if (tab.contentDocument.defaultView.documentLoaded) {
      frame.events.pass({'function':'controller.waitForPageLoad()'});
      return;
    }

    // Add event listener for "DOMContentLoaded" which will fire once the
    // content has been loaded and the DOM is ready. We cannot use the "load"
    // event, because that makes it impossible to detect page loads for error pages.
    function pageLoaded() {
      tab.removeEventListener("DOMContentLoaded", pageLoaded, true);
      self.loaded = true;
    }
    tab.addEventListener("DOMContentLoaded", pageLoaded, true);

    try {
      // Wait until the page has been loaded
      this.waitFor(function() { return self.loaded; }, aTimeout, aInterval);
      this.sleep(1000);

      frame.events.pass({'function':'controller.waitForPageLoad()'});
    } catch (ex) {
      // If a timeout happens the listener has to be removed manually
      tab.removeEventListener("DOMContentLoaded", pageLoaded, true);

      throw new Error("controller.waitForPageLoad(): Timeout waiting for page loaded.");
    }
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
  }, 500, 100, "MozMillAsyncTest timed out. Done is " + this._done); 

  return true;
}

MozMillAsyncTest.prototype.finish = function () {
  this._done = true;
}
