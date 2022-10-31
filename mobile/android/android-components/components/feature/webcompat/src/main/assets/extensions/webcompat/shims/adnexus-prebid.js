/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1694401 - Shim Prebid.js
 *
 * Some sites rely on prebid.js to place content, perhaps in conjunction with
 * other services like Google Publisher Tags and Amazon TAM. This shim prevents
 * site breakage like image galleries breaking as the user browsers them, by
 * allowing the content placement to succeed.
 */

if (!window.pbjs?.requestBids) {
  const que = window.pbjs?.que || [];
  const cmd = window.pbjs?.cmd || [];
  const adUnits = window.pbjs?.adUnits || [];

  window.pbjs = {
    adUnits,
    addAdUnits(arr) {
      if (!Array.isArray(arr)) {
        arr = [arr];
      }
      adUnits.push(arr);
    },
    cmd,
    offEvent() {},
    que,
    refreshAds() {},
    removeAdUnit(codes) {
      if (!Array.isArray(codes)) {
        codes = [codes];
      }
      for (const code of codes) {
        for (let i = adUnits.length - 1; i >= 0; i--) {
          if (adUnits[i].code === code) {
            adUnits.splice(i, 1);
          }
        }
      }
    },
    renderAd() {},
    requestBids(params) {
      params?.bidsBackHandler?.();
    },
    setConfig() {},
    setTargetingForGPTAsync() {},
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

  que.push = push;
  cmd.push = push;

  que.forEach(push);
  cmd.forEach(push);
}
