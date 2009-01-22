/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Spatial Navigation.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Doug Turner <dougt@meer.net>  (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/**
 * 
 * Import this module through
 *
 * Components.utils.import("resource://gre/modules/SpatialNavigation.js");
 *
 * Usage: (Literal class)
 *
 * SpatialNavigation(browser_element, optional_callback);
 *
 * optional_callback will be called when a new element is focused.
 *
 *    function optional_callback(element) {}
 *
 */


var EXPORTED_SYMBOLS = ["SpatialNavigation"];

var SpatialNavigation = {

  init: function(browser, callback) {
    browser.addEventListener("keypress", function (event) { _onInputKeyPress(event, callback) }, true);
  },
  
  uninit: function() {
  }
};


// Private stuff

const Cc = Components.classes;
const Ci = Components.interfaces;

function dump(msg)
{
  var console = Cc["@mozilla.org/consoleservice;1"].getService(Ci.nsIConsoleService);
  console.logStringMessage("*** SNAV: " + msg);
}

var gDirectionalBias = 10;
var gRectFudge = 1;

// modifier values
const kAlt   = "alt";
const kShift = "shift";
const kCtrl  = "ctrl";
const kNone  = "none";

function _onInputKeyPress (event, callback) {

  // Use whatever key value is available (either keyCode or charCode).
  // It might be useful for addons or whoever wants to set different
  // key to be used here (e.g. "a", "F1", "arrowUp", ...).
  var key = event.which || event.keyCode;

  // If it isn't enabled, bail.
  if (!PrefObserver['enabled'])
    return;

  if (key != PrefObserver['keyCodeDown']  &&
      key != PrefObserver['keyCodeRight'] &&
      key != PrefObserver['keyCodeUp'] &&
      key != PrefObserver['keyCodeLeft'])
    return;

  // If it is not using the modifiers it should, bail.
  if (!event.altKey && PrefObserver['modifierAlt'])
    return;

  if (!event.shiftKey && PrefObserver['modifierShift'])
    return;

  if (!event.crtlKey && PrefObserver['modifierCtrl'])
    return;

  var target = event.target;

  var doc = target.ownerDocument;

  // If it is XUL content (e.g. about:config), bail.
  if (!PrefObserver['xulContentEnabled'] && doc instanceof Ci.nsIDOMXULDocument)
    return ;

  // check to see if we are in a textarea or text input element, and if so,
  // ensure that we let the arrow keys work properly.
  if (target instanceof Ci.nsIDOMHTMLHtmlElement) {
      _focusNextUsingCmdDispatcher(key, callback);
      return;
  }

  if ((target instanceof Ci.nsIDOMHTMLInputElement && (target.type == "text" || target.type == "password")) ||
      target instanceof Ci.nsIDOMHTMLTextAreaElement ) {
    
    // if there is any selection at all, just ignore
    if (target.selectionEnd - target.selectionStart > 0)
      return;
    
    // if there is no text, there is nothing special to do.
    if (target.textLength > 0) {
      if (key == PrefObserver['keyCodeRight'] ||
          key == PrefObserver['keyCodeDown'] ) {
        // we are moving forward into the document
        if (target.textLength != target.selectionEnd)
          return;
      }
      else
      {
        // we are at the start of the text, okay to move 
        if (target.selectionStart != 0)
          return;
      }
    }
  }

  // Check to see if we are in a select
  if (target instanceof Ci.nsIDOMHTMLSelectElement)
  {
    if (key == PrefObserver['keyCodeDown']) {
      if (target.selectedIndex + 1 < target.length)
        return;
    }

    if (key == PrefObserver['keyCodeUp']) {
      if (target.selectedIndex > 0)
        return;
    }
  }

  function snavfilter(node) {

    if (node instanceof Ci.nsIDOMHTMLLinkElement ||
        node instanceof Ci.nsIDOMHTMLAnchorElement) {
      // if a anchor doesn't have a href, don't target it.
      if (node.href == "")
        return Ci.nsIDOMNodeFilter.FILTER_SKIP;
      return  Ci.nsIDOMNodeFilter.FILTER_ACCEPT;
    }
    
    if ((node instanceof Ci.nsIDOMHTMLButtonElement ||
         node instanceof Ci.nsIDOMHTMLInputElement ||
         node instanceof Ci.nsIDOMHTMLLinkElement ||
         node instanceof Ci.nsIDOMHTMLOptGroupElement ||
         node instanceof Ci.nsIDOMHTMLSelectElement ||
         node instanceof Ci.nsIDOMHTMLTextAreaElement) &&
        node.disabled == false)
      return Ci.nsIDOMNodeFilter.FILTER_ACCEPT;
    
    return Ci.nsIDOMNodeFilter.FILTER_SKIP;
  }

  var bestElementToFocus = null;
  var distanceToBestElement = Infinity;
  var focusedRect = _inflateRect(target.getBoundingClientRect(),
                                 - gRectFudge);

  var treeWalker = doc.createTreeWalker(doc, Ci.nsIDOMNodeFilter.SHOW_ELEMENT, snavfilter, false);
  var nextNode;
  
  while ((nextNode = treeWalker.nextNode())) {

    if (nextNode == target)
      continue;

    var nextRect = _inflateRect(nextNode.getBoundingClientRect(),
                                - gRectFudge);

    if (! _isRectInDirection(key, focusedRect, nextRect))
      continue;

    var distance = _spatialDistance(key, focusedRect, nextRect);

    //dump("looking at: " + nextNode + " " + distance);
    
    if (distance <= distanceToBestElement && distance > 0) {
      distanceToBestElement = distance;
      bestElementToFocus = nextNode;
    }
  }
  
  if (bestElementToFocus != null) {
    //dump("focusing element  " + bestElementToFocus.nodeName + " " + bestElementToFocus) + "id=" + bestElementToFocus.getAttribute("id");

    // Wishing we could do element.focus()
    doc.defaultView.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils).focus(bestElementToFocus);

    // if it is a text element, select all.
    if((bestElementToFocus instanceof Ci.nsIDOMHTMLInputElement && (bestElementToFocus.type == "text" || bestElementToFocus.type == "password")) ||
       bestElementToFocus instanceof Ci.nsIDOMHTMLTextAreaElement ) {
      bestElementToFocus.selectionStart = 0;
      bestElementToFocus.selectionEnd = bestElementToFocus.textLength;
    }

    if (callback != undefined)
      callback(bestElementToFocus);
    
  } else {
    // couldn't find anything.  just advance and hope.
    _focusNextUsingCmdDispatcher(key, callback);
  }

  event.preventDefault();
  event.stopPropagation();
}

