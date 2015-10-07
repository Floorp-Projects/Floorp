/* -*- mode: js; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [ "BrowserUtils" ];

const {interfaces: Ci, utils: Cu, classes: Cc} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.importGlobalProperties(['URL']);

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
   * restartApplication: Restarts the application, keeping it in
   * safe mode if it is already in safe mode.
   */
  restartApplication: function() {
    let appStartup = Cc["@mozilla.org/toolkit/app-startup;1"]
                       .getService(Ci.nsIAppStartup);
    let cancelQuit = Cc["@mozilla.org/supports-PRBool;1"]
                       .createInstance(Ci.nsISupportsPRBool);
    Services.obs.notifyObservers(cancelQuit, "quit-application-requested", "restart");
    if (cancelQuit.data) { // The quit request has been canceled.
      return false;
    }
    //if already in safe mode restart in safe mode
    if (Services.appinfo.inSafeMode) {
      appStartup.restartInSafeMode(Ci.nsIAppStartup.eAttemptQuit | Ci.nsIAppStartup.eRestart);
      return;
    }
    appStartup.quit(Ci.nsIAppStartup.eAttemptQuit | Ci.nsIAppStartup.eRestart);
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

  makeURIFromCPOW: function(aCPOWURI) {
    return Services.io.newURI(aCPOWURI.spec, aCPOWURI.originCharset, null);
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

  /**
   * Return true if linkNode has a rel="noreferrer" attribute.
   *
   * @param linkNode The <a> element, or null.
   * @return a boolean indicating if linkNode has a rel="noreferrer" attribute.
   */
  linkHasNoReferrer: function (linkNode) {
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
    return values.indexOf('noreferrer') != -1;
  },

  /**
   * Returns true if |mimeType| is text-based, or false otherwise.
   *
   * @param mimeType
   *        The MIME type to check.
   */
  mimeTypeIsTextBased: function(mimeType) {
    return mimeType.startsWith("text/") ||
           mimeType.endsWith("+xml") ||
           mimeType == "application/x-javascript" ||
           mimeType == "application/javascript" ||
           mimeType == "application/json" ||
           mimeType == "application/xml" ||
           mimeType == "mozilla.application/cached-xul";
  },

  /**
   * Return true if we can/should FAYT for this node + window (could be CPOW):
   *
   * @param elt
   *        The element that is focused
   * @param win
   *        The window that is focused
   *
   */
  shouldFastFind: function(elt, win) {
    if (elt) {
      if (elt instanceof win.HTMLInputElement && elt.mozIsTextField(false))
        return false;

      if (elt.isContentEditable)
        return false;

      if (elt instanceof win.HTMLTextAreaElement ||
          elt instanceof win.HTMLSelectElement ||
          elt instanceof win.HTMLObjectElement ||
          elt instanceof win.HTMLEmbedElement)
        return false;
    }

    if (win && !this.mimeTypeIsTextBased(win.document.contentType))
      return false;

    // disable FAYT in about:blank to prevent FAYT opening unexpectedly.
    let loc = win.location;
    if (loc.href == "about:blank")
      return false;

    // disable FAYT in documents that ask for it to be disabled.
    if ((loc.protocol == "about:" || loc.protocol == "chrome:") &&
        (win && win.document.documentElement &&
         win.document.documentElement.getAttribute("disablefastfind") == "true"))
      return false;

    if (win) {
      try {
        let editingSession = win.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
          .getInterface(Components.interfaces.nsIWebNavigation)
          .QueryInterface(Components.interfaces.nsIInterfaceRequestor)
          .getInterface(Components.interfaces.nsIEditingSession);
        if (editingSession.windowIsEditable(win))
          return false;
      }
      catch (e) {
        Cu.reportError(e);
        // If someone built with composer disabled, we can't get an editing session.
      }
    }
    return true;
  },

  getSelectionDetails: function(topWindow, aCharLen) {
    // selections of more than 150 characters aren't useful
    const kMaxSelectionLen = 150;
    const charLen = Math.min(aCharLen || kMaxSelectionLen, kMaxSelectionLen);

    let focusedWindow = {};
    let focusedElement = Services.focus.getFocusedElementForWindow(topWindow, true, focusedWindow);
    focusedWindow = focusedWindow.value;

    let selection = focusedWindow.getSelection();
    let selectionStr = selection.toString();

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
      }
      // Check if this could be a valid url, just missing the protocol.
      else if (/^(?:[a-z\d-]+\.)+[a-z]+$/i.test(linkText)) {
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

    return { text: selectionStr, docSelectionIsCollapsed: collapsed,
             linkURL: url ? url.spec : null, linkText: url ? linkText : "" };
  }
};
