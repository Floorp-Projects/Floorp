/* -*- mode: js; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["BrowserUtils"];

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const lazy = {};
ChromeUtils.defineModuleGetter(
  lazy,
  "Region",
  "resource://gre/modules/Region.jsm"
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "INVALID_SHAREABLE_SCHEMES",
  "services.sync.engine.tabs.filteredSchemes",
  "",
  null,
  val => {
    return new Set(val.split("|"));
  }
);

XPCOMUtils.defineLazyGetter(lazy, "gLocalization", () => {
  return new Localization(["toolkit/global/browser-utils.ftl"], true);
});

function stringPrefToSet(prefVal) {
  return new Set(
    prefVal
      .toLowerCase()
      .split(/\s*,\s*/g) // split on commas, ignoring whitespace
      .filter(v => !!v) // discard any falsey values
  );
}

var BrowserUtils = {
  /**
   * Return or create a principal with the content of one, and the originAttributes
   * of an existing principal (e.g. on a docshell, where the originAttributes ought
   * not to change, that is, we should keep the userContextId, privateBrowsingId,
   * etc. the same when changing the principal).
   *
   * @param principal
   *        The principal whose content/null/system-ness we want.
   * @param existingPrincipal
   *        The principal whose originAttributes we want, usually the current
   *        principal of a docshell.
   * @return an nsIPrincipal that matches the content/null/system-ness of the first
   *         param, and the originAttributes of the second.
   */
  principalWithMatchingOA(principal, existingPrincipal) {
    // Don't care about system principals:
    if (principal.isSystemPrincipal) {
      return principal;
    }

    // If the originAttributes already match, just return the principal as-is.
    if (existingPrincipal.originSuffix == principal.originSuffix) {
      return principal;
    }

    let secMan = Services.scriptSecurityManager;
    if (principal.isContentPrincipal) {
      return secMan.principalWithOA(
        principal,
        existingPrincipal.originAttributes
      );
    }

    if (principal.isNullPrincipal) {
      return secMan.createNullPrincipal(existingPrincipal.originAttributes);
    }
    throw new Error(
      "Can't change the originAttributes of an expanded principal!"
    );
  },

  /**
   * Returns true if |mimeType| is text-based, or false otherwise.
   *
   * @param mimeType
   *        The MIME type to check.
   */
  mimeTypeIsTextBased(mimeType) {
    return (
      mimeType.startsWith("text/") ||
      mimeType.endsWith("+xml") ||
      mimeType.endsWith("+json") ||
      mimeType == "application/x-javascript" ||
      mimeType == "application/javascript" ||
      mimeType == "application/json" ||
      mimeType == "application/xml"
    );
  },

  /**
   * Returns true if we can show a find bar, including FAYT, for the specified
   * document location. The location must not be in a blacklist of specific
   * "about:" pages for which find is disabled.
   *
   * This can be called from the parent process or from content processes.
   */
  canFindInPage(location) {
    return (
      !location.startsWith("about:addons") &&
      !location.startsWith(
        "chrome://mozapps/content/extensions/aboutaddons.html"
      ) &&
      !location.startsWith("about:preferences")
    );
  },

  isFindbarVisible(docShell) {
    const FINDER_JSM = "resource://gre/modules/Finder.jsm";
    return (
      Cu.isModuleLoaded(FINDER_JSM) &&
      ChromeUtils.import(FINDER_JSM).Finder.isFindbarVisible(docShell)
    );
  },

  /**
   * Returns a Promise which resolves when the given observer topic has been
   * observed.
   *
   * @param {string} topic
   *        The topic to observe.
   * @param {function(nsISupports, string)} [test]
   *        An optional test function which, when called with the
   *        observer's subject and data, should return true if this is the
   *        expected notification, false otherwise.
   * @returns {Promise<object>}
   */
  promiseObserved(topic, test = () => true) {
    return new Promise(resolve => {
      let observer = (subject, topic, data) => {
        if (test(subject, data)) {
          Services.obs.removeObserver(observer, topic);
          resolve({ subject, data });
        }
      };
      Services.obs.addObserver(observer, topic);
    });
  },

  formatURIStringForDisplay(uriString) {
    try {
      return this.formatURIForDisplay(Services.io.newURI(uriString));
    } catch (ex) {
      return uriString;
    }
  },

  formatURIForDisplay(uri) {
    switch (uri.scheme) {
      case "view-source":
        let innerURI = uri.spec.substring("view-source:".length);
        return this.formatURIStringForDisplay(innerURI);
      case "http":
      // Fall through.
      case "https":
        let host = uri.displayHostPort;
        if (host.startsWith("www.")) {
          host = Services.eTLD.getSchemelessSite(uri);
        }
        return host;
      case "about":
        return "about:" + uri.filePath;
      case "blob":
        try {
          let url = new URL(uri.specIgnoringRef);
          // _If_ we find a non-null origin, report that.
          if (url.origin && url.origin != "null") {
            return this.formatURIStringForDisplay(url.origin);
          }
          // otherwise, fall through...
        } catch (ex) {
          Cu.reportError(
            "Invalid blob URI passed to formatURIForDisplay: " + ex
          );
        }
      /* For blob URIs without an origin, fall through and use the data URI
       * logic (shows just "(data)", localized). */
      case "data":
        return lazy.gLocalization.formatValueSync("browser-utils-url-data");
      case "chrome":
      case "resource":
      case "jar":
      case "file":
      default:
        try {
          let url = uri.QueryInterface(Ci.nsIURL);
          // Just the filename if we have one:
          if (url.fileName) {
            return url.fileName;
          }
          // We won't get a filename for a path that looks like:
          // /foo/bar/baz/
          // So try the directory name:
          if (url.directory) {
            let parts = url.directory.split("/");
            // Pop off any empty bits at the end:
            let last;
            while (!last && parts.length) {
              last = parts.pop();
            }
            if (last) {
              return last;
            }
          }
        } catch (ex) {
          Cu.reportError(ex);
        }
    }
    return uri.asciiHost || uri.spec;
  },

  isShareableURL(url) {
    if (!url) {
      return false;
    }

    // Disallow sharing URLs with more than 65535 characters.
    if (url.spec.length > 65535) {
      return false;
    }
    // Use the same preference as synced tabs to disable what kind
    // of tabs we can send to another device
    return !lazy.INVALID_SHAREABLE_SCHEMES.has(url.scheme);
  },

  /**
   * Extracts linkNode and href for a click event.
   *
   * @param event
   *        The click event.
   * @return [href, linkNode, linkPrincipal].
   *
   * @note linkNode will be null if the click wasn't on an anchor
   *       element. This includes SVG links, because callers expect |node|
   *       to behave like an <a> element, which SVG links (XLink) don't.
   */
  hrefAndLinkNodeForClickEvent(event) {
    // We should get a window off the event, and bail if not:
    let content = event.view || event.composedTarget?.ownerGlobal;
    if (!content?.HTMLAnchorElement) {
      return null;
    }
    function isHTMLLink(aNode) {
      // Be consistent with what nsContextMenu.js does.
      return (
        (content.HTMLAnchorElement.isInstance(aNode) && aNode.href) ||
        (content.HTMLAreaElement.isInstance(aNode) && aNode.href) ||
        content.HTMLLinkElement.isInstance(aNode)
      );
    }

    let node = event.composedTarget;
    while (node && !isHTMLLink(node)) {
      node = node.flattenedTreeParentNode;
    }

    if (node) {
      return [node.href, node, node.ownerDocument.nodePrincipal];
    }

    // If there is no linkNode, try simple XLink.
    let href, baseURI;
    node = event.composedTarget;
    while (node && !href) {
      if (
        node.nodeType == content.Node.ELEMENT_NODE &&
        (node.localName == "a" ||
          node.namespaceURI == "http://www.w3.org/1998/Math/MathML")
      ) {
        href =
          node.getAttribute("href") ||
          node.getAttributeNS("http://www.w3.org/1999/xlink", "href");
        if (href) {
          baseURI = node.ownerDocument.baseURIObject;
          break;
        }
      }
      node = node.flattenedTreeParentNode;
    }

    // In case of XLink, we don't return the node we got href from since
    // callers expect <a>-like elements.
    // Note: makeURI() will throw if aUri is not a valid URI.
    return [
      href ? Services.io.newURI(href, null, baseURI).spec : null,
      null,
      node && node.ownerDocument.nodePrincipal,
    ];
  },

  /**
   * whereToOpenLink() looks at an event to decide where to open a link.
   *
   * The event may be a mouse event (click, double-click, middle-click) or keypress event (enter).
   *
   * On Windows, the modifiers are:
   * Ctrl        new tab, selected
   * Shift       new window
   * Ctrl+Shift  new tab, in background
   * Alt         save
   *
   * Middle-clicking is the same as Ctrl+clicking (it opens a new tab).
   *
   * Exceptions:
   * - Alt is ignored for menu items selected using the keyboard so you don't accidentally save stuff.
   *    (Currently, the Alt isn't sent here at all for menu items, but that will change in bug 126189.)
   * - Alt is hard to use in context menus, because pressing Alt closes the menu.
   * - Alt can't be used on the bookmarks toolbar because Alt is used for "treat this as something draggable".
   * - The button is ignored for the middle-click-paste-URL feature, since it's always a middle-click.
   *
   * @param e {Event|Object} Event or JSON Object
   * @param ignoreButton {Boolean}
   * @param ignoreAlt {Boolean}
   * @returns {"current" | "tabshifted" | "tab" | "save" | "window"}
   */
  whereToOpenLink(e, ignoreButton, ignoreAlt) {
    // This method must treat a null event like a left click without modifier keys (i.e.
    // e = { shiftKey:false, ctrlKey:false, metaKey:false, altKey:false, button:0 })
    // for compatibility purposes.
    if (!e) {
      return "current";
    }

    e = this.getRootEvent(e);

    var shift = e.shiftKey;
    var ctrl = e.ctrlKey;
    var meta = e.metaKey;
    var alt = e.altKey && !ignoreAlt;

    // ignoreButton allows "middle-click paste" to use function without always opening in a new window.
    let middle = !ignoreButton && e.button == 1;
    let middleUsesTabs = Services.prefs.getBoolPref(
      "browser.tabs.opentabfor.middleclick",
      true
    );
    let middleUsesNewWindow = Services.prefs.getBoolPref(
      "middlemouse.openNewWindow",
      false
    );

    // Don't do anything special with right-mouse clicks.  They're probably clicks on context menu items.

    var metaKey = AppConstants.platform == "macosx" ? meta : ctrl;
    if (metaKey || (middle && middleUsesTabs)) {
      return shift ? "tabshifted" : "tab";
    }

    if (alt && Services.prefs.getBoolPref("browser.altClickSave", false)) {
      return "save";
    }

    if (shift || (middle && !middleUsesTabs && middleUsesNewWindow)) {
      return "window";
    }

    return "current";
  },

  // Utility function to check command events for potential middle-click events
  // from checkForMiddleClick and unwrap them.
  getRootEvent(aEvent) {
    // Part of the fix for Bug 1523813.
    // Middle-click events arrive here wrapped in different numbers (1-2) of
    // command events, depending on the button originally clicked.
    if (!aEvent) {
      return aEvent;
    }
    let tempEvent = aEvent;
    while (tempEvent.sourceEvent) {
      if (tempEvent.sourceEvent.button == 1) {
        aEvent = tempEvent.sourceEvent;
        break;
      }
      tempEvent = tempEvent.sourceEvent;
    }
    return aEvent;
  },

  /**
   * An enumeration of the promotion types that can be passed to shouldShowPromo
   */
  PromoType: {
    DEFAULT: 0, // invalid
    VPN: 1,
    RALLY: 2,
    FOCUS: 3,
    PIN: 4,
  },

  /**
   * Should a given promo be shown to the user now, based on things including:
   *
   *  current region
   *  home region
   *  where ads for a particular thing are allowed
   *  where they are illegal
   *  in what regions is the thing being promoted supported?
   *  whether there is an active enterprise policy
   *  settings of specific preferences related to this promo
   *
   * @param {BrowserUtils.PromoType} promoType - What promo are we checking on?
   *
   * @return {boolean} - should we display this promo now or not?
   */
  shouldShowPromo(promoType) {
    switch (promoType) {
      case this.PromoType.VPN:
        return this._shouldShowPromoInternal(promoType);
      case this.PromoType.RALLY:
        return this._shouldShowRallyPromo();
      case this.PromoType.FOCUS:
        return this._shouldShowPromoInternal(promoType);
      case this.PromoType.PIN:
        return this._shouldShowPromoInternal(promoType);
      default:
        throw new Error("Unknown promo type: ", promoType);
    }
  },

  /**
   * @deprecated in favor of shouldShowPromo
   */
  shouldShowVPNPromo() {
    return this._shouldShowPromoInternal(this.PromoType.VPN);
  },

  _shouldShowPromoInternal(promoType) {
    const info = PromoInfo[promoType];
    const promoEnabled = Services.prefs.getBoolPref(info.enabledPref, true);

    const homeRegion = lazy.Region.home || "";
    const currentRegion = lazy.Region.current || "";

    let inSupportedRegion = true;
    if ("supportedRegions" in info.lazyStringSetPrefs) {
      const supportedRegions =
        info.lazyStringSetPrefs.supportedRegions.lazyValue;
      inSupportedRegion =
        supportedRegions.has(currentRegion.toLowerCase()) ||
        supportedRegions.has(homeRegion.toLowerCase());
    }

    const avoidAdsRegions =
      info.lazyStringSetPrefs.disallowedRegions?.lazyValue;

    // Don't show promo if there's an active enterprise policy
    const noActivePolicy =
      !Services.policies ||
      Services.policies.status !== Services.policies.ACTIVE;

    return (
      promoEnabled &&
      !avoidAdsRegions?.has(homeRegion.toLowerCase()) &&
      !avoidAdsRegions?.has(currentRegion.toLowerCase()) &&
      !info.illegalRegions.includes(homeRegion.toLowerCase()) &&
      !info.illegalRegions.includes(currentRegion.toLowerCase()) &&
      inSupportedRegion &&
      noActivePolicy
    );
  },

  shouldShowRallyPromo() {
    const homeRegion = lazy.Region.home || "";
    const currentRegion = lazy.Region.current || "";
    const region = currentRegion || homeRegion;
    const language = Services.locale.appLocaleAsBCP47;

    return language.startsWith("en-") && region.toLowerCase() == "us";
  },

  // Return true if Send to Device emails are supported for user's locale
  sendToDeviceEmailsSupported() {
    const userLocale = Services.locale.appLocaleAsBCP47.toLowerCase();
    return this.emailSupportedLocales.has(userLocale);
  },
};

