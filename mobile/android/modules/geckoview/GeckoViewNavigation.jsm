/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewNavigation"];

const { GeckoViewModule } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewModule.jsm"
);
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  E10SUtils: "resource://gre/modules/E10SUtils.jsm",
  LoadURIDelegate: "resource://gre/modules/LoadURIDelegate.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

XPCOMUtils.defineLazyGetter(this, "ReferrerInfo", () =>
  Components.Constructor(
    "@mozilla.org/referrer-info;1",
    "nsIReferrerInfo",
    "init"
  )
);

// Create default ReferrerInfo instance for the given referrer URI string.
const createReferrerInfo = aReferrer => {
  let referrerUri;
  try {
    referrerUri = Services.io.newURI(aReferrer);
  } catch (ignored) {}

  return new ReferrerInfo(Ci.nsIReferrerInfo.EMPTY, true, referrerUri);
};

function convertFlags(aFlags) {
  let navFlags = Ci.nsIWebNavigation.LOAD_FLAGS_NONE;
  if (!aFlags) {
    return navFlags;
  }
  // These need to match the values in GeckoSession.LOAD_FLAGS_*
  if (aFlags & (1 << 0)) {
    navFlags |= Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE;
  }
  if (aFlags & (1 << 1)) {
    navFlags |= Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_PROXY;
  }
  if (aFlags & (1 << 2)) {
    navFlags |= Ci.nsIWebNavigation.LOAD_FLAGS_FROM_EXTERNAL;
  }
  if (aFlags & (1 << 3)) {
    navFlags |= Ci.nsIWebNavigation.LOAD_FLAGS_ALLOW_POPUPS;
  }
  if (aFlags & (1 << 4)) {
    navFlags |= Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CLASSIFIER;
  }
  if (aFlags & (1 << 5)) {
    navFlags |= Ci.nsIWebNavigation.LOAD_FLAGS_FORCE_ALLOW_DATA_URI;
  }
  if (aFlags & (1 << 6)) {
    navFlags |= Ci.nsIWebNavigation.LOAD_FLAGS_REPLACE_HISTORY;
  }
  return navFlags;
}

// Handles navigation requests between Gecko and a GeckoView.
// Handles GeckoView:GoBack and :GoForward requests dispatched by
// GeckoView.goBack and .goForward.
// Dispatches GeckoView:LocationChange to the GeckoView on location change when
// active.
// Implements nsIBrowserDOMWindow.
class GeckoViewNavigation extends GeckoViewModule {
  onInitBrowser() {
    this.window.browserDOMWindow = this;

    debug`sessionContextId=${this.settings.sessionContextId}`;

    if (this.settings.sessionContextId !== null) {
      // Gecko may have issues with strings containing special characters,
      // so we restrict the string format to a specific pattern.
      if (!/^gvctx(-)?([a-f0-9]+)$/.test(this.settings.sessionContextId)) {
        throw new Error("sessionContextId has illegal format");
      }

      this.browser.setAttribute(
        "geckoViewSessionContextId",
        this.settings.sessionContextId
      );
    }

    // There may be a GeckoViewNavigation module in another window waiting for
    // us to create a browser so it can set openWindowInfo, so allow them to do
    // that now.
    Services.obs.notifyObservers(this.window, "geckoview-window-created");
  }

  onInit() {
    debug`onInit`;

    this.registerListener([
      "GeckoView:GoBack",
      "GeckoView:GoForward",
      "GeckoView:GotoHistoryIndex",
      "GeckoView:LoadUri",
      "GeckoView:Reload",
      "GeckoView:Stop",
      "GeckoView:PurgeHistory",
    ]);

    this._initialAboutBlank = true;
  }

