/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["ZoomChild"];

class ZoomChild extends JSWindowActorChild {
  constructor() {
    super();

    this._cache = {
      fullZoom: NaN,
      textZoom: NaN,
    };

    this._resolutionBeforeFullZoomChange = 0;
  }

  get fullZoom() {
    return this._cache.fullZoom;
  }

  get textZoom() {
    return this._cache.textZoom;
  }

  set fullZoom(value) {
    this._cache.fullZoom = value;
    this._markupViewer.fullZoom = value;
  }

  set textZoom(value) {
    this._cache.textZoom = value;
    this._markupViewer.textZoom = value;
  }

  refreshFullZoom() {
    return this._refreshZoomValue("fullZoom");
  }

  refreshTextZoom() {
    return this._refreshZoomValue("textZoom");
  }

  /**
   * Retrieves specified zoom property value from markupViewer and refreshes
   * cache if needed.
   * @param valueName Either 'fullZoom' or 'textZoom'.
   * @returns Returns true if cached value was actually refreshed.
   * @private
   */
  _refreshZoomValue(valueName) {
    let actualZoomValue = this._markupViewer[valueName];
    // Round to remove any floating-point error.
    actualZoomValue = Number(actualZoomValue.toFixed(2));
    if (actualZoomValue != this._cache[valueName]) {
      this._cache[valueName] = actualZoomValue;
      return true;
    }
    return false;
  }

  get _markupViewer() {
    return this.docShell.contentViewer;
  }

  receiveMessage(message) {
    if (message.name == "FullZoom") {
      this.fullZoom = message.data.value;
    } else if (message.name == "TextZoom") {
      this.textZoom = message.data.value;
    }
  }

  handleEvent(event) {
    if (event.type == "ZoomChangeUsingMouseWheel") {
      this.sendAsyncMessage("ZoomChangeUsingMouseWheel", {});
      return;
    }

    // Only handle this event for top-level content.
    if (this.browsingContext != this.browsingContext.top) {
      return;
    }

    if (event.type == "PreFullZoomChange") {
      // Check if we're in the middle of a full zoom change. If we are,
      // don't capture the resolution again, because it hasn't yet been
      // restored and may be in an indeterminate state.
      if (this._resolutionBeforeFullZoomChange == 0) {
        this._resolutionBeforeFullZoomChange = this.contentWindow.windowUtils.getResolution();
      }

      this.sendAsyncMessage("PreFullZoomChange", {});
      return;
    }

    if (event.type == "FullZoomChange") {
      if (this.refreshFullZoom()) {
        this.sendAsyncMessage("FullZoomChange", { value: this.fullZoom });
      }
      return;
    }

    if (event.type == "mozupdatedremoteframedimensions") {
      // Check to see if we've already restored resolution, in which case
      // there's no need to do it again.
      if (this._resolutionBeforeFullZoomChange != 0) {
        this.contentWindow.windowUtils.setResolutionAndScaleTo(
          this._resolutionBeforeFullZoomChange
        );
        this._resolutionBeforeFullZoomChange = 0;
      }

      this.sendAsyncMessage("PostFullZoomChange", {});
      return;
    }

    if (event.type == "TextZoomChange") {
      if (this.refreshTextZoom()) {
        this.sendAsyncMessage("TextZoomChange", { value: this.textZoom });
      }
    }
  }
}
