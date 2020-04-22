/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["ZoomChild"];

/**
 * FIXME(emilio): Most of this code is a bit useless and could be changed by an
 * event listener in the parent process I suspect.
 */
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
    this._cache.fullZoom = Number(value.toFixed(2));
    this.browsingContext.fullZoom = value;
  }

  set textZoom(value) {
    this._cache.textZoom = Number(value.toFixed(2));
    this.browsingContext.textZoom = value;
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
    let actualZoomValue = this.browsingContext[valueName];
    // Round to remove any floating-point error.
    actualZoomValue = Number(actualZoomValue.toFixed(2));
    if (actualZoomValue != this._cache[valueName]) {
      this._cache[valueName] = actualZoomValue;
      return true;
    }
    return false;
  }

  receiveMessage(message) {
    if (message.name == "FullZoom") {
      this.fullZoom = message.data.value;
    } else if (message.name == "TextZoom") {
      this.textZoom = message.data.value;
    }
  }

  handleEvent(event) {
    // Send do zoom events to our parent as messages, to be re-dispatched.
    if (event.type == "DoZoomEnlargeBy10") {
      this.sendAsyncMessage("DoZoomEnlargeBy10", {});
      return;
    }

    if (event.type == "DoZoomReduceBy10") {
      this.sendAsyncMessage("DoZoomReduceBy10", {});
      return;
    }

    // Only handle remaining events for top-level content.
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
      return;
    }

    if (event.type == "FullZoomChange") {
      if (this.refreshFullZoom()) {
        this.sendAsyncMessage("FullZoomChange", {});
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

      this.sendAsyncMessage("FullZoomResolutionStable", {});
      return;
    }

    if (event.type == "TextZoomChange") {
      if (this.refreshTextZoom()) {
        this.sendAsyncMessage("TextZoomChange", {});
      }
    }
  }
}
