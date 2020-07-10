// vim: set ts=2 sw=2 sts=2 tw=80:
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

var EXPORTED_SYMBOLS = ["Finder", "GetClipboardSearchString"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Rect } = ChromeUtils.import("resource://gre/modules/Geometry.jsm");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "BrowserUtils",
  "resource://gre/modules/BrowserUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "FinderIterator",
  "resource://gre/modules/FinderIterator.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "Clipboard",
  "@mozilla.org/widget/clipboard;1",
  "nsIClipboard"
);
XPCOMUtils.defineLazyServiceGetter(
  this,
  "ClipboardHelper",
  "@mozilla.org/widget/clipboardhelper;1",
  "nsIClipboardHelper"
);

const kSelectionMaxLen = 150;
const kMatchesCountLimitPref = "accessibility.typeaheadfind.matchesCountLimit";

function Finder(docShell) {
  this._fastFind = Cc["@mozilla.org/typeaheadfind;1"].createInstance(
    Ci.nsITypeAheadFind
  );
  this._fastFind.init(docShell);

  this._currentFoundRange = null;
  this._docShell = docShell;
  this._listeners = [];
  this._previousLink = null;
  this._searchString = null;
  this._highlighter = null;

  docShell
    .QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIWebProgress)
    .addProgressListener(this, Ci.nsIWebProgress.NOTIFY_LOCATION);
  docShell.domWindow.addEventListener(
    "unload",
    this.onLocationChange.bind(this, { isTopLevel: true })
  );
}

