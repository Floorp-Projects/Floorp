// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Import this module through
 *
 * Components.utils.import("resource://gre/modules/SpatialNavigation.jsm");
 *
 * Usage: (Literal class)
 *
 * SpatialNavigation.init(browser_element, optional_callback);
 *
 * optional_callback will be called when a new element is focused.
 *
 *    function optional_callback(element) {}
 */

"use strict";

this.EXPORTED_SYMBOLS = ["SpatialNavigation"];

var SpatialNavigation = {
  init: function(browser, callback) {
          browser.addEventListener("keydown", function (event) {
            _onInputKeyPress(event, callback);
          }, true);
  }
};

// Private stuff

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu["import"]("resource://gre/modules/Services.jsm", this);

let eventListenerService = Cc["@mozilla.org/eventlistenerservice;1"]
                             .getService(Ci.nsIEventListenerService);
let focusManager         = Cc["@mozilla.org/focus-manager;1"]
                             .getService(Ci.nsIFocusManager);
let windowMediator       = Cc['@mozilla.org/appshell/window-mediator;1']
                             .getService(Ci.nsIWindowMediator);

// Debug helpers:
function dump(a) {
  Services.console.logStringMessage("SpatialNavigation: " + a);
}

function dumpRect(desc, rect) {
  dump(desc + " " + Math.round(rect.left) + " " + Math.round(rect.top) + " " +
       Math.round(rect.right) + " " + Math.round(rect.bottom) + " width:" +
       Math.round(rect.width) + " height:" + Math.round(rect.height));
}

function dumpNodeCoord(desc, node) {
  let rect = node.getBoundingClientRect();
  dump(desc + " " + node.tagName + " x:" + Math.round(rect.left + rect.width/2) +
       " y:" + Math.round(rect.top + rect.height / 2));
}

// modifier values

const kAlt   = "alt";
const kShift = "shift";
const kCtrl  = "ctrl";
const kNone  = "none";

function _onInputKeyPress (event, callback) {
  //If Spatial Navigation isn't enabled, return.
  if (!PrefObserver['enabled']) {
    return;
  }

  // Use whatever key value is available (either keyCode or charCode).
  // It might be useful for addons or whoever wants to set different
  // key to be used here (e.g. "a", "F1", "arrowUp", ...).
  var key = event.which || event.keyCode;

  if (key != PrefObserver['keyCodeDown']  &&
      key != PrefObserver['keyCodeRight'] &&
      key != PrefObserver['keyCodeUp'] &&
      key != PrefObserver['keyCodeLeft'] &&
      key != PrefObserver['keyCodeReturn']) {
    return;
  }

  if (key == PrefObserver['keyCodeReturn']) {
    // We report presses of the action button on a gamepad "A" as the return
    // key to the DOM. The behaviour of hitting the return key and clicking an
    // element is the same for some elements, but not all, so we handle the
    // ones we want (like the Select element) here:
    if (event.target instanceof Ci.nsIDOMHTMLSelectElement &&
        event.target.click) {
      event.target.click();
      event.stopPropagation();
      event.preventDefault();
      return;
    } else {
      // Leave the action key press to get reported to the DOM as a return
      // keypress.
      return;
    }
  }

  // If it is not using the modifiers it should, return.
  if (!event.altKey && PrefObserver['modifierAlt'] ||
      !event.shiftKey && PrefObserver['modifierShift'] ||
      !event.crtlKey && PrefObserver['modifierCtrl']) {
    return;
  }

  let currentlyFocused = event.target;
  let currentlyFocusedWindow = currentlyFocused.ownerDocument.defaultView;
  let bestElementToFocus = null;

  // If currentlyFocused is an nsIDOMHTMLBodyElement then the page has just been
  // loaded, and this is the first keypress in the page.
  if (currentlyFocused instanceof Ci.nsIDOMHTMLBodyElement) {
    focusManager.moveFocus(currentlyFocusedWindow, null, focusManager.MOVEFOCUS_FIRST, 0);
    event.stopPropagation();
    event.preventDefault();
    return;
  }

  if ((currentlyFocused instanceof Ci.nsIDOMHTMLInputElement &&
      currentlyFocused.mozIsTextField(false)) ||
      currentlyFocused instanceof Ci.nsIDOMHTMLTextAreaElement) {
    // If there is a text selection, remain in the element.
    if (currentlyFocused.selectionEnd - currentlyFocused.selectionStart != 0) {
      return;
    }

    // If there is no text, there is nothing special to do.
    if (currentlyFocused.textLength > 0) {
      if (key == PrefObserver['keyCodeRight'] ||
          key == PrefObserver['keyCodeDown'] ) {
        // We are moving forward into the document.
        if (currentlyFocused.textLength != currentlyFocused.selectionEnd) {
          return;
        }
      } else if (currentlyFocused.selectionStart != 0) {
        return;
      }
    }
  }

  let windowUtils = currentlyFocusedWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                                          .getInterface(Ci.nsIDOMWindowUtils);
  let cssPageRect = _getRootBounds(windowUtils);
  let searchRect = _getSearchRect(currentlyFocused, key, cssPageRect);

  let nodes = {};
  nodes.length = 0;

  let searchRectOverflows = false;

  while (!bestElementToFocus && !searchRectOverflows) {
    switch (key) {
      case PrefObserver['keyCodeLeft']:
      case PrefObserver['keyCodeRight']: {
        if (searchRect.top < cssPageRect.top &&
            searchRect.bottom > cssPageRect.bottom) {
          searchRectOverflows = true;
        }
        break;
      }
      case PrefObserver['keyCodeUp']:
      case PrefObserver['keyCodeDown']: {
        if (searchRect.left < cssPageRect.left &&
            searchRect.right > cssPageRect.right) {
          searchRectOverflows = true;
        }
        break;
      }
    }

    nodes = windowUtils.nodesFromRect(searchRect.left, searchRect.top,
                                      0, searchRect.width, searchRect.height, 0,
                                      true, false);
    // Make the search rectangle "wider": double it's size in the direction
    // that is not the keypress.
    switch (key) {
      case PrefObserver['keyCodeLeft']:
      case PrefObserver['keyCodeRight']: {
        searchRect.top = searchRect.top - (searchRect.height / 2);
        searchRect.bottom = searchRect.top + (searchRect.height * 2);
        searchRect.height = searchRect.height * 2;
        break;
      }
      case PrefObserver['keyCodeUp']:
      case PrefObserver['keyCodeDown']: {
        searchRect.left = searchRect.left - (searchRect.width / 2);
        searchRect.right = searchRect.left + (searchRect.width * 2);
        searchRect.width = searchRect.width * 2;
        break;
      }
    }
    bestElementToFocus = _getBestToFocus(nodes, key, currentlyFocused);
  }


  if (bestElementToFocus === null) {
    // Couldn't find an element to focus.
    return;
  }

  focusManager.setFocus(bestElementToFocus, focusManager.FLAG_SHOWRING);

  //if it is a text element, select all.
  if ((bestElementToFocus instanceof Ci.nsIDOMHTMLInputElement &&
       bestElementToFocus.mozIsTextField(false)) ||
      bestElementToFocus instanceof Ci.nsIDOMHTMLTextAreaElement) {
    bestElementToFocus.selectionStart = 0;
    bestElementToFocus.selectionEnd = bestElementToFocus.textLength;
  }

  if (callback != undefined) {
    callback(bestElementToFocus);
  }

  event.preventDefault();
  event.stopPropagation();
}

