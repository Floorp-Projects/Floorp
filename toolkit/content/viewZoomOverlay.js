// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/** Document Zoom Management Code
 *
 * To use this, you'll need to have a global gBrowser variable
 * or use the methods that accept a browser to be modified.
 **/

var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

var ZoomManager = {
  set useFullZoom(aVal) {
    Services.prefs.setBoolPref("browser.zoom.full", aVal);
  },

  get zoom() {
    return this.getZoomForBrowser(gBrowser);
  },

  useFullZoomForBrowser: function ZoomManager_useFullZoomForBrowser(aBrowser) {
    return this.useFullZoom || aBrowser.isSyntheticDocument;
  },

  getFullZoomForBrowser: function ZoomManager_getFullZoomForBrowser(aBrowser) {
    if (!this.useFullZoomForBrowser(aBrowser)) {
      return 1.0;
    }
    return this.getZoomForBrowser(aBrowser);
  },

  getZoomForBrowser: function ZoomManager_getZoomForBrowser(aBrowser) {
    let zoom = this.useFullZoomForBrowser(aBrowser)
      ? aBrowser.fullZoom
      : aBrowser.textZoom;
    // Round to remove any floating-point error.
    return Number(zoom ? zoom.toFixed(2) : 1);
  },

  set zoom(aVal) {
    this.setZoomForBrowser(gBrowser, aVal);
  },

  setZoomForBrowser: function ZoomManager_setZoomForBrowser(aBrowser, aVal) {
    if (aVal < this.MIN || aVal > this.MAX) {
      throw Components.Exception("", Cr.NS_ERROR_INVALID_ARG);
    }

    if (this.useFullZoomForBrowser(aBrowser)) {
      aBrowser.textZoom = 1;
      aBrowser.fullZoom = aVal;
    } else {
      aBrowser.textZoom = aVal;
      aBrowser.fullZoom = 1;
    }
  },

  enlarge: function ZoomManager_enlarge() {
    var i = this.zoomValues.indexOf(this.snap(this.zoom)) + 1;
    if (i < this.zoomValues.length) {
      this.zoom = this.zoomValues[i];
    }
  },

  reduce: function ZoomManager_reduce() {
    var i = this.zoomValues.indexOf(this.snap(this.zoom)) - 1;
    if (i >= 0) {
      this.zoom = this.zoomValues[i];
    }
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
        if (i > 0 && aVal - values[i - 1] < values[i] - aVal) {
          i--;
        }
        return values[i];
      }
    }
    return values[i - 1];
  },
};

XPCOMUtils.defineLazyPreferenceGetter(
  ZoomManager,
  "MIN",
  "zoom.minPercent",
  30,
  null,
  v => v / 100
);
XPCOMUtils.defineLazyPreferenceGetter(
  ZoomManager,
  "MAX",
  "zoom.maxPercent",
  500,
  null,
  v => v / 100
);

XPCOMUtils.defineLazyPreferenceGetter(
  ZoomManager,
  "zoomValues",
  "toolkit.zoomManager.zoomValues",
  ".3,.5,.67,.8,.9,1,1.1,1.2,1.33,1.5,1.7,2,2.4,3,4,5",
  null,
  zoomValues => {
    zoomValues = zoomValues.split(",").map(parseFloat);
    zoomValues.sort((a, b) => a - b);

    while (zoomValues[0] < this.MIN) {
      zoomValues.shift();
    }

    while (zoomValues[zoomValues.length - 1] > this.MAX) {
      zoomValues.pop();
    }

    return zoomValues;
  }
);

XPCOMUtils.defineLazyPreferenceGetter(
  ZoomManager,
  "useFullZoom",
  "browser.zoom.full"
);