  // Bundle event handler.
  async onEvent(aEvent, aData, aCallback) {
    debug`onEvent: event=${aEvent}, data=${aData}`;

    switch (aEvent) {
      case "GeckoView:GoBack":
        this.browser.goBack();
        break;
      case "GeckoView:GoForward":
        this.browser.goForward();
        break;
      case "GeckoView:GotoHistoryIndex":
        this.browser.gotoIndex(aData.index);
        break;
      case "GeckoView:LoadUri":
        const { uri, referrerUri, referrerSessionId, flags, headers } = aData;

        let navFlags = convertFlags(flags);
        // For performance reasons we don't call the LoadUriDelegate.loadUri
        // from Gecko, and instead we call it directly in the loadUri Java API.
        navFlags |= Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_LOAD_URI_DELEGATE;

        let triggeringPrincipal, referrerInfo, csp;
        let parsedUri;
        if (referrerSessionId) {
          const referrerWindow = Services.ww.getWindowByName(
            referrerSessionId,
            this.window
          );
          triggeringPrincipal = referrerWindow.browser.contentPrincipal;
          csp = referrerWindow.browser.csp;

          const referrerPolicy = referrerWindow.browser.referrerInfo
            ? referrerWindow.browser.referrerInfo.referrerPolicy
            : Ci.nsIReferrerInfo.EMPTY;

          referrerInfo = new ReferrerInfo(
            referrerPolicy,
            true,
            referrerWindow.browser.documentURI
          );
        } else {
          try {
            parsedUri = Services.io.newURI(uri);
            if (
              parsedUri.schemeIs("about") ||
              parsedUri.schemeIs("data") ||
              parsedUri.schemeIs("file") ||
              parsedUri.schemeIs("resource") ||
              parsedUri.schemeIs("moz-extension")
            ) {
              // Only allow privileged loading for certain URIs.
              triggeringPrincipal = Services.scriptSecurityManager.createContentPrincipal(
                parsedUri,
                {}
              );
            }
          } catch (ignored) {}

          referrerInfo = createReferrerInfo(referrerUri);
        }

        if (!triggeringPrincipal) {
          triggeringPrincipal = Services.scriptSecurityManager.createNullPrincipal(
            {}
          );
        }

        let additionalHeaders = null;
        if (headers) {
          // Filter out request headers as per discussion in Bug #1567549
          // CONNECTION: Used by Gecko to manage connections
          // HOST: Relates to how gecko will ultimately interpret the resulting resource as that
          //       determines the effective request URI
          const badHeaders = ["connection", "host"];
          additionalHeaders = "";
          headers.forEach(entry => {
            const key = entry
              .split(/:/)[0]
              .toLowerCase()
              .trim();

            if (!badHeaders.includes(key)) {
              additionalHeaders += entry + "\r\n";
            }
          });
          if (additionalHeaders != "") {
            additionalHeaders = E10SUtils.makeInputStream(additionalHeaders);
          } else {
            additionalHeaders = null;
          }
        }

        // For any navigation here, we should have an appropriate triggeringPrincipal:
        //
        // 1) If we have a referring session, triggeringPrincipal is the contentPrincipal from the
        //    referring document.
        // 2) For certain URI schemes listed above, we will have a codebase principal.
        // 3) In all other cases, we create a NullPrincipal.
        //
        // The navigation flags are driven by the app. We purposely do not propagate these from
        // the referring document, but expect that the app will in most cases.
        //
        // The referrerInfo is derived from the referring document, if present, by propagating any
        // referrer policy. If we only have the referrerUri from the app, we create a referrerInfo
        // with the specified URI and no policy set. If no referrerUri is present and we have no
        // referring session, the referrerInfo is null.
        //
        // csp is only present if we have a referring document, null otherwise.
        this.loadURI({
          uri: parsedUri ? parsedUri.spec : uri,
          flags: navFlags,
          referrerInfo,
          triggeringPrincipal,
          headers: additionalHeaders,
          csp,
        });
        break;
      case "GeckoView:Reload":
        // At the moment, GeckoView only supports one reload, which uses
        // nsIWebNavigation.LOAD_FLAGS_NONE flag, and the telemetry doesn't
        // do anything to differentiate reloads (i.e normal vs skip caches)
        // So whenever we add more reload methods, please make sure the
        // telemetry probe is adjusted
        this.browser.reloadWithFlags(convertFlags(aData.flags));
        break;
      case "GeckoView:Stop":
        this.browser.stop();
        break;
      case "GeckoView:PurgeHistory":
        this.browser.purgeSessionHistory();
        break;
    }
  }

