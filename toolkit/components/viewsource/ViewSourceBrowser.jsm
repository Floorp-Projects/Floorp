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

const NS_XHTML = "http://www.w3.org/1999/xhtml";
const VIEW_SOURCE_CSS = "resource://gre-resources/viewsource.css";
const BUNDLE_URL = "chrome://global/locale/viewSource.properties";

// These are markers used to delimit the selection during processing. They
// are removed from the final rendering.
// We use noncharacter Unicode codepoints to minimize the risk of clashing
// with anything that might legitimately be present in the document.
// U+FDD0..FDEF <noncharacters>
const MARK_SELECTION_START = "\uFDD0";
const MARK_SELECTION_END = "\uFDEF";

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
   * Holds the value of the last line found via the "Go to line"
   * command, to pre-populate the prompt the next time it is
   * opened.
   */
  lastLineFound: null,

  /**
   * These are the messages that ViewSourceBrowser will listen for
   * from the frame script it injects. Any message names added here
   * will automatically have ViewSourceBrowser listen for those messages,
   * and remove the listeners on teardown.
   */
  messages: [
    "ViewSource:PromptAndGoToLine",
    "ViewSource:GoToLine:Success",
    "ViewSource:GoToLine:Failed",
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

    switch(message.name) {
      case "ViewSource:PromptAndGoToLine":
        this.promptAndGoToLine();
        break;
      case "ViewSource:GoToLine:Success":
        this.onGoToLineSuccess(data.lineNumber);
        break;
      case "ViewSource:GoToLine:Failed":
        this.onGoToLineFailed();
        break;
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
   * Getter for whether long lines should be wrapped.
   */
  get wrapLongLines() {
    return Services.prefs.getBoolPref("view_source.wrap_long_lines");
  },

  /**
   * A getter for the view source string bundle.
   */
  get bundle() {
    if (this._bundle) {
      return this._bundle;
    }
    return this._bundle = Services.strings.createBundle(BUNDLE_URL);
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

  /**
   * Load the view source browser from a fragment of some document, as in
   * markups such as MathML where reformatting the output is helpful.
   *
   * @param aNode
   *        Some element within the fragment of interest.
   * @param aContext
   *        A string denoting the type of fragment.  Currently, "mathml" is the
   *        only accepted value.
   */
  loadViewSourceFromFragment(node, context) {
    var Node = node.ownerDocument.defaultView.Node;
    this._lineCount = 0;
    this._startTargetLine = 0;
    this._endTargetLine = 0;
    this._targetNode = node;
    if (this._targetNode && this._targetNode.nodeType == Node.TEXT_NODE)
      this._targetNode = this._targetNode.parentNode;

    // walk up the tree to the top-level element (e.g., <math>, <svg>)
    var topTag;
    if (context == "mathml")
      topTag = "math";
    else
      throw "not reached";
    var topNode = this._targetNode;
    while (topNode && topNode.localName != topTag)
      topNode = topNode.parentNode;
    if (!topNode)
      return;

    // serialize
    var title = this.bundle.GetStringFromName("viewMathMLSourceTitle");
    var wrapClass = this.wrapLongLines ? ' class="wrap"' : '';
    var source =
      '<!DOCTYPE html>'
    + '<html>'
    + '<head><title>' + title + '</title>'
    + '<link rel="stylesheet" type="text/css" href="' + VIEW_SOURCE_CSS + '">'
    + '<style type="text/css">'
    + '#target { border: dashed 1px; background-color: lightyellow; }'
    + '</style>'
    + '</head>'
    + '<body id="viewsource"' + wrapClass
    +        ' onload="document.title=\''+title+'\'; document.getElementById(\'target\').scrollIntoView(true)">'
    + '<pre>'
    + this._getOuterMarkup(topNode, 0)
    + '</pre></body></html>'
    ; // end

    // display
    this.browser.loadURI("data:text/html;charset=utf-8," +
                         encodeURIComponent(source));
  },

  _getInnerMarkup(node, indent) {
    var str = '';
    for (var i = 0; i < node.childNodes.length; i++) {
      str += this._getOuterMarkup(node.childNodes.item(i), indent);
    }
    return str;
  },

  _getOuterMarkup(node, indent) {
    var Node = node.ownerDocument.defaultView.Node;
    var newline = "";
    var padding = "";
    var str = "";
    if (node == this._targetNode) {
      this._startTargetLine = this._lineCount;
      str += '</pre><pre id="target">';
    }

    switch (node.nodeType) {
    case Node.ELEMENT_NODE: // Element
      // to avoid the wide gap problem, '\n' is not emitted on the first
      // line and the lines before & after the <pre id="target">...</pre>
      if (this._lineCount > 0 &&
          this._lineCount != this._startTargetLine &&
          this._lineCount != this._endTargetLine) {
        newline = "\n";
      }
      this._lineCount++;
      for (var k = 0; k < indent; k++) {
        padding += " ";
      }
      str += newline + padding
          +  '&lt;<span class="start-tag">' + node.nodeName + '</span>';
      for (var i = 0; i < node.attributes.length; i++) {
        var attr = node.attributes.item(i);
        if (attr.nodeName.match(/^[-_]moz/)) {
          continue;
        }
        str += ' <span class="attribute-name">'
            +  attr.nodeName
            +  '</span>=<span class="attribute-value">"'
            +  this._unicodeToEntity(attr.nodeValue)
            +  '"</span>';
      }
      if (!node.hasChildNodes()) {
        str += "/&gt;";
      }
      else {
        str += "&gt;";
        var oldLine = this._lineCount;
        str += this._getInnerMarkup(node, indent + 2);
        if (oldLine == this._lineCount) {
          newline = "";
          padding = "";
        }
        else {
          newline = (this._lineCount == this._endTargetLine) ? "" : "\n";
          this._lineCount++;
        }
        str += newline + padding
            +  '&lt;/<span class="end-tag">' + node.nodeName + '</span>&gt;';
      }
      break;
    case Node.TEXT_NODE: // Text
      var tmp = node.nodeValue;
      tmp = tmp.replace(/(\n|\r|\t)+/g, " ");
      tmp = tmp.replace(/^ +/, "");
      tmp = tmp.replace(/ +$/, "");
      if (tmp.length != 0) {
        str += '<span class="text">' + this._unicodeToEntity(tmp) + '</span>';
      }
      break;
    default:
      break;
    }

    if (node == this._targetNode) {
      this._endTargetLine = this._lineCount;
      str += '</pre><pre>';
    }
    return str;
  },

  _unicodeToEntity(text) {
    const charTable = {
      '&': '&amp;<span class="entity">amp;</span>',
      '<': '&amp;<span class="entity">lt;</span>',
      '>': '&amp;<span class="entity">gt;</span>',
      '"': '&amp;<span class="entity">quot;</span>'
    };

    function charTableLookup(letter) {
      return charTable[letter];
    }

    function convertEntity(letter) {
      try {
        var unichar = this._entityConverter
                          .ConvertToEntity(letter, entityVersion);
        var entity = unichar.substring(1); // extract '&'
        return '&amp;<span class="entity">' + entity + '</span>';
      } catch (ex) {
        return letter;
      }
    }

    if (!this._entityConverter) {
      try {
        this._entityConverter = Cc["@mozilla.org/intl/entityconverter;1"]
                                  .createInstance(Ci.nsIEntityConverter);
      } catch(e) { }
    }

    const entityVersion = Ci.nsIEntityConverter.entityW3C;

    var str = text;

    // replace chars in our charTable
    str = str.replace(/[<>&"]/g, charTableLookup);

    // replace chars > 0x7f via nsIEntityConverter
    str = str.replace(/[^\0-\u007f]/g, convertEntity);

    return str;
  },

  /**
   * Opens the "Go to line" prompt for a user to hop to a particular line
   * of the source code they're viewing. This will keep prompting until the
   * user either cancels out of the prompt, or enters a valid line number.
   */
  promptAndGoToLine() {
    let input = { value: this.lastLineFound };
    let window = Services.wm.getMostRecentWindow(null);

    let ok = Services.prompt.prompt(
        window,
        this.bundle.GetStringFromName("goToLineTitle"),
        this.bundle.GetStringFromName("goToLineText"),
        input,
        null,
        {value:0});

    if (!ok)
      return;

    let line = parseInt(input.value, 10);

    if (!(line > 0)) {
      Services.prompt.alert(window,
                            this.bundle.GetStringFromName("invalidInputTitle"),
                            this.bundle.GetStringFromName("invalidInputText"));
      this.promptAndGoToLine();
    } else {
      this.goToLine(line);
    }
  },

  /**
   * Go to a particular line of the source code. This act is asynchronous.
   *
   * @param lineNumber
   *        The line number to try to go to to.
   */
  goToLine(lineNumber) {
    this.sendAsyncMessage("ViewSource:GoToLine", { lineNumber });
  },

  /**
   * Called when the frame script reports that a line was successfully gotten
   * to.
   *
   * @param lineNumber
   *        The line number that we successfully got to.
   */
  onGoToLineSuccess(lineNumber) {
    // We'll pre-populate the "Go to line" prompt with this value the next
    // time it comes up.
    this.lastLineFound = lineNumber;
  },

  /**
   * Called when the frame script reports that we failed to go to a particular
   * line. This informs the user that their selection was likely out of range,
   * and then reprompts the user to try again.
   */
  onGoToLineFailed() {
    let window = Services.wm.getMostRecentWindow(null);
    Services.prompt.alert(window,
                          this.bundle.GetStringFromName("outOfRangeTitle"),
                          this.bundle.GetStringFromName("outOfRangeText"));
    this.promptAndGoToLine();
  },
};
