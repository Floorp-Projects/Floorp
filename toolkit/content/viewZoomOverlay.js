# -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
# 
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is this file as it was released upon
# January 6, 2001.
# 
# The Initial Developer of the Original Code is Peter Annema.
# Portions created by Peter Annema are Copyright (C) 2000
# Peter Annema. All Rights Reserved.
# 
# Contributor(s):
#   Peter Annema <disttsc@bart.nl> (Original Author)
#   Jonas Sicking <sicking@bigfoot.com>
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable
# instead of those above.  If you wish to allow use of your
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.

/** Document Zoom Management Code
 *
 * To use this, you'll need to have a getMarkupDocumentViewer() function which returns a
 * nsIMarkupDocumentViewer.
 *
 **/


function ZoomManager() {
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
  factorOther : 300,
  factorAnchor : 300,
    zoomFactors: [50, 75, 90, 100, 120, 150, 200],
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

  indexOf : function(aZoom) {
    var index = -1;
    if (this.isZoomInRange(aZoom)) {
      index = this.zoomFactors.length - 1;
      while (index >= 0 && this.zoomFactors[index] != aZoom)
        --index;
    }

    return index;
  },

  /***** internal helper functions below here *****/

  isZoomInRange : function(aZoom) {
    return (aZoom >= this.zoomFactors[0] && aZoom <= this.zoomFactors[this.zoomFactors.length - 1]);
  },

  jump : function(aDirection) {
    if (aDirection != -1 && aDirection != 1)
      throw Components.results.NS_ERROR_INVALID_ARG;

    var currentZoom = this.textZoom;
    var insertIndex = -1;
    const stepFactor = 1.5;

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


