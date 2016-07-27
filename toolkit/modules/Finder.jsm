// vim: set ts=2 sw=2 sts=2 tw=80:
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

this.EXPORTED_SYMBOLS = ["Finder","GetClipboardSearchString"];

const { interfaces: Ci, classes: Cc, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Geometry.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "TextToSubURIService",
                                         "@mozilla.org/intl/texttosuburi;1",
                                         "nsITextToSubURI");
XPCOMUtils.defineLazyServiceGetter(this, "Clipboard",
                                         "@mozilla.org/widget/clipboard;1",
                                         "nsIClipboard");
XPCOMUtils.defineLazyServiceGetter(this, "ClipboardHelper",
                                         "@mozilla.org/widget/clipboardhelper;1",
                                         "nsIClipboardHelper");

const kSelectionMaxLen = 150;

function Finder(docShell) {
  this._fastFind = Cc["@mozilla.org/typeaheadfind;1"].createInstance(Ci.nsITypeAheadFind);
  this._fastFind.init(docShell);

  this._docShell = docShell;
  this._listeners = [];
  this._previousLink = null;
  this._searchString = null;
  this._highlighter = null;

  docShell.QueryInterface(Ci.nsIInterfaceRequestor)
          .getInterface(Ci.nsIWebProgress)
          .addProgressListener(this, Ci.nsIWebProgress.NOTIFY_LOCATION);
}

