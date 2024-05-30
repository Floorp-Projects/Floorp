/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { GeckoViewModule } from "resource://gre/modules/GeckoViewModule.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  GeckoViewUtils: "resource://gre/modules/GeckoViewUtils.sys.mjs",
  E10SUtils: "resource://gre/modules/E10SUtils.sys.mjs",
  LoadURIDelegate: "resource://gre/modules/LoadURIDelegate.sys.mjs",
  TranslationsParent: "resource://gre/actors/TranslationsParent.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "ReferrerInfo", () =>
  Components.Constructor(
    "@mozilla.org/referrer-info;1",
    "nsIReferrerInfo",
    "init"
  )
);

// Filter out request headers as per discussion in Bug #1567549
// CONNECTION: Used by Gecko to manage connections
// HOST: Relates to how gecko will ultimately interpret the resulting resource as that
//       determines the effective request URI
const BAD_HEADERS = ["connection", "host"];

// Headers use |\r\n| as separator so these characters cannot appear
// in the header name or value
const FORBIDDEN_HEADER_CHARACTERS = ["\n", "\r"];

// Keep in sync with GeckoSession.java
const HEADER_FILTER_CORS_SAFELISTED = 1;
// eslint-disable-next-line no-unused-vars
const HEADER_FILTER_UNRESTRICTED_UNSAFE = 2;

// Create default ReferrerInfo instance for the given referrer URI string.
const createReferrerInfo = aReferrer => {
  let referrerUri;
  try {
    referrerUri = Services.io.newURI(aReferrer);
  } catch (ignored) {}

  return new lazy.ReferrerInfo(Ci.nsIReferrerInfo.EMPTY, true, referrerUri);
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
  if (aFlags & (1 << 7)) {
    navFlags |= Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_LOAD_URI_DELEGATE;
  }
  return navFlags;
}

