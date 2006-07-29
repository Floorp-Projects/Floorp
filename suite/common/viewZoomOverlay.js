/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-

 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is this file as it was released upon
 * January 6, 2001.
 *
 * The Initial Developer of the Original Code is Peter Annema.
 * Portions created by Peter Annema are Copyright (C) 2000
 * Peter Annema. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Annema <disttsc@bart.nl> (Original Author)
 *   Jonas Sicking <sicking@bigfoot.com>
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable
 * instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

/** Document Zoom Management Code
 *
 * To use this, you'll need to have a <menu id="menu_textZoom"/>
 * and a getMarkupDocumentViewer() function which returns a
 * nsIMarkupDocumentViewer.
 *
 **/

function ZoomManager() {
  this.bundle = document.getElementById("bundle_viewZoom");

  // factorAnchor starts on factorOther
  this.factorOther = parseInt(this.bundle.getString("valueOther"));
  this.factorAnchor = this.factorOther;
}

ZoomManager.prototype = {
  instance : null,

  getInstance : function() {
    if (!ZoomManager.prototype.instance)
      ZoomManager.prototype.instance = new ZoomManager();

    return ZoomManager.prototype.instance;
  },

  MIN : 1,
  MAX : 2000,

  bundle : null,

  zoomFactorsString : "", // cache
  zoomFactors : null,

  factorOther : 300,
  factorAnchor : 300,
  steps : 0,

  get textZoom() {
    var currentZoom;
    try {
      currentZoom = Math.round(getMarkupDocumentViewer().textZoom * 100);
      if (this.indexOf(currentZoom) == -1) {
        if (currentZoom != this.factorOther) {
          this.factorOther = currentZoom;
          this.factorAnchor = this.factorOther;
        }
      }
    } catch (e) {
      currentZoom = 100;
    }
    return currentZoom;
  },

  set textZoom(aZoom) {
    if (aZoom < this.MIN || aZoom > this.MAX)
      throw Components.results.NS_ERROR_INVALID_ARG;

    getMarkupDocumentViewer().textZoom = aZoom / 100;
  },

  enlarge : function() {
    this.jump(1);
  },

  reduce : function() {
    this.jump(-1);
  },

  reset : function() {
    this.textZoom = 100;
  },

  getZoomFactors : function() {
    this.ensureZoomFactors();

    return this.zoomFactors;
  },

  indexOf : function(aZoom) {
    this.ensureZoomFactors();

    var index = -1;
    if (this.isZoomInRange(aZoom)) {
      index = this.zoomFactors.length - 1;
      while (index >= 0 && this.zoomFactors[index] != aZoom)
        --index;
    }

    return index;
  },

  /***** internal helper functions below here *****/

  ensureZoomFactors : function() {
    var zoomFactorsString = this.bundle.getString("values");
    if (this.zoomFactorsString != zoomFactorsString) {
      this.zoomFactorsString = zoomFactorsString;
      this.zoomFactors = zoomFactorsString.split(",");
      for (var i = 0; i<this.zoomFactors.length; ++i)
        this.zoomFactors[i] = parseInt(this.zoomFactors[i]);
    }
  },

  isLevelInRange : function(aLevel) {
    return (aLevel >= 0 && aLevel < this.zoomFactors.length);
  },

  isZoomInRange : function(aZoom) {
    return (aZoom >= this.zoomFactors[0] && aZoom <= this.zoomFactors[this.zoomFactors.length - 1]);
  },

  jump : function(aDirection) {
    if (aDirection != -1 && aDirection != 1)
      throw Components.results.NS_ERROR_INVALID_ARG;

    this.ensureZoomFactors();

    var currentZoom = this.textZoom;
    var insertIndex = -1;
    var stepFactor = parseFloat(this.bundle.getString("stepFactor"));

    // temporarily add factorOther to list
    if (this.isZoomInRange(this.factorOther)) {
      insertIndex = 0;
      while (this.zoomFactors[insertIndex] < this.factorOther)
        ++insertIndex;

      if (this.zoomFactors[insertIndex] != this.factorOther)
        this.zoomFactors.splice(insertIndex, 0, this.factorOther);
    }

    var factor;
    var done = false;

    if (this.isZoomInRange(currentZoom)) {
      var index = this.indexOf(currentZoom);
      if (aDirection == -1 && index == 0 ||
          aDirection ==  1 && index == this.zoomFactors.length - 1) {
        this.steps = 0;
        this.factorAnchor = this.zoomFactors[index];
      } else {
        factor = this.zoomFactors[index + aDirection];
        done = true;
      }
    }

    if (!done) {
      this.steps += aDirection;
      factor = this.factorAnchor * Math.pow(stepFactor, this.steps);
      if (factor < this.MIN || factor > this.MAX) {
        this.steps -= aDirection;
        factor = this.factorAnchor * Math.pow(stepFactor, this.steps);
      }
      factor = Math.round(factor);
      if (this.isZoomInRange(factor))
        factor = this.snap(factor);
      else
        this.factorOther = factor;
    }

    if (insertIndex != -1)
      this.zoomFactors.splice(insertIndex, 1);

    this.textZoom = factor;
  },

  snap : function(aZoom) {
    if (this.isZoomInRange(aZoom)) {
      var level = 0;
      while (this.zoomFactors[level + 1] < aZoom)
        ++level;

      // if aZoom closer to [level + 1] than [level], snap to [level + 1]
      if ((this.zoomFactors[level + 1] - aZoom) < (aZoom - this.zoomFactors[level]))
        ++level;

      aZoom = this.zoomFactors[level];
    }

    return aZoom;
  }
}

