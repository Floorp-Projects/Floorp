/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  ViewSourcePageChild: "resource://gre/actors/ViewSourcePageChild.sys.mjs",
});

export class ViewSourceChild extends JSWindowActorChild {
  receiveMessage(message) {
    let data = message.data;
    switch (message.name) {
      case "ViewSource:LoadSource":
        this.viewSource(data.URL, data.outerWindowID, data.lineNumber);
        break;
      case "ViewSource:LoadSourceWithSelection":
        this.viewSourceWithSelection(
          data.URL,
          data.drawSelection,
          data.baseURI
        );
        break;
      case "ViewSource:GetSelection":
        let selectionDetails;
        try {
          selectionDetails = this.getSelection(this.document.ownerGlobal);
        } catch (e) {}
        return selectionDetails;
    }

    return undefined;
  }

  /**
   * Called when the parent sends a message to view some source code.
   *
   * @param URL (required)
   *        The URL string of the source to be shown.
   * @param outerWindowID (optional)
   *        The outerWindowID of the content window that has hosted
   *        the document, in case we want to retrieve it from the network
   *        cache.
   * @param lineNumber (optional)
   *        The line number to focus as soon as the source has finished
   *        loading.
   */
  viewSource(URL, outerWindowID, lineNumber) {
    let otherDocShell;
    let forceEncodingDetection = false;

    if (outerWindowID) {
      let contentWindow = Services.wm.getOuterWindowWithId(outerWindowID);
      if (contentWindow) {
        otherDocShell = contentWindow.docShell;

        forceEncodingDetection = contentWindow.windowUtils.docCharsetIsForced;
      }
    }

    this.loadSource(URL, otherDocShell, lineNumber, forceEncodingDetection);
  }

  /**
   * Loads a view source selection showing the given view-source url and
   * highlight the selection.
   *
   * @param uri view-source uri to show
   * @param drawSelection true to highlight the selection
   * @param baseURI base URI of the original document
   */
  viewSourceWithSelection(uri, drawSelection, baseURI) {
    // This isn't ideal, but set a global in the view source page actor
    // that indicates that a selection should be drawn. It will be read
    // when by the page's pageshow listener. This should work as the
    // view source page is always loaded in the same process.
    lazy.ViewSourcePageChild.setNeedsDrawSelection(drawSelection);

    // all our content is held by the data:URI and URIs are internally stored as utf-8 (see nsIURI.idl)
    let loadFlags = Ci.nsIWebNavigation.LOAD_FLAGS_NONE;
    let webNav = this.docShell.QueryInterface(Ci.nsIWebNavigation);
    let loadURIOptions = {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      loadFlags,
      baseURI: Services.io.newURI(baseURI),
    };
    webNav.loadURI(uri, loadURIOptions);
  }

  /**
   * Common utility function used by both the current and deprecated APIs
   * for loading source.
   *
   * @param URL (required)
   *        The URL string of the source to be shown.
   * @param otherDocShell (optional)
   *        The docshell of the content window that is hosting the document.
   * @param lineNumber (optional)
   *        The line number to focus as soon as the source has finished
   *        loading.
   * @param forceEncodingDetection (optional)
   *        Force autodetection of the character encoding.
   */
  loadSource(URL, otherDocShell, lineNumber, forceEncodingDetection) {
    const viewSrcURL = "view-source:" + URL;

    if (forceEncodingDetection) {
      this.docShell.forceEncodingDetection();
    }

    lazy.ViewSourcePageChild.setInitialLineNumber(lineNumber);

    if (!otherDocShell) {
      this.loadSourceFromURL(viewSrcURL);
      return;
    }

    try {
      let pageLoader = this.docShell.QueryInterface(Ci.nsIWebPageDescriptor);
      pageLoader.loadPageAsViewSource(otherDocShell, viewSrcURL);
    } catch (e) {
      // We were not able to load the source from the network cache.
      this.loadSourceFromURL(viewSrcURL);
    }
  }

