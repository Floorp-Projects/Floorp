/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// based on https://github.com/gorhill/uBlock/blob/6f49e079db0262e669b70f4169924f796ac8db7c/src/web_accessible_resources/google-analytics_ga.js

"use strict";

if (!window._gaq) {
  function noopfn() {}

  const gaq = {
    Na: noopfn,
    O: noopfn,
    Sa: noopfn,
    Ta: noopfn,
    Va: noopfn,
    _createAsyncTracker: noopfn,
    _getAsyncTracker: noopfn,
    _getPlugin: noopfn,
    push: a => {
      if (typeof a === "function") {
        a();
        return;
      }
      if (!Array.isArray(a)) {
        return;
      }
      if (
        typeof a[0] === "string" &&
        /(^|\.)_link$/.test(a[0]) &&
        typeof a[1] === "string"
      ) {
        window.location.assign(a[1]);
      }
      if (
        a[0] === "_set" &&
        a[1] === "hitCallback" &&
        typeof a[2] === "function"
      ) {
        a[2]();
      }
    },
  };

  const tracker = {
    _addIgnoredOrganic: noopfn,
    _addIgnoredRef: noopfn,
    _addItem: noopfn,
    _addOrganic: noopfn,
    _addTrans: noopfn,
    _clearIgnoredOrganic: noopfn,
    _clearIgnoredRef: noopfn,
    _clearOrganic: noopfn,
    _cookiePathCopy: noopfn,
    _deleteCustomVar: noopfn,
    _getName: noopfn,
    _setAccount: noopfn,
    _getAccount: noopfn,
    _getClientInfo: noopfn,
    _getDetectFlash: noopfn,
    _getDetectTitle: noopfn,
    _getLinkerUrl: a => a,
    _getLocalGifPath: noopfn,
    _getServiceMode: noopfn,
    _getVersion: noopfn,
    _getVisitorCustomVar: noopfn,
    _initData: noopfn,
    _link: noopfn,
    _linkByPost: noopfn,
    _setAllowAnchor: noopfn,
    _setAllowHash: noopfn,
    _setAllowLinker: noopfn,
    _setCampContentKey: noopfn,
    _setCampMediumKey: noopfn,
    _setCampNameKey: noopfn,
    _setCampNOKey: noopfn,
    _setCampSourceKey: noopfn,
    _setCampTermKey: noopfn,
    _setCampaignCookieTimeout: noopfn,
    _setCampaignTrack: noopfn,
    _setClientInfo: noopfn,
    _setCookiePath: noopfn,
    _setCookiePersistence: noopfn,
    _setCookieTimeout: noopfn,
    _setCustomVar: noopfn,
    _setDetectFlash: noopfn,
    _setDetectTitle: noopfn,
    _setDomainName: noopfn,
    _setLocalGifPath: noopfn,
    _setLocalRemoteServerMode: noopfn,
    _setLocalServerMode: noopfn,
    _setReferrerOverride: noopfn,
    _setRemoteServerMode: noopfn,
    _setSampleRate: noopfn,
    _setSessionTimeout: noopfn,
    _setSiteSpeedSampleRate: noopfn,
    _setSessionCookieTimeout: noopfn,
    _setVar: noopfn,
    _setVisitorCookieTimeout: noopfn,
    _trackEvent: noopfn,
    _trackPageLoadTime: noopfn,
    _trackPageview: noopfn,
    _trackSocial: noopfn,
    _trackTiming: noopfn,
    _trackTrans: noopfn,
    _visitCode: noopfn,
  };

  const gat = {
    _anonymizeIP: noopfn,
    _createTracker: noopfn,
    _forceSSL: noopfn,
    _getPlugin: noopfn,
    _getTracker: () => tracker,
    _getTrackerByName: () => tracker,
    _getTrackers: noopfn,
    aa: noopfn,
    ab: noopfn,
    hb: noopfn,
    la: noopfn,
    oa: noopfn,
    pa: noopfn,
    u: noopfn,
  };

  window._gat = gat;

  const aa = window._gaq || [];
  if (Array.isArray(aa)) {
    while (aa[0]) {
      gaq.push(aa.shift());
    }
  }

  window._gaq = gaq.qf = gaq;
}