function _focusNextUsingCmdDispatcher(key, callback) {

    var windowMediator = Cc['@mozilla.org/appshell/window-mediator;1'].getService(Ci.nsIWindowMediator);
    var window = windowMediator.getMostRecentWindow("navigator:browser");

    if (key == PrefObserver['keyCodeRight'] || key == PrefObserver['keyCodeDown']) {
      window.document.commandDispatcher.advanceFocus();
    } else {
      window.document.commandDispatcher.rewindFocus();
    }

    if (callback != undefined)
      callback(null);
}

function _isRectInDirection(key, focusedRect, anotherRect)
{
  if (key == PrefObserver['keyCodeLeft']) {
    return (anotherRect.left < focusedRect.left);
  }

  if (key == PrefObserver['keyCodeRight']) {
    return (anotherRect.right > focusedRect.right);
  }

  if (key == PrefObserver['keyCodeUp']) {
    return (anotherRect.top < focusedRect.top);
  }

  if (key == PrefObserver['keyCodeDown']) {
    return (anotherRect.bottom > focusedRect.bottom);
  }
    return false;
}

function _inflateRect(rect, value)
{
  var newRect = new Object();
  
  newRect.left   = rect.left - value;
  newRect.top    = rect.top - value;
  newRect.right  = rect.right  + value;
  newRect.bottom = rect.bottom + value;
  return newRect;
}

function _containsRect(a, b)
{
  return ( (b.left  <= a.right) &&
           (b.right >= a.left)  &&
           (b.top  <= a.bottom) &&
           (b.bottom >= a.top) );
}

function _spatialDistance(key, a, b)
{
  var inlineNavigation = false;
  var mx, my, nx, ny;

  if (key == PrefObserver['keyCodeLeft']) {

    //  |---|
    //  |---|
    //
    //  |---|  |---|
    //  |---|  |---|
    //
    //  |---|
    //  |---|
    //
    
    if (a.top > b.bottom) {
      // the b rect is above a.
      mx = a.left;
      my = a.top;
      nx = b.right;
      ny = b.bottom;
    }
    else if (a.bottom < b.top) {
      // the b rect is below a.
      mx = a.left;
      my = a.bottom;
      nx = b.right;
      ny = b.top;       
    }
    else {
      mx = a.left;
      my = 0;
      nx = b.right;
      ny = 0;
    }
  } else if (key == PrefObserver['keyCodeRight']) {

    //         |---|
    //         |---|
    //
    //  |---|  |---|
    //  |---|  |---|
    //
    //         |---|
    //         |---|
    //
    
    if (a.top > b.bottom) {
      // the b rect is above a.
      mx = a.right;
      my = a.top;
      nx = b.left;
      ny = b.bottom;
    }
    else if (a.bottom < b.top) {
      // the b rect is below a.
      mx = a.right;
      my = a.bottom;
      nx = b.left;
      ny = b.top;       
    } else {
      mx = a.right;
      my = 0;
      nx = b.left;
      ny = 0;
    }
  } else if (key == PrefObserver['keyCodeUp']) {

    //  |---|  |---|  |---|
    //  |---|  |---|  |---|
    //
    //         |---|
    //         |---|
    //
    
    if (a.left > b.right) {
      // the b rect is to the left of a.
      mx = a.left;
      my = a.top;
      nx = b.right;
      ny = b.bottom;
    } else if (a.right < b.left) {
      // the b rect is to the right of a
      mx = a.right;
      my = a.top;
      nx = b.left;
      ny = b.bottom;       
    } else {
      // both b and a share some common x's.
      mx = 0;
      my = a.top;
      nx = 0;
      ny = b.bottom;
    }
  } else if (key == PrefObserver['keyCodeDown']) {

    //         |---|
    //         |---|
    //
    //  |---|  |---|  |---|
    //  |---|  |---|  |---|
    //
    
    if (a.left > b.right) {
      // the b rect is to the left of a.
      mx = a.left;
      my = a.bottom;
      nx = b.right;
      ny = b.top;
    } else if (a.right < b.left) {
      // the b rect is to the right of a
      mx = a.right;
      my = a.bottom;
      nx = b.left;
      ny = b.top;      
    } else {
      // both b and a share some common x's.
      mx = 0;
      my = a.bottom;
      nx = 0;
      ny = b.top;
    }
  }
  
  var scopedRect = _inflateRect(a, gRectFudge);

  if (key == PrefObserver['keyCodeLeft'] ||
      key == PrefObserver['keyCodeRight']) {
    scopedRect.left = 0;
    scopedRect.right = Infinity;
    inlineNavigation = _containsRect(scopedRect, b);
  }
  else if (key == PrefObserver['keyCodeUp'] ||
           key == PrefObserver['keyCodeDown']) {
    scopedRect.top = 0;
    scopedRect.bottom = Infinity;
    inlineNavigation = _containsRect(scopedRect, b);
  }
  
  var d = Math.pow((mx-nx), 2) + Math.pow((my-ny), 2);
  
  // prefer elements directly aligned with the focused element
  if (inlineNavigation)
    d /= gDirectionalBias;
  
  return d;
}