/**
 * A table of promos used by  _shouldShowPromoInternal to decide whether or not to
 * show. Each entry defines the criteria for a given promo, and also houses lazy
 * getters for specified string set preferences.
 */
let PromoInfo = {
  [BrowserUtils.PromoType.VPN]: {
    enabledPref: "browser.vpn_promo.enabled",
    lazyStringSetPrefs: {
      supportedRegions: {
        name: "browser.contentblocking.report.vpn_region",
        default: "us,ca,nz,sg,my,gb,de,fr",
      },
      disallowedRegions: {
        name: "browser.vpn_promo.disallowed_regions",
        default: "ae,by,cn,cu,iq,ir,kp,om,ru,sd,sy,tm,tr,ua",
      },
    },
    illegalRegions: ["cn", "kp", "tm"],
  },
  [BrowserUtils.PromoType.FOCUS]: {
    enabledPref: "browser.promo.focus.enabled",
    lazyStringSetPrefs: {
      // there are no particular limitions to where it is "supported",
      // so we leave out the supported pref
      disallowedRegions: {
        name: "browser.promo.focus.disallowed_regions",
        default: "cn",
      },
    },
    illegalRegions: ["cn"],
  },
  [BrowserUtils.PromoType.PIN]: {
    enabledPref: "browser.promo.pin.enabled",
    lazyStringSetPrefs: {},
    illegalRegions: [],
  },
};

/*
 * Finish setting up the PromoInfo data structure by attaching lazy prefs getters
 * as specified in the structure. (the object for each pref in the lazyStringSetPrefs
 * gets a `lazyValue` property attached to it).
 */
for (let promo of Object.values(PromoInfo)) {
  for (let prefObj of Object.values(promo.lazyStringSetPrefs)) {
    XPCOMUtils.defineLazyPreferenceGetter(
      prefObj,
      "lazyValue",
      prefObj.name,
      prefObj.default,
      null,
      stringPrefToSet
    );
  }
}

XPCOMUtils.defineLazyPreferenceGetter(
  BrowserUtils,
  "navigationRequireUserInteraction",
  "browser.navigation.requireUserInteraction",
  false
);

XPCOMUtils.defineLazyPreferenceGetter(
  BrowserUtils,
  "emailSupportedLocales",
  "browser.send_to_device_locales",
  "de,en-GB,en-US,es-AR,es-CL,es-ES,es-MX,fr,id,pl,pt-BR,ru,zh-TW",
  null,
  stringPrefToSet
);