  async loadURI({
    uri,
    flags,
    referrerInfo,
    triggeringPrincipal,
    headers,
    csp,
  }) {
    if (!this.moduleManager.shouldLoadInThisProcess(uri)) {
      referrerInfo = E10SUtils.serializeReferrerInfo(referrerInfo);
      triggeringPrincipal = E10SUtils.serializePrincipal(triggeringPrincipal);
      csp = E10SUtils.serializeCSP(csp);

      this.moduleManager.updateRemoteAndNavigate(uri, {
        referrerInfo,
        triggeringPrincipal,
        headers,
        csp,
        flags,
        uri,
      });
      return;
    }

    this.browser.loadURI(uri, {
      flags,
      referrerInfo,
      triggeringPrincipal,
      csp,
      headers,
    });
  }

  waitAndSetupWindow(aSessionId, aOpenWindowInfo) {
    if (!aSessionId) {
      return Promise.resolve(null);
    }

    return new Promise(resolve => {
      const handler = {
        observe(aSubject, aTopic, aData) {
          if (
            aTopic === "geckoview-window-created" &&
            aSubject.name === aSessionId
          ) {
            // This value will be read by nsFrameLoader while it is being initialized.
            aSubject.browser.openWindowInfo = aOpenWindowInfo;

            if (
              !aOpenWindowInfo.isRemote &&
              aSubject.browser.hasAttribute("remote")
            ) {
              // We cannot start in remote mode when we have an opener.
              aSubject.browser.setAttribute("remote", "false");
              aSubject.browser.removeAttribute("remoteType");
            }
            Services.obs.removeObserver(handler, "geckoview-window-created");
            resolve(aSubject);
          }
        },
      };

      // This event is emitted from createBrowser() in geckoview.js
      Services.obs.addObserver(handler, "geckoview-window-created");
    });
  }

  handleNewSession(aUri, aOpenWindowInfo, aWhere, aFlags) {
    debug`handleNewSession: uri=${aUri && aUri.spec}
                             where=${aWhere} flags=${aFlags}`;

    if (!this.enabled) {
      return null;
    }

    const message = {
      type: "GeckoView:OnNewSession",
      uri: aUri ? aUri.displaySpec : "",
    };

    let browser = undefined;
    this.eventDispatcher
      .sendRequestForResult(message)
      .then(sessionId => {
        return this.waitAndSetupWindow(sessionId, aOpenWindowInfo);
      })
      .then(
        window => {
          browser = (window && window.browser) || null;
        },
        () => {
          browser = null;
        }
      );

    // Wait indefinitely for app to respond with a browser or null
    Services.tm.spinEventLoopUntil(
      () => this.window.closed || browser !== undefined
    );
    return browser || null;
  }

  // nsIBrowserDOMWindow.
  createContentWindow(
    aUri,
    aOpenWindowInfo,
    aWhere,
    aFlags,
    aTriggeringPrincipal,
    aCsp
  ) {
    debug`createContentWindow: uri=${aUri && aUri.spec}
                                where=${aWhere} flags=${aFlags}`;

    if (
      LoadURIDelegate.load(
        this.window,
        this.eventDispatcher,
        aUri,
        aWhere,
        aFlags,
        aTriggeringPrincipal
      )
    ) {
      // The app has handled the load, abort open-window handling.
      Components.returnCode = Cr.NS_ERROR_ABORT;
      return null;
    }

    const browser = this.handleNewSession(
      aUri,
      aOpenWindowInfo,
      aWhere,
      aFlags,
      null
    );
    if (!browser) {
      Components.returnCode = Cr.NS_ERROR_ABORT;
      return null;
    }

    return browser.browsingContext;
  }

  // nsIBrowserDOMWindow.
  createContentWindowInFrame(aUri, aParams, aWhere, aFlags, aName) {
    debug`createContentWindowInFrame: uri=${aUri && aUri.spec}
                                       where=${aWhere} flags=${aFlags}
                                       name=${aName}`;

    if (
      LoadURIDelegate.load(
        this.window,
        this.eventDispatcher,
        aUri,
        aWhere,
        aFlags,
        aParams.triggeringPrincipal
      )
    ) {
      // The app has handled the load, abort open-window handling.
      Components.returnCode = Cr.NS_ERROR_ABORT;
      return null;
    }

    const browser = this.handleNewSession(
      aUri,
      aParams.openWindowInfo,
      aWhere,
      aFlags
    );
    if (!browser) {
      Components.returnCode = Cr.NS_ERROR_ABORT;
      return null;
    }

    return browser;
  }