// Snav preference observer

PrefObserver = {

  register: function()
  {
    this.prefService = Cc["@mozilla.org/preferences-service;1"]
                       .getService(Ci.nsIPrefService);

    this._branch = this.prefService.getBranch("snav.");
    this._branch.QueryInterface(Ci.nsIPrefBranch2);
    this._branch.addObserver("", this, false);

    // set current or default pref values
    this.observe(null, "nsPref:changed", "enabled");
    this.observe(null, "nsPref:changed", "xulContentEnabled");
    this.observe(null, "nsPref:changed", "keyCode.modifier");
    this.observe(null, "nsPref:changed", "keyCode.right");
    this.observe(null, "nsPref:changed", "keyCode.up");
    this.observe(null, "nsPref:changed", "keyCode.down");
    this.observe(null, "nsPref:changed", "keyCode.left");
  },

  observe: function(aSubject, aTopic, aData)
  {
    if(aTopic != "nsPref:changed")
      return;

    // aSubject is the nsIPrefBranch we're observing (after appropriate QI)
    // aData is the name of the pref that's been changed (relative to aSubject)
    switch (aData) {
      case "enabled":
        try {
          this.enabled = this._branch.getBoolPref("enabled");
        } catch(e) {
          this.enabled = false;
        }
        break;
      case "xulContentEnabled":
        try {
          this.xulContentEnabled = this._branch.getBoolPref("xulContentEnabled");
        } catch(e) {
          this.xulContentEnabled = false;
        }
        break;

      case "keyCode.modifier":
        try {
          this.keyCodeModifier = this._branch.getCharPref("keyCode.modifier");

          // resetting modifiers
          this.modifierAlt = false;
          this.modifierShift = false;
          this.modifierCtrl = false;

          if (this.keyCodeModifier != this.kNone)
          {
            // use are using '+' as a separator in about:config.
            var mods = this.keyCodeModifier.split(/\++/);
            for (var i = 0; i < mods.length; i++) {
              var mod = mods[i].toLowerCase();
              if (mod == "")
                continue;
              else if (mod == kAlt)
                this.modifierAlt = true;
              else if (mod == kShift)
                this.modifierShift = true;
              else if (mod == kCtrl)
                this.modifierCtrl = true;
              else {
                this.keyCodeModifier = kNone;
                break;
              }
            }
          }
        } catch(e) {
            this.keyCodeModifier = kNone;
        }
        break;
      case "keyCode.up":
        try {
          this.keyCodeUp = this._branch.getIntPref("keyCode.up");
        } catch(e) {
          this.keyCodeUp = Ci.nsIDOMKeyEvent.DOM_VK_UP;
        }
        break;
      case "keyCode.down":
        try {
          this.keyCodeDown = this._branch.getIntPref("keyCode.down");
        } catch(e) {
          this.keyCodeDown = Ci.nsIDOMKeyEvent.DOM_VK_DOWN;
        }
        break;
      case "keyCode.left":
        try {
          this.keyCodeLeft = this._branch.getIntPref("keyCode.left");
        } catch(e) {
          this.keyCodeLeft = Ci.nsIDOMKeyEvent.DOM_VK_LEFT;
        }
        break;
      case "keyCode.right":
        try {
          this.keyCodeRight = this._branch.getIntPref("keyCode.right");
        } catch(e) {
          this.keyCodeRight = Ci.nsIDOMKeyEvent.DOM_VK_RIGHT;
        }
        break;
    }
  },
}

PrefObserver.register();