Finder.prototype = {
  destroy: function() {
    if (this._highlighter) {
      this._highlighter.clear();
      this._highlighter.hide();
    }
    this.listeners = [];
    this._docShell.QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebProgress)
      .removeProgressListener(this, Ci.nsIWebProgress.NOTIFY_LOCATION);
    this._listeners = [];
    this._fastFind = this._docShell = this._previousLink = this._highlighter = null;
  },

  addResultListener: function (aListener) {
    if (this._listeners.indexOf(aListener) === -1)
      this._listeners.push(aListener);
  },

  removeResultListener: function (aListener) {
    this._listeners = this._listeners.filter(l => l != aListener);
  },

  _notify: function (options) {
    if (typeof options.storeResult != "boolean")
      options.storeResult = true;

    if (options.storeResult) {
      this._searchString = options.searchString;
      this.clipboardSearchString = options.searchString
    }
    this._outlineLink(options.drawOutline);

    let foundLink = this._fastFind.foundLink;
    let linkURL = null;
    if (foundLink) {
      let docCharset = null;
      let ownerDoc = foundLink.ownerDocument;
      if (ownerDoc)
        docCharset = ownerDoc.characterSet;

      linkURL = TextToSubURIService.unEscapeURIForUI(docCharset, foundLink.href);
    }

    options.linkURL = linkURL;
    options.rect = this._getResultRect();
    options.searchString = this._searchString;

    this.highlighter.update(options);

    for (let l of this._listeners) {
      try {
        l.onFindResult(options);
      } catch (ex) {}
    }
  },

  get searchString() {
    if (!this._searchString && this._fastFind.searchString)
      this._searchString = this._fastFind.searchString;
    return this._searchString;
  },

  get clipboardSearchString() {
    return GetClipboardSearchString(this._getWindow()
                                        .QueryInterface(Ci.nsIInterfaceRequestor)
                                        .getInterface(Ci.nsIWebNavigation)
                                        .QueryInterface(Ci.nsILoadContext));
  },

  set clipboardSearchString(aSearchString) {
    if (!aSearchString || !Clipboard.supportsFindClipboard())
      return;

    ClipboardHelper.copyStringToClipboard(aSearchString,
                                          Ci.nsIClipboard.kFindClipboard);
  },

  set caseSensitive(aSensitive) {
    this._fastFind.caseSensitive = aSensitive;
  },

  set entireWord(aEntireWord) {
    this._fastFind.entireWord = aEntireWord;
  },

  get highlighter() {
    if (this._highlighter)
      return this._highlighter;

    const {FinderHighlighter} = Cu.import("resource://gre/modules/FinderHighlighter.jsm", {});
    return this._highlighter = new FinderHighlighter(this);
  },

  _lastFindResult: null,

  /**
   * Used for normal search operations, highlights the first match.
   *
   * @param aSearchString String to search for.
   * @param aLinksOnly Only consider nodes that are links for the search.
   * @param aDrawOutline Puts an outline around matched links.
   */
  fastFind: function (aSearchString, aLinksOnly, aDrawOutline) {
    this._lastFindResult = this._fastFind.find(aSearchString, aLinksOnly);
    let searchString = this._fastFind.searchString;
    this._notify({
      searchString,
      result: this._lastFindResult,
      findBackwards: false,
      findAgain: false,
      drawOutline: aDrawOutline
    });
  },

  /**
   * Repeat the previous search. Should only be called after a previous
   * call to Finder.fastFind.
   *
   * @param aFindBackwards Controls the search direction:
   *    true: before current match, false: after current match.
   * @param aLinksOnly Only consider nodes that are links for the search.
   * @param aDrawOutline Puts an outline around matched links.
   */
  findAgain: function (aFindBackwards, aLinksOnly, aDrawOutline) {
    this._lastFindResult = this._fastFind.findAgain(aFindBackwards, aLinksOnly);
    let searchString = this._fastFind.searchString;
    this._notify({
      searchString,
      result: this._lastFindResult,
      findBackwards: aFindBackwards,
      fidnAgain: true,
      drawOutline: aDrawOutline
    });
  },

  /**
   * Forcibly set the search string of the find clipboard to the currently
   * selected text in the window, on supported platforms (i.e. OSX).
   */
  setSearchStringToSelection: function() {
    let searchString = this.getActiveSelectionText();

    // Empty strings are rather useless to search for.
    if (!searchString.length)
      return null;

    this.clipboardSearchString = searchString;
    return searchString;
  },

  highlight: Task.async(function* (aHighlight, aWord) {
    this.highlighter.maybeAbort();

    let found = yield this.highlighter.highlight(aHighlight, aWord, null);
    this.highlighter.notifyFinished(aHighlight);
    if (aHighlight) {
      let result = found ? Ci.nsITypeAheadFind.FIND_FOUND
                         : Ci.nsITypeAheadFind.FIND_NOTFOUND;
      this._notify({
        searchString: aWord,
        result,
        findBackwards: false,
        findAgain: false,
        drawOutline: false,
        storeResult: false
      });
    }
  }),

  getInitialSelection: function() {
    this._getWindow().setTimeout(() => {
      let initialSelection = this.getActiveSelectionText();
      for (let l of this._listeners) {
        try {
          l.onCurrentSelection(initialSelection, true);
        } catch (ex) {}
      }
    }, 0);
  },

  getActiveSelectionText: function() {
    let focusedWindow = {};
    let focusedElement =
      Services.focus.getFocusedElementForWindow(this._getWindow(), true,
                                                focusedWindow);
    focusedWindow = focusedWindow.value;

    let selText;

    if (focusedElement instanceof Ci.nsIDOMNSEditableElement &&
        focusedElement.editor) {
      // The user may have a selection in an input or textarea.
      selText = focusedElement.editor.selectionController
        .getSelection(Ci.nsISelectionController.SELECTION_NORMAL)
        .toString();
    } else {
      // Look for any selected text on the actual page.
      selText = focusedWindow.getSelection().toString();
    }

    if (!selText)
      return "";

    // Process our text to get rid of unwanted characters.
    selText = selText.trim().replace(/\s+/g, " ");
    let truncLength = kSelectionMaxLen;
    if (selText.length > truncLength) {
      let truncChar = selText.charAt(truncLength).charCodeAt(0);
      if (truncChar >= 0xDC00 && truncChar <= 0xDFFF)
        truncLength++;
      selText = selText.substr(0, truncLength);
    }

    return selText;
  },

  enableSelection: function() {
    this._fastFind.setSelectionModeAndRepaint(Ci.nsISelectionController.SELECTION_ON);
    this._restoreOriginalOutline();
  },

  removeSelection: function() {
    this._fastFind.collapseSelection();
    this.enableSelection();
    this.highlighter.clear();
  },

  focusContent: function() {
    // Allow Finder listeners to cancel focusing the content.
    for (let l of this._listeners) {
      try {
        if ("shouldFocusContent" in l &&
            !l.shouldFocusContent())
          return;
      } catch (ex) {
        Cu.reportError(ex);
      }
    }

    let fastFind = this._fastFind;
    const fm = Cc["@mozilla.org/focus-manager;1"].getService(Ci.nsIFocusManager);
    try {
      // Try to find the best possible match that should receive focus and
      // block scrolling on focus since find already scrolls. Further
      // scrolling is due to user action, so don't override this.
      if (fastFind.foundLink) {
        fm.setFocus(fastFind.foundLink, fm.FLAG_NOSCROLL);
      } else if (fastFind.foundEditable) {
        fm.setFocus(fastFind.foundEditable, fm.FLAG_NOSCROLL);
        fastFind.collapseSelection();
      } else {
        this._getWindow().focus()
      }
    } catch (e) {}
  },

  onFindbarClose: function() {
    this.enableSelection();
    this.highlighter.highlight(false);
  },

  onModalHighlightChange(useModalHighlight) {
    if (this._highlighter)
      this._highlighter.onModalHighlightChange(useModalHighlight);
  },

  keyPress: function (aEvent) {
    let controller = this._getSelectionController(this._getWindow());

    switch (aEvent.keyCode) {
      case Ci.nsIDOMKeyEvent.DOM_VK_RETURN:
        if (this._fastFind.foundLink) {
          let view = this._fastFind.foundLink.ownerDocument.defaultView;
          this._fastFind.foundLink.dispatchEvent(new view.MouseEvent("click", {
            view: view,
            cancelable: true,
            bubbles: true,
            ctrlKey: aEvent.ctrlKey,
            altKey: aEvent.altKey,
            shiftKey: aEvent.shiftKey,
            metaKey: aEvent.metaKey
          }));
        }
        break;
      case Ci.nsIDOMKeyEvent.DOM_VK_TAB:
        let direction = Services.focus.MOVEFOCUS_FORWARD;
        if (aEvent.shiftKey) {
          direction = Services.focus.MOVEFOCUS_BACKWARD;
        }
        Services.focus.moveFocus(this._getWindow(), null, direction, 0);
        break;
      case Ci.nsIDOMKeyEvent.DOM_VK_PAGE_UP:
        controller.scrollPage(false);
        break;
      case Ci.nsIDOMKeyEvent.DOM_VK_PAGE_DOWN:
        controller.scrollPage(true);
        break;
      case Ci.nsIDOMKeyEvent.DOM_VK_UP:
        controller.scrollLine(false);
        break;
      case Ci.nsIDOMKeyEvent.DOM_VK_DOWN:
        controller.scrollLine(true);
        break;
    }
  },

  _notifyMatchesCount: function(result) {
    for (let l of this._listeners) {
      try {
        l.onMatchesCountResult(result);
      } catch (ex) {}
    }
  },

  requestMatchesCount: function(aWord, aMatchLimit, aLinksOnly) {
    if (this._lastFindResult == Ci.nsITypeAheadFind.FIND_NOTFOUND ||
        this.searchString == "") {
      return this._notifyMatchesCount({
        total: 0,
        current: 0
      });
    }
    let window = this._getWindow();
    let result = this._countMatchesInWindow(aWord, aMatchLimit, aLinksOnly, window);

    // Count matches in (i)frames AFTER searching through the main window.
    for (let frame of result._framesToCount) {
      // We've reached our limit; no need to do more work.
      if (result.total == -1 || result.total == aMatchLimit)
        break;
      this._countMatchesInWindow(aWord, aMatchLimit, aLinksOnly, frame, result);
    }

    // The `_currentFound` and `_framesToCount` properties are only used for
    // internal bookkeeping between recursive calls.
    delete result._currentFound;
    delete result._framesToCount;

    this._notifyMatchesCount(result);
    return undefined;
  },

  /**
   * Counts the number of matches for the searched word in the passed window's
   * content.
   * @param aWord
   *        the word to search for.
   * @param aMatchLimit
   *        the maximum number of matches shown (for speed reasons).
   * @param aLinksOnly
   *        whether we should only search through links.
   * @param aWindow
   *        the window to search in. Passing undefined will search the
   *        current content window. Optional.
   * @param aStats
   *        the Object that is returned by this function. It may be passed as an
   *        argument here in the case of a recursive call.
   * @returns an object stating the number of matches and a vector for the current match.
   */
  _countMatchesInWindow: function(aWord, aMatchLimit, aLinksOnly, aWindow = null, aStats = null) {
    aWindow = aWindow || this._getWindow();
    aStats = aStats || {
      total: 0,
      current: 0,
      _framesToCount: new Set(),
      _currentFound: false
    };

    // If we already reached our max, there's no need to do more work!
    if (aStats.total == -1 || aStats.total == aMatchLimit) {
      aStats.total = -1;
      return aStats;
    }

    this._collectFrames(aWindow, aStats);

    let foundRange = this._fastFind.getFoundRange();

    for(let range of this._findIterator(aWord, aWindow)) {
      if (!aLinksOnly || this._rangeStartsInLink(range)) {
        ++aStats.total;
        if (!aStats._currentFound) {
          ++aStats.current;
          aStats._currentFound = (foundRange &&
            range.startContainer == foundRange.startContainer &&
            range.startOffset == foundRange.startOffset &&
            range.endContainer == foundRange.endContainer &&
            range.endOffset == foundRange.endOffset);
        }
      }
      if (aStats.total == aMatchLimit) {
        aStats.total = -1;
        break;
      }
    }

    return aStats;
  },

  /**
   * Basic wrapper around nsIFind that provides a generator yielding
   * a range each time an occurence of `aWord` string is found.
   *
   * @param aWord
   *        the word to search for.
   * @param aWindow
   *        the window to search in.
   */
  _findIterator: function* (aWord, aWindow) {
    let doc = aWindow.document;
    let body = (doc instanceof Ci.nsIDOMHTMLDocument && doc.body) ?
               doc.body : doc.documentElement;

    if (!body)
      return;

    let searchRange = doc.createRange();
    searchRange.selectNodeContents(body);

    let startPt = searchRange.cloneRange();
    startPt.collapse(true);

    let endPt = searchRange.cloneRange();
    endPt.collapse(false);

    let retRange = null;

    let finder = Cc["@mozilla.org/embedcomp/rangefind;1"]
                   .createInstance()
                   .QueryInterface(Ci.nsIFind);
    finder.caseSensitive = this._fastFind.caseSensitive;
    finder.entireWord = this._fastFind.entireWord;

    while ((retRange = finder.Find(aWord, searchRange, startPt, endPt))) {
      yield retRange;
      startPt = retRange.cloneRange();
      startPt.collapse(false);
    }
  },

  /**
   * Helper method for `_countMatchesInWindow` that recursively collects all
   * visible (i)frames inside a window.
   *
   * @param aWindow
   *        the window to extract the (i)frames from.
   * @param aStats
   *        Object that contains a Set called '_framesToCount'
   */
  _collectFrames: function(aWindow, aStats) {
    if (!aWindow.frames || !aWindow.frames.length)
      return;
    // Casting `aWindow.frames` to an Iterator doesn't work, so we're stuck with
    // a plain, old for-loop.
    for (let i = 0, l = aWindow.frames.length; i < l; ++i) {
      let frame = aWindow.frames[i];
      // Don't count matches in hidden frames.
      let frameEl = frame && frame.frameElement;
      if (!frameEl)
        continue;
      // Construct a range around the frame element to check its visiblity.
      let range = aWindow.document.createRange();
      range.setStart(frameEl, 0);
      range.setEnd(frameEl, 0);
      if (!this._fastFind.isRangeVisible(range, this._getDocShell(range), true))
        continue;
      // All good, so add it to the set to count later.
      if (!aStats._framesToCount.has(frame))
        aStats._framesToCount.add(frame);
      this._collectFrames(frame, aStats);
    }
  },

  /**
   * Helper method to extract the docShell reference from a Window or Range object.
   *
   * @param aWindowOrRange
   *        Window object to query. May also be a Range, from which the owner
   *        window will be queried.
   * @returns nsIDocShell
   */
  _getDocShell: function(aWindowOrRange) {
    let window = aWindowOrRange;
    // Ranges may also be passed in, so fetch its window.
    if (aWindowOrRange instanceof Ci.nsIDOMRange)
      window = aWindowOrRange.startContainer.ownerDocument.defaultView;
    return window.QueryInterface(Ci.nsIInterfaceRequestor)
                 .getInterface(Ci.nsIWebNavigation)
                 .QueryInterface(Ci.nsIDocShell);
  },

  _getWindow: function () {
    return this._docShell.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindow);
  },

  /**
   * Get the bounding selection rect in CSS px relative to the origin of the
   * top-level content document.
   */
  _getResultRect: function () {
    let topWin = this._getWindow();
    let win = this._fastFind.currentWindow;
    if (!win)
      return null;

    let selection = win.getSelection();
    if (!selection.rangeCount || selection.isCollapsed) {
      // The selection can be into an input or a textarea element.
      let nodes = win.document.querySelectorAll("input, textarea");
      for (let node of nodes) {
        if (node instanceof Ci.nsIDOMNSEditableElement && node.editor) {
          try {
            let sc = node.editor.selectionController;
            selection = sc.getSelection(Ci.nsISelectionController.SELECTION_NORMAL);
            if (selection.rangeCount && !selection.isCollapsed) {
              break;
            }
          } catch (e) {
            // If this textarea is hidden, then its selection controller might
            // not be intialized. Ignore the failure.
          }
        }
      }
    }

    if (!selection.rangeCount || selection.isCollapsed) {
      return null;
    }

    let utils = topWin.QueryInterface(Ci.nsIInterfaceRequestor)
                      .getInterface(Ci.nsIDOMWindowUtils);

    let scrollX = {}, scrollY = {};
    utils.getScrollXY(false, scrollX, scrollY);

    for (let frame = win; frame != topWin; frame = frame.parent) {
      let rect = frame.frameElement.getBoundingClientRect();
      let left = frame.getComputedStyle(frame.frameElement, "").borderLeftWidth;
      let top = frame.getComputedStyle(frame.frameElement, "").borderTopWidth;
      scrollX.value += rect.left + parseInt(left, 10);
      scrollY.value += rect.top + parseInt(top, 10);
    }
    let rect = Rect.fromRect(selection.getRangeAt(0).getBoundingClientRect());
    return rect.translate(scrollX.value, scrollY.value);
  },

  _outlineLink: function (aDrawOutline) {
    let foundLink = this._fastFind.foundLink;

    // Optimization: We are drawing outlines and we matched
    // the same link before, so don't duplicate work.
    if (foundLink == this._previousLink && aDrawOutline)
      return;

    this._restoreOriginalOutline();

    if (foundLink && aDrawOutline) {
      // Backup original outline
      this._tmpOutline = foundLink.style.outline;
      this._tmpOutlineOffset = foundLink.style.outlineOffset;

      // Draw pseudo focus rect
      // XXX Should we change the following style for FAYT pseudo focus?
      // XXX Shouldn't we change default design if outline is visible
      //     already?
      // Don't set the outline-color, we should always use initial value.
      foundLink.style.outline = "1px dotted";
      foundLink.style.outlineOffset = "0";

      this._previousLink = foundLink;
    }
  },

  _restoreOriginalOutline: function () {
    // Removes the outline around the last found link.
    if (this._previousLink) {
      this._previousLink.style.outline = this._tmpOutline;
      this._previousLink.style.outlineOffset = this._tmpOutlineOffset;
      this._previousLink = null;
    }
  },

  _getSelectionController: function(aWindow) {
    // display: none iframes don't have a selection controller, see bug 493658
    try {
      if (!aWindow.innerWidth || !aWindow.innerHeight)
        return null;
    } catch (e) {
      // If getting innerWidth or innerHeight throws, we can't get a selection
      // controller.
      return null;
    }

    // Yuck. See bug 138068.
    let docShell = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIWebNavigation)
                          .QueryInterface(Ci.nsIDocShell);

    let controller = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                             .getInterface(Ci.nsISelectionDisplay)
                             .QueryInterface(Ci.nsISelectionController);
    return controller;
  },

  /**
   * Determines whether a range is inside a link.
   * @param aRange
   *        the range to check
   * @returns true if the range starts in a link
   */
  _rangeStartsInLink: function(aRange) {
    let isInsideLink = false;
    let node = aRange.startContainer;

    if (node.nodeType == node.ELEMENT_NODE) {
      if (node.hasChildNodes) {
        let childNode = node.item(aRange.startOffset);
        if (childNode)
          node = childNode;
      }
    }

    const XLink_NS = "http://www.w3.org/1999/xlink";
    const HTMLAnchorElement = (node.ownerDocument || node).defaultView.HTMLAnchorElement;
    do {
      if (node instanceof HTMLAnchorElement) {
        isInsideLink = node.hasAttribute("href");
        break;
      } else if (typeof node.hasAttributeNS == "function" &&
                 node.hasAttributeNS(XLink_NS, "href")) {
        isInsideLink = (node.getAttributeNS(XLink_NS, "type") == "simple");
        break;
      }

      node = node.parentNode;
    } while (node);

    return isInsideLink;
  },

  // Start of nsIWebProgressListener implementation.

  onLocationChange: function(aWebProgress, aRequest, aLocation, aFlags) {
    if (!aWebProgress.isTopLevel)
      return;

    // Avoid leaking if we change the page.
    this._previousLink = null;
    this.highlighter.onLocationChange();
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                         Ci.nsISupportsWeakReference])
};

function GetClipboardSearchString(aLoadContext) {
  let searchString = "";
  if (!Clipboard.supportsFindClipboard())
    return searchString;

  try {
    let trans = Cc["@mozilla.org/widget/transferable;1"]
                  .createInstance(Ci.nsITransferable);
    trans.init(aLoadContext);
    trans.addDataFlavor("text/unicode");

    Clipboard.getData(trans, Ci.nsIClipboard.kFindClipboard);

    let data = {};
    let dataLen = {};
    trans.getTransferData("text/unicode", data, dataLen);
    if (data.value) {
      data = data.value.QueryInterface(Ci.nsISupportsString);
      searchString = data.toString();
    }
  } catch (ex) {}

  return searchString;
}

this.Finder = Finder;
this.GetClipboardSearchString = GetClipboardSearchString;
