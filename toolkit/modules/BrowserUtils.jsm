/* -*- mode: js; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["BrowserUtils"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

var BrowserUtils = {
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
      if (aURL instanceof Ci.nsIURI) {
        secMan.checkLoadURIWithPrincipal(aPrincipal, aURL, aFlags);
      } else {
        secMan.checkLoadURIStrWithPrincipal(aPrincipal, aURL, aFlags);
      }
    } catch (e) {
      let principalStr = "";
      try {
        principalStr = " from " + aPrincipal.spec;
      } catch (e2) {}

      throw new Error(`Load of ${aURL + principalStr} denied.`);
    }
  },

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

    let x = rect.left;
    let y = rect.top;

    // We need to compensate for any iframes that might shift things
    // over. We also need to compensate for zooming.
    let parentFrame = win.frameElement;
    while (parentFrame) {
      win = parentFrame.ownerGlobal;
      let cstyle = win.getComputedStyle(parentFrame);

      let framerect = parentFrame.getBoundingClientRect();
      x +=
        framerect.left +
        parseFloat(cstyle.borderLeftWidth) +
        parseFloat(cstyle.paddingLeft);
      y +=
        framerect.top +
        parseFloat(cstyle.borderTopWidth) +
        parseFloat(cstyle.paddingTop);

      parentFrame = win.frameElement;
    }

    rect = {
      left: x,
      top: y,
      width: rect.width,
      height: rect.height,
    };
    rect = win.windowUtils.transformRectLayoutToVisual(
      rect.left,
      rect.top,
      rect.width,
      rect.height
    );

    if (aInScreenCoords) {
      rect = {
        left: rect.left + win.mozInnerScreenX,
        top: rect.top + win.mozInnerScreenY,
        width: rect.width,
        height: rect.height,
      };
    }

    let fullZoom = win.windowUtils.fullZoom;
    rect = {
      left: rect.left * fullZoom,
      top: rect.top * fullZoom,
      width: rect.width * fullZoom,
      height: rect.height * fullZoom,
    };

    return rect;
  },

  onBeforeLinkTraversal(originalTarget, linkURI, linkNode, isAppTab) {
    // Don't modify non-default targets or targets that aren't in top-level app
    // tab docshells (isAppTab will be false for app tab subframes).
    if (originalTarget != "" || !isAppTab) {
      return originalTarget;
    }

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

    if (docHost == linkHost) {
      return originalTarget;
    }

    // Special case: ignore "www" prefix if it is part of host string
    let [longHost, shortHost] =
      linkHost.length > docHost.length
        ? [linkHost, docHost]
        : [docHost, linkHost];
    if (longHost == "www." + shortHost) {
      return originalTarget;
    }

    return "_blank";
  },

  /**
   * Map the plugin's name to a filtered version more suitable for UI.
   *
   * @param aName The full-length name string of the plugin.
   * @return the simplified name string.
   */
  makeNicePluginName(aName) {
    if (aName == "Shockwave Flash") {
      return "Adobe Flash";
    }
    // Regex checks if aName begins with "Java" + non-letter char
    if (/^Java\W/.exec(aName)) {
      return "Java";
    }

    // Clean up the plugin name by stripping off parenthetical clauses,
    // trailing version numbers or "plugin".
    // EG, "Foo Bar (Linux) Plugin 1.23_02" --> "Foo Bar"
    // Do this by first stripping the numbers, etc. off the end, and then
    // removing "Plugin" (and then trimming to get rid of any whitespace).
    // (Otherwise, something like "Java(TM) Plug-in 1.7.0_07" gets mangled)
    let newName = aName
      .replace(/\(.*?\)/g, "")
      .replace(/[\s\d\.\-\_\(\)]+$/, "")
      .replace(/\bplug-?in\b/i, "")
      .trim();
    return newName;
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
   * Trim the selection text to a reasonable size and sanitize it to make it
   * safe for search query input.
   *
   * @param aSelection
   *        The selection text to trim.
   * @param aMaxLen
   *        The maximum string length, defaults to a reasonable size if undefined.
   * @return The trimmed selection text.
   */
  trimSelection(aSelection, aMaxLen) {
    // Selections of more than 150 characters aren't useful.
    const maxLen = Math.min(aMaxLen || 150, aSelection.length);

    if (aSelection.length > maxLen) {
      // only use the first maxLen important chars. see bug 221361
      let pattern = new RegExp("^(?:\\s*.){0," + maxLen + "}");
      pattern.test(aSelection);
      aSelection = RegExp.lastMatch;
    }

    aSelection = aSelection.trim().replace(/\s+/g, " ");

    if (aSelection.length > maxLen) {
      aSelection = aSelection.substr(0, maxLen);
    }

    return aSelection;
  },

  /**
   * Retrieve the text selection details for the given window.
   *
   * @param  aTopWindow
   *         The top window of the element containing the selection.
   * @param  aCharLen
   *         The maximum string length for the selection text.
   * @return The selection details containing the full and trimmed selection text
   *         and link details for link selections.
   */
  getSelectionDetails(aTopWindow, aCharLen) {
    let focusedWindow = {};
    let focusedElement = Services.focus.getFocusedElementForWindow(
      aTopWindow,
      true,
      focusedWindow
    );
    focusedWindow = focusedWindow.value;

    let selection = focusedWindow.getSelection();
    let selectionStr = selection.toString();
    let fullText;

    let url;
    let linkText;

    let isDocumentLevelSelection = true;
    // try getting a selected text in text input.
    if (!selectionStr && focusedElement) {
      // Don't get the selection for password fields. See bug 565717.
      if (
        ChromeUtils.getClassName(focusedElement) === "HTMLTextAreaElement" ||
        (ChromeUtils.getClassName(focusedElement) === "HTMLInputElement" &&
          focusedElement.mozIsTextField(true))
      ) {
        selection = focusedElement.editor.selection;
        selectionStr = selection.toString();
        isDocumentLevelSelection = false;
      }
    }

    let collapsed = selection.isCollapsed;

    if (selectionStr) {
      // Have some text, let's figure out if it looks like a URL that isn't
      // actually a link.
      linkText = selectionStr.trim();
      if (/^(?:https?|ftp):/i.test(linkText)) {
        try {
          url = Services.io.newURI(linkText);
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
          if (container.nodeType == container.TEXT_NODE && offset > 0) {
            delimitedAtStart = /\W/.test(container.textContent[offset - 1]);
          } else {
            delimitedAtStart = true;
          }
        }

        let delimitedAtEnd = false;
        if (delimitedAtStart) {
          let endRange = selection.getRangeAt(selection.rangeCount - 1);
          delimitedAtEnd = /\s$/.test(endRange);
          if (!delimitedAtEnd) {
            let container = endRange.endContainer;
            let offset = endRange.endOffset;
            if (
              container.nodeType == container.TEXT_NODE &&
              offset < container.textContent.length
            ) {
              delimitedAtEnd = /\W/.test(container.textContent[offset]);
            } else {
              delimitedAtEnd = true;
            }
          }
        }

        if (delimitedAtStart && delimitedAtEnd) {
          try {
            url = Services.uriFixup.getFixupURIInfo(linkText).preferredURI;
          } catch (ex) {}
        }
      }
    }

    if (selectionStr) {
      // Pass up to 16K through unmolested.  If an add-on needs more, they will
      // have to use a content script.
      fullText = selectionStr.substr(0, 16384);
      selectionStr = this.trimSelection(selectionStr, aCharLen);
    }

    if (url && !url.host) {
      url = null;
    }

    return {
      text: selectionStr,
      docSelectionIsCollapsed: collapsed,
      isDocumentLevelSelection,
      fullText,
      linkURL: url ? url.spec : null,
      linkText: url ? linkText : "",
    };
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
};

XPCOMUtils.defineLazyPreferenceGetter(
  BrowserUtils,
  "navigationRequireUserInteraction",
  "browser.navigation.requireUserInteraction",
  false
);
