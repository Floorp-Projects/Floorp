// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { utils: Cu, interfaces: Ci, classes: Cc } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Deprecated",
  "resource://gre/modules/Deprecated.jsm");

const NS_XHTML = 'http://www.w3.org/1999/xhtml';

// These are markers used to delimit the selection during processing. They
// are removed from the final rendering.
// We use noncharacter Unicode codepoints to minimize the risk of clashing
// with anything that might legitimately be present in the document.
// U+FDD0..FDEF <noncharacters>
const MARK_SELECTION_START = '\uFDD0';
const MARK_SELECTION_END = '\uFDEF';

this.EXPORTED_SYMBOLS = ["ViewSourceBrowser"];

/**
 * ViewSourceBrowser manages the view source <browser> from the chrome side.
 * It's companion frame script, viewSource-content.js, needs to be loaded as a
 * frame script into the browser being managed.
 *
 * For a view source window using viewSource.xul, the script viewSource.js in
 * the window extends an instance of this with more window specific functions.
 * The page script takes care of loading the companion frame script.
 *
 * For a view source tab (or some other non-window case), an instance of this is
 * created by viewSourceUtils.js to wrap the <browser>.  The caller that manages
 * the <browser> is responsible for ensuring the companion frame script has been
 * loaded.
 */
this.ViewSourceBrowser = function ViewSourceBrowser(aBrowser) {
  this._browser = aBrowser;
  this.init();
}

ViewSourceBrowser.prototype = {
  /**
   * The <browser> that will be displaying the view source content.
   */
  get browser() {
    return this._browser;
  },

  /**
   * These are the messages that ViewSourceBrowser will listen for
   * from the frame script it injects. Any message names added here
   * will automatically have ViewSourceBrowser listen for those messages,
   * and remove the listeners on teardown.
   */
  // TODO: Some messages will appear here in a later patch
  messages: [
  ],

  /**
   * This should be called as soon as the script loads. When this function
   * executes, we can assume the DOM content has not yet loaded.
   */
  init() {
    this.messages.forEach((msgName) => {
      this.mm.addMessageListener(msgName, this);
    });
  },

  /**
   * This should be called when the window is closing. This function should
   * clean up event and message listeners.
   */
  uninit() {
    this.messages.forEach((msgName) => {
      this.mm.removeMessageListener(msgName, this);
    });
  },

  /**
   * Anything added to the messages array will get handled here, and should
   * get dispatched to a specific function for the message name.
   */
  receiveMessage(message) {
    let data = message.data;

    // TODO: Some messages will appear here in a later patch
    switch(message.name) {
    }
  },

  /**
   * Getter for the message manager of the view source browser.
   */
  get mm() {
    return this.browser.messageManager;
  },

  /**
   * Send a message to the view source browser.
   */
  sendAsyncMessage(...args) {
    this.browser.messageManager.sendAsyncMessage(...args);
  },

  /**
   * Getter for the nsIWebNavigation of the view source browser.
   */
  get webNav() {
    return this.browser.webNavigation;
  },

  /**
   * Loads the source for a URL while applying some optional features if
   * enabled.
   *
   * For the viewSource.xul window, this is called by onXULLoaded above.
   * For view source in a specific browser, this is manually called after
   * this object is constructed.
   *
   * This takes a single object argument containing:
   *
   *   URL (required):
   *     A string URL for the page we'd like to view the source of.
   *   browser:
   *     The browser containing the document that we would like to view the
   *     source of. This argument is optional if outerWindowID is not passed.
   *   outerWindowID (optional):
   *     The outerWindowID of the content window containing the document that
   *     we want to view the source of. This is the only way of attempting to
   *     load the source out of the network cache.
   *   lineNumber (optional):
   *     The line number to focus on once the source is loaded.
   */
  loadViewSource({ URL, browser, outerWindowID, lineNumber }) {
    if (!URL) {
      throw new Error("Must supply a URL when opening view source.");
    }

    if (browser) {
      // If we're dealing with a remote browser, then the browser
      // for view source needs to be remote as well.
      this.updateBrowserRemoteness(browser.isRemoteBrowser);
    } else {
      if (outerWindowID) {
        throw new Error("Must supply the browser if passing the outerWindowID");
      }
    }

    this.sendAsyncMessage("ViewSource:LoadSource",
                          { URL, outerWindowID, lineNumber });
  },

  /**
   * Updates the "remote" attribute of the view source browser. This
   * will remove the browser from the DOM, and then re-add it in the
   * same place it was taken from.
   *
   * @param shouldBeRemote
   *        True if the browser should be made remote. If the browsers
   *        remoteness already matches this value, this function does
   *        nothing.
   */
  updateBrowserRemoteness(shouldBeRemote) {
    if (this.browser.isRemoteBrowser != shouldBeRemote) {
      // In this base case, where we are handed a <browser> someone else is
      // managing, we don't know for sure that it's safe to toggle remoteness.
      // For view source in a window, this is overridden to actually do the
      // flip if needed.
      throw new Error("View source browser's remoteness mismatch");
    }
  },

  /**
   * Load the view source browser from a selection in some document.
   *
   * @param selection
   *        A Selection object for the content of interest.
   */
  loadViewSourceFromSelection(selection) {
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
    var startPath = this._getPath(ancestorContainer, startContainer);
    var endPath = this._getPath(ancestorContainer, endContainer);

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
      for (i = startPath ? startPath.length-1 : -1; i >= 0; i--) {
        startContainer = startContainer.childNodes.item(startPath[i]);
      }
      for (i = endPath ? endPath.length-1 : -1; i >= 0; i--) {
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
      }
      else {
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
      }
      else {
        tmpNode = dataDoc.createTextNode(MARK_SELECTION_START);
        startContainer.insertBefore(tmpNode, startContainer.childNodes.item(startOffset));
      }
    }

    // now extract and display the syntax highlighted source
    tmpNode = dataDoc.createElementNS(NS_XHTML, "div");
    tmpNode.appendChild(ancestorContainer);

    // Tell content to draw a selection after the load below
    if (canDrawSelection) {
      this.sendAsyncMessage("ViewSource:ScheduleDrawSelection");
    }

    // all our content is held by the data:URI and URIs are internally stored as utf-8 (see nsIURI.idl)
    var loadFlags = Components.interfaces.nsIWebNavigation.LOAD_FLAGS_NONE;
    var referrerPolicy = Components.interfaces.nsIHttpChannel.REFERRER_POLICY_DEFAULT;
    this.webNav.loadURIWithOptions((isHTML ?
                                    "view-source:data:text/html;charset=utf-8," :
                                    "view-source:data:application/xml;charset=utf-8,")
                                   + encodeURIComponent(tmpNode.innerHTML),
                                   loadFlags,
                                   null, referrerPolicy,  // referrer
                                   null, null,  // postData, headers
                                   Services.io.newURI(doc.baseURI, null, null));
  },

  /**
   * A helper to get a path like FIXptr, but with an array instead of the
   * "tumbler" notation.
   * See FIXptr: http://lists.w3.org/Archives/Public/www-xml-linking-comments/2001AprJun/att-0074/01-NOTE-FIXptr-20010425.htm
   */
  _getPath(ancestor, node) {
    var n = node;
    var p = n.parentNode;
    if (n == ancestor || !p)
      return null;
    var path = new Array();
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
};
