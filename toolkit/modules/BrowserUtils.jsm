/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [ "BrowserUtils" ];

const {interfaces: Ci, utils: Cu, classes: Cc} = Components;

Cu.import("resource://gre/modules/Services.jsm");

this.BrowserUtils = {

  /**
   * Prints arguments separated by a space and appends a new line.
   */
  dumpLn: function (...args) {
    for (let a of args)
      dump(a + " ");
    dump("\n");
  },

  /**
   * urlSecurityCheck: JavaScript wrapper for checkLoadURIWithPrincipal
   * and checkLoadURIStrWithPrincipal.
   * If |aPrincipal| is not allowed to link to |aURL|, this function throws with
   * an error message.
   *
   * @param aURL
   *        The URL a page has linked to. This could be passed either as a string
   *        or as a nsIURI object.
   * @param aPrincipal
   *        The principal of the document from which aURL came.
   * @param aFlags
   *        Flags to be passed to checkLoadURIStr. If undefined,
   *        nsIScriptSecurityManager.STANDARD will be passed.
   */
  urlSecurityCheck: function(aURL, aPrincipal, aFlags) {
    var secMan = Services.scriptSecurityManager;
    if (aFlags === undefined) {
      aFlags = secMan.STANDARD;
    }

    try {
      if (aURL instanceof Ci.nsIURI)
        secMan.checkLoadURIWithPrincipal(aPrincipal, aURL, aFlags);
      else
        secMan.checkLoadURIStrWithPrincipal(aPrincipal, aURL, aFlags);
    } catch (e) {
      let principalStr = "";
      try {
        principalStr = " from " + aPrincipal.URI.spec;
      }
      catch(e2) { }

      throw "Load of " + aURL + principalStr + " denied.";
    }
  },

  /**
   * Constructs a new URI, using nsIIOService.
   * @param aURL The URI spec.
   * @param aOriginCharset The charset of the URI.
   * @param aBaseURI Base URI to resolve aURL, or null.
   * @return an nsIURI object based on aURL.
   */
  makeURI: function(aURL, aOriginCharset, aBaseURI) {
    return Services.io.newURI(aURL, aOriginCharset, aBaseURI);
  },

  makeFileURI: function(aFile) {
    return Services.io.newFileURI(aFile);
  },

  /**
   * Return the current focus element and window. If the current focus
   * is in a content process, then this function returns CPOWs
   * (cross-process object wrappers) that refer to the focused
   * items. Note that calling this function synchronously contacts the
   * content process, which may block for a long time.
   *
   * @param document The document in question.
   * @return [focusedElement, focusedWindow]
   */
  getFocusSync: function(document) {
    let elt = document.commandDispatcher.focusedElement;
    var window = document.commandDispatcher.focusedWindow;

    const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
    if (elt instanceof window.XULElement &&
        elt.localName == "browser" &&
        elt.namespaceURI == XUL_NS &&
        elt.getAttribute("remote")) {
      [elt, window] = elt.syncHandler.getFocusedElementAndWindow();
    }

    return [elt, window];
  },

  /**
   * For a given DOM element, returns its position in "screen"
   * coordinates. In a content process, the coordinates returned will
   * be relative to the left/top of the tab. In the chrome process,
   * the coordinates are relative to the user's screen.
   */
  getElementBoundingScreenRect: function(aElement) {
    let rect = aElement.getBoundingClientRect();
    let window = aElement.ownerDocument.defaultView;

    // We need to compensate for any iframes that might shift things
    // over. We also need to compensate for zooming.
    let fullZoom = window.getInterface(Ci.nsIDOMWindowUtils).fullZoom;
    rect = {
      left: (rect.left + window.mozInnerScreenX) * fullZoom,
      top: (rect.top + window.mozInnerScreenY) * fullZoom,
      width: rect.width * fullZoom,
      height: rect.height * fullZoom
    };

    return rect;
  },

  /**
   * Given an element potentially within a subframe, calculate the offsets
   * up to the top level browser.
   *
   * @param aTopLevelWindow content window to calculate offsets to.
   * @param aElement The element in question.
   * @return [targetWindow, offsetX, offsetY]
   */
  offsetToTopLevelWindow: function (aTopLevelWindow, aElement) {
    let offsetX = 0;
    let offsetY = 0;
    let element = aElement;
    while (element &&
           element.ownerDocument &&
           element.ownerDocument.defaultView != aTopLevelWindow) {
      element = element.ownerDocument.defaultView.frameElement;
      let rect = element.getBoundingClientRect();
      offsetX += rect.left;
      offsetY += rect.top;
    }
    let win = null;
    if (element == aElement)
      win = aTopLevelWindow;
    else
      win = element.contentDocument.defaultView;
    return { targetWindow: win, offsetX: offsetX, offsetY: offsetY };
  },

  onBeforeLinkTraversal: function(originalTarget, linkURI, linkNode, isAppTab) {
    // Don't modify non-default targets or targets that aren't in top-level app
    // tab docshells (isAppTab will be false for app tab subframes).
    if (originalTarget != "" || !isAppTab)
      return originalTarget;

    // External links from within app tabs should always open in new tabs
    // instead of replacing the app tab's page (Bug 575561)
    let linkHost;
    let docHost;
    try {
      linkHost = linkURI.host;
      docHost = linkNode.ownerDocument.documentURIObject.host;
    } catch(e) {
      // nsIURI.host can throw for non-nsStandardURL nsIURIs.
      // If we fail to get either host, just return originalTarget.
      return originalTarget;
    }

    if (docHost == linkHost)
      return originalTarget;

    // Special case: ignore "www" prefix if it is part of host string
    let [longHost, shortHost] =
      linkHost.length > docHost.length ? [linkHost, docHost] : [docHost, linkHost];
    if (longHost == "www." + shortHost)
      return originalTarget;

    return "_blank";
  },

  /**
   * Map the plugin's name to a filtered version more suitable for UI.
   *
   * @param aName The full-length name string of the plugin.
   * @return the simplified name string.
   */
  makeNicePluginName: function (aName) {
    if (aName == "Shockwave Flash")
      return "Adobe Flash";
    // Regex checks if aName begins with "Java" + non-letter char
    if (/^Java\W/.exec(aName))
      return "Java";

    // Clean up the plugin name by stripping off parenthetical clauses,
    // trailing version numbers or "plugin".
    // EG, "Foo Bar (Linux) Plugin 1.23_02" --> "Foo Bar"
    // Do this by first stripping the numbers, etc. off the end, and then
    // removing "Plugin" (and then trimming to get rid of any whitespace).
    // (Otherwise, something like "Java(TM) Plug-in 1.7.0_07" gets mangled)
    let newName = aName.replace(/\(.*?\)/g, "").
                        replace(/[\s\d\.\-\_\(\)]+$/, "").
                        replace(/\bplug-?in\b/i, "").trim();
    return newName;
  },
};
