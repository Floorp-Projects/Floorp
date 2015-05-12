/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { utils: Cu, interfaces: Ci, classes: Cc } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "BrowserUtils",
  "resource://gre/modules/BrowserUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DeferredTask",
  "resource://gre/modules/DeferredTask.jsm");

const BUNDLE_URL = "chrome://global/locale/viewSource.properties";

let global = this;

/**
 * ViewSourceContent should be loaded in the <xul:browser> of the
 * view source window, and initialized as soon as it has loaded.
 */
let ViewSourceContent = {
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
    "ViewSource:GoToLine",
    "ViewSource:ToggleWrapping",
    "ViewSource:ToggleSyntaxHighlighting",
    "ViewSource:SetCharacterSet",
  ],

  /**
   * ViewSourceContent is attached as an nsISelectionListener on pageshow,
   * and removed on pagehide. When the initial about:blank is transitioned
   * away from, a pagehide is fired without us having attached ourselves
   * first. We use this boolean to keep track of whether or not we're
   * attached, so we don't attempt to remove our listener when it's not
   * yet there (which throws).
   */
  selectionListenerAttached: false,

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
    let data = msg.data;
    let objects = msg.objects;
    switch(msg.name) {
      case "ViewSource:LoadSource":
        this.viewSource(data.URL, data.outerWindowID, data.lineNumber,
                        data.shouldWrap);
        break;
      case "ViewSource:LoadSourceDeprecated":
        this.viewSourceDeprecated(data.URL, objects.pageDescriptor, data.lineNumber,
                                  data.forcedCharSet);
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
    switch(event.type) {
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
      } catch(e) {
        // We couldn't get the page descriptor, so we'll probably end up re-retrieving
        // this document off of the network.
      }

      let utils = requestor.getInterface(Ci.nsIDOMWindowUtils);
      let doc = contentWindow.document;
      let forcedCharSet = utils.docCharsetIsForced ? doc.characterSet
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
    let loadFromURL = false;

    if (forcedCharSet) {
      docShell.charset = forcedCharSet;
    }

    if (lineNumber) {
      let doneLoading = (event) => {
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
    } catch(e) {
      // We were not able to load the source from the network cache.
      this.loadSourceFromURL(viewSrcURL);
      return;
    }

    let shEntrySource = pageDescriptor.QueryInterface(Ci.nsISHEntry);
    let shEntry = Cc["@mozilla.org/browser/session-history-entry;1"]
                    .createInstance(Ci.nsISHEntry);
    shEntry.setURI(BrowserUtils.makeURI(viewSrcURL, null, null));
    shEntry.setTitle(viewSrcURL);
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
   * This handler is specifically for click events bubbling up from
   * error page content, which can show up if the user attempts to
   * view the source of an attack page.
   */
  onClick(event) {
    // Don't trust synthetic events
    if (!event.isTrusted || event.target.localName != "button")
      return;

    let target = event.originalTarget;
    let errorDoc = target.ownerDocument;

    if (/^about:blocked/.test(errorDoc.documentURI)) {
      // The event came from a button on a malware/phishing block page

      if (target == errorDoc.getElementById("getMeOutButton")) {
        // Instead of loading some safe page, just close the window
        sendAsyncMessage("ViewSource:Close");
      } else if (target == errorDoc.getElementById("reportButton")) {
        // This is the "Why is this site blocked" button. We redirect
        // to the generic page describing phishing/malware protection.
        let URL = Services.urlFormatter.formatURLPref("app.support.baseURL");
        sendAsyncMessage("ViewSource:OpenURL", { URL })
      } else if (target == errorDoc.getElementById("ignoreWarningButton")) {
        // Allow users to override and continue through to the site
        docShell.QueryInterface(Ci.nsIWebNavigation)
                .loadURIWithOptions(content.location.href,
                                    Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CLASSIFIER,
                                    null, Ci.nsIHttpChannel.REFERRER_POLICY_DEFAULT,
                                    null, null, null);
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
    content.getSelection()
           .QueryInterface(Ci.nsISelectionPrivate)
           .addSelectionListener(this);
    this.selectionListenerAttached = true;

    content.focus();
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
    let addonInfo = {};
    let subject = {
      event: event,
      addonInfo: addonInfo,
    };

    subject.wrappedJSObject = subject;
    Services.obs.notifyObservers(subject, "content-contextmenu", null);

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
      }
      else {
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

        } else {
          if (curLine == lineNumber && !("range" in result)) {
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
    }

    return found || ("range" in result);
  },

  /**
   * Toggles the "wrap" class on the document body, which sets whether
   * or not long lines are wrapped.
   */
  toggleWrapping() {
    let body = content.document.body;
    body.classList.toggle("wrap");
  },

  /**
   * Called when the parent has changed the syntax highlighting pref.
   */
  toggleSyntaxHighlighting() {
    // The parent process should have set the view_source.syntax_highlight
    // pref to the desired value. The reload brings that setting into
    // effect.
    this.reload();
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
    } catch(e) {
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
};
ViewSourceContent.init();
