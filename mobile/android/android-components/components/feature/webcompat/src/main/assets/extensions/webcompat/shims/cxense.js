/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1713721 - Shim Cxense
 *
 * Sites relying on window.cX can experience breakage if it is blocked.
 * Stubbing out the API in a shim can mitigate this breakage. There are
 * two versions of the API, one including window.cX.CCE, but both appear
 * to be very similar so we use one shim for both.
 */

if (window.cX?.getUserSegmentIds === undefined) {
  const callQueue = window.cX?.callQueue || [];
  const callQueueCCE = window.cX?.CCE?.callQueue || [];

  function getRandomString(l = 16) {
    const v = crypto.getRandomValues(new Uint8Array(l));
    const s = Array.from(v, c => c.toString(16)).join("");
    return s.slice(0, l);
  }

  const call = (cb, ...args) => {
    if (typeof cb !== "function") {
      return;
    }
    try {
      cb(...args);
    } catch (e) {
      console.error(e);
    }
  };

  const invokeOn = lib => {
    return (fn, ...args) => {
      try {
        lib[fn](...args);
      } catch (e) {
        console.error(e);
      }
    };
  };

  const userId = getRandomString();
  const cxUserId = `cx:${getRandomString(25)}:${getRandomString(12)}`;
  const topLeft = { left: 0, top: 0 };
  const margins = { left: 0, top: 0, right: 0, bottom: 0 };
  const ccePushUrl =
    "https://comcluster.cxense.com/cce/push?callback={{callback}}";
  const displayWidget = (divId, a, ctx, callback) => call(callback, ctx, divId);
  const getUserSegmentIds = a => call(a?.callback, a?.defaultValue || []);
  const init = (a, b, c, d, callback) => call(callback);
  const render = (a, data, ctx, callback) => call(callback, data, ctx);
  const run = (params, ctx, callback) => call(callback, params, ctx);
  const runCtrlVersion = (a, b, callback) => call(callback);
  const runCxVersion = (a, data, b, ctx, callback) => call(callback, data, ctx);
  const runTest = (a, divId, b, c, ctx, callback) => call(callback, divId, ctx);
  const sendConversionEvent = (a, options) => call(options?.callback, {});
  const sendEvent = (a, b, args) => call(args?.callback, {});

  const getDivId = className => {
    const e = document.querySelector(`.${className}`);
    if (e) {
      return `${className}-01`;
    }
    return null;
  };

  const getDocumentSize = () => {
    const width = document.body.clientWidth;
    const height = document.body.clientHeight;
    return { width, height };
  };

  const getNowSeconds = () => {
    return Math.round(new Date().getTime() / 1000);
  };

  const getPageContext = () => {
    return {
      location: location.href,
      pageViewRandom: "",
      userId,
    };
  };

  const getWindowSize = () => {
    const width = window.innerWidth;
    const height = window.innerHeight;
    return { width, height };
  };

  const isObject = i => {
    return typeof i === "object" && i !== null && !Array.isArray(i);
  };

  const runMulti = widgets => {
    widgets?.forEach(({ widgetParams, widgetContext, widgetCallback }) => {
      call(widgetCallback, widgetParams, widgetContext);
    });
  };

  let testGroup = -1;
  let snapPoints = [];
  const startTime = new Date();

  const library = {
    addCustomerScript() {},
    addEventListener() {},
    addExternalId() {},
    afterInitializePage() {},
    allUserConsents() {},
    backends: {
      production: {
        baseAdDeliveryUrl: "http://adserver.cxad.cxense.com/adserver/search",
        secureBaseAdDeliveryUrl:
          "https://s-adserver.cxad.cxense.com/adserver/search",
      },
      sandbox: {
        baseAdDeliveryUrl:
          "http://adserver.sandbox.cxad.cxense.com/adserver/search",
        secureBaseAdDeliveryUrl:
          "https://s-adserver.sandbox.cxad.cxense.com/adserver/search",
      },
    },
    calculateAdSpaceSize(adCount, adUnitSize, marginA, marginB) {
      return adCount * (adUnitSize + marginA + marginB);
    },
    cdn: {
      template: {
        direct: {
          http: "http://cdn.cxpublic.com/",
          https: "https://cdn.cxpublic.com/",
        },
        mapped: {
          http: "http://cdn-templates.cxpublic.com/",
          https: "https://cdn-templates.cxpublic.com/",
        },
      },
    },
    cint() {},
    cleanUpGlobalIds: [],
    clearBaseUrl: "https://scdn.cxense.com/sclear.html",
    clearCustomParameters() {},
    clearIdUrl: "https://scomcluster.cxense.com/public/clearid",
    clearIds() {},
    clickTracker: (a, b, callback) => call(callback),
    clientStorageUrl: "https://clientstorage.cxense.com",
    combineArgs: () => Object.create(),
    combineKeywordsIntoArray: () => [],
    consentClasses: ["pv", "segment", "ad", "recs"],
    consentClassesV2: ["geo", "device"],
    cookieSyncRUrl: "csyn-r.cxense.com",
    createDelegate() {},
    csdUrls: {
      domainScriptUrl: "//csd.cxpublic.com/d/",
      customerScriptUrl: "//csd.cxpublic.com/t/",
    },
    cxenseGlobalIdIframeUrl: "https://scdn.cxense.com/sglobal.html",
    cxenseUserIdUrl: "https://id.cxense.com/public/user/id",
    decodeUrlEncodedNameValuePairs: () => Object.create(),
    defaultAdRenderer: () => "",
    deleteCookie() {},
    denyWithoutConsent: {
      addExternalId: "pv",
      getUserSegmentIds: "segment",
      insertAdSpace: "ad",
      insertMultipleAdSpaces: "ad",
      sendEvent: "pv",
      sendPageViewEvent: "pv",
      sync: "ad",
    },
    dmpPushUrl: "https://comcluster.cxense.com/dmp/push?callback={{callback}}",
    emptyWidgetUrl: "https://scdn.cxense.com/empty.html",
    eventReceiverBaseUrl: "https://scomcluster.cxense.com/Repo/rep.html",
    eventReceiverBaseUrlGif: "https://scomcluster.cxense.com/Repo/rep.gif",
    getAllText: () => "",
    getClientStorageVariable() {},
    getCookie: () => null,
    getCxenseUserId: () => cxUserId,
    getDocumentSize,
    getElementPosition: () => topLeft,
    getHashFragment: () => location.hash.substr(1),
    getLocalStats: () => Object.create(),
    getNodeValue: n => n.nodeValue,
    getNowSeconds,
    getPageContext,
    getRandomString,
    getScrollPos: () => topLeft,
    getSessionId: () => "",
    getSiteId: () => "",
    getTimezoneOffset: () => new Date().getTimezoneOffset(),
    getTopLevelDomain: () => location.hostname,
    getUserId: () => userId,
    getUserSegmentIds,
    getWindowSize,
    hasConsent: () => true,
    hasHistory: () => true,
    hasLocalStorage: () => true,
    hasPassiveEventListeners: () => true,
    hasPostMessage: () => true,
    hasSessionStorage() {},
    initializePage() {},
    insertAdSpace() {},
    insertMultipleAdSpaces() {},
    insertWidget() {},
    invoke: invokeOn(library),
    isAmpIFrame() {},
    isArray() {},
    isCompatModeActive() {},
    isConsentRequired() {},
    isEdge: () => false,
    isFirefox: () => true,
    isIE6Or7: () => false,
    isObject,
    isRecsDestination: () => false,
    isSafari: () => false,
    isTextNode: n => n?.nodeType === 3,
    isTopWindow: () => window === top,
    jsonpRequest: () => false,
    loadScript() {},
    m_accountId: "0",
    m_activityEvents: false,
    m_activityState: {
      activeTime: startTime,
      currScrollLeft: 0,
      currScrollTop: 0,
      exitLink: "",
      hadHIDActivity: false,
      maxViewLeft: 1,
      maxViewTop: 1,
      parentMetrics: undefined,
      prevActivityTime: startTime + 2,
      prevScreenX: 0,
      prevScreenY: 0,
      prevScrollLeft: 0,
      prevScrollTop: 0,
      prevTime: startTime + 1,
      prevWindowHeight: 1,
      prevWindowWidth: 1,
      scrollDepthPercentage: 0,
      scrollDepthPixels: 0,
    },
    m_atfr: null,
    m_c1xTpWait: 0,
    m_clientStorage: {
      iframeEl: null,
      iframeIsLoaded: false,
      iframeOrigin: "https://clientstorage.cxense.com",
      iframePath: "/clientstorage_v2.html",
      messageContexts: {},
      messageQueue: [],
    },
    m_compatMode: {},
    m_compatModeActive: false,
    m_compatPvSent: false,
    m_consentVersion: 1,
    m_customParameters: [],
    m_documentSizeRequestedFromChild: false,
    m_externalUserIds: [],
    m_globalIdLoading: {
      globalIdIFrameEl: null,
      globalIdIFrameElLoaded: false,
    },
    m_isSpaRecsDestination: false,
    m_knownMessageSources: [],
    m_p1Complete: false,
    m_prevLocationHash: "",
    m_previousPageViewReport: null,
    m_rawCustomParameters: {},
    m_rnd: getRandomString(),
    m_scriptStartTime: startTime,
    m_siteId: "0",
    m_spaRecsClickUrl: null,
    m_thirdPartyIds: true,
    m_usesConsent: false,
    m_usesIabConsent: false,
    m_usesSecureCookies: true,
    m_usesTcf20Consent: false,
    m_widgetSpecs: {},
    Object,
    onClearIds() {},
    onFFP1() {},
    onP1() {},
    p1BaseUrl: "https://scdn.cxense.com/sp1.html",
    p1JsUrl: "https://p1cluster.cxense.com/p1.js",
    parseHashArgs: () => Object.create(),
    parseMargins: () => margins,
    parseUrlArgs: () => Object.create(),
    postMessageToParent() {},
    publicWidgetDataUrl: "https://api.cxense.com/public/widget/data",
    removeClientStorageVariable() {},
    removeEventListener() {},
    renderContainedImage: () => "<div/>",
    renderTemplate: () => "<div/>",
    reportActivity() {},
    requireActivityEvents() {},
    requireConsent() {},
    requireOnlyFirstPartyIds() {},
    requireSecureCookies() {},
    requireTcf20() {},
    sendEvent,
    sendSpaRecsClick: (a, callback) => call(callback),
    setAccountId() {},
    setAllConsentsTo() {},
    setClientStorageVariable() {},
    setCompatMode() {},
    setConsent() {},
    setCookie() {},
    setCustomParameters() {},
    setEventAttributes() {},
    setGeoPosition() {},
    setNodeValue() {},
    setRandomId() {},
    setRestrictionsToConsentClasses() {},
    setRetargetingParameters() {},
    setSiteId() {},
    setUserProfileParameters() {},
    setupIabCmp() {},
    setupTcfApi() {},
    shouldPollActivity() {},
    startLocalStats() {},
    startSessionAnnotation() {},
    stopAllSessionAnnotations() {},
    stopSessionAnnotation() {},
    sync() {},
    trackAmpIFrame() {},
    trackElement() {},
    trim: s => s.trim(),
    tsridUrl: "https://tsrid.cxense.com/lookup?callback={{callback}}",
    userSegmentUrl:
      "https://api.cxense.com/profile/user/segment?callback={{callback}}",
  };

  const libraryCCE = {
    "__cx-toolkit__": {
      isShown: true,
      data: [],
    },
    activeSnapPoint: null,
    activeWidgets: [],
    ccePushUrl,
    clickTracker: () => "",
    displayResult() {},
    displayWidget,
    getDivId,
    getTestGroup: () => testGroup,
    init,
    insertMaster() {},
    instrumentClickLinks() {},
    invoke: invokeOn(libraryCCE),
    noCache: false,
    offerProductId: null,
    persistedQueryId: null,
    prefix: null,
    previewCampaign: null,
    previewDiv: null,
    previewId: null,
    previewTestId: null,
    processCxResult() {},
    render,
    reportTestImpression() {},
    run,
    runCtrlVersion,
    runCxVersion,
    runMulti,
    runTest,
    sendConversionEvent,
    sendPageViewEvent: (a, b, c, callback) => call(callback),
    setSnapPoints(x) {
      snapPoints = x;
    },
    setTestGroup(x) {
      testGroup = x;
    },
    setVisibilityField() {},
    get snapPoints() {
      return snapPoints;
    },
    startTime,
    get testGroup() {
      return testGroup;
    },
    testVariant: null,
    trackTime: 0.5,
    trackVisibility() {},
    updateRecsClickUrls() {},
    utmParams: [],
    version: "2.42",
    visibilityField: "timeHalf",
  };

  const CCE = {
    activeSnapPoint: null,
    activeWidgets: [],
    callQueue: callQueueCCE,
    ccePushUrl,
    clickTracker: () => "",
    displayResult() {},
    displayWidget,
    getDivId,
    getTestGroup: () => testGroup,
    init,
    insertMaster() {},
    instrumentClickLinks() {},
    invoke: invokeOn(libraryCCE),
    library: libraryCCE,
    noCache: false,
    offerProductId: null,
    persistedQueryId: null,
    prefix: null,
    previewCampaign: null,
    previewDiv: null,
    previewId: null,
    previewTestId: null,
    processCxResult() {},
    render,
    reportTestImpression() {},
    run,
    runCtrlVersion,
    runCxVersion,
    runMulti,
    runTest,
    sendConversionEvent,
    sendPageViewEvent: (a, b, c, callback) => call(callback),
    setSnapPoints(x) {
      snapPoints = x;
    },
    setTestGroup(x) {
      testGroup = x;
    },
    setVisibilityField() {},
    get snapPoints() {
      return snapPoints;
    },
    startTime,
    get testGroup() {
      return testGroup;
    },
    testVariant: null,
    trackTime: 0.5,
    trackVisibility() {},
    updateRecsClickUrls() {},
    utmParams: [],
    version: "2.42",
    visibilityField: "timeHalf",
  };

  window.cX = {
    addCustomerScript() {},
    addEventListener() {},
    addExternalId() {},
    afterInitializePage() {},
    allUserConsents: () => undefined,
    Array,
    calculateAdSpaceSize: () => 0,
    callQueue,
    CCE,
    cint: () => undefined,
    clearCustomParameters() {},
    clearIds() {},
    clickTracker: () => "",
    combineArgs: () => Object.create(),
    combineKeywordsIntoArray: () => [],
    createDelegate() {},
    decodeUrlEncodedNameValuePairs: () => Object.create(),
    defaultAdRenderer: () => "",
    deleteCookie() {},
    getAllText: () => "",
    getClientStorageVariable() {},
    getCookie: () => null,
    getCxenseUserId: () => cxUserId,
    getDocumentSize,
    getElementPosition: () => topLeft,
    getHashFragment: () => location.hash.substr(1),
    getLocalStats: () => Object.create(),
    getNodeValue: n => n.nodeValue,
    getNowSeconds,
    getPageContext,
    getRandomString,
    getScrollPos: () => topLeft,
    getSessionId: () => "",
    getSiteId: () => "",
    getTimezoneOffset: () => new Date().getTimezoneOffset(),
    getTopLevelDomain: () => location.hostname,
    getUserId: () => userId,
    getUserSegmentIds,
    getWindowSize,
    hasConsent: () => true,
    hasHistory: () => true,
    hasLocalStorage: () => true,
    hasPassiveEventListeners: () => true,
    hasPostMessage: () => true,
    hasSessionStorage() {},
    initializePage() {},
    insertAdSpace() {},
    insertMultipleAdSpaces() {},
    insertWidget() {},
    invoke: invokeOn(library),
    isAmpIFrame() {},
    isArray() {},
    isCompatModeActive() {},
    isConsentRequired() {},
    isEdge: () => false,
    isFirefox: () => true,
    isIE6Or7: () => false,
    isObject,
    isRecsDestination: () => false,
    isSafari: () => false,
    isTextNode: n => n?.nodeType === 3,
    isTopWindow: () => window === top,
    JSON,
    jsonpRequest: () => false,
    library,
    loadScript() {},
    Object,
    onClearIds() {},
    onFFP1() {},
    onP1() {},
    parseHashArgs: () => Object.create(),
    parseMargins: () => margins,
    parseUrlArgs: () => Object.create(),
    postMessageToParent() {},
    removeClientStorageVariable() {},
    removeEventListener() {},
    renderContainedImage: () => "<div/>",
    renderTemplate: () => "<div/>",
    reportActivity() {},
    requireActivityEvents() {},
    requireConsent() {},
    requireOnlyFirstPartyIds() {},
    requireSecureCookies() {},
    requireTcf20() {},
    sendEvent,
    sendPageViewEvent: (a, callback) => call(callback, {}),
    sendSpaRecsClick() {},
    setAccountId() {},
    setAllConsentsTo() {},
    setClientStorageVariable() {},
    setCompatMode() {},
    setConsent() {},
    setCookie() {},
    setCustomParameters() {},
    setEventAttributes() {},
    setGeoPosition() {},
    setNodeValue() {},
    setRandomId() {},
    setRestrictionsToConsentClasses() {},
    setRetargetingParameters() {},
    setSiteId() {},
    setUserProfileParameters() {},
    setupIabCmp() {},
    setupTcfApi() {},
    shouldPollActivity() {},
    startLocalStats() {},
    startSessionAnnotation() {},
    stopAllSessionAnnotations() {},
    stopSessionAnnotation() {},
    sync() {},
    trackAmpIFrame() {},
    trackElement() {},
    trim: s => s.trim(),
  };

  window.cxTest = window.cX;

  window.cx_pollActiveTime = () => undefined;
  window.cx_pollActivity = () => undefined;
  window.cx_pollFragmentMessage = () => undefined;

  const execQueue = (lib, queue) => {
    return () => {
      const invoke = invokeOn(lib);
      setTimeout(() => {
        queue.push = cmd => {
          setTimeout(() => invoke(...cmd), 1);
        };
        for (const cmd of queue) {
          invoke(...cmd);
        }
      }, 25);
    };
  };

  window.cx_callQueueExecute = execQueue(library, callQueue);
  window.cxCCE_callQueueExecute = execQueue(libraryCCE, callQueueCCE);

  window.cx_callQueueExecute();
  window.cxCCE_callQueueExecute();
}
