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

var events = {}; Components.utils.import('resource://mozmill/modules/events.js', events);
var EventUtils = {}; Components.utils.import('resource://mozmill/modules/EventUtils.js', EventUtils);
var utils = {}; Components.utils.import('resource://mozmill/modules/utils.js', utils);
var elementslib = {}; Components.utils.import('resource://mozmill/modules/elementslib.js', elementslib);
var frame = {}; Components.utils.import('resource://mozmill/modules/frame.js', frame);

var hwindow = Components.classes["@mozilla.org/appshell/appShellService;1"]
                .getService(Components.interfaces.nsIAppShellService)
                .hiddenDOMWindow;
var aConsoleService = Components.classes["@mozilla.org/consoleservice;1"].
     getService(Components.interfaces.nsIConsoleService);

function waitForEval (expression, timeout, interval, subject) {
  if (interval == undefined) {
    interval = 100;
  }
  if (timeout == undefined) {
    timeout = 30000;
  }

  var self = {};
  self.counter = 0;
  self.result = eval(expression);

  function wait(){
    self.result = eval(expression);
    self.counter += interval;
  }

  var timeoutInterval = hwindow.setInterval(wait, interval);

  var thread = Components.classes["@mozilla.org/thread-manager;1"]
            .getService()
            .currentThread;

  while((self.result != true) && (self.counter < timeout))  {
    thread.processNextEvent(true);
  }
  if (self.counter < timeout) { var r = true; }
  else { var r = false; }

  hwindow.clearInterval(timeoutInterval);

  return r;
}

waitForEvents = function() {}
waitForEvents.prototype = {
  /**
   *  Initialize list of events for given node
   */
  init : function waitForEvents_init(node, events)
{
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
  for (e in this.registry) {
      var r = waitForEval("subject['" + e + "'] == true",
                          timeout, interval, this.node.firedEvents)
    if (!r)
        throw new Error("Timeout happened before event '" + e +"' was fired.");

    this.node.removeEventListener(e, this.registry[e], true);
  }
}
}

function waitForImage(elem, timeout, interval) {
  if (interval == undefined) {
    interval = 100;
  }
  if (timeout == undefined) {
    timeout = 30000;
  }
  return waitForEval('subject.complete == true', timeout, interval, elem.getNode());
}

function waitForElement(elem, timeout, interval) {
  if (interval == undefined) {
    interval = 100;
  }
  if (timeout == undefined) {
    timeout = 30000;
  }
  return waitForEval('subject.exists()', timeout, interval, elem);
}

var Menu = function (elements, doc, window) {
  this.doc = doc;
  this.window = window;
  for each(node in elements) {
    if (node.tagName){
      if (node.tagName == "menu") {
        var label = node.getAttribute("label");
        var id = node.id;
        this[label] = new Menu(node.getElementsByTagName("menupopup")[0].childNodes);
        this[id] = this[label];
      } else if (node.tagName == "menuitem") {
        this[node.getAttribute("label")] = node;
        this[node.id] = node;
      }
    }
  }
};

Menu.prototype.reload = function () {
  var utils = this.window.QueryInterface(Components.interfaces.nsIInterfaceRequestor).
              getInterface(Components.interfaces.nsIDOMWindowUtils);
  utils.forceUpdateNativeMenuAt("4");
  utils.activateNativeMenuItemAt("4|10");

  var elements = this.doc.getElementsByTagName('menubar')[0].childNodes;
  for each(node in elements) {
    if (node.tagName){
      if (node.tagName == "menu") {
        var label = node.getAttribute("label");
        var id = node.id;
        this[label] = new Menu(node.getElementsByTagName("menupopup")[0].childNodes);
        this[id] = this[label];
      } else if (node.tagName == "menuitem") {
        this[node.getAttribute("label")] = node;
        this[node.id] = node;
      }
    }
  }
}

var MozMillController = function (window) {
  // TODO: Check if window is loaded and block until it has if it hasn't.
  this.window = window;

  this.mozmillModule = {};
  Components.utils.import('resource://mozmill/modules/mozmill.js', this.mozmillModule);

  waitForEval("try { subject != null; } catch(err){}", 5000, undefined, window)
  waitForEval("try { subject.documentLoaded != undefined; } catch(err){}", 5000, undefined, window)

  if ( controllerAdditions[window.document.documentElement.getAttribute('windowtype')] != undefined ) {
    this.prototype = new utils.Copy(this.prototype);
    controllerAdditions[window.document.documentElement.getAttribute('windowtype')](this);
    this.windowtype = window.document.documentElement.getAttribute('windowtype');
  }
}

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
    this.waitForEval("subject.checked == " + state, 500, 100, element);
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
  this.waitForEval("subject.selected == true", 500, 100, element);

  frame.events.pass({'function':'Controller.radio(' + el.getInfo() + ')'});
  return true;
}