// Returns the bounds of the page relative to the viewport.
function _getRootBounds(windowUtils) {
  let cssPageRect = windowUtils.getRootBounds();

  let scrollX = {};
  let scrollY = {};
  windowUtils.getScrollXY(false, scrollX, scrollY);

  let cssPageRectCopy = {};

  cssPageRectCopy.right = cssPageRect.right - scrollX.value;
  cssPageRectCopy.left = cssPageRect.left - scrollX.value;
  cssPageRectCopy.top = cssPageRect.top - scrollY.value;
  cssPageRectCopy.bottom = cssPageRect.bottom - scrollY.value;
  cssPageRectCopy.width = cssPageRect.width;
  cssPageRectCopy.height = cssPageRect.height;

  return cssPageRectCopy;
}

// Returns the best node to focus from the list of nodes returned by the hit
// test.
function _getBestToFocus(nodes, key, currentlyFocused) {
  let best = null;
  let bestDist;
  let bestMid;
  let nodeMid;
  let currentlyFocusedMid = _getMidpoint(currentlyFocused);
  let currentlyFocusedRect = currentlyFocused.getBoundingClientRect();

  for (let i = 0; i < nodes.length; i++) {
    // Reject the currentlyFocused, and all node types we can't focus
    if (!_canFocus(nodes[i]) || nodes[i] === currentlyFocused) {
      continue;
    }

    // Reject all nodes that aren't "far enough" in the direction of the
    // keypress
    nodeMid = _getMidpoint(nodes[i]);
    switch (key) {
      case PrefObserver['keyCodeLeft']:
        if (nodeMid.x >= (currentlyFocusedMid.x - currentlyFocusedRect.width / 2)) {
          continue;
        }
        break;
      case PrefObserver['keyCodeRight']:
        if (nodeMid.x <= (currentlyFocusedMid.x + currentlyFocusedRect.width / 2)) {
          continue;
        }
        break;

      case PrefObserver['keyCodeUp']:
        if (nodeMid.y >= (currentlyFocusedMid.y - currentlyFocusedRect.height / 2)) {
          continue;
        }
        break;
      case PrefObserver['keyCodeDown']:
        if (nodeMid.y <= (currentlyFocusedMid.y + currentlyFocusedRect.height / 2)) {
          continue;
        }
        break;
    }

    // Initialize best to the first viable value:
    if (!best) {
      best = nodes[i];
      bestDist = _spatialDistance(best, currentlyFocused);
      continue;
    }

    // Of the remaining nodes, pick the one closest to the currently focused
    // node.
    let curDist = _spatialDistance(nodes[i], currentlyFocused);
    if (curDist > bestDist) {
      continue;
    }

    bestMid = _getMidpoint(best);
    switch (key) {
      case PrefObserver['keyCodeLeft']:
        if (nodeMid.x > bestMid.x) {
          best = nodes[i];
          bestDist = curDist;
        }
        break;
      case PrefObserver['keyCodeRight']:
        if (nodeMid.x < bestMid.x) {
          best = nodes[i];
          bestDist = curDist;
        }
        break;
      case PrefObserver['keyCodeUp']:
        if (nodeMid.y > bestMid.y) {
          best = nodes[i];
          bestDist = curDist;
        }
        break;
      case PrefObserver['keyCodeDown']:
        if (nodeMid.y < bestMid.y) {
          best = nodes[i];
          bestDist = curDist;
        }
        break;
    }
  }
  return best;
}

