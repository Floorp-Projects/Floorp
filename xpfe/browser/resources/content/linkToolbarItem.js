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
 * The Original Code is Eric Hodel's <drbrain@segment7.net> code.
 *
 * The Initial Developer of the Original Code is
 * Eric Hodel.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *      Christopher Hoess <choess@force.stwing.upenn.edu>
 *      Tim Taylor <tim@tool-man.org>
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

/* 
 * LinkToolbarItem and its subclasses represent the buttons, menuitems, 
 * and menus that handle the various link types.
 */
function LinkToolbarItem (linkType) {
  this.linkType = linkType;
  this.xulElementId = "link-" + linkType;
  this.xulPopupId = this.xulElementId + "-popup";
  this.parentMenuButton = null;

  this.getXULElement = function() {
    return document.getElementById(this.xulElementId);
  }

  this.clear = function() {
    this.disableParentMenuButton();
    this.getXULElement().setAttribute("disabled", "true");
    this.getXULElement().removeAttribute("href");
  }

  this.displayLink = function(linkElement) {
    if (this.getXULElement().hasAttribute("href")) return false;

    this.setItem(linkElement);
    this.enableParentMenuButton();
    return true;
  }

  this.setItem = function(linkElement) {
    this.getXULElement().setAttribute("href", linkElement.href);
    this.getXULElement().removeAttribute("disabled");
  }

  this.enableParentMenuButton = function() {
    if(this.getParentMenuButton())
      this.getParentMenuButton().removeAttribute("disabled");
  }

  this.disableParentMenuButton = function() {
    if (!this.parentMenuButton) return;

    this.parentMenuButton.setAttribute("disabled", "true");
    this.parentMenuButton = null;
  }

  this.getParentMenuButton = function() {
    if (!this.parentMenuButton)
      this.parentMenuButton = getParentMenuButtonRecursive(
          this.getXULElement());

    return this.parentMenuButton;
  }

  function getParentMenuButtonRecursive(xulElement) {
    if (!xulElement) return null;

    if (xulElement.tagName == "toolbarbutton") 
      return xulElement;

    return getParentMenuButtonRecursive(xulElement.parentNode)
  }
}


function LinkToolbarButton (linkType) {
  this.constructor(linkType);

  this.clear = function() {
    this.__proto__.clear.apply(this);

    this.getXULElement().removeAttribute("tooltiptext");
  }

  this.setItem = function(linkElement) {
    this.__proto__.setItem.apply(this, [linkElement]);

    this.getXULElement().setAttribute("tooltiptext", linkElement.getTooltip());
  }

  this.enableParentMenuButton = function() { /* do nothing */ }
  this.disableParentMenuButton = function() { /* do nothing */ }
}
LinkToolbarButton.prototype = new LinkToolbarItem;


function LinkToolbarMenu (linkType) {
  this.constructor(linkType);

  this.knownLinks = null;

  this.clear = function() {
    this.disableParentMenuButton();
    this.getXULElement().setAttribute("disabled", "true");
    clearPopup(this.getPopup());
    this.knownLinks = null;
  }

  function clearPopup(popup) {
    while (popup.hasChildNodes())
      popup.removeChild(popup.lastChild);
  }

  this.getPopup = function() {
    return document.getElementById(this.xulPopupId);
  }

  this.displayLink = function(linkElement) {
    if (this.isAlreadyAdded(linkElement)) return false;

    this.getKnownLinks()[linkElement.href] = true;
    this.addMenuItem(linkElement);
    this.getXULElement().removeAttribute("disabled");
    this.enableParentMenuButton();
    return true;
  }

  this.isAlreadyAdded = function(linkElement) {
    return this.getKnownLinks()[linkElement.href];
  }

  this.getKnownLinks = function() {
    if (!this.knownLinks)
      this.knownLinks = new Array();
    return this.knownLinks;
  }
  
  function match(first, second) {
    if (!first && !second) return true;
    if (!first || !second) return false;

    return first == second;
  }

  this.addMenuItem = function(linkElement) {
    this.getPopup().appendChild(this.createMenuItem(linkElement));
  }

  this.createMenuItem = function(linkElement) {
    // XXX: clone a prototypical XUL element instead of hardcoding these
    //   attributes
    var menuitem = document.createElement("menuitem");
    menuitem.setAttribute("label", linkElement.getLabel());
    menuitem.setAttribute("href", linkElement.href);
    menuitem.setAttribute("class", "menuitem-iconic bookmark-item");
    menuitem.setAttribute("rdf:type", 
        "rdf:http://www.w3.org/1999/02/22-rdf-syntax-ns#linkType");

    return menuitem;
  }
}
LinkToolbarMenu.prototype = new LinkToolbarItem;


function LinkToolbarTransientMenu (linkType) {
  this.constructor(linkType);

  this.getXULElement = function() {
    if (this.__proto__.getXULElement.apply(this)) 
      return this.__proto__.getXULElement.apply(this);
    else
      return this.createXULElement();
  }

  this.createXULElement = function() {
    // XXX: clone a prototypical XUL element instead of hardcoding these
    //   attributes
    var menu = document.createElement("menu");
    menu.setAttribute("id", this.xulElementId);
    menu.setAttribute("label", this.linkType);
    menu.setAttribute("disabled", "true");
    menu.setAttribute("class", "menu-iconic bookmark-item");
    menu.setAttribute("container", "true");
    menu.setAttribute("type", "rdf:http://www.w3.org/1999/02/22-rdf-syntax-ns#type");

    document.getElementById("more-menu-popup").appendChild(menu);

    return menu;
  }

  this.getPopup = function() {
    if (!this.__proto__.getPopup.apply(this))
      this.getXULElement().appendChild(this.createPopup());

    return this.__proto__.getPopup.apply(this) 
  }

  this.createPopup = function() {
    var popup = document.createElement("menupopup");
    popup.setAttribute("id", this.xulPopupId);

    return popup;
  }

  this.clear = function() {
    this.__proto__.clear.apply(this);

    // XXX: we really want to use this instead of removeXULElement
    //this.hideXULElement();
    this.removeXULElement();
  }

  this.hideXULElement = function() {
    /*
     * XXX: using "hidden" or "collapsed" leads to a crash when you 
     *        open the More menu under certain circumstances.  Maybe
     *        related to bug 83906.  As of 0.9.2 I it doesn't seem
     *        to crash anymore.
     */
    this.getXULElement().setAttribute("collapsed", "true");
  }

  this.removeXULElement = function() {
    // XXX: stop using this method once it's safe to use hideXULElement
    if (this.__proto__.getXULElement.apply(this))
      this.__proto__.getXULElement.apply(this).parentNode.removeChild(
          this.__proto__.getXULElement.apply(this));
  }

  this.displayLink = function(linkElement) {
    if(!this.__proto__.displayLink.apply(this, [linkElement])) return false;

    this.getXULElement().removeAttribute("collapsed");
    showMiscellaneousSeparator();
    return true;
  }
}

showMiscellaneousSeparator =
function()
{
  document.getElementById("misc-separator").removeAttribute("collapsed");
}

hideMiscellaneousSeparator =
function()
{
  document.getElementById("misc-separator").setAttribute("collapsed", "true");
}
LinkToolbarTransientMenu.prototype = new LinkToolbarMenu;