// The global sleep function has been moved to utils.js. Leave this symbol
// for compatibility reasons
var sleep = utils.sleep;

MozMillController.prototype.sleep = utils.sleep;

MozMillController.prototype.waitForEval = function (expression, timeout, interval, subject) {
  var r = waitForEval(expression, timeout, interval, subject);
  if (!r) {
    throw new Error("timeout exceeded for waitForEval('"+expression+"')");
  }
}

MozMillController.prototype.waitForElement = function (elem, timeout, interval) {
  var r = waitForElement(elem, timeout, interval);
  if (!r) {
    throw new Error("timeout exceeded for waitForElement "+elem.getInfo());
  }
  frame.events.pass({'function':'Controller.waitForElement()'});
}

MozMillController.prototype.__defineGetter__("waitForEvents", function() {
  if (this._waitForEvents == undefined)
    this._waitForEvents = new waitForEvents();
  return this._waitForEvents;
});

MozMillController.prototype.__defineGetter__("menus", function() {
  if(this.window.document.getElementsByTagName('menubar').length > 0)
    return new Menu(this.window.document.getElementsByTagName('menubar')[0].childNodes, this.window.document, this.window);
});

MozMillController.prototype.waitForImage = function (elem, timeout, interval) {
  var r = waitForImage(elem, timeout, interval);
  if (!r) {
    throw new Error("timeout exceeded for waitForImage "+elem.getInfo());
  }
  frame.events.pass({'function':'Controller.waitForImage()'});
}

MozMillController.prototype.waitThenClick = function (elem, timeout, interval) {
  this.waitForElement(elem, timeout, interval);
  this.click(elem);
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

// Assert that the result of a Javascript expression is true
MozMillController.prototype.assertJS = function(expression, subject)
{
  var desc = 'Controller.assertJS("' + expression + '")';
  var result = eval(expression);

  if (result)
    frame.events.pass({'function': desc});
  else
    throw new Error(desc);

  return result; 
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

// Assert that a an element's property is a particular value
MozMillController.prototype.assertProperty = function(el, attrib, val) {
  var element = el.getNode();
  if (!element){
    throw new Error("could not find element " + el.getInfo());
    return false;
  }
  var value = eval ('element.' + attrib+';');
  var res = false;
  try {
    if (value.indexOf(val) != -1){
      res = true;
    }
  }
  catch(err){
  }
  if (String(value) == String(val)) {
    res = true;
  }
  if (res) {
    frame.events.pass({'function':'Controller.assertProperty("' + el.getInfo() + '") : ' + val});
  } else {
    throw new Error("Controller.assertProperty(" + el.getInfo() + ") : " + val + " == " + value);
  }

  return res;
};

// Assert that an element's property does not exist
MozMillController.prototype.assertPropertyNotExist = function(el, attrib) {
  var element = el.getNode();
  if (!element) {
    throw new Error("could not find element " + el.getInfo());
	return false;
  }
  if (!element.hasAttribute(attrib)) {
    frame.events.pass({'function':'Controller.assertPropertyNotExist()'});
    return true;
  }
  throw new Error("assert failed for checked element " + el.getInfo());
  return false;
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
  for (i=0;i<=(this.controller.window.frames.length-1);i++) {
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

function browserAdditions( controller ) {
  controller.tabs = new Tabs(controller);
  controller.waitForPageLoad = function(_document, timeout, interval) {
    //if a user tries to do waitForPageLoad(2000), this will assign the interval the first arg
    //which is most likely what they were expecting
    if (typeof(_document) == "number"){
      var timeout = _document;
    }
    //incase they pass null
    if (_document == null){
      _document = 0;
    }
    //if _document isn't a document object
    if (typeof(_document) != "object") {
      var _document = controller.tabs.activeTab;
    }

    if (interval == undefined) {
      var interval = 100;
    }
    if (timeout == undefined) {
      var timeout = 30000;
    }

    var win = _document.defaultView;

    waitForEval("subject.documentLoaded == true", timeout, interval, win);
    //Once the object is available it's somewhere between 1 and 3 seconds before the DOM
    //Actually becomes available to us
    utils.sleep(1);
    frame.events.pass({'function':'Controller.waitForPageLoad()'});
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
  for (i in this) {
    if (withs.startsWith(i, 'test') && typeof(this[i]) == 'function') {
      this[i]();
    }
  }

  var r = waitForEval("subject._done == true", this.timeout, undefined, this);
  if (r == true) {
    return true;
  } else {
    throw new Error("MozMillAsyncTest did not finish properly: timed out. Done is " + this._done);
  }
}
MozMillAsyncTest.prototype.finish = function () {
  this._done = true;
}
