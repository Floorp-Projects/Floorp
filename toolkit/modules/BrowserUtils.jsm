/* -*- mode: js; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [ "BrowserUtils" ];

const {interfaces: Ci, utils: Cu, classes: Cc} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm");

Cu.importGlobalProperties(["URL"]);

let reflowObservers = new WeakMap();

function ReflowObserver(doc) {
  this._doc = doc;

  doc.docShell.addWeakReflowObserver(this);
  reflowObservers.set(this._doc, this);

  this.callbacks = [];
}

ReflowObserver.prototype = {
  QueryInterface: XPCOMUtils.generateQI(["nsIReflowObserver", "nsISupportsWeakReference"]),

  _onReflow() {
    reflowObservers.delete(this._doc);
    this._doc.docShell.removeWeakReflowObserver(this);

    for (let callback of this.callbacks) {
      try {
        callback();
      } catch (e) {
        Cu.reportError(e);
      }
    }
  },

  reflow() {
    this._onReflow();
  },

  reflowInterruptible() {
    this._onReflow();
  },
};

const FLUSH_TYPES = {
  "style": Ci.nsIDOMWindowUtils.FLUSH_STYLE,
  "layout": Ci.nsIDOMWindowUtils.FLUSH_LAYOUT,
  "display": Ci.nsIDOMWindowUtils.FLUSH_DISPLAY,
};

this.BrowserUtils = {

  /**
   * Prints arguments separated by a space and appends a new line.
   */
  dumpLn(...args) {
    for (let a of args)
      dump(a + " ");
    dump("\n");
  },

  /**
   * restartApplication: Restarts the application, keeping it in
   * safe mode if it is already in safe mode.
   */
  restartApplication() {
    let appStartup = Cc["@mozilla.org/toolkit/app-startup;1"]
                       .getService(Ci.nsIAppStartup);
    let cancelQuit = Cc["@mozilla.org/supports-PRBool;1"]
                       .createInstance(Ci.nsISupportsPRBool);
    Services.obs.notifyObservers(cancelQuit, "quit-application-requested", "restart");
    if (cancelQuit.data) { // The quit request has been canceled.
      return false;
    }
    // if already in safe mode restart in safe mode
    if (Services.appinfo.inSafeMode) {
      appStartup.restartInSafeMode(Ci.nsIAppStartup.eAttemptQuit | Ci.nsIAppStartup.eRestart);
      return undefined;
    }
    appStartup.quit(Ci.nsIAppStartup.eAttemptQuit | Ci.nsIAppStartup.eRestart);
    return undefined;
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
  urlSecurityCheck(aURL, aPrincipal, aFlags) {
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
      } catch (e2) { }

      throw "Load of " + aURL + principalStr + " denied.";
    }
  },

  /**
   * Return or create a principal with the codebase of one, and the originAttributes
   * of an existing principal (e.g. on a docshell, where the originAttributes ought
   * not to change, that is, we should keep the userContextId, privateBrowsingId,
   * etc. the same when changing the principal).
   *
   * @param principal
   *        The principal whose codebase/null/system-ness we want.
   * @param existingPrincipal
   *        The principal whose originAttributes we want, usually the current
   *        principal of a docshell.
   * @return an nsIPrincipal that matches the codebase/null/system-ness of the first
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
    if (principal.isCodebasePrincipal) {
      return secMan.createCodebasePrincipal(principal.URI, existingPrincipal.originAttributes);
    }

    if (principal.isNullPrincipal) {
      return secMan.createNullPrincipal(existingPrincipal.originAttributes);
    }
    throw new Error("Can't change the originAttributes of an expanded principal!");
  },

  /**
   * Constructs a new URI, using nsIIOService.
   * @param aURL The URI spec.
   * @param aOriginCharset The charset of the URI.
   * @param aBaseURI Base URI to resolve aURL, or null.
   * @return an nsIURI object based on aURL.
   *
   * @deprecated Use Services.io.newURI directly instead.
   */
  makeURI(aURL, aOriginCharset, aBaseURI) {
    return Services.io.newURI(aURL, aOriginCharset, aBaseURI);
  },

  /**
   * @deprecated Use Services.io.newFileURI directly instead.
   */
  makeFileURI(aFile) {
    return Services.io.newFileURI(aFile);
  },

  makeURIFromCPOW(aCPOWURI) {
    return Services.io.newURI(aCPOWURI.spec, aCPOWURI.originCharset);
  },

  /**
   * For a given DOM element, returns its position in "screen"
   * coordinates. In a content process, the coordinates returned will
   * be relative to the left/top of the tab. In the chrome process,
   * the coordinates are relative to the user's screen.
   */
  getElementBoundingScreenRect(aElement) {
    return this.getElementBoundingRect(aElement, true);
  },

  /**
   * For a given DOM element, returns its position as an offset from the topmost
   * window. In a content process, the coordinates returned will be relative to
   * the left/top of the topmost content area. If aInScreenCoords is true,
   * screen coordinates will be returned instead.
   */
  getElementBoundingRect(aElement, aInScreenCoords) {
    let rect = aElement.getBoundingClientRect();
    let win = aElement.ownerGlobal;

    let x = rect.left, y = rect.top;

    // We need to compensate for any iframes that might shift things
    // over. We also need to compensate for zooming.
    let parentFrame = win.frameElement;
    while (parentFrame) {
      win = parentFrame.ownerGlobal;
      let cstyle = win.getComputedStyle(parentFrame);

      let framerect = parentFrame.getBoundingClientRect();
      x += framerect.left + parseFloat(cstyle.borderLeftWidth) + parseFloat(cstyle.paddingLeft);
      y += framerect.top + parseFloat(cstyle.borderTopWidth) + parseFloat(cstyle.paddingTop);

      parentFrame = win.frameElement;
    }

    if (aInScreenCoords) {
      x += win.mozInnerScreenX;
      y += win.mozInnerScreenY;
    }

    let fullZoom = win.getInterface(Ci.nsIDOMWindowUtils).fullZoom;
    rect = {
      left: x * fullZoom,
      top: y * fullZoom,
      width: rect.width * fullZoom,
      height: rect.height * fullZoom
    };

    return rect;
  },

  onBeforeLinkTraversal(originalTarget, linkURI, linkNode, isAppTab) {
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
    } catch (e) {
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
  makeNicePluginName(aName) {
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

  /**
   * Return true if linkNode has a rel="noreferrer" attribute.
   *
   * @param linkNode The <a> element, or null.
   * @return a boolean indicating if linkNode has a rel="noreferrer" attribute.
   */
  linkHasNoReferrer(linkNode) {
    // A null linkNode typically means that we're checking a link that wasn't
    // provided via an <a> link, like a text-selected URL.  Don't leak
    // referrer information in this case.
    if (!linkNode)
      return true;

    let rel = linkNode.getAttribute("rel");
    if (!rel)
      return false;

    // The HTML spec says that rel should be split on spaces before looking
    // for particular rel values.
    let values = rel.split(/[ \t\r\n\f]/);
    return values.indexOf("noreferrer") != -1;
  },

  /**
   * Returns true if |mimeType| is text-based, or false otherwise.
   *
   * @param mimeType
   *        The MIME type to check.
   */
  mimeTypeIsTextBased(mimeType) {
    return mimeType.startsWith("text/") ||
           mimeType.endsWith("+xml") ||
           mimeType == "application/x-javascript" ||
           mimeType == "application/javascript" ||
           mimeType == "application/json" ||
           mimeType == "application/xml" ||
           mimeType == "mozilla.application/cached-xul";
  },

  /**
   * Return true if we should FAYT for this node + window (could be CPOW):
   *
   * @param elt
   *        The element that is focused
   * @param win
   *        The window that is focused
   *
   */
  shouldFastFind(elt, win) {
    if (elt) {
      if (elt instanceof win.HTMLInputElement && elt.mozIsTextField(false))
        return false;

      if (elt.isContentEditable || win.document.designMode == "on")
        return false;

      if (elt instanceof win.HTMLTextAreaElement ||
          elt instanceof win.HTMLSelectElement ||
          elt instanceof win.HTMLObjectElement ||
          elt instanceof win.HTMLEmbedElement)
        return false;
    }

    return true;
  },

  /**
   * Return true if we can FAYT for this window (could be CPOW):
   *
   * @param win
   *        The top level window that is focused
   *
   */
  canFastFind(win) {
    if (!win)
      return false;

    if (!this.mimeTypeIsTextBased(win.document.contentType))
      return false;

    // disable FAYT in about:blank to prevent FAYT opening unexpectedly.
    let loc = win.location;
    if (loc.href == "about:blank")
      return false;

    // disable FAYT in documents that ask for it to be disabled.
    if ((loc.protocol == "about:" || loc.protocol == "chrome:") &&
        (win.document.documentElement &&
         win.document.documentElement.getAttribute("disablefastfind") == "true"))
      return false;

    return true;
  },

  _visibleToolbarsMap: new WeakMap(),

  /**
   * Return true if any or a specific toolbar that interacts with the content
   * document is visible.
   *
   * @param  {nsIDocShell} docShell The docShell instance that a toolbar should
   *                                be interacting with
   * @param  {String}      which    Identifier of a specific toolbar
   * @return {Boolean}
   */
  isToolbarVisible(docShell, which) {
    let window = this.getRootWindow(docShell);
    if (!this._visibleToolbarsMap.has(window))
      return false;
    let toolbars = this._visibleToolbarsMap.get(window);
    return !!toolbars && toolbars.has(which);
  },

  /**
   * Sets the --toolbarbutton-button-height CSS property on the closest
   * toolbar to the provided element. Useful if you need to vertically
   * center a position:absolute element within a toolbar that uses
   * -moz-pack-align:stretch, and thus a height which is dependant on
   * the font-size.
   *
   * @param element An element within the toolbar whose height is desired.
   * @param options An object with the following properties:
              {
                forceLayoutFlushIfNeeded:
                  Set to true if a sync layout flush is acceptable.
              }
   */
  setToolbarButtonHeightProperty(element, options) {
    let window = element.ownerGlobal;
    let dwu = window.getInterface(Ci.nsIDOMWindowUtils);
    let toolbarItem = element;
    let urlBarContainer = element.closest("#urlbar-container");
    if (urlBarContainer) {
      // The stop-reload-button, which is contained in #urlbar-container,
      // needs to use #urlbar-container to calculate the bounds.
      toolbarItem = urlBarContainer;
    }
    if (!toolbarItem) {
      return;
    }
    let bounds = dwu.getBoundsWithoutFlushing(toolbarItem);
    if (!bounds.height && options.forceLayoutFlushIfNeeded) {
      bounds = toolbarItem.getBoundingClientRect();
    }
    if (bounds.height) {
      toolbarItem.style.setProperty("--toolbarbutton-height", bounds.height + "px");
    }
  },

  /**
   * Track whether a toolbar is visible for a given a docShell.
   *
   * @param  {nsIDocShell} docShell  The docShell instance that a toolbar should
   *                                 be interacting with
   * @param  {String}      which     Identifier of a specific toolbar
   * @param  {Boolean}     [visible] Whether the toolbar is visible. Optional,
   *                                 defaults to `true`.
   */
  trackToolbarVisibility(docShell, which, visible = true) {
    // We have to get the root window object, because XPConnect WrappedNatives
    // can't be used as WeakMap keys.
    let window = this.getRootWindow(docShell);
    let toolbars = this._visibleToolbarsMap.get(window);
    if (!toolbars) {
      toolbars = new Set();
      this._visibleToolbarsMap.set(window, toolbars);
    }
    if (!visible)
      toolbars.delete(which);
    else
      toolbars.add(which);
  },

  /**
   * Retrieve the root window object (i.e. the top-most content global) for a
   * specific docShell object.
   *
   * @param  {nsIDocShell} docShell
   * @return {nsIDOMWindow}
   */
  getRootWindow(docShell) {
    return docShell.QueryInterface(Ci.nsIDocShellTreeItem)
      .sameTypeRootTreeItem.QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIDOMWindow);
  },

  getSelectionDetails(topWindow, aCharLen) {
    // selections of more than 150 characters aren't useful
    const kMaxSelectionLen = 150;
    const charLen = Math.min(aCharLen || kMaxSelectionLen, kMaxSelectionLen);

    let focusedWindow = {};
    let focusedElement = Services.focus.getFocusedElementForWindow(topWindow, true, focusedWindow);
    focusedWindow = focusedWindow.value;

    let selection = focusedWindow.getSelection();
    let selectionStr = selection.toString();
    let fullText;

    let collapsed = selection.isCollapsed;

    let url;
    let linkText;
    if (selectionStr) {
      // Have some text, let's figure out if it looks like a URL that isn't
      // actually a link.
      linkText = selectionStr.trim();
      if (/^(?:https?|ftp):/i.test(linkText)) {
        try {
          url = this.makeURI(linkText);
        } catch (ex) {}
      } else if (/^(?:[a-z\d-]+\.)+[a-z]+$/i.test(linkText)) {
        // Check if this could be a valid url, just missing the protocol.
        // Now let's see if this is an intentional link selection. Our guess is
        // based on whether the selection begins/ends with whitespace or is
        // preceded/followed by a non-word character.

        // selection.toString() trims trailing whitespace, so we look for
        // that explicitly in the first and last ranges.
        let beginRange = selection.getRangeAt(0);
        let delimitedAtStart = /^\s/.test(beginRange);
        if (!delimitedAtStart) {
          let container = beginRange.startContainer;
          let offset = beginRange.startOffset;
          if (container.nodeType == container.TEXT_NODE && offset > 0)
            delimitedAtStart = /\W/.test(container.textContent[offset - 1]);
          else
            delimitedAtStart = true;
        }

        let delimitedAtEnd = false;
        if (delimitedAtStart) {
          let endRange = selection.getRangeAt(selection.rangeCount - 1);
          delimitedAtEnd = /\s$/.test(endRange);
          if (!delimitedAtEnd) {
            let container = endRange.endContainer;
            let offset = endRange.endOffset;
            if (container.nodeType == container.TEXT_NODE &&
                offset < container.textContent.length)
              delimitedAtEnd = /\W/.test(container.textContent[offset]);
            else
              delimitedAtEnd = true;
          }
        }

        if (delimitedAtStart && delimitedAtEnd) {
          let uriFixup = Cc["@mozilla.org/docshell/urifixup;1"]
                           .getService(Ci.nsIURIFixup);
          try {
            url = uriFixup.createFixupURI(linkText, uriFixup.FIXUP_FLAG_NONE);
          } catch (ex) {}
        }
      }
    }

    // try getting a selected text in text input.
    if (!selectionStr && focusedElement instanceof Ci.nsIDOMNSEditableElement) {
      // Don't get the selection for password fields. See bug 565717.
      if (focusedElement instanceof Ci.nsIDOMHTMLTextAreaElement ||
          (focusedElement instanceof Ci.nsIDOMHTMLInputElement &&
           focusedElement.mozIsTextField(true))) {
        selectionStr = focusedElement.editor.selection.toString();
      }
    }

    if (selectionStr) {
      // Pass up to 16K through unmolested.  If an add-on needs more, they will
      // have to use a content script.
      fullText = selectionStr.substr(0, 16384);

      if (selectionStr.length > charLen) {
        // only use the first charLen important chars. see bug 221361
        var pattern = new RegExp("^(?:\\s*.){0," + charLen + "}");
        pattern.test(selectionStr);
        selectionStr = RegExp.lastMatch;
      }

      selectionStr = selectionStr.trim().replace(/\s+/g, " ");

      if (selectionStr.length > charLen) {
        selectionStr = selectionStr.substr(0, charLen);
      }
    }

    if (url && !url.host) {
      url = null;
    }

    return { text: selectionStr, docSelectionIsCollapsed: collapsed, fullText,
             linkURL: url ? url.spec : null, linkText: url ? linkText : "" };
  },

  // Iterates through every docshell in the window and calls PermitUnload.
  canCloseWindow(window) {
    let docShell = window.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIWebNavigation);
    let node = docShell.QueryInterface(Ci.nsIDocShellTreeItem);
    for (let i = 0; i < node.childCount; ++i) {
      let docShell = node.getChildAt(i).QueryInterface(Ci.nsIDocShell);
      let contentViewer = docShell.contentViewer;
      if (contentViewer && !contentViewer.permitUnload()) {
        return false;
      }
    }

    return true;
  },

  /**
   * Replaces %s or %S in the provided url or postData with the given parameter,
   * acccording to the best charset for the given url.
   *
   * @return [url, postData]
   * @throws if nor url nor postData accept a param, but a param was provided.
   */
  async parseUrlAndPostData(url, postData, param) {
    let hasGETParam = /%s/i.test(url)
    let decodedPostData = postData ? unescape(postData) : "";
    let hasPOSTParam = /%s/i.test(decodedPostData);

    if (!hasGETParam && !hasPOSTParam) {
      if (param) {
        // If nor the url, nor postData contain parameters, but a parameter was
        // provided, return the original input.
        throw new Error("A param was provided but there's nothing to bind it to");
      }
      return [url, postData];
    }

    let charset = "";
    const re = /^(.*)\&mozcharset=([a-zA-Z][_\-a-zA-Z0-9]+)\s*$/;
    let matches = url.match(re);
    if (matches) {
      [, url, charset] = matches;
    } else {
      // Try to fetch a charset from History.
      try {
        // Will return an empty string if character-set is not found.
        charset = await PlacesUtils.getCharsetForURI(this.makeURI(url));
      } catch (ex) {
        // makeURI() throws if url is invalid.
        Cu.reportError(ex);
      }
    }

    // encodeURIComponent produces UTF-8, and cannot be used for other charsets.
    // escape() works in those cases, but it doesn't uri-encode +, @, and /.
    // Therefore we need to manually replace these ASCII characters by their
    // encodeURIComponent result, to match the behavior of nsEscape() with
    // url_XPAlphas.
    let encodedParam = "";
    if (charset && charset != "UTF-8") {
      try {
        let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                          .createInstance(Ci.nsIScriptableUnicodeConverter);
        converter.charset = charset;
        encodedParam = converter.ConvertFromUnicode(param) + converter.Finish();
      } catch (ex) {
        encodedParam = param;
      }
      encodedParam = escape(encodedParam).replace(/[+@\/]+/g, encodeURIComponent);
    } else {
      // Default charset is UTF-8
      encodedParam = encodeURIComponent(param);
    }

    url = url.replace(/%s/g, encodedParam).replace(/%S/g, param);
    if (hasPOSTParam) {
      postData = decodedPostData.replace(/%s/g, encodedParam)
                                .replace(/%S/g, param);
    }
    return [url, postData];
  },

  /**
   * Calls the given function when the given document has just reflowed,
   * and returns a promise which resolves to its return value after it
   * has been called.
   *
   * The function *must not trigger any reflows*, or make any changes
   * which would require a layout flush.
   *
   * @param {Document} doc
   * @param {function} callback
   * @returns {Promise}
   */
  promiseReflowed(doc, callback) {
    let observer = reflowObservers.get(doc);
    if (!observer) {
      observer = new ReflowObserver(doc);
      reflowObservers.set(doc, observer);
    }

    return new Promise((resolve, reject) => {
      observer.callbacks.push(() => {
        try {
          resolve(callback());
        } catch (e) {
          reject(e);
        }
      });
    });
  },

  /**
   * Calls the given function as soon as a layout flush of the given
   * type is not necessary, and returns a promise which resolves to the
   * callback's return value after it executes.
   *
   * The function *must not trigger any reflows*, or make any changes
   * which would require a layout flush.
   *
   * @param {Document} doc
   * @param {string} flushType
   *        The flush type required. Must be one of:
   *
   *          - "style"
   *          - "layout"
   *          - "display"
   * @param {function} callback
   * @returns {Promise}
   */
  async promiseLayoutFlushed(doc, flushType, callback) {
    let utils = doc.defaultView.QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIDOMWindowUtils);

    if (!utils.needsFlush(FLUSH_TYPES[flushType])) {
      return callback();
    }

    return this.promiseReflowed(doc, callback);
  },
};
