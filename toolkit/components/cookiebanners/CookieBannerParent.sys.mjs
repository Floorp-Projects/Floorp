/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "serviceMode",
  "cookiebanners.service.mode",
  Ci.nsICookieBannerService.MODE_DISABLED
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "serviceModePBM",
  "cookiebanners.service.mode.privateBrowsing",
  Ci.nsICookieBannerService.MODE_DISABLED
);

export class CookieBannerParent extends JSWindowActorParent {
  /**
   * Get the browser associated with this window which is the top level embedder
   * element. Returns null if the top embedder isn't a browser.
   */
  get #browserElement() {
    let topBC = this.browsingContext.top;

    // Not all embedders are browsers.
    if (topBC.embedderElementType != "browser") {
      return null;
    }

    return topBC.embedderElement;
  }

  #isPrivateBrowsing() {
    let browser = this.#browserElement;
    if (!browser) {
      return false;
    }
    return lazy.PrivateBrowsingUtils.isBrowserPrivate(browser);
  }

  /**
   * Dispatches a custom "cookiebannerhandled" event on the chrome window.
   */
  #notifyCookieBannerState(eventType) {
    let chromeWin = this.browsingContext.topChromeWindow;
    if (!chromeWin) {
      return;
    }
    let windowUtils = chromeWin.windowUtils;
    if (!windowUtils) {
      return;
    }
    let event = new CustomEvent(eventType, {
      bubbles: true,
      cancelable: false,
      detail: {
        windowContext: this.manager,
      },
    });
    windowUtils.dispatchEventToChromeOnly(chromeWin, event);
  }

  async receiveMessage(message) {
    if (message.name == "CookieBanner::Test-FinishClicking") {
      Services.obs.notifyObservers(
        null,
        "cookie-banner-test-clicking-finish",
        this.manager.documentPrincipal?.baseDomain
      );
      return undefined;
    }

    // Forwards cookie banner detected signals to frontend consumers.
    if (message.name == "CookieBanner::DetectedBanner") {
      this.#notifyCookieBannerState("cookiebannerdetected");
      return undefined;
    }

    // Forwards cookie banner handled signals to frontend consumers.
    if (message.name == "CookieBanner::HandledBanner") {
      this.#notifyCookieBannerState("cookiebannerhandled");
      return undefined;
    }

    if (message.name != "CookieBanner::GetClickRules") {
      return undefined;
    }

    // TODO: Bug 1790688: consider moving this logic to the cookie banner service.
    let mode;
    let isPrivateBrowsing = this.#isPrivateBrowsing();
    if (isPrivateBrowsing) {
      mode = lazy.serviceModePBM;
    } else {
      mode = lazy.serviceMode;
    }

    // Check if we have a site preference of the top-level URI. If so, it
    // takes precedence over the pref setting.
    let topBrowsingContext = this.manager.browsingContext.top;
    let topURI = topBrowsingContext.currentWindowGlobal?.documentURI;

    // We don't need to check the domain preference if the cookie banner
    // handling was disabled by pref.
    if (mode != Ci.nsICookieBannerService.MODE_DISABLED && topURI) {
      try {
        let perDomainMode = Services.cookieBanners.getDomainPref(
          topURI,
          isPrivateBrowsing
        );

        if (perDomainMode != Ci.nsICookieBannerService.MODE_UNSET) {
          mode = perDomainMode;
        }
      } catch (e) {
        // getPerSitePref could throw with NS_ERROR_NOT_AVAILABLE if the service
        // is disabled. We will fallback to global pref setting if any errors
        // occur.
        if (e.result == Cr.NS_ERROR_NOT_AVAILABLE) {
          console.error("The cookie banner handling service is not available");
        } else {
          console.error("Fail on getting domain pref:", e);
        }
      }
    }

    // Service is disabled for current context (normal or private browsing),
    // return empty array.
    if (mode == Ci.nsICookieBannerService.MODE_DISABLED) {
      return [];
    }

    let domain = this.manager.documentPrincipal?.baseDomain;

    if (!domain) {
      return [];
    }

    let isTopLevel = !this.manager.browsingContext.parent;
    let rules = Services.cookieBanners.getClickRulesForDomain(
      domain,
      isTopLevel
    );

    if (!rules.length) {
      return [];
    }

    // Determine whether we can fall back to opt-in rules. This includes the
    // detect-only mode where don't interact with the banner.
    let modeAllowsOptIn =
      mode == Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT;
    return rules.map(rule => {
      let target = rule.optOut;

      if (modeAllowsOptIn && !target) {
        target = rule.optIn;
      }
      return {
        hide: rule.hide ?? rule.presence,
        presence: rule.presence,
        skipPresenceVisibilityCheck: rule.skipPresenceVisibilityCheck,
        target,
      };
    });
  }
}
