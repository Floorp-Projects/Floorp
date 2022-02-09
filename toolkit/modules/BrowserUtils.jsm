/* -*- mode: js; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["BrowserUtils"];

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "Region",
  "resource://gre/modules/Region.jsm"
);

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

  isShareableURL(url) {
    if (!url) {
      return false;
    }

    // Disallow sharing URLs with more than 65535 characters.
    if (url.spec.length > 65535) {
      return false;
    }

    let scheme = url.scheme;

    return !(
      "about" == scheme ||
      "resource" == scheme ||
      "chrome" == scheme ||
      "blob" == scheme ||
      "moz-extension" == scheme
    );
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
        (aNode instanceof content.HTMLAnchorElement && aNode.href) ||
        (aNode instanceof content.HTMLAreaElement && aNode.href) ||
        aNode instanceof content.HTMLLinkElement
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

  // Returns true if user should see VPN promos
  shouldShowVPNPromo() {
    const enablePromoPref = Services.prefs.getBoolPref(
      "browser.vpn_promo.enabled"
    );
    const homeRegion = Region.home || "";
    const currentRegion = Region.current || "";
    let avoidAdsCountries = BrowserUtils.vpnDisallowedRegions;
    // Extra check for countries where VPNs are illegal and compliance is strongly enforced
    let vpnIllegalCountries = ["cn", "kp", "tm"];
    vpnIllegalCountries.forEach(country => avoidAdsCountries.add(country));

    return (
      enablePromoPref &&
      !avoidAdsCountries.has(homeRegion.toLowerCase()) &&
      !avoidAdsCountries.has(currentRegion.toLowerCase())
    );
  },

  // Returns true if user should see Rally promos
  shouldShowRallyPromo() {
    const homeRegion = Region.home || "";
    const currentRegion = Region.current || "";
    const region = currentRegion || homeRegion;
    const language = Services.locale.appLocaleAsBCP47;

    return language.startsWith("en-") && region.toLowerCase() == "us";
  },
};

XPCOMUtils.defineLazyPreferenceGetter(
  BrowserUtils,
  "navigationRequireUserInteraction",
  "browser.navigation.requireUserInteraction",
  false
);

XPCOMUtils.defineLazyPreferenceGetter(
  BrowserUtils,
  "vpnDisallowedRegions",
  "browser.vpn_promo.disallowed_regions",
  "ae,by,cn,cu,iq,ir,kp,om,ru,sd,sy,tm,tr,ua",
  null,
  prefVal =>
    new Set(
      prefVal
        .toLowerCase()
        .split(/\s*,\s*/g) // split on commas, ignoring whitespace
        .filter(v => !!v) // discard any falsey values
    )
);
