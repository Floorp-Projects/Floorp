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

var EXPORTED_SYMBOLS = ["createEventObject", "triggerEvent", "getKeyCodeFromKeySequence",
                        "triggerKeyEvent", "triggerMouseEvent", "fakeOpenPopup"];
                        
var EventUtils = {}; Components.utils.import('resource://mozmill/stdlib/EventUtils.js', EventUtils);

var utils = {}; Components.utils.import('resource://mozmill/modules/utils.js', utils);

// var logging = {}; Components.utils.import('resource://mozmill/stdlib/logging.js', logging);

// var eventsLogger = logging.getLogger('eventsLogger');

var createEventObject = function(element, controlKeyDown, altKeyDown, shiftKeyDown, metaKeyDown) {
  var evt = element.ownerDocument.createEventObject();
  evt.shiftKey = shiftKeyDown;
  evt.metaKey = metaKeyDown;
  evt.altKey = altKeyDown;
  evt.ctrlKey = controlKeyDown;
  return evt;
};

/**
 * Fakes a click on a menupopup
 *
 * @param window aWindow
 *               Browser window to use
 * @param menupopup aPopup
 *                  Popup to fake the click for
 */
function fakeOpenPopup(aWindow, aPopup) {
  var popupEvent = aWindow.document.createEvent("MouseEvent");
  popupEvent.initMouseEvent("popupshowing", true, true, aWindow, 0,
                            0, 0, 0, 0, false, false, false, false,
                            0, null);
  aPopup.dispatchEvent(popupEvent);  
}

    /* Fire an event in a browser-compatible manner */
var triggerEvent = function(element, eventType, canBubble, controlKeyDown, altKeyDown, shiftKeyDown, metaKeyDown) {
  canBubble = (typeof(canBubble) == undefined) ? true: canBubble;
  var evt = element.ownerDocument.createEvent('HTMLEvents');

  evt.shiftKey = shiftKeyDown;
  evt.metaKey = metaKeyDown;
  evt.altKey = altKeyDown;
  evt.ctrlKey = controlKeyDown;

  evt.initEvent(eventType, canBubble, true);
  element.dispatchEvent(evt);

};

var getKeyCodeFromKeySequence = function(keySequence) {
  
  var match = /^\\(\d{1,3})$/.exec(keySequence);
  if (match != null) {
      return match[1];

  }
  match = /^.$/.exec(keySequence);
  if (match != null) {
      return match[0].charCodeAt(0);

  }
  // this is for backward compatibility with existing tests
  // 1 digit ascii codes will break however because they are used for the digit chars
  match = /^\d{2,3}$/.exec(keySequence);
  if (match != null) {
      return match[0];

  }
  if (keySequence != null){
    // eventsLogger.error("invalid keySequence "+String(keySequence));
  }
  // mozmill.results.writeResult("invalid keySequence");
}
    
var triggerKeyEvent = function(element, eventType, aKey, modifiers) {
  // get the window and send focus event
  var win = element.ownerDocument ? element.ownerDocument.defaultView : element;
  win.focus();
  utils.sleep(5);
  
  // If we have an element check if it needs to be focused
  if (element.ownerDocument) {
    var focusedElement = utils.getChromeWindow(win).document.commandDispatcher.focusedElement;
    for (var node = focusedElement; node && node != element; )
      node = node.parentNode;

    // Only focus the element when it's not focused yet
    if (!node)
      element.focus();
  }

  try {
      EventUtils.synthesizeKey(aKey, modifiers, win);
  } catch(e) {
    throw new Error("Synthesizing key event failed. \n"+e)
  }
}

    /* Fire a mouse event in a browser-compatible manner */
var triggerMouseEvent = function(element, eventType, canBubble, clientX, clientY, controlKeyDown, altKeyDown, shiftKeyDown, metaKeyDown) {
  
  clientX = clientX ? clientX: 0;
  clientY = clientY ? clientY: 0;

  // Fixing this - make the mouse understand where it is on the screen, needed
  // for double click.
  var screenX = element.boxObject.screenX ? element.boxObject.screenX : 0;
  var screenY = element.boxObject.screenY ? element.boxObject.screenY : 0;;

  canBubble = (typeof(canBubble) == undefined) ? true: canBubble;

  var evt = element.ownerDocument.defaultView.document.createEvent('MouseEvents');
  if (evt.initMouseEvent) {
      //LOG.info("element has initMouseEvent");
      //Safari
      evt.initMouseEvent(eventType, canBubble, true, element.ownerDocument.defaultView, 1, screenX, screenY, clientX, clientY, controlKeyDown, altKeyDown, shiftKeyDown, metaKeyDown, 0, null)

  }
  else {
      //LOG.warn("element doesn't have initMouseEvent; firing an event which should -- but doesn't -- have other mouse-event related attributes here, as well as controlKeyDown, altKeyDown, shiftKeyDown, metaKeyDown");
      evt.initEvent(eventType, canBubble, true);
      evt.shiftKey = shiftKeyDown;
      evt.metaKey = metaKeyDown;
      evt.altKey = altKeyDown;
      evt.ctrlKey = controlKeyDown;

  }
  //Used by safari
  element.dispatchEvent(evt);
}
