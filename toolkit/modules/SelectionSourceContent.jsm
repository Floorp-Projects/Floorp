/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["SelectionSourceContent"];

ChromeUtils.import("resource://gre/modules/Services.jsm");

var SelectionSourceContent = {
  receiveMessage(message) {
    const global = message.target;

    if (message.name == "ViewSource:GetSelection") {
      let selectionDetails;
      try {
        selectionDetails = this.getSelection(global);
      } finally {
        global.sendAsyncMessage("ViewSource:GetSelectionDone", selectionDetails);
      }
    }
  },

  /**
   * A helper to get a path like FIXptr, but with an array instead of the
   * "tumbler" notation.
   * See FIXptr: http://lists.w3.org/Archives/Public/www-xml-linking-comments/2001AprJun/att-0074/01-NOTE-FIXptr-20010425.htm
   */
  getPath(ancestor, node) {
    var n = node;
    var p = n.parentNode;
    if (n == ancestor || !p)
      return null;
    var path = [];
    if (!path)
      return null;
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
  },

  getSelection(global) {
    const {content} = global;

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
    if (ancestorContainer.nodeType == Node.TEXT_NODE ||
        ancestorContainer.nodeType == Node.CDATA_SECTION_NODE)
      ancestorContainer = ancestorContainer.parentNode;

    // for selectAll, let's use the entire document, including <html>...</html>
    // @see nsDocumentViewer::SelectAll() for how selectAll is implemented
    try {
      if (ancestorContainer == doc.body)
        ancestorContainer = doc.documentElement;
    } catch (e) { }

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
    var isHTML = (doc.createElement("div").tagName == "DIV");
    var dataDoc = isHTML ?
      ancestorContainer.ownerDocument.implementation.createHTMLDocument("") :
      ancestorContainer.ownerDocument.implementation.createDocument("", "", null);
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
      if (endContainer.nodeType == Node.TEXT_NODE ||
          endContainer.nodeType == Node.CDATA_SECTION_NODE) {
        // do some extra tweaks to try to avoid the view-source output to look like
        // ...<tag>]... or ...]</tag>... (where ']' marks the end of the selection).
        // To get a neat output, the idea here is to remap the end point from:
        // 1. ...<tag>]...   to   ...]<tag>...
        // 2. ...]</tag>...  to   ...</tag>]...
        if ((endOffset > 0 && endOffset < endContainer.data.length) ||
            !endContainer.parentNode || !endContainer.parentNode.parentNode)
          endContainer.insertData(endOffset, MARK_SELECTION_END);
        else {
          tmpNode = dataDoc.createTextNode(MARK_SELECTION_END);
          endContainer = endContainer.parentNode;
          if (endOffset === 0)
            endContainer.parentNode.insertBefore(tmpNode, endContainer);
          else
            endContainer.parentNode.insertBefore(tmpNode, endContainer.nextSibling);
        }
      } else {
        tmpNode = dataDoc.createTextNode(MARK_SELECTION_END);
        endContainer.insertBefore(tmpNode, endContainer.childNodes.item(endOffset));
      }

      if (startContainer.nodeType == Node.TEXT_NODE ||
          startContainer.nodeType == Node.CDATA_SECTION_NODE) {
        // do some extra tweaks to try to avoid the view-source output to look like
        // ...<tag>[... or ...[</tag>... (where '[' marks the start of the selection).
        // To get a neat output, the idea here is to remap the start point from:
        // 1. ...<tag>[...   to   ...[<tag>...
        // 2. ...[</tag>...  to   ...</tag>[...
        if ((startOffset > 0 && startOffset < startContainer.data.length) ||
            !startContainer.parentNode || !startContainer.parentNode.parentNode ||
            startContainer != startContainer.parentNode.lastChild)
          startContainer.insertData(startOffset, MARK_SELECTION_START);
        else {
          tmpNode = dataDoc.createTextNode(MARK_SELECTION_START);
          startContainer = startContainer.parentNode;
          if (startOffset === 0)
            startContainer.parentNode.insertBefore(tmpNode, startContainer);
          else
            startContainer.parentNode.insertBefore(tmpNode, startContainer.nextSibling);
        }
      } else {
        tmpNode = dataDoc.createTextNode(MARK_SELECTION_START);
        startContainer.insertBefore(tmpNode, startContainer.childNodes.item(startOffset));
      }
    }

    // now extract and display the syntax highlighted source
    tmpNode = dataDoc.createElementNS("http://www.w3.org/1999/xhtml", "div");
    tmpNode.appendChild(ancestorContainer);

    return { uri: (isHTML ? "view-source:data:text/html;charset=utf-8," :
                            "view-source:data:application/xml;charset=utf-8,")
                  + encodeURIComponent(tmpNode.innerHTML),
             drawSelection: canDrawSelection,
             baseURI: doc.baseURI };
  },

  get wrapLongLines() {
    return Services.prefs.getBoolPref("view_source.wrap_long_lines");
  },
};
