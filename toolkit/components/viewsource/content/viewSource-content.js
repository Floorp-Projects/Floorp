/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

var { utils: Cu, interfaces: Ci, classes: Cc } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "DeferredTask",
  "resource://gre/modules/DeferredTask.jsm");

const NS_XHTML = "http://www.w3.org/1999/xhtml";
const BUNDLE_URL = "chrome://global/locale/viewSource.properties";

// These are markers used to delimit the selection during processing. They
// are removed from the final rendering.
// We use noncharacter Unicode codepoints to minimize the risk of clashing
// with anything that might legitimately be present in the document.
// U+FDD0..FDEF <noncharacters>
const MARK_SELECTION_START = "\uFDD0";
const MARK_SELECTION_END = "\uFDEF";

var global = this;

/**
 * ViewSourceContent should be loaded in the <xul:browser> of the
 * view source window, and initialized as soon as it has loaded.
 */
var ViewSourceContent = {
  /**
   * We'll act as an nsISelectionListener as well so that we can send
   * updates to the view source window's status bar.
   */
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISelectionListener]),

  /**
   * These are the messages that ViewSourceContent is prepared to listen
   * for. If you need ViewSourceContent to handle more messages, add them
   * here.
   */
  messages: [
    "ViewSource:LoadSource",
    "ViewSource:LoadSourceDeprecated",
    "ViewSource:LoadSourceWithSelection",
    "ViewSource:GoToLine",
    "ViewSource:ToggleWrapping",
    "ViewSource:ToggleSyntaxHighlighting",
    "ViewSource:SetCharacterSet",
  ],

  /**
   * When showing selection source, chrome will construct a page fragment to
   * show, and then instruct content to draw a selection after load.  This is
   * set true when there is a pending request to draw selection.
   */
  needsDrawSelection: false,

  /**
   * ViewSourceContent is attached as an nsISelectionListener on pageshow,
   * and removed on pagehide. When the initial about:blank is transitioned
   * away from, a pagehide is fired without us having attached ourselves
   * first. We use this boolean to keep track of whether or not we're
   * attached, so we don't attempt to remove our listener when it's not
   * yet there (which throws).
   */
  selectionListenerAttached: false,

  get isViewSource() {
    let uri = content.document.documentURI;
    return uri.startsWith("view-source:") ||
           (uri.startsWith("data:") && uri.includes("MathML"));
  },

  get isAboutBlank() {
    let uri = content.document.documentURI;
    return uri == "about:blank";
  },

  /**
   * This should be called as soon as this frame script has loaded.
   */
  init() {
    this.messages.forEach((msgName) => {
      addMessageListener(msgName, this);
    });

    addEventListener("pagehide", this, true);
    addEventListener("pageshow", this, true);
    addEventListener("click", this);
    addEventListener("unload", this);
    Services.els.addSystemEventListener(global, "contextmenu", this, false);
  },

  /**
   * This should be called when the frame script is being unloaded,
   * and the browser is tearing down.
   */
  uninit() {
    this.messages.forEach((msgName) => {
      removeMessageListener(msgName, this);
    });

    removeEventListener("pagehide", this, true);
    removeEventListener("pageshow", this, true);
    removeEventListener("click", this);
    removeEventListener("unload", this);

    Services.els.removeSystemEventListener(global, "contextmenu", this, false);

    // Cancel any pending toolbar updates.
    if (this.updateStatusTask) {
      this.updateStatusTask.disarm();
    }
  },

  /**
   * Anything added to the messages array will get handled here, and should
   * get dispatched to a specific function for the message name.
   */
  receiveMessage(msg) {
    if (!this.isViewSource && !this.isAboutBlank) {
      return;
    }
    let data = msg.data;
    let objects = msg.objects;
    switch (msg.name) {
      case "ViewSource:LoadSource":
        this.viewSource(data.URL, data.outerWindowID, data.lineNumber,
                        data.shouldWrap);
        break;
      case "ViewSource:LoadSourceDeprecated":
        this.viewSourceDeprecated(data.URL, objects.pageDescriptor, data.lineNumber,
                                  data.forcedCharSet);
        break;
      case "ViewSource:LoadSourceWithSelection":
        this.viewSourceWithSelection(data.URL, data.drawSelection, data.baseURI);
        break;
      case "ViewSource:GoToLine":
        this.goToLine(data.lineNumber);
        break;
      case "ViewSource:ToggleWrapping":
        this.toggleWrapping();
        break;
      case "ViewSource:ToggleSyntaxHighlighting":
        this.toggleSyntaxHighlighting();
        break;
      case "ViewSource:SetCharacterSet":
        this.setCharacterSet(data.charset, data.doPageLoad);
        break;
    }
  },

  /**
   * Any events should get handled here, and should get dispatched to
   * a specific function for the event type.
   */
  handleEvent(event) {
    if (!this.isViewSource) {
      return;
    }
    switch (event.type) {
      case "pagehide":
        this.onPageHide(event);
        break;
      case "pageshow":
        this.onPageShow(event);
        break;
      case "click":
        this.onClick(event);
        break;
      case "unload":
        this.uninit();
        break;
      case "contextmenu":
        this.onContextMenu(event);
        break;
    }
  },

  /**
   * A getter for the view source string bundle.
   */
  get bundle() {
    delete this.bundle;
    this.bundle = Services.strings.createBundle(BUNDLE_URL);
    return this.bundle;
  },

  /**
   * A shortcut to the nsISelectionController for the content.
   */
  get selectionController() {
    return docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsISelectionDisplay)
                   .QueryInterface(Ci.nsISelectionController);
  },

  /**
   * A shortcut to the nsIWebBrowserFind for the content.
   */
  get webBrowserFind() {
    return docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIWebBrowserFind);
  },

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
    let pageDescriptor, forcedCharSet;

    if (outerWindowID) {
      let contentWindow = Services.wm.getOuterWindowWithId(outerWindowID);
      let requestor = contentWindow.QueryInterface(Ci.nsIInterfaceRequestor);

      try {
        let otherWebNav = requestor.getInterface(Ci.nsIWebNavigation);
        pageDescriptor = otherWebNav.QueryInterface(Ci.nsIWebPageDescriptor)
                                    .currentDescriptor;
      } catch (e) {
        // We couldn't get the page descriptor, so we'll probably end up re-retrieving
        // this document off of the network.
      }

      let utils = requestor.getInterface(Ci.nsIDOMWindowUtils);
      let doc = contentWindow.document;
      forcedCharSet = utils.docCharsetIsForced ? doc.characterSet
                                               : null;
    }

    this.loadSource(URL, pageDescriptor, lineNumber, forcedCharSet);
  },

  /**
   * Called when the parent is using the deprecated API for viewSource.xul.
   * This function will throw if it's called on a remote browser.
   *
   * @param URL (required)
   *        The URL string of the source to be shown.
   * @param pageDescriptor (optional)
   *        The currentDescriptor off of an nsIWebPageDescriptor, in the
   *        event that the caller wants to try to load the source out of
   *        the network cache.
   * @param lineNumber (optional)
   *        The line number to focus as soon as the source has finished
   *        loading.
   * @param forcedCharSet (optional)
   *        The document character set to use instead of the default one.
   */
  viewSourceDeprecated(URL, pageDescriptor, lineNumber, forcedCharSet) {
    // This should not be called if this frame script is running
    // in a content process!
    if (Services.appinfo.processType != Services.appinfo.PROCESS_TYPE_DEFAULT) {
      throw new Error("ViewSource deprecated API should not be used with " +
                      "remote browsers.");
    }

    this.loadSource(URL, pageDescriptor, lineNumber, forcedCharSet);
  },

  /**
   * Common utility function used by both the current and deprecated APIs
   * for loading source.
   *
   * @param URL (required)
   *        The URL string of the source to be shown.
   * @param pageDescriptor (optional)
   *        The currentDescriptor off of an nsIWebPageDescriptor, in the
   *        event that the caller wants to try to load the source out of
   *        the network cache.
   * @param lineNumber (optional)
   *        The line number to focus as soon as the source has finished
   *        loading.
   * @param forcedCharSet (optional)
   *        The document character set to use instead of the default one.
   */
  loadSource(URL, pageDescriptor, lineNumber, forcedCharSet) {
    const viewSrcURL = "view-source:" + URL;

    if (forcedCharSet) {
      try {
        docShell.charset = forcedCharSet;
      } catch (e) { /* invalid charset */ }
    }

    if (lineNumber && lineNumber > 0) {
      let doneLoading = (event) => {
        // Ignore possible initial load of about:blank
        if (this.isAboutBlank ||
            !content.document.body) {
          return;
        }
        this.goToLine(lineNumber);
        removeEventListener("pageshow", doneLoading);
      };

      addEventListener("pageshow", doneLoading);
    }

    if (!pageDescriptor) {
      this.loadSourceFromURL(viewSrcURL);
      return;
    }

    try {
      let pageLoader = docShell.QueryInterface(Ci.nsIWebPageDescriptor);
      pageLoader.loadPage(pageDescriptor,
                          Ci.nsIWebPageDescriptor.DISPLAY_AS_SOURCE);
    } catch (e) {
      // We were not able to load the source from the network cache.
      this.loadSourceFromURL(viewSrcURL);
      return;
    }

    let shEntrySource = pageDescriptor.QueryInterface(Ci.nsISHEntry);
    let shEntry = Cc["@mozilla.org/browser/session-history-entry;1"]
                    .createInstance(Ci.nsISHEntry);
    shEntry.setURI(Services.io.newURI(viewSrcURL));
    shEntry.setTitle(viewSrcURL);
    let systemPrincipal = Services.scriptSecurityManager.getSystemPrincipal();
    shEntry.triggeringPrincipal = systemPrincipal;
    shEntry.loadType = Ci.nsIDocShellLoadInfo.loadHistory;
    shEntry.cacheKey = shEntrySource.cacheKey;
    docShell.QueryInterface(Ci.nsIWebNavigation)
            .sessionHistory
            .QueryInterface(Ci.nsISHistoryInternal)
            .addEntry(shEntry, true);
  },

  /**
   * Load some URL in the browser.
   *
   * @param URL
   *        The URL string to load.
   */
  loadSourceFromURL(URL) {
    let loadFlags = Ci.nsIWebNavigation.LOAD_FLAGS_NONE;
    let webNav = docShell.QueryInterface(Ci.nsIWebNavigation);
    webNav.loadURI(URL, loadFlags, null, null, null);
  },

  /**
   * This handler is for click events from:
   *   * error page content, which can show up if the user attempts to view the
   *     source of an attack page.
   *   * in-page context menu actions
   */
  onClick(event) {
    let target = event.originalTarget;
    // Check for content menu actions
    if (target.id) {
      this.contextMenuItems.forEach(itemSpec => {
        if (itemSpec.id !== target.id) {
          return;
        }
        itemSpec.handler.call(this, event);
        event.stopPropagation();
      });
    }

    // Don't trust synthetic events
    if (!event.isTrusted || event.target.localName != "button")
      return;

    let errorDoc = target.ownerDocument;

    if (/^about:blocked/.test(errorDoc.documentURI)) {
      // The event came from a button on a malware/phishing block page

      if (target == errorDoc.getElementById("goBackButton")) {
        // Instead of loading some safe page, just close the window
        sendAsyncMessage("ViewSource:Close");
      }
    }
  },

  /**
   * Handler for the pageshow event.
   *
   * @param event
   *        The pageshow event being handled.
   */
  onPageShow(event) {
    let selection = content.getSelection();
    if (selection) {
      selection.QueryInterface(Ci.nsISelectionPrivate)
               .addSelectionListener(this);
      this.selectionListenerAttached = true;
    }
    content.focus();

    // If we need to draw the selection, wait until an actual view source page
    // has loaded, instead of about:blank.
    if (this.needsDrawSelection &&
        content.document.documentURI.startsWith("view-source:")) {
      this.needsDrawSelection = false;
      this.drawSelection();
    }

    if (content.document.body) {
      this.injectContextMenu();
    }

    sendAsyncMessage("ViewSource:SourceLoaded");
  },

  /**
   * Handler for the pagehide event.
   *
   * @param event
   *        The pagehide event being handled.
   */
  onPageHide(event) {
    // The initial about:blank will fire pagehide before we
    // ever set a selectionListener, so we have a boolean around
    // to keep track of when the listener is attached.
    if (this.selectionListenerAttached) {
      content.getSelection()
             .QueryInterface(Ci.nsISelectionPrivate)
             .removeSelectionListener(this);
      this.selectionListenerAttached = false;
    }
    sendAsyncMessage("ViewSource:SourceUnloaded");
  },

  onContextMenu(event) {
    let node = event.target;

    let result = {
      isEmail: false,
      isLink: false,
      href: "",
      // We have to pass these in the event that we're running in
      // a remote browser, so that ViewSourceChrome knows where to
      // open the context menu.
      screenX: event.screenX,
      screenY: event.screenY,
    };

    if (node && node.localName == "a") {
      result.isLink = node.href.startsWith("view-source:");
      result.isEmail = node.href.startsWith("mailto:");
      result.href = node.href.substring(node.href.indexOf(":") + 1);
    }

    sendSyncMessage("ViewSource:ContextMenuOpening", result);
  },

  /**
   * Attempts to go to a particular line in the source code being
   * shown. If it succeeds in finding the line, it will fire a
   * "ViewSource:GoToLine:Success" message, passing up an object
   * with the lineNumber we just went to. If it cannot find the line,
   * it will fire a "ViewSource:GoToLine:Failed" message.
   *
   * @param lineNumber
   *        The line number to attempt to go to.
   */
  goToLine(lineNumber) {
    let body = content.document.body;

    // The source document is made up of a number of pre elements with
    // id attributes in the format <pre id="line123">, meaning that
    // the first line in the pre element is number 123.
    // Do binary search to find the pre element containing the line.
    // However, in the plain text case, we have only one pre without an
    // attribute, so assume it begins on line 1.
    let pre;
    for (let lbound = 0, ubound = body.childNodes.length; ; ) {
      let middle = (lbound + ubound) >> 1;
      pre = body.childNodes[middle];

      let firstLine = pre.id ? parseInt(pre.id.substring(4)) : 1;

      if (lbound == ubound - 1) {
        break;
      }

      if (lineNumber >= firstLine) {
        lbound = middle;
      } else {
        ubound = middle;
      }
    }

    let result = {};
    let found = this.findLocation(pre, lineNumber, null, -1, false, result);

    if (!found) {
      sendAsyncMessage("ViewSource:GoToLine:Failed");
      return;
    }

    let selection = content.getSelection();
    selection.removeAllRanges();

    // In our case, the range's startOffset is after "\n" on the previous line.
    // Tune the selection at the beginning of the next line and do some tweaking
    // to position the focusNode and the caret at the beginning of the line.
    selection.QueryInterface(Ci.nsISelectionPrivate)
      .interlinePosition = true;

    selection.addRange(result.range);

    if (!selection.isCollapsed) {
      selection.collapseToEnd();

      let offset = result.range.startOffset;
      let node = result.range.startContainer;
      if (offset < node.data.length) {
        // The same text node spans across the "\n", just focus where we were.
        selection.extend(node, offset);
      } else {
        // There is another tag just after the "\n", hook there. We need
        // to focus a safe point because there are edgy cases such as
        // <span>...\n</span><span>...</span> vs.
        // <span>...\n<span>...</span></span><span>...</span>
        node = node.nextSibling ? node.nextSibling : node.parentNode.nextSibling;
        selection.extend(node, 0);
      }
    }

    let selCon = this.selectionController;
    selCon.setDisplaySelection(Ci.nsISelectionController.SELECTION_ON);
    selCon.setCaretVisibilityDuringSelection(true);

    // Scroll the beginning of the line into view.
    selCon.scrollSelectionIntoView(
      Ci.nsISelectionController.SELECTION_NORMAL,
      Ci.nsISelectionController.SELECTION_FOCUS_REGION,
      true);

    sendAsyncMessage("ViewSource:GoToLine:Success", { lineNumber });
  },


  /**
   * Some old code from the original view source implementation. Original
   * documentation follows:
   *
   * "Loops through the text lines in the pre element. The arguments are either
   *  (pre, line) or (node, offset, interlinePosition). result is an out
   *  argument. If (pre, line) are specified (and node == null), result.range is
   *  a range spanning the specified line. If the (node, offset,
   *  interlinePosition) are specified, result.line and result.col are the line
   *  and column number of the specified offset in the specified node relative to
   *  the whole file."
   */
  findLocation(pre, lineNumber, node, offset, interlinePosition, result) {
    if (node && !pre) {
      // Look upwards to find the current pre element.
      for (pre = node;
           pre.nodeName != "PRE";
           pre = pre.parentNode);
    }

    // The source document is made up of a number of pre elements with
    // id attributes in the format <pre id="line123">, meaning that
    // the first line in the pre element is number 123.
    // However, in the plain text case, there is only one <pre> without an id,
    // so assume line 1.
    let curLine = pre.id ? parseInt(pre.id.substring(4)) : 1;

    // Walk through each of the text nodes and count newlines.
    let treewalker = content.document
        .createTreeWalker(pre, Ci.nsIDOMNodeFilter.SHOW_TEXT, null);

    // The column number of the first character in the current text node.
    let firstCol = 1;

    let found = false;
    for (let textNode = treewalker.firstChild();
         textNode && !found;
         textNode = treewalker.nextNode()) {

      // \r is not a valid character in the DOM, so we only check for \n.
      let lineArray = textNode.data.split(/\n/);
      let lastLineInNode = curLine + lineArray.length - 1;

      // Check if we can skip the text node without further inspection.
      if (node ? (textNode != node) : (lastLineInNode < lineNumber)) {
        if (lineArray.length > 1) {
          firstCol = 1;
        }
        firstCol += lineArray[lineArray.length - 1].length;
        curLine = lastLineInNode;
        continue;
      }

      // curPos is the offset within the current text node of the first
      // character in the current line.
      for (var i = 0, curPos = 0;
           i < lineArray.length;
           curPos += lineArray[i++].length + 1) {

        if (i > 0) {
          curLine++;
        }

        if (node) {
          if (offset >= curPos && offset <= curPos + lineArray[i].length) {
            // If we are right after the \n of a line and interlinePosition is
            // false, the caret looks as if it were at the end of the previous
            // line, so we display that line and column instead.

            if (i > 0 && offset == curPos && !interlinePosition) {
              result.line = curLine - 1;
              var prevPos = curPos - lineArray[i - 1].length;
              result.col = (i == 1 ? firstCol : 1) + offset - prevPos;
            } else {
              result.line = curLine;
              result.col = (i == 0 ? firstCol : 1) + offset - curPos;
            }
            found = true;

            break;
          }

        } else if (curLine == lineNumber && !("range" in result)) {
          result.range = content.document.createRange();
          result.range.setStart(textNode, curPos);

          // This will always be overridden later, except when we look for
          // the very last line in the file (this is the only line that does
          // not end with \n).
          result.range.setEndAfter(pre.lastChild);

        } else if (curLine == lineNumber + 1) {
          result.range.setEnd(textNode, curPos - 1);
          found = true;
          break;
        }
      }
    }

    return found || ("range" in result);
  },

  /**
   * Toggles the "wrap" class on the document body, which sets whether
   * or not long lines are wrapped.  Notifies parent to update the pref.
   */
  toggleWrapping() {
    let body = content.document.body;
    let state = body.classList.toggle("wrap");
    sendAsyncMessage("ViewSource:StoreWrapping", { state });
  },

  /**
   * Toggles the "highlight" class on the document body, which sets whether
   * or not syntax highlighting is displayed.  Notifies parent to update the
   * pref.
   */
  toggleSyntaxHighlighting() {
    let body = content.document.body;
    let state = body.classList.toggle("highlight");
    sendAsyncMessage("ViewSource:StoreSyntaxHighlighting", { state });
  },

  /**
   * Called when the parent has changed the character set to view the
   * source with.
   *
   * @param charset
   *        The character set to use.
   * @param doPageLoad
   *        Whether or not we should reload the page ourselves with the
   *        nsIWebPageDescriptor. Part of a workaround for bug 136322.
   */
  setCharacterSet(charset, doPageLoad) {
    docShell.charset = charset;
    if (doPageLoad) {
      this.reload();
    }
  },

  /**
   * Reloads the content.
   */
  reload() {
    let pageLoader = docShell.QueryInterface(Ci.nsIWebPageDescriptor);
    try {
      pageLoader.loadPage(pageLoader.currentDescriptor,
                          Ci.nsIWebPageDescriptor.DISPLAY_NORMAL);
    } catch (e) {
      let webNav = docShell.QueryInterface(Ci.nsIWebNavigation);
      webNav.reload(Ci.nsIWebNavigation.LOAD_FLAGS_NONE);
    }
  },

  /**
   * A reference to a DeferredTask that is armed every time the
   * selection changes.
   */
  updateStatusTask: null,

  /**
   * Called once the DeferredTask fires. Sends a message up to the
   * parent to update the status bar text.
   */
  updateStatus() {
    let selection = content.getSelection();

    if (!selection.focusNode) {
      sendAsyncMessage("ViewSource:UpdateStatus", { label: "" });
      return;
    }
    if (selection.focusNode.nodeType != Ci.nsIDOMNode.TEXT_NODE) {
      return;
    }

    let selCon = this.selectionController;
    selCon.setDisplaySelection(Ci.nsISelectionController.SELECTION_ON);
    selCon.setCaretVisibilityDuringSelection(true);

    let interlinePosition = selection.QueryInterface(Ci.nsISelectionPrivate)
                                     .interlinePosition;

    let result = {};
    this.findLocation(null, -1,
        selection.focusNode, selection.focusOffset, interlinePosition, result);

    let label = this.bundle.formatStringFromName("statusBarLineCol",
                                                 [result.line, result.col], 2);
    sendAsyncMessage("ViewSource:UpdateStatus", { label });
  },

  /**
   * Loads a view source selection showing the given view-source url and
   * highlight the selection.
   *
   * @param uri view-source uri to show
   * @param drawSelection true to highlight the selection
   * @param baseURI base URI of the original document
   */
  viewSourceWithSelection(uri, drawSelection, baseURI) {
    this.needsDrawSelection = drawSelection;

    // all our content is held by the data:URI and URIs are internally stored as utf-8 (see nsIURI.idl)
    let loadFlags = Ci.nsIWebNavigation.LOAD_FLAGS_NONE;
    let referrerPolicy = Ci.nsIHttpChannel.REFERRER_POLICY_UNSET;
    let webNav = docShell.QueryInterface(Ci.nsIWebNavigation);
    webNav.loadURIWithOptions(uri, loadFlags,
                              null, referrerPolicy,  // referrer
                              null, null,  // postData, headers
                              Services.io.newURI(baseURI));
  },

  /**
   * nsISelectionListener
   */

  /**
   * Gets called every time the selection is changed. Coalesces frequent
   * changes, and calls updateStatus after 100ms of no selection change
   * activity.
   */
  notifySelectionChanged(doc, sel, reason) {
    if (!this.updateStatusTask) {
      this.updateStatusTask = new DeferredTask(() => {
        this.updateStatus();
      }, 100);
    }

    this.updateStatusTask.arm();
  },

  /**
   * Using special markers left in the serialized source, this helper makes the
   * underlying markup of the selected fragment to automatically appear as
   * selected on the inflated view-source DOM.
   */
  drawSelection() {
    content.document.title =
      this.bundle.GetStringFromName("viewSelectionSourceTitle");

    // find the special selection markers that we added earlier, and
    // draw the selection between the two...
    var findService = null;
    try {
      // get the find service which stores the global find state
      findService = Cc["@mozilla.org/find/find_service;1"]
                    .getService(Ci.nsIFindService);
    } catch (e) { }
    if (!findService)
      return;

    // cache the current global find state
    var matchCase     = findService.matchCase;
    var entireWord    = findService.entireWord;
    var wrapFind      = findService.wrapFind;
    var findBackwards = findService.findBackwards;
    var searchString  = findService.searchString;
    var replaceString = findService.replaceString;

    // setup our find instance
    var findInst = this.webBrowserFind;
    findInst.matchCase = true;
    findInst.entireWord = false;
    findInst.wrapFind = true;
    findInst.findBackwards = false;

    // ...lookup the start mark
    findInst.searchString = MARK_SELECTION_START;
    var startLength = MARK_SELECTION_START.length;
    findInst.findNext();

    var selection = content.getSelection();
    if (!selection.rangeCount)
      return;

    var range = selection.getRangeAt(0);

    var startContainer = range.startContainer;
    var startOffset = range.startOffset;

    // ...lookup the end mark
    findInst.searchString = MARK_SELECTION_END;
    var endLength = MARK_SELECTION_END.length;
    findInst.findNext();

    var endContainer = selection.anchorNode;
    var endOffset = selection.anchorOffset;

    // reset the selection that find has left
    selection.removeAllRanges();

    // delete the special markers now...
    endContainer.deleteData(endOffset, endLength);
    startContainer.deleteData(startOffset, startLength);
    if (startContainer == endContainer)
      endOffset -= startLength; // has shrunk if on same text node...
    range.setEnd(endContainer, endOffset);

    // show the selection and scroll it into view
    selection.addRange(range);
    // the default behavior of the selection is to scroll at the end of
    // the selection, whereas in this situation, it is more user-friendly
    // to scroll at the beginning. So we override the default behavior here
    try {
      this.selectionController.scrollSelectionIntoView(
                                 Ci.nsISelectionController.SELECTION_NORMAL,
                                 Ci.nsISelectionController.SELECTION_ANCHOR_REGION,
                                 true);
    } catch (e) { }

    // restore the current find state
    findService.matchCase     = matchCase;
    findService.entireWord    = entireWord;
    findService.wrapFind      = wrapFind;
    findService.findBackwards = findBackwards;
    findService.searchString  = searchString;
    findService.replaceString = replaceString;

    findInst.matchCase     = matchCase;
    findInst.entireWord    = entireWord;
    findInst.wrapFind      = wrapFind;
    findInst.findBackwards = findBackwards;
    findInst.searchString  = searchString;
  },

  /**
   * In-page context menu items that are injected after page load.
   */
  contextMenuItems: [
    {
      id: "goToLine",
      accesskey: true,
      handler() {
        sendAsyncMessage("ViewSource:PromptAndGoToLine");
      }
    },
    {
      id: "wrapLongLines",
      get checked() {
        return Services.prefs.getBoolPref("view_source.wrap_long_lines");
      },
      handler() {
        this.toggleWrapping();
      }
    },
    {
      id: "highlightSyntax",
      get checked() {
        return Services.prefs.getBoolPref("view_source.syntax_highlight");
      },
      handler() {
        this.toggleSyntaxHighlighting();
      }
    },
  ],

  /**
   * Add context menu items for view source specific actions.
   */
  injectContextMenu() {
    let doc = content.document;

    let menu = doc.createElementNS(NS_XHTML, "menu");
    menu.setAttribute("type", "context");
    menu.setAttribute("id", "actions");
    doc.body.appendChild(menu);
    doc.body.setAttribute("contextmenu", "actions");

    this.contextMenuItems.forEach(itemSpec => {
      let item = doc.createElementNS(NS_XHTML, "menuitem");
      item.setAttribute("id", itemSpec.id);
      let labelName = `context_${itemSpec.id}_label`;
      let label = this.bundle.GetStringFromName(labelName);
      item.setAttribute("label", label);
      if ("checked" in itemSpec) {
        item.setAttribute("type", "checkbox");
      }
      if (itemSpec.accesskey) {
        let accesskeyName = `context_${itemSpec.id}_accesskey`;
        item.setAttribute("accesskey",
                          this.bundle.GetStringFromName(accesskeyName));
      }
      menu.appendChild(item);
    });

    this.updateContextMenu();
  },

  /**
   * Update state of checkbox-style context menu items.
   */
  updateContextMenu() {
    let doc = content.document;
    this.contextMenuItems.forEach(itemSpec => {
      if (!("checked" in itemSpec)) {
        return;
      }
      let item = doc.getElementById(itemSpec.id);
      if (itemSpec.checked) {
        item.setAttribute("checked", true);
      } else {
        item.removeAttribute("checked");
      }
    });
  },
};
ViewSourceContent.init();
