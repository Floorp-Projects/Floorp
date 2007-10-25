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
 * To use this, you'll need to have a getBrowser() function.
 **/

var ZoomManager = {
  get _prefBranch() {
    return Components.classes["@mozilla.org/preferences-service;1"]
                     .getService(Components.interfaces.nsIPrefBranch);
  },

  get MIN() {
    delete this.MIN;
    return this.MIN = this._prefBranch.getIntPref("fullZoom.minPercent") / 100;
  },

  get MAX() {
    delete this.MAX;
    return this.MAX = this._prefBranch.getIntPref("fullZoom.maxPercent") / 100;
  },

  get fullZoom() {
    return getBrowser().markupDocumentViewer.fullZoom;
  },

  set fullZoom(aVal) {
    if (aVal < this.MIN || aVal > this.MAX)
      throw Components.results.NS_ERROR_INVALID_ARG;

    return (getBrowser().markupDocumentViewer.fullZoom = aVal);
  },

  get fullZoomValues() {
    var fullZoomValues = this._prefBranch.getCharPref("toolkit.zoomManager.fullZoomValues")
                                         .split(",").map(parseFloat);
    fullZoomValues.sort();

    while (fullZoomValues[0] < this.MIN)
      fullZoomValues.shift();

    while (fullZoomValues[fullZoomValues.length - 1] > this.MAX)
      fullZoomValues.pop();

    delete this.fullZoomValues;
    return this.fullZoomValues = fullZoomValues;
  },

  enlarge: function () {
    var i = this.fullZoomValues.indexOf(this.snap(this.fullZoom)) + 1;
    if (i < this.fullZoomValues.length)
      this.fullZoom = this.fullZoomValues[i];
  },

  reduce: function () {
    var i = this.fullZoomValues.indexOf(this.snap(this.fullZoom)) - 1;
    if (i >= 0)
      this.fullZoom = this.fullZoomValues[i];
  },

  reset: function () {
    this.fullZoom = 1;
  },

  snap: function (aVal) {
    var values = this.fullZoomValues;
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
