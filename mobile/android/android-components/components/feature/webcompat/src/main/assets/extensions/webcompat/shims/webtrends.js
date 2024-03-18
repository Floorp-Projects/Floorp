/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1766414 - Shim WebTrends Core Tag and Advanced Link Tracking
 *
 * Sites using WebTrends Core Tag or Link Tracking can break if they are
 * are blocked. This shim mitigates that breakage by loading an empty module.
 */

if (!window.dcsMultiTrack) {
  window.dcsMultiTrack = o => {
    o?.callback?.({});
  };
}

if (!window.WebTrends) {
  class dcs {
    addSelector() {
      return this;
    }
    addTransform() {
      return this;
    }
    DCSext = {};
    init(obj) {
      return this;
    }
    track() {
      return this;
    }
  }

  window.Webtrends = window.WebTrends = {
    dcs,
    multiTrack: window.dcsMultiTrack,
  };

  window.requestAnimationFrame(() => {
    window.webtrendsAsyncLoad?.(dcs);
    window.webtrendsAsyncInit?.();
  });
}
