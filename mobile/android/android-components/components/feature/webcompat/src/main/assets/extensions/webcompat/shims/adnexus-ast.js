/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1734130 - Shim AdNexus AST
 *
 * Some sites expect AST to successfully load, or they break.
 * This shim mitigates that breakage.
 */

if (!window.apntag?.loaded) {
  const anq = window.apntag?.anq || [];

  const gTags = new Map();
  const gAds = new Map();
  const gEventHandlers = {};

  const Ad = class {
    adType = "banner";
    auctionId = "-";
    banner = {
      width: 1,
      height: 1,
      content: "",
      trackers: {
        impression_urls: [],
        video_events: {},
      },
    };
    brandCategoryId = 0;
    buyerMemberId = 0;
    cpm = 0.1;
    cpm_publisher_currency = 0.1;
    creativeId = 0;
    dealId = undefined;
    height = 1;
    mediaSubtypeId = 1;
    mediaTypeId = 1;
    publisher_currency_code = "US";
    source = "-";
    tagId = -1;
    targetId = "";
    width = 1;

    constructor(tagId, targetId) {
      this.tagId = tagId;
      this.targetId = targetId;
    }
  };

  const fireAdEvent = (type, adObj) => {
    const { targetId } = adObj;
    const handlers = gEventHandlers[type]?.[targetId];
    if (!handlers) {
      return Promise.resolve();
    }
    const evt = { adObj, type };
    return new Promise(done => {
      setTimeout(() => {
        for (const cb of handlers) {
          try {
            cb(evt);
          } catch (e) {
            console.error(e);
          }
        }
        done();
      }, 1);
    });
  };

  const refreshTag = targetId => {
    const tag = gTags.get(targetId);
    if (!tag) {
      return;
    }
    if (!gAds.has(targetId)) {
      gAds.set(targetId, new Ad(tag.tagId, targetId));
    }
    const adObj = gAds.get(targetId);
    fireAdEvent("adRequested", adObj).then(() => {
      // TODO: do some sites expect adAvailable+adLoaded instead of adNoBid?
      fireAdEvent("adNoBid", adObj);
    });
  };

  const off = (type, targetId, cb) => {
    gEventHandlers[type]?.[targetId]?.delete(cb);
  };

  const on = (type, targetId, cb) => {
    gEventHandlers[type] = gEventHandlers[type] || {};
    gEventHandlers[type][targetId] =
      gEventHandlers[type][targetId] || new Set();
    gEventHandlers[type][targetId].add(cb);
  };

  const Tag = class {
    static #nextId = 0;
    debug = undefined;
    displayed = false;
    initialHeight = 1;
    initialWidth = 1;
    keywords = {};
    member = 0;
    showTagCalled = false;
    sizes = [];
    targetId = "";
    utCalled = true;
    utDivId = "";
    utiframeId = "";
    uuid = "";

    constructor(raw) {
      const { keywords, sizes, targetId } = raw;
      this.tagId = Tag.#nextId++;
      this.keywords = keywords || {};
      this.sizes = sizes || [];
      this.targetId = targetId || "";
    }
    modifyTag() {}
    off(type, cb) {
      off(type, this.targetId, cb);
    }
    on(type, cb) {
      on(type, this.targetId, cb);
    }
    setKeywords(kw) {
      this.keywords = kw;
    }
  };

  window.apntag = {
    anq,
    attachClickTrackers() {},
    checkAdAvailable() {},
    clearPageTargeting() {},
    clearRequest() {},
    collapseAd() {},
    debug: false,
    defineTag(dfn) {
      const { targetId } = dfn;
      if (!targetId) {
        return;
      }
      gTags.set(targetId, new Tag(dfn));
    },
    disableDebug() {},
    dongle: undefined,
    emitEvent(adObj, type) {
      fireAdEvent(type, adObj);
    },
    enableCookieSet() {},
    enableDebug() {},
    fireImpressionTrackers() {},
    getAdMarkup: () => "",
    getAdWrap() {},
    getAstVersion: () => "0.49.0",
    getPageTargeting() {},
    getTag(targetId) {
      return gTags.get(targetId);
    },
    handleCb() {},
    handleMediationBid() {},
    highlightAd() {},
    loaded: true,
    loadTags() {
      for (const tagName of gTags.keys()) {
        refreshTag(tagName);
      }
    },
    modifyTag() {},
    notify() {},
    offEvent(type, target, cb) {
      off(type, target, cb);
    },
    onEvent(type, target, cb) {
      on(type, target, cb);
    },
    recordErrorEvent() {},
    refresh() {},
    registerRenderer() {},
    requests: {},
    resizeAd() {},
    setEndpoint() {},
    setKeywords() {},
    setPageOpts() {},
    setPageTargeting() {},
    setSafeFrameConfig() {},
    setSizes() {},
    showTag() {},
  };

  const push = function(fn) {
    if (typeof fn === "function") {
      try {
        fn();
      } catch (e) {
        console.trace(e);
      }
    }
  };

  anq.push = push;

  anq.forEach(push);
}
