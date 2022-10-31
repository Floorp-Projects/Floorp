/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1728114 - Shim Adobe EverestJS
 *
 * Sites assuming EverestJS will load can break if it is blocked.
 * This shim mitigates that breakage.
 */

if (!window.__ql) {
  window.__ql = {};
}

if (!window.EF) {
  const AdCloudLocalStorage = {
    get: (_, cb) => cb(),
    isInitDone: true,
    isInitSuccess: true,
  };

  const emptyObj = {};

  const nullSrc = {
    getHosts: () => [undefined],
    getProtocols: () => [undefined],
    hash: {},
    hashParamsOrder: [],
    host: undefined,
    path: [],
    port: undefined,
    query: {},
    queryDelimiter: "&",
    queryParamsOrder: [],
    queryPrefix: "?",
    queryWithoutEncode: {},
    respectEmptyQueryParamValue: undefined,
    scheme: undefined,
    text: "//",
    userInfo: undefined,
  };

  const pixelDetailsEvent = {
    addToDom() {},
    canAddToDom: () => false,
    fire() {},
    getDomElement() {},
    initializeUri() {},
    pixelDetailsReceiver() {},
    scheme: "https:",
    uri: nullSrc,
    userid: 0,
  };

  window.EF = {
    AdCloudLocalStorage,
    accessTopUrl: 0,
    acquireCookieMatchingSlot() {},
    addListener() {},
    addPixelDetailsReadyListener() {},
    addToDom() {},
    allow3rdPartyPixels: 1,
    appData: "",
    appendDictionary() {},
    checkGlobalSid() {},
    checkUrlParams() {},
    cmHost: "cm.everesttech.net",
    context: {
      isFbApp: () => 0,
      isPageview: () => false,
      isSegmentation: () => false,
      isTransaction: () => false,
    },
    conversionData: "",
    cookieMatchingSlots: 1,
    debug: 0,
    deserializeUrlParams: () => emptyObj,
    doCookieMatching() {},
    ef_itp_ls: false,
    eventType: "",
    executeAfterLoad() {},
    executeOnloadCallbacks() {},
    expectedTrackingParams: ["ev_cl", "ev_sid"],
    fbIsApp: 0,
    fbsCM: 0,
    fbsPixelId: 0,
    filterList: () => [],
    getArrayIndex: -1,
    getConversionData: () => "",
    getConversionDataFromLocalStorage: cb => cb(),
    getDisplayClickUri: () => "",
    getEpochFromEfUniq: () => 0,
    getFirstLevelObjectCopy: () => emptyObj,
    getInvisibleIframeElement() {},
    getInvisibleImageElement() {},
    getMacroSubstitutedText: () => "",
    getPixelDetails: cb => cb({}),
    getScriptElement() {},
    getScriptSrc: () => "",
    getServerParams: () => emptyObj,
    getSortedAttributes: () => [],
    getTrackingParams: () => emptyObj,
    getTransactionParams: () => emptyObj,
    handleConversionData() {},
    impressionProperties: "",
    impressionTypes: ["impression", "impression_served"],
    inFloodlight: 0,
    init(config) {
      try {
        const { userId } = config;
        window.EF.userId = userId;
        pixelDetailsEvent.userId = userId;
      } catch (_) {}
    },
    initializeEFVariables() {},
    isArray: a => Array.isArray(a),
    isEmptyDictionary: () => true,
    isITPEnabled: () => false,
    isPermanentCookieSet: () => false,
    isSearchClick: () => 0,
    isXSSReady() {},
    jsHost: "www.everestjs.net",
    jsTagAdded: 0,
    location: nullSrc,
    locationHref: nullSrc,
    locationSkipBang: nullSrc,
    log() {},
    main() {},
    main2() {},
    newCookieMatchingEvent: () => emptyObj,
    newFbsCookieMatching: () => emptyObj,
    newImpression: () => emptyObj,
    newPageview: () => emptyObj,
    newPixelDetails: () => emptyObj,
    newPixelEvent: () => emptyObj,
    newPixelServerDisplayClickRedirectUri: () => emptyObj,
    newPixelServerGenericRedirectUri: () => emptyObj,
    newPixelServerUri: () => emptyObj,
    newProductSegment: () => emptyObj,
    newSegmentJavascript: () => emptyObj,
    newTransaction: () => emptyObj,
    newUri: () => emptyObj,
    onloadCallbacks: [],
    pageViewProperties: "",
    pageviewProperties: "",
    pixelDetails: {},
    pixelDetailsAdded: 1,
    pixelDetailsEvent,
    pixelDetailsParams: [],
    pixelDetailsReadyCallbackFns: [],
    pixelDetailsRecieverCalled: 1,
    pixelHost: "pixel.everesttech.net",
    protocol: document?.location?.protocol || "",
    referrer: nullSrc,
    removeListener() {},
    searchSegment: "",
    segment: "",
    serverParamsListener() {},
    sid: 0,
    sku: "",
    throttleCookie: "",
    trackingJavascriptSrc: nullSrc,
    transactionObjectList: [],
    transactionProperties: "",
    userServerParams: {},
    userid: 0,
  };
}
