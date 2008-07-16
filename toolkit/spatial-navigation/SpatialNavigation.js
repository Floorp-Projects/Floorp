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
 * Usage:
 *
 *
 * var snav = new SpatialNavigation(browser_element, optional_callback);
 *
 * optional_callback will be called when a new element is focused.
 *
 *    function optional_callback(element) {}
 *
 */


var EXPORTED_SYMBOLS = ["SpatialNavigation"];

function SpatialNavigation (browser, callback)
{
  browser.addEventListener("keypress", function (event) { _onInputKeyPress(event, callback) }, true);
};

SpatialNavigation.prototype = {
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

function _onInputKeyPress (event, callback) {
  
  // If it isn't an arrow key, bail.
  if (event.keyCode != event.DOM_VK_LEFT  &&
      event.keyCode != event.DOM_VK_RIGHT &&
      event.keyCode != event.DOM_VK_UP    &&
      event.keyCode != event.DOM_VK_DOWN  )
    return;
  
  // check to see if we are in a textarea or text input element, and if so,
  // ensure that we let the arrow keys work properly.

  if ((event.target instanceof Ci.nsIDOMHTMLInputElement && (event.target.type == "text" || event.target.type == "password")) ||
      event.target instanceof Ci.nsIDOMHTMLTextAreaElement ) {
    
    // if there is any selection at all, just ignore
    if (event.target.selectionEnd - event.target.selectionStart > 0)
      return;
    
    // if there is no text, there is nothing special to do.
    
    if (event.target.textLength > 0) {

      if (event.keyCode == event.DOM_VK_RIGHT || event.keyCode == event.DOM_VK_DOWN  ) {
        // we are moving forward into the document
        if (event.target.textLength != event.target.selectionEnd)
          return;
      }
      else
      {
        // we are at the start of the text, okay to move 
        if (event.target.selectionStart != 0)
          return;
      }
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
         node instanceof Ci.nsIDOMHTMLOptionElement ||
         node instanceof Ci.nsIDOMHTMLSelectElement ||
         node instanceof Ci.nsIDOMHTMLTextAreaElement) &&
        node.disabled == false)
      return Ci.nsIDOMNodeFilter.FILTER_ACCEPT;
    
    return Ci.nsIDOMNodeFilter.FILTER_SKIP;
  }
  var bestElementToFocus = null;
  var distanceToBestElement = Infinity;
  var focusedRect = _inflateRect(event.target.getBoundingClientRect(),
                                 - gRectFudge);
  var doc = event.target.ownerDocument;
  
  var treeWalker = doc.createTreeWalker(doc, Ci.nsIDOMNodeFilter.SHOW_ELEMENT, snavfilter, false);
  var nextNode;
  
  while ((nextNode = treeWalker.nextNode())) {
    
    if (nextNode == event.target)
      continue;
    
    var nextRect = _inflateRect(nextNode.getBoundingClientRect(),
                                - gRectFudge);
    
    if (! _isRectInDirection(event, focusedRect, nextRect))
      continue;
    
    var distance = _spatialDistance(event, focusedRect, nextRect);
    
    if (distance <= distanceToBestElement && distance > 0) {
      distanceToBestElement = distance;
      bestElementToFocus = nextNode;
    }
  }
  
  if (bestElementToFocus != null) {
    dump("focusing element  " + bestElementToFocus.nodeName + " " + bestElementToFocus);
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
    var windowMediator = Cc['@mozilla.org/appshell/window-mediator;1'].getService(Ci.nsIWindowMediator);
    var window = windowMediator.getMostRecentWindow("navigator:browser");
    
    if (event.keyCode == event.DOM_VK_RIGHT || event.keyCode != event.DOM_VK_DOWN  )
      window.document.commandDispatcher.advanceFocus();
    else
      window.document.commandDispatcher.rewindFocus();
    
    if (callback != undefined)
      callback(null);
  }
  
  event.preventDefault();
  event.stopPropagation();
}

function _isRectInDirection(event, focusedRect, anotherRect)
{
    if (event.keyCode == event.DOM_VK_LEFT) {  
      return (anotherRect.left < focusedRect.left);
    }
    
    if (event.keyCode == event.DOM_VK_RIGHT) {
      return (anotherRect.right > focusedRect.right);
    }
    
    if (event.keyCode == event.DOM_VK_UP) {
      return (anotherRect.top < focusedRect.top);
    }
    
    if (event.keyCode == event.DOM_VK_DOWN) {
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

function _spatialDistance(event, a, b)
{
  var inlineNavigation = false;
  var mx, my, nx, ny;
  
  if (event.keyCode == event.DOM_VK_LEFT) {
    
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
  } else if (event.keyCode == event.DOM_VK_RIGHT) {
    
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
  } else if (event.keyCode == event.DOM_VK_UP) {
    
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
  } else if (event.keyCode == event.DOM_VK_DOWN) {
    
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
  
  if (event.keyCode == event.DOM_VK_LEFT || 
      event.keyCode == event.DOM_VK_RIGHT) {
    scopedRect.left = 0;
    scopedRect.right = Infinity;
    inlineNavigation = _containsRect(scopedRect, b);
  }
  else if (event.keyCode == event.DOM_VK_UP ||
           event.keyCode == event.DOM_VK_DOWN) {
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