  handleOpenUri(
    aUri,
    aOpenWindowInfo,
    aWhere,
    aFlags,
    aTriggeringPrincipal,
    aCsp,
    aReferrerInfo
  ) {
    debug`handleOpenUri: uri=${aUri && aUri.spec}
                          where=${aWhere} flags=${aFlags}`;

    if (
      LoadURIDelegate.load(
        this.window,
        this.eventDispatcher,
        aUri,
        aWhere,
        aFlags,
        aTriggeringPrincipal
      )
    ) {
      return null;
    }

    let browser = this.browser;

    if (
      aWhere === Ci.nsIBrowserDOMWindow.OPEN_NEWWINDOW ||
      aWhere === Ci.nsIBrowserDOMWindow.OPEN_NEWTAB ||
      aWhere === Ci.nsIBrowserDOMWindow.OPEN_SWITCHTAB
    ) {
      browser = this.handleNewSession(
        aUri,
        aOpenWindowInfo,
        aWhere,
        aFlags,
        aTriggeringPrincipal
      );
    }

    if (!browser) {
      // Should we throw?
      return null;
    }

    // 3) We have a new session and a browser element, load the requested URI.
    browser.loadURI(aUri.spec, {
      triggeringPrincipal: aTriggeringPrincipal,
      csp: aCsp,
      referrerInfo: aReferrerInfo,
    });
    return browser;
  }

  // nsIBrowserDOMWindow.
  openURI(aUri, aOpenWindowInfo, aWhere, aFlags, aTriggeringPrincipal, aCsp) {
    const browser = this.handleOpenUri(
      aUri,
      aOpenWindowInfo,
      aWhere,
      aFlags,
      aTriggeringPrincipal,
      aCsp,
      null
    );
    return browser && browser.browsingContext;
  }

  // nsIBrowserDOMWindow.
  openURIInFrame(aUri, aParams, aWhere, aFlags, aNextRemoteTabId, aName) {
    const browser = this.handleOpenUri(
      aUri,
      aParams.openWindowInfo,
      aWhere,
      aFlags,
      aParams.triggeringPrincipal,
      aParams.csp,
      aParams.referrerInfo
    );
    return browser;
  }

  // nsIBrowserDOMWindow.
  isTabContentWindow(aWindow) {
    return this.browser.contentWindow === aWindow;
  }

  // nsIBrowserDOMWindow.
  canClose() {
    debug`canClose`;
    return true;
  }

  onEnable() {
    debug`onEnable`;

    const flags = Ci.nsIWebProgress.NOTIFY_LOCATION;
    this.progressFilter = Cc[
      "@mozilla.org/appshell/component/browser-status-filter;1"
    ].createInstance(Ci.nsIWebProgress);
    this.progressFilter.addProgressListener(this, flags);
    this.browser.addProgressListener(this.progressFilter, flags);
  }

  onDisable() {
    debug`onDisable`;

    if (!this.progressFilter) {
      return;
    }
    this.progressFilter.removeProgressListener(this);
    this.browser.removeProgressListener(this.progressFilter);
  }

  // WebProgress event handler.
  onLocationChange(aWebProgress, aRequest, aLocationURI, aFlags) {
    debug`onLocationChange`;

    let fixedURI = aLocationURI;

    try {
      fixedURI = Services.io.createExposableURI(aLocationURI);
    } catch (ex) {}

    // We manually fire the initial about:blank messages to make sure that we
    // consistently send them so there's nothing to do here.
    const ignore = this._initialAboutBlank && fixedURI.spec === "about:blank";
    this._initialAboutBlank = false;

    if (ignore) {
      return;
    }

    const message = {
      type: "GeckoView:LocationChange",
      uri: fixedURI.displaySpec,
      canGoBack: this.browser.canGoBack,
      canGoForward: this.browser.canGoForward,
      isTopLevel: aWebProgress.isTopLevel,
    };

    this.eventDispatcher.sendRequest(message);
  }
}

const { debug, warn } = GeckoViewNavigation.initLogging("GeckoViewNavigation"); // eslint-disable-line no-unused-vars