// Returns the midpoint of a node.
function _getMidpoint(node) {
  let mid = {};
  let box = node.getBoundingClientRect();
  mid.x = box.left + (box.width / 2);
  mid.y = box.top + (box.height / 2);

  return mid;
}

// Returns true if the node is a type that we want to focus, false otherwise.
function _canFocus(node) {
  if (node instanceof Ci.nsIDOMHTMLLinkElement ||
      node instanceof Ci.nsIDOMHTMLAnchorElement) {
    return true;
  }
  if ((node instanceof Ci.nsIDOMHTMLButtonElement ||
        node instanceof Ci.nsIDOMHTMLInputElement ||
        node instanceof Ci.nsIDOMHTMLLinkElement ||
        node instanceof Ci.nsIDOMHTMLOptGroupElement ||
        node instanceof Ci.nsIDOMHTMLSelectElement ||
        node instanceof Ci.nsIDOMHTMLTextAreaElement) &&
      node.disabled === false) {
    return true;
  }
  return false;
}

// Returns a rectangle that extends to the end of the screen in the direction that
// the key is pressed.
function _getSearchRect(currentlyFocused, key, cssPageRect) {
  let currentlyFocusedRect = currentlyFocused.getBoundingClientRect();

  let newRect = {};
  newRect.left   = currentlyFocusedRect.left;
  newRect.top    = currentlyFocusedRect.top;
  newRect.right  = currentlyFocusedRect.right;
  newRect.bottom = currentlyFocusedRect.bottom;
  newRect.width  = currentlyFocusedRect.width;
  newRect.height = currentlyFocusedRect.height;

  switch (key) {
    case PrefObserver['keyCodeLeft']:
      newRect.left = cssPageRect.left;
      newRect.width = newRect.right - newRect.left;
      break;

    case PrefObserver['keyCodeRight']:
      newRect.right = cssPageRect.right;
      newRect.width = newRect.right - newRect.left;
      break;

    case PrefObserver['keyCodeUp']:
      newRect.top = cssPageRect.top;
      newRect.height = newRect.bottom - newRect.top;
      break;

    case PrefObserver['keyCodeDown']:
      newRect.bottom = cssPageRect.bottom;
      newRect.height = newRect.bottom - newRect.top;
      break;
  }
  return newRect;
}

// Gets the distance between two points a and b.
function _spatialDistance(a, b) {
  let mida = _getMidpoint(a);
  let midb = _getMidpoint(b);

  return Math.round(Math.pow(mida.x - midb.x, 2) +
                    Math.pow(mida.y - midb.y, 2));
}

// Snav preference observer
var PrefObserver = {
  register: function() {
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
    this.observe(null, "nsPref:changed", "keyCode.return");
  },

  observe: function(aSubject, aTopic, aData) {
    if (aTopic != "nsPref:changed") {
      return;
    }

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

      case "keyCode.modifier": {
        let keyCodeModifier;
        try {
          keyCodeModifier = this._branch.getCharPref("keyCode.modifier");

          // resetting modifiers
          this.modifierAlt = false;
          this.modifierShift = false;
          this.modifierCtrl = false;

          if (keyCodeModifier != this.kNone) {
            // we are using '+' as a separator in about:config.
            let mods = keyCodeModifier.split(/\++/);
            for (let i = 0; i < mods.length; i++) {
              let mod = mods[i].toLowerCase();
              if (mod === "")
                continue;
              else if (mod == kAlt)
                this.modifierAlt = true;
              else if (mod == kShift)
                this.modifierShift = true;
              else if (mod == kCtrl)
                this.modifierCtrl = true;
              else {
                keyCodeModifier = kNone;
                break;
              }
            }
          }
        } catch(e) { }
        break;
      }

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
      case "keyCode.return":
        try {
          this.keyCodeReturn = this._branch.getIntPref("keyCode.return");
        } catch(e) {
          this.keyCodeReturn = Ci.nsIDOMKeyEvent.DOM_VK_RETURN;
        }
        break;
    }
  }
};

PrefObserver.register();
