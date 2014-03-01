/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [ "BrowserUtils" ];

const {interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");

this.BrowserUtils = {

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

};