// Handles navigation requests between Gecko and a GeckoView.
// Handles GeckoView:GoBack and :GoForward requests dispatched by
// GeckoView.goBack and .goForward.
// Dispatches GeckoView:LocationChange to the GeckoView on location change when
// active.
// Implements nsIBrowserDOMWindow.
export class GeckoViewNavigation extends GeckoViewModule {
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
      "GeckoView:DotPrintFinish",
    ]);

    this._initialAboutBlank = true;
  }

  validateHeader(key, value, filter) {
    if (!key) {
      // Key cannot be empty
      return false;
    }

    for (const c of FORBIDDEN_HEADER_CHARACTERS) {
      if (key.includes(c) || value?.includes(c)) {
        return false;
      }
    }

    if (BAD_HEADERS.includes(key.toLowerCase().trim())) {
      return false;
    }

    if (
      filter == HEADER_FILTER_CORS_SAFELISTED &&
      !this.window.windowUtils.isCORSSafelistedRequestHeader(key, value)
    ) {
      return false;
    }

    return true;
  }

  // Bundle event handler.
  async onEvent(aEvent, aData) {
    debug`onEvent: event=${aEvent}, data=${aData}`;

    switch (aEvent) {
      case "GeckoView:GoBack":
        this.browser.goBack(aData.userInteraction);
        break;
      case "GeckoView:GoForward":
        this.browser.goForward(aData.userInteraction);
        break;
      case "GeckoView:GotoHistoryIndex":
        this.browser.gotoIndex(aData.index);
        break;
      case "GeckoView:LoadUri":
        const {
          uri,
          referrerUri,
          referrerSessionId,
          flags,
          headers,
          headerFilter,
        } = aData;

        let navFlags = convertFlags(flags);
        // For performance reasons we don't call the LoadUriDelegate.loadUri
        // from Gecko, and instead we call it directly in the loadUri Java API.
        navFlags |= Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_LOAD_URI_DELEGATE;

        let triggeringPrincipal, referrerInfo, csp;
        if (referrerSessionId) {
          const referrerWindow = Services.ww.getWindowByName(referrerSessionId);
          triggeringPrincipal = referrerWindow.browser.contentPrincipal;
          csp = referrerWindow.browser.csp;

          const { contentPrincipal } = this.browser;
          const isNormal = contentPrincipal.privateBrowsingId == 0;
          const referrerIsPrivate = triggeringPrincipal.privateBrowsingId != 0;

          const referrerPolicy = referrerWindow.browser.referrerInfo
            ? referrerWindow.browser.referrerInfo.referrerPolicy
            : Ci.nsIReferrerInfo.EMPTY;

          referrerInfo = new lazy.ReferrerInfo(
            referrerPolicy,
            // Don't `sendReferrer` if the private session (current) is opened by a normal session (referrer)
            isNormal || referrerIsPrivate,
            referrerWindow.browser.documentURI
          );
        } else if (referrerUri) {
          referrerInfo = createReferrerInfo(referrerUri);
        } else {
          // External apps are treated like web pages, so they should not get
          // a privileged principal.
          const isExternal =
            navFlags & Ci.nsIWebNavigation.LOAD_FLAGS_FROM_EXTERNAL;
          if (!isExternal || Services.io.newURI(uri).schemeIs("content")) {
            // Always use the system principal as the triggering principal
            // for user-initiated (ie. no referrer session and not external)
            // loads. See discussion in bug 1573860.
            triggeringPrincipal =
              Services.scriptSecurityManager.getSystemPrincipal();
          }
        }

        if (!triggeringPrincipal) {
          triggeringPrincipal =
            Services.scriptSecurityManager.createNullPrincipal({});
        }

        let additionalHeaders = null;
        if (headers) {
          additionalHeaders = "";
          for (const [key, value] of Object.entries(headers)) {
            if (!this.validateHeader(key, value, headerFilter)) {
              console.error(`Ignoring invalid header '${key}'='${value}'.`);
              continue;
            }

            additionalHeaders += `${key}:${value ?? ""}\r\n`;
          }

          if (additionalHeaders != "") {
            additionalHeaders =
              lazy.E10SUtils.makeInputStream(additionalHeaders);
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
        this.browser.fixupAndLoadURIString(uri, {
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
      case "GeckoView:DotPrintFinish":
        var printActor = this.moduleManager.getActor("GeckoViewPrintDelegate");
        printActor.clearStaticClone();
        break;
    }
  }

  waitAndSetupWindow(aSessionId, aOpenWindowInfo, aName) {
    if (!aSessionId) {
      return Promise.reject();
    }

    return new Promise((resolve, reject) => {
      const handler = {
        observe(aSubject, aTopic) {
          if (
            aTopic === "geckoview-window-created" &&
            aSubject.name === aSessionId
          ) {
            // This value will be read by nsFrameLoader while it is being initialized.
            aSubject.browser.openWindowInfo = aOpenWindowInfo;

            // Gecko will use this attribute to set the name of the opened window.
            if (aName) {
              aSubject.browser.setAttribute("name", aName);
            }

            if (
              !aOpenWindowInfo.isRemote &&
              aSubject.browser.hasAttribute("remote")
            ) {
              // We cannot start in remote mode when we have an opener.
              aSubject.browser.setAttribute("remote", "false");
              aSubject.browser.removeAttribute("remoteType");
            }
            Services.obs.removeObserver(handler, "geckoview-window-created");
            if (!aSubject) {
              reject();
              return;
            }
            resolve(aSubject);
          }
        },
      };

      // This event is emitted from createBrowser() in geckoview.js
      Services.obs.addObserver(handler, "geckoview-window-created");
    });
  }

  handleNewSession(aUri, aOpenWindowInfo, aWhere, aFlags, aName) {
    debug`handleNewSession: uri=${aUri && aUri.spec}
                             where=${aWhere} flags=${aFlags}`;

    const setupPromise = this.#handleNewSessionAsync(
      aUri,
      aOpenWindowInfo,
      aFlags,
      aName
    );

    let browser = undefined;
    setupPromise.then(
      window => {
        browser = window.browser;
      },
      () => {
        browser = null;
      }
    );

    // Wait indefinitely for app to respond with a browser or null
    Services.tm.spinEventLoopUntil(
      "GeckoViewNavigation.jsm:handleNewSession",
      () => this.window.closed || browser !== undefined
    );
    return browser || null;
  }

  #isNewTab(aWhere) {
    return [
      Ci.nsIBrowserDOMWindow.OPEN_NEWTAB,
      Ci.nsIBrowserDOMWindow.OPEN_NEWTAB_BACKGROUND,
    ].includes(aWhere);
  }

  /**
   * Similar to handleNewSession. But this returns a promise to wait for new
   * browser.
   */
  #handleNewSessionAsync(aUri, aOpenWindowInfo, aFlags, aName) {
    if (!this.enabled) {
      return Promise.reject();
    }

    const newSessionId = Services.uuid
      .generateUUID()
      .toString()
      .slice(1, -1)
      .replace(/-/g, "");

    const message = {
      type: "GeckoView:OnNewSession",
      uri: aUri ? aUri.displaySpec : "",
      newSessionId,
    };

    // The window might be already open by the time we get the response from
    // the Java layer, so we need to start waiting before sending the message.
    const setupPromise = this.waitAndSetupWindow(
      newSessionId,
      aOpenWindowInfo,
      aName
    );

    return this.eventDispatcher
      .sendRequestForResult(message)
      .then(didOpenSession => {
        if (!didOpenSession) {
          // New session cannot be opened, so we should throw NS_ERROR_ABORT.
          return Promise.reject();
        }
        return setupPromise;
      });
  }

  // nsIBrowserDOMWindow.
  createContentWindow(
    aUri,
    aOpenWindowInfo,
    aWhere,
    aFlags,
    aTriggeringPrincipal
  ) {
    debug`createContentWindow: uri=${aUri && aUri.spec}
                                where=${aWhere} flags=${aFlags}`;

    if (!this.enabled) {
      Components.returnCode = Cr.NS_ERROR_ABORT;
      return null;
    }

    if (
      lazy.LoadURIDelegate.load(
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

    const newTab = this.#isNewTab(aWhere);
    const promise = this.#handleNewSessionAsync(
      aUri,
      aOpenWindowInfo,
      aWhere,
      aFlags,
      null
    );

    // Actually, GeckoView's createContentWindow always creates new window even
    // if OPEN_NEWTAB. So the browsing context will be observed via
    // nsFrameLoader.
    if (aOpenWindowInfo && !newTab) {
      promise.catch(() => {
        aOpenWindowInfo.cancel();
      });
      // If nsIOpenWindowInfo isn't null, caller should use the callback.
      // Also, nsIWindowProvider.provideWindow doesn't use callback, if new
      // tab option, we have to return browsing context instead of async.
      return null;
    }

    let browser = undefined;
    promise.then(
      window => {
        browser = window.browser;
      },
      () => {
        browser = null;
      }
    );

    // Wait indefinitely for app to respond with a browser or null.
    // if browser is null, return error.
    Services.tm.spinEventLoopUntil(
      "GeckoViewNavigation.sys.mjs:createContentWindow",
      () => this.window.closed || browser !== undefined
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

    if (aWhere === Ci.nsIBrowserDOMWindow.OPEN_PRINT_BROWSER) {
      return this.window.moduleManager.onPrintWindow(aParams);
    }

    if (
      lazy.LoadURIDelegate.load(
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
      aFlags,
      aName
    );
    if (!browser) {
      Components.returnCode = Cr.NS_ERROR_ABORT;
      return null;
    }

    return browser;
  }

  handleOpenUri({
    uri,
    openWindowInfo,
    where,
    flags,
    triggeringPrincipal,
    csp,
    referrerInfo = null,
    name = null,
  }) {
    debug`handleOpenUri: uri=${uri && uri.spec}
                          where=${where} flags=${flags}`;

    if (
      lazy.LoadURIDelegate.load(
        this.window,
        this.eventDispatcher,
        uri,
        where,
        flags,
        triggeringPrincipal
      )
    ) {
      return null;
    }

    let browser = this.browser;

    if (
      where === Ci.nsIBrowserDOMWindow.OPEN_NEWWINDOW ||
      where === Ci.nsIBrowserDOMWindow.OPEN_NEWTAB ||
      where === Ci.nsIBrowserDOMWindow.OPEN_NEWTAB_BACKGROUND
    ) {
      browser = this.handleNewSession(uri, openWindowInfo, where, flags, name);
    }

    if (!browser) {
      // Should we throw?
      return null;
    }

    // 3) We have a new session and a browser element, load the requested URI.
    browser.loadURI(uri, {
      triggeringPrincipal,
      csp,
      referrerInfo,
    });
    return browser;
  }

  // nsIBrowserDOMWindow.
  openURI(aUri, aOpenWindowInfo, aWhere, aFlags, aTriggeringPrincipal, aCsp) {
    const browser = this.handleOpenUri({
      uri: aUri,
      openWindowInfo: aOpenWindowInfo,
      where: aWhere,
      flags: aFlags,
      triggeringPrincipal: aTriggeringPrincipal,
      csp: aCsp,
    });
    return browser && browser.browsingContext;
  }

  // nsIBrowserDOMWindow.
  openURIInFrame(aUri, aParams, aWhere, aFlags, aName) {
    const browser = this.handleOpenUri({
      uri: aUri,
      openWindowInfo: aParams.openWindowInfo,
      where: aWhere,
      flags: aFlags,
      triggeringPrincipal: aParams.triggeringPrincipal,
      csp: aParams.csp,
      referrerInfo: aParams.referrerInfo,
      name: aName,
    });
    return browser;
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

  serializePermission({ type, capability, principal }) {
    const { URI, originAttributes, privateBrowsingId } = principal;
    return {
      uri: Services.io.createExposableURI(URI).displaySpec,
      principal: lazy.E10SUtils.serializePrincipal(principal),
      perm: type,
      value: capability,
      contextId: originAttributes.geckoViewSessionContextId,
      privateMode: privateBrowsingId != 0,
    };
  }

  // WebProgress event handler.
  onLocationChange(aWebProgress, aRequest, aLocationURI) {
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

    const { contentPrincipal } = this.browser;
    let permissions;
    if (
      contentPrincipal &&
      lazy.GeckoViewUtils.isSupportedPermissionsPrincipal(contentPrincipal)
    ) {
      let rawPerms = [];
      try {
        rawPerms = Services.perms.getAllForPrincipal(contentPrincipal);
      } catch (ex) {
        warn`Could not get permissions for principal. ${ex}`;
      }
      permissions = rawPerms.map(this.serializePermission);

      // The only way for apps to set permissions is to get hold of an existing
      // permission and change its value.
      // Tracking protection exception permissions are only present when
      // explicitly added by the app, so if one is not present, we need to send
      // a DENY_ACTION tracking protection permission so that apps can use it
      // to add tracking protection exceptions.
      const trackingProtectionPermission =
        contentPrincipal.privateBrowsingId == 0
          ? "trackingprotection"
          : "trackingprotection-pb";
      if (
        contentPrincipal.isContentPrincipal &&
        rawPerms.findIndex(p => p.type == trackingProtectionPermission) == -1
      ) {
        permissions.push(
          this.serializePermission({
            type: trackingProtectionPermission,
            capability: Ci.nsIPermissionManager.DENY_ACTION,
            principal: contentPrincipal,
          })
        );
      }
    }

    const message = {
      type: "GeckoView:LocationChange",
      uri: fixedURI.displaySpec,
      canGoBack: this.browser.canGoBack,
      canGoForward: this.browser.canGoForward,
      isTopLevel: aWebProgress.isTopLevel,
      permissions,
      hasUserGesture:
        this.window.document.hasValidTransientUserGestureActivation !== null
          ? this.window.document.hasValidTransientUserGestureActivation
          : false,
    };
    lazy.TranslationsParent.onLocationChange(this.browser);
    this.eventDispatcher.sendRequest(message);
  }
}

const { debug, warn } = GeckoViewNavigation.initLogging("GeckoViewNavigation");
