/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["ZoomChild"];

const { ActorChild } = ChromeUtils.import(
  "resource://gre/modules/ActorChild.jsm"
);

class ZoomChild extends ActorChild {
  constructor(dispatcher) {
    super(dispatcher);

    this._cache = {
      fullZoom: NaN,
      textZoom: NaN,
    };
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
    if (event.type == "FullZoomChange") {
      if (this.refreshFullZoom()) {
        this.mm.sendAsyncMessage("FullZoomChange", { value: this.fullZoom });
      }
    } else if (event.type == "TextZoomChange") {
      if (this.refreshTextZoom()) {
        this.mm.sendAsyncMessage("TextZoomChange", { value: this.textZoom });
      }
    } else if (event.type == "ZoomChangeUsingMouseWheel") {
      this.mm.sendAsyncMessage("ZoomChangeUsingMouseWheel", {});
    }
  }
}
