# -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
# 
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is this file as it was released upon January 6, 2001.
#
# The Initial Developer of the Original Code is
# Peter Annema.
# Portions created by the Initial Developer are Copyright (C) 2000
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Peter Annema <disttsc@bart.nl> (Original Author)
#   Jonas Sicking <sicking@bigfoot.com>
#   Jason Barnabe <jason_barnabe@fastmail.fm>
#   Dão Gottwald <dao@mozilla.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

/** Document Zoom Management Code
 *
 * To use this, you'll need to have a getBrowser() function or use the methods
 * that accept a browser to be modified.
 **/

var ZoomManager = {
  get _prefBranch ZoomManager_get__prefBranch() {
    delete this._prefBranch;
    return this._prefBranch = Components.classes["@mozilla.org/preferences-service;1"]
                                        .getService(Components.interfaces.nsIPrefBranch);
  },

  get MIN ZoomManager_get_MIN() {
    delete this.MIN;
    return this.MIN = this._prefBranch.getIntPref("zoom.minPercent") / 100;
  },

  get MAX ZoomManager_get_MAX() {
    delete this.MAX;
    return this.MAX = this._prefBranch.getIntPref("zoom.maxPercent") / 100;
  },

  get useFullZoom ZoomManager_get_useFullZoom() {
    return this._prefBranch.getBoolPref("browser.zoom.full");
  },

  set useFullZoom ZoomManager_set_useFullZoom(aVal) {
    this._prefBranch.setBoolPref("browser.zoom.full", aVal);
    return aVal;
  },

  get zoom ZoomManager_get_zoom() {
    return this.getZoomForBrowser(getBrowser());
  },

  getZoomForBrowser: function ZoomManager_getZoomForBrowser(aBrowser) {
    var markupDocumentViewer = aBrowser.markupDocumentViewer;

    return this.useFullZoom ? markupDocumentViewer.fullZoom
                            : markupDocumentViewer.textZoom;
  },

  set zoom ZoomManager_set_zoom(aVal) {
    this.setZoomForBrowser(getBrowser(), aVal);
    return aVal;
  },

  setZoomForBrowser: function ZoomManager_setZoomForBrowser(aBrowser, aVal) {
    if (aVal < this.MIN || aVal > this.MAX)
      throw Components.results.NS_ERROR_INVALID_ARG;

    var markupDocumentViewer = aBrowser.markupDocumentViewer;

    if (this.useFullZoom) {
      markupDocumentViewer.textZoom = 1;
      markupDocumentViewer.fullZoom = aVal;
    } else {
      markupDocumentViewer.textZoom = aVal;
      markupDocumentViewer.fullZoom = 1;
    }
  },

  get zoomValues ZoomManager_get_zoomValues() {
    var zoomValues = this._prefBranch.getCharPref("toolkit.zoomManager.zoomValues")
                                     .split(",").map(parseFloat);
    zoomValues.sort();

    while (zoomValues[0] < this.MIN)
      zoomValues.shift();

    while (zoomValues[zoomValues.length - 1] > this.MAX)
      zoomValues.pop();

    delete this.zoomValues;
    return this.zoomValues = zoomValues;
  },

  enlarge: function ZoomManager_enlarge() {
    var i = this.zoomValues.indexOf(this.snap(this.zoom)) + 1;
    if (i < this.zoomValues.length)
      this.zoom = this.zoomValues[i];
  },

  reduce: function ZoomManager_reduce() {
    var i = this.zoomValues.indexOf(this.snap(this.zoom)) - 1;
    if (i >= 0)
      this.zoom = this.zoomValues[i];
  },

  reset: function ZoomManager_reset() {
    this.zoom = 1;
  },

  toggleZoom: function ZoomManager_toggleZoom() {
    var zoomLevel = this.zoom;

    this.useFullZoom = !this.useFullZoom;
    this.zoom = zoomLevel;
  },

  snap: function ZoomManager_snap(aVal) {
    var values = this.zoomValues;
    for (var i = 0; i < values.length; i++) {
      if (values[i] >= aVal) {
        if (i > 0 && aVal - values[i - 1] < values[i] - aVal)
          i--;
        return values[i];
      }
    }
    return values[i - 1];
  }
}