  /**
   * Load some URL in the browser.
   *
   * @param URL
   *        The URL string to load.
   */
  loadSourceFromURL(URL) {
    let loadFlags = Ci.nsIWebNavigation.LOAD_FLAGS_NONE;
    let webNav = this.docShell.QueryInterface(Ci.nsIWebNavigation);
    let loadURIOptions = {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      loadFlags,
    };
    webNav.loadURI(URL, loadURIOptions);
  }

  /**
   * A helper to get a path like FIXptr, but with an array instead of the
   * "tumbler" notation.
   * See FIXptr: http://lists.w3.org/Archives/Public/www-xml-linking-comments/2001AprJun/att-0074/01-NOTE-FIXptr-20010425.htm
   */
  getPath(ancestor, node) {
    var n = node;
    var p = n.parentNode;
    if (n == ancestor || !p) {
      return null;
    }
    var path = [];
    if (!path) {
      return null;
    }
    do {
      for (var i = 0; i < p.childNodes.length; i++) {
        if (p.childNodes.item(i) == n) {
          path.push(i);
          break;
        }
      }
      n = p;
      p = n.parentNode;
    } while (n != ancestor && p);
    return path;
  }

  getSelection(global) {
    const { content } = global;

    // These are markers used to delimit the selection during processing. They
    // are removed from the final rendering.
    // We use noncharacter Unicode codepoints to minimize the risk of clashing
    // with anything that might legitimately be present in the document.
    // U+FDD0..FDEF <noncharacters>
    const MARK_SELECTION_START = "\uFDD0";
    const MARK_SELECTION_END = "\uFDEF";

    var focusedWindow = Services.focus.focusedWindow || content;
    var selection = focusedWindow.getSelection();

    var range = selection.getRangeAt(0);
    var ancestorContainer = range.commonAncestorContainer;
    var doc = ancestorContainer.ownerDocument;

    var startContainer = range.startContainer;
    var endContainer = range.endContainer;
    var startOffset = range.startOffset;
    var endOffset = range.endOffset;

    // let the ancestor be an element
    var Node = doc.defaultView.Node;
    if (
      ancestorContainer.nodeType == Node.TEXT_NODE ||
      ancestorContainer.nodeType == Node.CDATA_SECTION_NODE
    ) {
      ancestorContainer = ancestorContainer.parentNode;
    }

    // for selectAll, let's use the entire document, including <html>...</html>
    // @see nsDocumentViewer::SelectAll() for how selectAll is implemented
    try {
      if (ancestorContainer == doc.body) {
        ancestorContainer = doc.documentElement;
      }
    } catch (e) {}

    // each path is a "child sequence" (a.k.a. "tumbler") that
    // descends from the ancestor down to the boundary point
    var startPath = this.getPath(ancestorContainer, startContainer);
    var endPath = this.getPath(ancestorContainer, endContainer);

    // clone the fragment of interest and reset everything to be relative to it
    // note: it is with the clone that we operate/munge from now on.  Also note
    // that we clone into a data document to prevent images in the fragment from
    // loading and the like.  The use of importNode here, as opposed to adoptNode,
    // is _very_ important.
    // XXXbz wish there were a less hacky way to create an untrusted document here
    var isHTML = doc.createElement("div").tagName == "DIV";
    var dataDoc = isHTML
      ? ancestorContainer.ownerDocument.implementation.createHTMLDocument("")
      : ancestorContainer.ownerDocument.implementation.createDocument(
          "",
          "",
          null
        );
    ancestorContainer = dataDoc.importNode(ancestorContainer, true);
    startContainer = ancestorContainer;
    endContainer = ancestorContainer;

    // Only bother with the selection if it can be remapped. Don't mess with
    // leaf elements (such as <isindex>) that secretly use anynomous content
    // for their display appearance.
    var canDrawSelection = ancestorContainer.hasChildNodes();
    var tmpNode;
    if (canDrawSelection) {
      var i;
      for (i = startPath ? startPath.length - 1 : -1; i >= 0; i--) {
        startContainer = startContainer.childNodes.item(startPath[i]);
      }
      for (i = endPath ? endPath.length - 1 : -1; i >= 0; i--) {
        endContainer = endContainer.childNodes.item(endPath[i]);
      }

      // add special markers to record the extent of the selection
      // note: |startOffset| and |endOffset| are interpreted either as
      // offsets in the text data or as child indices (see the Range spec)
      // (here, munging the end point first to keep the start point safe...)
      if (
        endContainer.nodeType == Node.TEXT_NODE ||
        endContainer.nodeType == Node.CDATA_SECTION_NODE
      ) {
        // do some extra tweaks to try to avoid the view-source output to look like
        // ...<tag>]... or ...]</tag>... (where ']' marks the end of the selection).
        // To get a neat output, the idea here is to remap the end point from:
        // 1. ...<tag>]...   to   ...]<tag>...
        // 2. ...]</tag>...  to   ...</tag>]...
        if (
          (endOffset > 0 && endOffset < endContainer.data.length) ||
          !endContainer.parentNode ||
          !endContainer.parentNode.parentNode
        ) {
          endContainer.insertData(endOffset, MARK_SELECTION_END);
        } else {
          tmpNode = dataDoc.createTextNode(MARK_SELECTION_END);
          endContainer = endContainer.parentNode;
          if (endOffset === 0) {
            endContainer.parentNode.insertBefore(tmpNode, endContainer);
          } else {
            endContainer.parentNode.insertBefore(
              tmpNode,
              endContainer.nextSibling
            );
          }
        }
      } else {
        tmpNode = dataDoc.createTextNode(MARK_SELECTION_END);
        endContainer.insertBefore(
          tmpNode,
          endContainer.childNodes.item(endOffset)
        );
      }

      if (
        startContainer.nodeType == Node.TEXT_NODE ||
        startContainer.nodeType == Node.CDATA_SECTION_NODE
      ) {
        // do some extra tweaks to try to avoid the view-source output to look like
        // ...<tag>[... or ...[</tag>... (where '[' marks the start of the selection).
        // To get a neat output, the idea here is to remap the start point from:
        // 1. ...<tag>[...   to   ...[<tag>...
        // 2. ...[</tag>...  to   ...</tag>[...
        if (
          (startOffset > 0 && startOffset < startContainer.data.length) ||
          !startContainer.parentNode ||
          !startContainer.parentNode.parentNode ||
          startContainer != startContainer.parentNode.lastChild
        ) {
          startContainer.insertData(startOffset, MARK_SELECTION_START);
        } else {
          tmpNode = dataDoc.createTextNode(MARK_SELECTION_START);
          startContainer = startContainer.parentNode;
          if (startOffset === 0) {
            startContainer.parentNode.insertBefore(tmpNode, startContainer);
          } else {
            startContainer.parentNode.insertBefore(
              tmpNode,
              startContainer.nextSibling
            );
          }
        }
      } else {
        tmpNode = dataDoc.createTextNode(MARK_SELECTION_START);
        startContainer.insertBefore(
          tmpNode,
          startContainer.childNodes.item(startOffset)
        );
      }
    }

    // now extract and display the syntax highlighted source
    tmpNode = dataDoc.createElementNS("http://www.w3.org/1999/xhtml", "div");
    tmpNode.appendChild(ancestorContainer);

    return {
      URL:
        (isHTML
          ? "view-source:data:text/html;charset=utf-8,"
          : "view-source:data:application/xml;charset=utf-8,") +
        encodeURIComponent(tmpNode.innerHTML),
      drawSelection: canDrawSelection,
      baseURI: doc.baseURI,
    };
  }

  get wrapLongLines() {
    return Services.prefs.getBoolPref("view_source.wrap_long_lines");
  }
}
