/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Bug 1714431 - Shim Optimizely
 *
 * This shim stubs out window.optimizely for those sites which
 * break when it is not successfully loaded.
 */

if (!window.optimizely?.state) {
  const behavior = {
    query: () => [],
  };

  const dcp = {
    getAttributeValue() {},
    waitForAttributeValue: () => Promise.resolve(),
  };

  const data = {
    accountId: "",
    audiences: {},
    campaigns: {},
    clientName: "js",
    clientVersion: "0.166.0",
    dcpServiceId: null,
    events: {},
    experiments: {},
    groups: {},
    pages: {},
    projectId: "",
    revision: "",
    snippetId: null,
    variations: {},
  };

  const activationId = String(Date.now());

  const state = {
    getActivationId() {
      return activationId;
    },
    getActiveExperimentIds() {
      return [];
    },
    getCampaignStateLists() {
      return {};
    },
    getCampaignStates() {
      return {};
    },
    getDecisionObject() {
      return null;
    },
    getDecisionString() {
      return null;
    },
    getExperimentStates() {
      return {};
    },
    getPageStates() {
      return {};
    },
    getRedirectInfo() {
      return null;
    },
    getVariationMap() {
      return {};
    },
    isGlobalHoldback() {
      return false;
    },
  };

  const poll = (fn, to) => {
    setInterval(() => {
      try {
        fn();
      } catch (_) {}
    }, to);
  };

  const waitUntil = test => {
    let interval, resolve;
    function check() {
      try {
        if (test()) {
          clearInterval(interval);
          resolve?.();
          return true;
        }
      } catch (_) {}
      return false;
    }
    return new Promise(r => {
      resolve = r;
      if (check()) {
        resolve();
        return;
      }
      interval = setInterval(check, 250);
    });
  };

  const waitForElement = sel => {
    return waitUntil(() => {
      document.querySelector(sel);
    });
  };

  const observeSelector = (sel, fn, opts) => {
    let interval;
    const observed = new Set();
    function check() {
      try {
        for (const e of document.querySelectorAll(sel)) {
          if (observed.has(e)) {
            continue;
          }
          observed.add(e);
          try {
            fn(e);
          } catch (_) {}
          if (opts.once) {
            clearInterval(interval);
          }
        }
      } catch (_) {}
    }
    interval = setInterval(check, 250);
    const timeout = { opts };
    if (timeout) {
      setTimeout(() => {
        clearInterval(interval);
      }, timeout);
    }
  };

  const utils = {
    Promise: window.Promise,
    observeSelector,
    poll,
    waitForElement,
    waitUntil,
  };

  const visitorId = {
    randomId: "",
  };

  let browserVersion = "";
  try {
    browserVersion = navigator.userAgent.match(/rv:(.*)\)/)[1];
  } catch (_) {}

  const visitor = {
    browserId: "ff",
    browserVersion,
    currentTimestamp: Date.now(),
    custom: {},
    customBehavior: {},
    device: "desktop",
    device_type: "desktop_laptop",
    events: [],
    first_session: true,
    offset: 240,
    referrer: null,
    source_type: "direct",
    visitorId,
  };

  window.optimizely = {
    data: {
      note: "Obsolete, use optimizely.get('data') instead",
    },
    get(e) {
      switch (e) {
        case "behavior":
          return behavior;
        case "data":
          return data;
        case "dcp":
          return dcp;
        case "jquery":
          throw new Error("jQuery not implemented");
        case "session":
          return undefined;
        case "state":
          return state;
        case "utils":
          return utils;
        case "visitor":
          return visitor;
        case "visitor_id":
          return visitorId;
      }
      return undefined;
    },
    initialized: true,
    push() {},
    state: {},
  };
}