Finder.prototype = {
  get iterator() {
    if (!this._iterator) {
      this._iterator = new FinderIterator();
    }
    return this._iterator;
  },

  destroy() {
    if (this._iterator) {
      this._iterator.reset();
    }
    let window = this._getWindow();
    if (this._highlighter && window) {
      // if we clear all the references before we hide the highlights (in both
      // highlighting modes), we simply can't use them to find the ranges we
      // need to clear from the selection.
      this._highlighter.hide(window);
      this._highlighter.clear(window);
    }
    this.listeners = [];
    this._docShell
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebProgress)
      .removeProgressListener(this, Ci.nsIWebProgress.NOTIFY_LOCATION);
    this._listeners = [];
    this._currentFoundRange = this._fastFind = this._docShell = this._previousLink = this._highlighter = null;
  },

  addResultListener(aListener) {
    if (!this._listeners.includes(aListener)) {
      this._listeners.push(aListener);
    }
  },

  removeResultListener(aListener) {
    this._listeners = this._listeners.filter(l => l != aListener);
  },

  _setResults(options, mode) {
    if (typeof options.storeResult != "boolean") {
      options.storeResult = true;
    }

    if (options.storeResult) {
      this._searchString = options.searchString;
      this.clipboardSearchString = options.searchString;
    }

    let foundLink = this._fastFind.foundLink;
    let linkURL = null;
    if (foundLink) {
      linkURL = Services.textToSubURI.unEscapeURIForUI(foundLink.href);
    }

    options.linkURL = linkURL;
    options.rect = this._getResultRect();
    options.searchString = this._searchString;

    this._outlineLink(options.drawOutline);

    for (let l of this._listeners) {
      try {
        l.onFindResult(options);
      } catch (ex) {}
    }
  },

  get searchString() {
    if (!this._searchString && this._fastFind.searchString) {
      this._searchString = this._fastFind.searchString;
    }
    return this._searchString;
  },

  get clipboardSearchString() {
    return GetClipboardSearchString(
      this._getWindow().docShell.QueryInterface(Ci.nsILoadContext)
    );
  },

  set clipboardSearchString(aSearchString) {
    if (!aSearchString || !Clipboard.supportsFindClipboard()) {
      return;
    }

    ClipboardHelper.copyStringToClipboard(
      aSearchString,
      Ci.nsIClipboard.kFindClipboard
    );
  },

  set caseSensitive(aSensitive) {
    if (this._fastFind.caseSensitive === aSensitive) {
      return;
    }
    this._fastFind.caseSensitive = aSensitive;
    this.iterator.reset();
  },

  set matchDiacritics(aMatchDiacritics) {
    if (this._fastFind.matchDiacritics === aMatchDiacritics) {
      return;
    }
    this._fastFind.matchDiacritics = aMatchDiacritics;
    this.iterator.reset();
  },

  set entireWord(aEntireWord) {
    if (this._fastFind.entireWord === aEntireWord) {
      return;
    }
    this._fastFind.entireWord = aEntireWord;
    this.iterator.reset();
  },

  get highlighter() {
    if (this._highlighter) {
      return this._highlighter;
    }

    const { FinderHighlighter } = ChromeUtils.import(
      "resource://gre/modules/FinderHighlighter.jsm"
    );
    return (this._highlighter = new FinderHighlighter(this));
  },

  get matchesCountLimit() {
    if (typeof this._matchesCountLimit == "number") {
      return this._matchesCountLimit;
    }

    this._matchesCountLimit =
      Services.prefs.getIntPref(kMatchesCountLimitPref) || 0;
    return this._matchesCountLimit;
  },

  _lastFindResult: null,

  /**
   * Used for normal search operations, highlights the first match.
   * This method is used only for compatibility with non-remote browsers.
   *
   * @param aSearchString String to search for.
   * @param aLinksOnly Only consider nodes that are links for the search.
   * @param aDrawOutline Puts an outline around matched links.
   */
  fastFind(aSearchString, aLinksOnly, aDrawOutline) {
    this._lastFindResult = this._fastFind.find(
      aSearchString,
      aLinksOnly,
      Ci.nsITypeAheadFind.FIND_INITIAL,
      false
    );
    let searchString = this._fastFind.searchString;

    let results = {
      searchString,
      result: this._lastFindResult,
      findBackwards: false,
      findAgain: false,
      drawOutline: aDrawOutline,
      linksOnly: aLinksOnly,
      useSubFrames: true,
    };

    this._setResults(results);
    this.updateHighlightAndMatchCount(results);

    return this._lastFindResult;
  },

  /**
   * Repeat the previous search. Should only be called after a previous
   * call to Finder.fastFind.
   * This method is used only for compatibility with non-remote browsers.
   *
   * @param aSearchString String to search for.
   * @param aFindBackwards Controls the search direction:
   *    true: before current match, false: after current match.
   * @param aLinksOnly Only consider nodes that are links for the search.
   * @param aDrawOutline Puts an outline around matched links.
   */
  findAgain(aSearchString, aFindBackwards, aLinksOnly, aDrawOutline) {
    let mode = aFindBackwards
      ? Ci.nsITypeAheadFind.FIND_PREVIOUS
      : Ci.nsITypeAheadFind.FIND_NEXT;
    this._lastFindResult = this._fastFind.find(
      aFindBackwards,
      aLinksOnly,
      mode,
      false
    );
    let searchString = this._fastFind.searchString;

    let results = {
      searchString,
      result: this._lastFindResult,
      findBackwards: aFindBackwards,
      findAgain: true,
      drawOutline: aDrawOutline,
      linksOnly: aLinksOnly,
      useSubFrames: true,
    };
    this._setResults(results);
    this.updateHighlightAndMatchCount(results);

    return this._lastFindResult;
  },

  /**
   * Used for normal search operations, highlights the first or
   * subsequent match depending on the mode.
   *
   * Options are:
   *  searchString String to search for.
   *  findAgain True if this a find again operation.
   *  mode Search mode from nsITypeAheadFind.
   *  linksOnly Only consider nodes that are links for the search.
   *  drawOutline Puts an outline around matched links.
   *  useSubFrames True to iterate over subframes.
   *  caseSensitive True for case sensitive searching.
   *  entireWord True to match entire words.
   *  matchDiacritics True to match diacritics.
   */
  find(options) {
    this.caseSensitive = options.caseSensitive;
    this.entireWord = options.entireWord;
    this.matchDiacritics = options.matchDiacritics;

    this._lastFindResult = this._fastFind.find(
      options.searchString,
      options.linksOnly,
      options.mode,
      !options.useSubFrames
    );
    let searchString = this._fastFind.searchString;
    let results = {
      searchString,
      result: this._lastFindResult,
      findBackwards:
        options.mode == Ci.nsITypeAheadFind.FIND_PREVIOUS ||
        options.mode == Ci.nsITypeAheadFind.FIND_LAST,
      findAgain: options.findAgain,
      drawOutline: options.drawOutline,
      linksOnly: options.linksOnly,
      entireWord: this._fastFind.entireWord,
      useSubFrames: options.useSubFrames,
    };
    this._setResults(results, options.mode);
    return new Promise(resolve => resolve(results));
  },

  /**
   * Forcibly set the search string of the find clipboard to the currently
   * selected text in the window, on supported platforms (i.e. OSX).
   */
  setSearchStringToSelection() {
    let searchInfo = this.getActiveSelectionText();

    // If an empty string is returned or a subframe is focused, don't
    // assign the search string.
    if (searchInfo.selectedText) {
      this.clipboardSearchString = searchInfo.selectedText;
    }

    return searchInfo;
  },

  async highlight(aHighlight, aWord, aLinksOnly, aUseSubFrames = true) {
    return this.highlighter.highlight(
      aHighlight,
      aWord,
      aLinksOnly,
      false,
      aUseSubFrames
    );
  },

  async updateHighlightAndMatchCount(aArgs) {
    this._lastFindResult = aArgs;

    if (
      !this.iterator.continueRunning({
        caseSensitive: this._fastFind.caseSensitive,
        entireWord: this._fastFind.entireWord,
        linksOnly: aArgs.linksOnly,
        matchDiacritics: this._fastFind.matchDiacritics,
        word: aArgs.searchString,
        useSubFrames: aArgs.useSubFrames,
      })
    ) {
      this.iterator.stop();
    }

    let highlightPromise = this.highlighter.update(
      aArgs,
      aArgs.useSubFrames ? false : aArgs.foundInThisFrame
    );
    let matchCountPromise = this.requestMatchesCount(
      aArgs.searchString,
      aArgs.linksOnly,
      aArgs.useSubFrames
    );

    let results = await Promise.all([highlightPromise, matchCountPromise]);
    if (results[1]) {
      return Object.assign(results[1], results[0]);
    } else if (results[0]) {
      return results[0];
    }

    return null;
  },

  getInitialSelection() {
    this._getWindow().setTimeout(() => {
      let initialSelection = this.getActiveSelectionText().selectedText;
      for (let l of this._listeners) {
        try {
          l.onCurrentSelection(initialSelection, true);
        } catch (ex) {}
      }
    }, 0);
  },

  getActiveSelectionText() {
    let focusedWindow = {};
    let focusedElement = Services.focus.getFocusedElementForWindow(
      this._getWindow(),
      true,
      focusedWindow
    );
    focusedWindow = focusedWindow.value;

    let selText;

    // If this is a remote subframe, return an empty string but
    // indiciate which browsing context was focused.
    if (
      focusedElement &&
      "frameLoader" in focusedElement &&
      focusedElement.browsingContext instanceof BrowsingContext
    ) {
      return {
        focusedChildBrowserContextId: focusedElement.browsingContext.id,
        selectedText: "",
      };
    }

    if (focusedElement && focusedElement.editor) {
      // The user may have a selection in an input or textarea.
      selText = focusedElement.editor.selectionController
        .getSelection(Ci.nsISelectionController.SELECTION_NORMAL)
        .toString();
    } else {
      // Look for any selected text on the actual page.
      selText = focusedWindow.getSelection().toString();
    }

    if (!selText) {
      return { selectedText: "" };
    }

    // Process our text to get rid of unwanted characters.
    selText = selText.trim().replace(/\s+/g, " ");
    let truncLength = kSelectionMaxLen;
    if (selText.length > truncLength) {
      let truncChar = selText.charAt(truncLength).charCodeAt(0);
      if (truncChar >= 0xdc00 && truncChar <= 0xdfff) {
        truncLength++;
      }
      selText = selText.substr(0, truncLength);
    }

    return { selectedText: selText };
  },

  enableSelection() {
    this._fastFind.setSelectionModeAndRepaint(
      Ci.nsISelectionController.SELECTION_ON
    );
    this._restoreOriginalOutline();
  },

  removeSelection(keepHighlight) {
    this._fastFind.collapseSelection();
    this.enableSelection();
    let window = this._getWindow();
    if (keepHighlight) {
      this.highlighter.clearCurrentOutline(window);
    } else {
      this.highlighter.clear(window);
    }
  },

  focusContent() {
    // Allow Finder listeners to cancel focusing the content.
    for (let l of this._listeners) {
      try {
        if ("shouldFocusContent" in l && !l.shouldFocusContent()) {
          return;
        }
      } catch (ex) {
        Cu.reportError(ex);
      }
    }

    let fastFind = this._fastFind;
    try {
      // Try to find the best possible match that should receive focus and
      // block scrolling on focus since find already scrolls. Further
      // scrolling is due to user action, so don't override this.
      if (fastFind.foundLink) {
        Services.focus.setFocus(
          fastFind.foundLink,
          Services.focus.FLAG_NOSCROLL
        );
      } else if (fastFind.foundEditable) {
        Services.focus.setFocus(
          fastFind.foundEditable,
          Services.focus.FLAG_NOSCROLL
        );
        fastFind.collapseSelection();
      } else {
        this._getWindow().focus();
      }
    } catch (e) {}
  },

  onFindbarClose() {
    this.enableSelection();
    this.highlighter.highlight(false);
    this.iterator.reset();
    BrowserUtils.trackToolbarVisibility(this._docShell, "findbar", false);
  },

  onFindbarOpen() {
    BrowserUtils.trackToolbarVisibility(this._docShell, "findbar", true);
  },

  onModalHighlightChange(useModalHighlight) {
    if (this._highlighter) {
      this._highlighter.onModalHighlightChange(useModalHighlight);
    }
  },

  onHighlightAllChange(highlightAll) {
    if (this._highlighter) {
      this._highlighter.onHighlightAllChange(highlightAll);
    }
    if (this._iterator) {
      this._iterator.reset();
    }
  },

  keyPress(aEvent) {
    let controller = this._getSelectionController(this._getWindow());

    switch (aEvent.keyCode) {
      case aEvent.DOM_VK_RETURN:
        if (this._fastFind.foundLink) {
          let view = this._fastFind.foundLink.ownerGlobal;
          this._fastFind.foundLink.dispatchEvent(
            new view.MouseEvent("click", {
              view,
              cancelable: true,
              bubbles: true,
              ctrlKey: aEvent.ctrlKey,
              altKey: aEvent.altKey,
              shiftKey: aEvent.shiftKey,
              metaKey: aEvent.metaKey,
            })
          );
        }
        break;
      case aEvent.DOM_VK_TAB:
        let direction = Services.focus.MOVEFOCUS_FORWARD;
        if (aEvent.shiftKey) {
          direction = Services.focus.MOVEFOCUS_BACKWARD;
        }
        Services.focus.moveFocus(this._getWindow(), null, direction, 0);
        break;
      case aEvent.DOM_VK_PAGE_UP:
        controller.scrollPage(false);
        break;
      case aEvent.DOM_VK_PAGE_DOWN:
        controller.scrollPage(true);
        break;
      case aEvent.DOM_VK_UP:
        controller.scrollLine(false);
        break;
      case aEvent.DOM_VK_DOWN:
        controller.scrollLine(true);
        break;
    }
  },

  _notifyMatchesCount(aWord, result = this._currentMatchesCountResult) {
    // The `_currentFound` property is only used for internal bookkeeping.
    delete result._currentFound;
    result.searchString = aWord;
    result.limit = this.matchesCountLimit;
    if (result.total == result.limit) {
      result.total = -1;
    }

    for (let l of this._listeners) {
      try {
        l.onMatchesCountResult(result);
      } catch (ex) {}
    }

    this._currentMatchesCountResult = null;
    return result;
  },

  async requestMatchesCount(aWord, aLinksOnly, aUseSubFrames = true) {
    if (
      this._lastFindResult == Ci.nsITypeAheadFind.FIND_NOTFOUND ||
      this.searchString == "" ||
      !aWord ||
      !this.matchesCountLimit
    ) {
      return this._notifyMatchesCount(aWord, {
        total: 0,
        current: 0,
      });
    }

    this._currentFoundRange = this._fastFind.getFoundRange();

    let params = {
      caseSensitive: this._fastFind.caseSensitive,
      entireWord: this._fastFind.entireWord,
      linksOnly: aLinksOnly,
      matchDiacritics: this._fastFind.matchDiacritics,
      word: aWord,
      useSubFrames: aUseSubFrames,
    };
    if (!this.iterator.continueRunning(params)) {
      this.iterator.stop();
    }

    await this.iterator.start(
      Object.assign(params, {
        finder: this,
        limit: this.matchesCountLimit,
        listener: this,
        useCache: true,
        useSubFrames: aUseSubFrames,
      })
    );

    // Without a valid result, there's nothing to notify about. This happens
    // when the iterator was started before and won the race.
    if (!this._currentMatchesCountResult) {
      return null;
    }

    return this._notifyMatchesCount(aWord);
  },

  // FinderIterator listener implementation

  onIteratorRangeFound(range) {
    let result = this._currentMatchesCountResult;
    if (!result) {
      return;
    }

    ++result.total;
    if (!result._currentFound) {
      ++result.current;
      result._currentFound =
        this._currentFoundRange &&
        range.startContainer == this._currentFoundRange.startContainer &&
        range.startOffset == this._currentFoundRange.startOffset &&
        range.endContainer == this._currentFoundRange.endContainer &&
        range.endOffset == this._currentFoundRange.endOffset;
    }
  },

  onIteratorReset() {},

  onIteratorRestart({ word, linksOnly, useSubFrames }) {
    this.requestMatchesCount(word, linksOnly, useSubFrames);
  },

  onIteratorStart() {
    this._currentMatchesCountResult = {
      total: 0,
      current: 0,
      _currentFound: false,
    };
  },

  _getWindow() {
    if (!this._docShell) {
      return null;
    }
    return this._docShell.domWindow;
  },

  /**
   * Get the bounding selection rect in CSS px relative to the origin of the
   * top-level content document.
   */
  _getResultRect() {
    let topWin = this._getWindow();
    let win = this._fastFind.currentWindow;
    if (!win) {
      return null;
    }

    let selection = win.getSelection();
    if (!selection.rangeCount || selection.isCollapsed) {
      // The selection can be into an input or a textarea element.
      let nodes = win.document.querySelectorAll("input, textarea");
      for (let node of nodes) {
        if (node.editor) {
          try {
            let sc = node.editor.selectionController;
            selection = sc.getSelection(
              Ci.nsISelectionController.SELECTION_NORMAL
            );
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

    let utils = topWin.windowUtils;

    let scrollX = {},
      scrollY = {};
    utils.getScrollXY(false, scrollX, scrollY);

    for (let frame = win; frame != topWin; frame = frame.parent) {
      let rect = frame.frameElement.getBoundingClientRect();
      let left = frame.getComputedStyle(frame.frameElement).borderLeftWidth;
      let top = frame.getComputedStyle(frame.frameElement).borderTopWidth;
      scrollX.value += rect.left + parseInt(left, 10);
      scrollY.value += rect.top + parseInt(top, 10);
    }
    let rect = Rect.fromRect(selection.getRangeAt(0).getBoundingClientRect());
    return rect.translate(scrollX.value, scrollY.value);
  },

  _outlineLink(aDrawOutline) {
    let foundLink = this._fastFind.foundLink;

    // Optimization: We are drawing outlines and we matched
    // the same link before, so don't duplicate work.
    if (foundLink == this._previousLink && aDrawOutline) {
      return;
    }

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

  _restoreOriginalOutline() {
    // Removes the outline around the last found link.
    if (this._previousLink) {
      this._previousLink.style.outline = this._tmpOutline;
      this._previousLink.style.outlineOffset = this._tmpOutlineOffset;
      this._previousLink = null;
    }
  },

  _getSelectionController(aWindow) {
    // display: none iframes don't have a selection controller, see bug 493658
    try {
      if (!aWindow.innerWidth || !aWindow.innerHeight) {
        return null;
      }
    } catch (e) {
      // If getting innerWidth or innerHeight throws, we can't get a selection
      // controller.
      return null;
    }

    // Yuck. See bug 138068.
    let docShell = aWindow.docShell;

    let controller = docShell
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsISelectionDisplay)
      .QueryInterface(Ci.nsISelectionController);
    return controller;
  },

  // Start of nsIWebProgressListener implementation.

  onLocationChange(aWebProgress, aRequest, aLocation, aFlags) {
    if (!aWebProgress.isTopLevel) {
      return;
    }
    // Ignore events that don't change the document.
    if (aFlags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT) {
      return;
    }

    // Avoid leaking if we change the page.
    this._lastFindResult = this._previousLink = this._currentFoundRange = null;
    this.highlighter.onLocationChange();
    this.iterator.reset();
  },

  QueryInterface: ChromeUtils.generateQI([
    "nsIWebProgressListener",
    "nsISupportsWeakReference",
  ]),
};

function GetClipboardSearchString(aLoadContext) {
  let searchString = "";
  if (!Clipboard.supportsFindClipboard()) {
    return searchString;
  }

  try {
    let trans = Cc["@mozilla.org/widget/transferable;1"].createInstance(
      Ci.nsITransferable
    );
    trans.init(aLoadContext);
    trans.addDataFlavor("text/unicode");

    Clipboard.getData(trans, Ci.nsIClipboard.kFindClipboard);

    let data = {};
    trans.getTransferData("text/unicode", data);
    if (data.value) {
      data = data.value.QueryInterface(Ci.nsISupportsString);
      searchString = data.toString();
    }
  } catch (ex) {}

  return searchString;
}
