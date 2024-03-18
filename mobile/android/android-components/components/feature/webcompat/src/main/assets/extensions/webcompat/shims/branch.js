/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1716220 - Shim Branch Web SDK
 *
 * Sites such as TataPlay may not load properly if Branch Web SDK is
 * blocked. This shim stubs out its script so the page still loads.
 */

if (!window?.branch?.b) {
  const queue = window?.branch?._q || [];
  window.branch = new (class {
    V = {};
    g = 0;
    X = "web2.62.0";
    b = {
      A: {},
      clear() {},
      get() {},
      getAll() {},
      isEnabled: () => true,
      remove() {},
      set() {},
      ca() {},
      g: [],
      l: 0,
      o: 0,
      s: null,
    };
    addListener() {}
    applyCode() {}
    autoAppIndex() {}
    banner() {}
    c() {}
    closeBanner() {}
    closeJourney() {}
    constructor() {}
    creditHistory() {}
    credits() {}
    crossPlatformIds() {}
    data() {}
    deepview() {}
    deepviewCta() {}
    disableTracking() {}
    first() {}
    getBrowserFingerprintId() {}
    getCode() {}
    init(key, ...args) {
      const cb = args.pop();
      if (typeof cb === "function") {
        cb(undefined, {});
      }
    }
    lastAttributedTouchData() {}
    link() {}
    logEvent() {}
    logout() {}
    qrCode() {}
    redeem() {}
    referrals() {}
    removeListener() {}
    renderFinalize() {}
    renderQueue() {}
    sendSMS() {}
    setAPIResponseCallback() {}
    setBranchViewData() {}
    setIdentity() {}
    track() {}
    trackCommerceEvent() {}
    validateCode() {}
  })();
  const push = ([fn, ...args]) => {
    try {
      window.branch[fn].apply(window.branch, args);
    } catch (e) {
      console.error(e);
    }
  };
  queue.forEach(push);
}