/***** init and helper functions for viewZoomOverlay.xul *****/
window.addEventListener("load", registerZoomManager, false);

function registerZoomManager()
{
  var textZoomMenu = document.getElementById("menu_textZoom");
  var zoom = ZoomManager.prototype.getInstance();

  var parentMenu = textZoomMenu.parentNode;
  parentMenu.addEventListener("popupshowing", updateViewMenu, false);

  var insertBefore = document.getElementById("menu_textZoomInsertBefore");
  var popup = insertBefore.parentNode;
  var accessKeys = zoom.bundle.getString("accessKeys").split(",");
  var zoomFactors = zoom.getZoomFactors();
  for (var i = 0; i < zoomFactors.length; ++i) {
    var menuItem = document.createElement("menuitem");
    menuItem.setAttribute("type", "radio");
    menuItem.setAttribute("name", "textZoom");

    var label;
    if (zoomFactors[i] == 100) {
      label = zoom.bundle.getString("labelOriginal");
      menuItem.setAttribute("key", "key_textZoomReset");
    }
    else
      label = zoom.bundle.getString("label");

    menuItem.setAttribute("label", label.replace(/%zoom%/, zoomFactors[i]));
    menuItem.setAttribute("accesskey", accessKeys[i]);
    menuItem.setAttribute("oncommand", "ZoomManager.prototype.getInstance().textZoom = this.value;");
    menuItem.setAttribute("value", zoomFactors[i]);
    popup.insertBefore(menuItem, insertBefore);
  }
}

function updateViewMenu()
{
  var zoom = ZoomManager.prototype.getInstance();

  var textZoomMenu = document.getElementById("menu_textZoom");
  var menuLabel = zoom.bundle.getString("menuLabel").replace(/%zoom%/, zoom.textZoom);
  textZoomMenu.setAttribute("label", menuLabel);
}

function updateTextZoomMenu()
{
  var zoom = ZoomManager.prototype.getInstance();

  var currentZoom = zoom.textZoom;

  var textZoomOther = document.getElementById("menu_textZoomOther");
  var label = zoom.bundle.getString("labelOther");
  textZoomOther.setAttribute("label", label.replace(/%zoom%/, zoom.factorOther));
  textZoomOther.setAttribute("value", zoom.factorOther);

  var popup = document.getElementById("menu_textZoomPopup");
  var item = popup.firstChild;
  while (item) {
    if (item.getAttribute("name") == "textZoom") {
      if (item.getAttribute("value") == currentZoom)
        item.setAttribute("checked","true");
      else
        item.removeAttribute("checked");
    }
    item = item.nextSibling;
  }
}

function setTextZoomOther()
{
  var zoom = ZoomManager.prototype.getInstance();

  // open dialog and ask for new value
  var o = {value: zoom.factorOther, zoomMin: zoom.MIN, zoomMax: zoom.MAX};
  window.openDialog("chrome://communicator/content/askViewZoom.xul", "AskViewZoom", "chrome,modal,titlebar", o);
  if (o.zoomOK)
    zoom.textZoom = o.value;
}
