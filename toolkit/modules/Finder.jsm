// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

this.EXPORTED_SYMBOLS = ["Finder"];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Geometry.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "TextToSubURIService",
                                         "@mozilla.org/intl/texttosuburi;1",
                                         "nsITextToSubURI");
XPCOMUtils.defineLazyServiceGetter(this, "Clipboard",
                                         "@mozilla.org/widget/clipboard;1",
                                         "nsIClipboard");
XPCOMUtils.defineLazyServiceGetter(this, "ClipboardHelper",
                                         "@mozilla.org/widget/clipboardhelper;1",
                                         "nsIClipboardHelper");

function Finder(docShell) {
  this._fastFind = Cc["@mozilla.org/typeaheadfind;1"].createInstance(Ci.nsITypeAheadFind);
  this._fastFind.init(docShell);

  this._docShell = docShell;
  this._listeners = [];
  this._previousLink = null;
  this._searchString = null;

  docShell.QueryInterface(Ci.nsIInterfaceRequestor)
          .getInterface(Ci.nsIWebProgress)
          .addProgressListener(this, Ci.nsIWebProgress.NOTIFY_LOCATION);
}

Finder.prototype = {
  addResultListener: function (aListener) {
    if (this._listeners.indexOf(aListener) === -1)
      this._listeners.push(aListener);
  },

  removeResultListener: function (aListener) {
    this._listeners = this._listeners.filter(l => l != aListener);
  },

  _notify: function (aSearchString, aResult, aFindBackwards, aDrawOutline, aStoreResult = true) {
    if (aStoreResult) {
      this._searchString = aSearchString;
      this.clipboardSearchString = aSearchString
    }
    this._outlineLink(aDrawOutline);

    let foundLink = this._fastFind.foundLink;
    let linkURL = null;
    if (foundLink) {
      let docCharset = null;
      let ownerDoc = foundLink.ownerDocument;
      if (ownerDoc)
        docCharset = ownerDoc.characterSet;

      linkURL = TextToSubURIService.unEscapeURIForUI(docCharset, foundLink.href);
    }

    let data = {
      result: aResult,
      findBackwards: aFindBackwards,
      linkURL: linkURL,
      rect: this._getResultRect(),
      searchString: this._searchString,
      storeResult: aStoreResult
    };

    for (let l of this._listeners) {
      try {
        l.onFindResult(data);
      } catch (ex) {}
    }
  },

  get searchString() {
    if (!this._searchString && this._fastFind.searchString)
      this._searchString = this._fastFind.searchString;
    return this._searchString;
  },

  get clipboardSearchString() {
    let searchString = "";
    if (!Clipboard.supportsFindClipboard())
      return searchString;

    try {
      let trans = Cc["@mozilla.org/widget/transferable;1"]
                    .createInstance(Ci.nsITransferable);
      trans.init(this._getWindow()
                     .QueryInterface(Ci.nsIInterfaceRequestor)
                     .getInterface(Ci.nsIWebNavigation)
                     .QueryInterface(Ci.nsILoadContext));
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
  },

  set clipboardSearchString(aSearchString) {
    if (!aSearchString || !Clipboard.supportsFindClipboard())
      return;

    ClipboardHelper.copyStringToClipboard(aSearchString,
                                          Ci.nsIClipboard.kFindClipboard,
                                          this._getWindow().document);
  },

  set caseSensitive(aSensitive) {
    this._fastFind.caseSensitive = aSensitive;
  },

  /**
   * Used for normal search operations, highlights the first match.
   *
   * @param aSearchString String to search for.
   * @param aLinksOnly Only consider nodes that are links for the search.
   * @param aDrawOutline Puts an outline around matched links.
   */
  fastFind: function (aSearchString, aLinksOnly, aDrawOutline) {
    let result = this._fastFind.find(aSearchString, aLinksOnly);
    let searchString = this._fastFind.searchString;
    this._notify(searchString, result, false, aDrawOutline);
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
    let result = this._fastFind.findAgain(aFindBackwards, aLinksOnly);
    let searchString = this._fastFind.searchString;
    this._notify(searchString, result, aFindBackwards, aDrawOutline);
  },

  /**
   * Forcibly set the search string of the find clipboard to the currently
   * selected text in the window, on supported platforms (i.e. OSX).
   */
  setSearchStringToSelection: function() {
    // Find the selected text.
    let selection = this._getWindow().getSelection();
    // Don't go for empty selections.
    if (!selection.rangeCount)
      return null;
    let searchString = (selection.toString() || "").trim();
    // Empty strings are rather useless to search for.
    if (!searchString.length)
      return null;

    this.clipboardSearchString = searchString;
    return searchString;
  },

  highlight: function (aHighlight, aWord) {
    let found = this._highlight(aHighlight, aWord, null);
    if (aHighlight) {
      let result = found ? Ci.nsITypeAheadFind.FIND_FOUND
                         : Ci.nsITypeAheadFind.FIND_NOTFOUND;
      this._notify(aWord, result, false, false, false);
    }
  },

  enableSelection: function() {
    this._fastFind.setSelectionModeAndRepaint(Ci.nsISelectionController.SELECTION_ON);
    this._restoreOriginalOutline();
  },

  removeSelection: function() {
    this._fastFind.collapseSelection();
    this.enableSelection();
  },

  focusContent: function() {
    // Allow Finder listeners to cancel focusing the content.
    for (let l of this._listeners) {
      try {
        if (!l.shouldFocusContent())
          return;
      } catch (ex) {}
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

  requestMatchesCount: function(aWord, aMatchLimit, aLinksOnly) {
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

    for (let l of this._listeners) {
      try {
        l.onMatchesCountResult(result);
      } catch (ex) {}
    }
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

    this._findIterator(aWord, aWindow, aRange => {
      if (!aLinksOnly || this._rangeStartsInLink(aRange)) {
        ++aStats.total;
        if (!aStats._currentFound) {
          ++aStats.current;
          aStats._currentFound = (foundRange &&
            aRange.startContainer == foundRange.startContainer &&
            aRange.startOffset == foundRange.startOffset &&
            aRange.endContainer == foundRange.endContainer &&
            aRange.endOffset == foundRange.endOffset);
        }
      }
      if (aStats.total == aMatchLimit) {
        aStats.total = -1;
        return false;
      }
    });

    return aStats;
  },

  /**
   * Basic wrapper around nsIFind that provides invoking a callback `aOnFind`
   * each time an occurence of `aWord` string is found.
   *
   * @param aWord
   *        the word to search for.
   * @param aWindow
   *        the window to search in.
   * @param aOnFind
   *        the Function to invoke when a word is found. if Boolean `false` is
   *        returned, the find operation will be stopped and the Function will
   *        not be invoked again.
   */
  _findIterator: function(aWord, aWindow, aOnFind) {
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

    while ((retRange = finder.Find(aWord, searchRange, startPt, endPt))) {
      if (aOnFind(retRange) === false)
        break;
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
          let sc = node.editor.selectionController;
          selection = sc.getSelection(Ci.nsISelectionController.SELECTION_NORMAL);
          if (selection.rangeCount && !selection.isCollapsed) {
            break;
          }
        }
      }
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

  _highlight: function (aHighlight, aWord, aWindow) {
    let win = aWindow || this._getWindow();

    let found = false;
    for (let i = 0; win.frames && i < win.frames.length; i++) {
      if (this._highlight(aHighlight, aWord, win.frames[i]))
        found = true;
    }

    let controller = this._getSelectionController(win);
    let doc = win.document;
    if (!controller || !doc || !doc.documentElement) {
      // Without the selection controller,
      // we are unable to (un)highlight any matches
      return found;
    }

    if (aHighlight) {
      this._findIterator(aWord, win, aRange => {
        this._highlightRange(aRange, controller);
        found = true;
      });
    } else {
      // First, attempt to remove highlighting from main document
      let sel = controller.getSelection(Ci.nsISelectionController.SELECTION_FIND);
      sel.removeAllRanges();

      // Next, check our editor cache, for editors belonging to this
      // document
      if (this._editors) {
        for (let x = this._editors.length - 1; x >= 0; --x) {
          if (this._editors[x].document == doc) {
            sel = this._editors[x].selectionController
                                  .getSelection(Ci.nsISelectionController.SELECTION_FIND);
            sel.removeAllRanges();
            // We don't need to listen to this editor any more
            this._unhookListenersAtIndex(x);
          }
        }
      }

      // Removing the highlighting always succeeds, so return true.
      found = true;
    }

    return found;
  },

  _highlightRange: function(aRange, aController) {
    let node = aRange.startContainer;
    let controller = aController;

    let editableNode = this._getEditableNode(node);
    if (editableNode)
      controller = editableNode.editor.selectionController;

    let findSelection = controller.getSelection(Ci.nsISelectionController.SELECTION_FIND);
    findSelection.addRange(aRange);

    if (editableNode) {
      // Highlighting added, so cache this editor, and hook up listeners
      // to ensure we deal properly with edits within the highlighting
      if (!this._editors) {
        this._editors = [];
        this._stateListeners = [];
      }

      let existingIndex = this._editors.indexOf(editableNode.editor);
      if (existingIndex == -1) {
        let x = this._editors.length;
        this._editors[x] = editableNode.editor;
        this._stateListeners[x] = this._createStateListener();
        this._editors[x].addEditActionListener(this);
        this._editors[x].addDocumentStateListener(this._stateListeners[x]);
      }
    }
  },

  _getSelectionController: function(aWindow) {
    // display: none iframes don't have a selection controller, see bug 493658
    if (!aWindow.innerWidth || !aWindow.innerHeight)
      return null;

    // Yuck. See bug 138068.
    let docShell = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIWebNavigation)
                          .QueryInterface(Ci.nsIDocShell);

    let controller = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                             .getInterface(Ci.nsISelectionDisplay)
                             .QueryInterface(Ci.nsISelectionController);
    return controller;
  },

  /*
   * For a given node, walk up it's parent chain, to try and find an
   * editable node.
   *
   * @param aNode the node we want to check
   * @returns the first node in the parent chain that is editable,
   *          null if there is no such node
   */
  _getEditableNode: function (aNode) {
    while (aNode) {
      if (aNode instanceof Ci.nsIDOMNSEditableElement)
        return aNode.editor ? aNode : null;

      aNode = aNode.parentNode;
    }
    return null;
  },

  /*
   * Helper method to unhook listeners, remove cached editors
   * and keep the relevant arrays in sync
   *
   * @param aIndex the index into the array of editors/state listeners
   *        we wish to remove
   */
  _unhookListenersAtIndex: function (aIndex) {
    this._editors[aIndex].removeEditActionListener(this);
    this._editors[aIndex]
        .removeDocumentStateListener(this._stateListeners[aIndex]);
    this._editors.splice(aIndex, 1);
    this._stateListeners.splice(aIndex, 1);
    if (!this._editors.length) {
      delete this._editors;
      delete this._stateListeners;
    }
  },

  /*
   * Remove ourselves as an nsIEditActionListener and
   * nsIDocumentStateListener from a given cached editor
   *
   * @param aEditor the editor we no longer wish to listen to
   */
  _removeEditorListeners: function (aEditor) {
    // aEditor is an editor that we listen to, so therefore must be
    // cached. Find the index of this editor
    let idx = this._editors.indexOf(aEditor);
    if (idx == -1)
      return;
    // Now unhook ourselves, and remove our cached copy
    this._unhookListenersAtIndex(idx);
  },

  /*
   * nsIEditActionListener logic follows
   *
   * We implement this interface to allow us to catch the case where
   * the findbar found a match in a HTML <input> or <textarea>. If the
   * user adjusts the text in some way, it will no longer match, so we
   * want to remove the highlight, rather than have it expand/contract
   * when letters are added or removed.
   */

  /*
   * Helper method used to check whether a selection intersects with
   * some highlighting
   *
   * @param aSelectionRange the range from the selection to check
   * @param aFindRange the highlighted range to check against
   * @returns true if they intersect, false otherwise
   */
  _checkOverlap: function (aSelectionRange, aFindRange) {
    // The ranges overlap if one of the following is true:
    // 1) At least one of the endpoints of the deleted selection
    //    is in the find selection
    // 2) At least one of the endpoints of the find selection
    //    is in the deleted selection
    if (aFindRange.isPointInRange(aSelectionRange.startContainer,
                                  aSelectionRange.startOffset))
      return true;
    if (aFindRange.isPointInRange(aSelectionRange.endContainer,
                                  aSelectionRange.endOffset))
      return true;
    if (aSelectionRange.isPointInRange(aFindRange.startContainer,
                                       aFindRange.startOffset))
      return true;
    if (aSelectionRange.isPointInRange(aFindRange.endContainer,
                                       aFindRange.endOffset))
      return true;

    return false;
  },

  /*
   * Helper method to determine if an edit occurred within a highlight
   *
   * @param aSelection the selection we wish to check
   * @param aNode the node we want to check is contained in aSelection
   * @param aOffset the offset into aNode that we want to check
   * @returns the range containing (aNode, aOffset) or null if no ranges
   *          in the selection contain it
   */
  _findRange: function (aSelection, aNode, aOffset) {
    let rangeCount = aSelection.rangeCount;
    let rangeidx = 0;
    let foundContainingRange = false;
    let range = null;

    // Check to see if this node is inside one of the selection's ranges
    while (!foundContainingRange && rangeidx < rangeCount) {
      range = aSelection.getRangeAt(rangeidx);
      if (range.isPointInRange(aNode, aOffset)) {
        foundContainingRange = true;
        break;
      }
      rangeidx++;
    }

    if (foundContainingRange)
      return range;

    return null;
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
  },

  // Start of nsIEditActionListener implementations

  WillDeleteText: function (aTextNode, aOffset, aLength) {
    let editor = this._getEditableNode(aTextNode).editor;
    let controller = editor.selectionController;
    let fSelection = controller.getSelection(Ci.nsISelectionController.SELECTION_FIND);
    let range = this._findRange(fSelection, aTextNode, aOffset);

    if (range) {
      // Don't remove the highlighting if the deleted text is at the
      // end of the range
      if (aTextNode != range.endContainer ||
          aOffset != range.endOffset) {
        // Text within the highlight is being removed - the text can
        // no longer be a match, so remove the highlighting
        fSelection.removeRange(range);
        if (fSelection.rangeCount == 0)
          this._removeEditorListeners(editor);
      }
    }
  },

  DidInsertText: function (aTextNode, aOffset, aString) {
    let editor = this._getEditableNode(aTextNode).editor;
    let controller = editor.selectionController;
    let fSelection = controller.getSelection(Ci.nsISelectionController.SELECTION_FIND);
    let range = this._findRange(fSelection, aTextNode, aOffset);

    if (range) {
      // If the text was inserted before the highlight
      // adjust the highlight's bounds accordingly
      if (aTextNode == range.startContainer &&
          aOffset == range.startOffset) {
        range.setStart(range.startContainer,
                       range.startOffset+aString.length);
      } else if (aTextNode != range.endContainer ||
                 aOffset != range.endOffset) {
        // The edit occurred within the highlight - any addition of text
        // will result in the text no longer being a match,
        // so remove the highlighting
        fSelection.removeRange(range);
        if (fSelection.rangeCount == 0)
          this._removeEditorListeners(editor);
      }
    }
  },

  WillDeleteSelection: function (aSelection) {
    let editor = this._getEditableNode(aSelection.getRangeAt(0)
                                                 .startContainer).editor;
    let controller = editor.selectionController;
    let fSelection = controller.getSelection(Ci.nsISelectionController.SELECTION_FIND);

    let selectionIndex = 0;
    let findSelectionIndex = 0;
    let shouldDelete = {};
    let numberOfDeletedSelections = 0;
    let numberOfMatches = fSelection.rangeCount;

    // We need to test if any ranges in the deleted selection (aSelection)
    // are in any of the ranges of the find selection
    // Usually both selections will only contain one range, however
    // either may contain more than one.

    for (let fIndex = 0; fIndex < numberOfMatches; fIndex++) {
      shouldDelete[fIndex] = false;
      let fRange = fSelection.getRangeAt(fIndex);

      for (let index = 0; index < aSelection.rangeCount; index++) {
        if (shouldDelete[fIndex])
          continue;

        let selRange = aSelection.getRangeAt(index);
        let doesOverlap = this._checkOverlap(selRange, fRange);
        if (doesOverlap) {
          shouldDelete[fIndex] = true;
          numberOfDeletedSelections++;
        }
      }
    }

    // OK, so now we know what matches (if any) are in the selection
    // that is being deleted. Time to remove them.
    if (numberOfDeletedSelections == 0)
      return;

    for (let i = numberOfMatches - 1; i >= 0; i--) {
      if (shouldDelete[i])
        fSelection.removeRange(fSelection.getRangeAt(i));
    }

    // Remove listeners if no more highlights left
    if (fSelection.rangeCount == 0)
      this._removeEditorListeners(editor);
  },

  /*
   * nsIDocumentStateListener logic follows
   *
   * When attaching nsIEditActionListeners, there are no guarantees
   * as to whether the findbar or the documents in the browser will get
   * destructed first. This leads to the potential to either leak, or to
   * hold on to a reference an editable element's editor for too long,
   * preventing it from being destructed.
   *
   * However, when an editor's owning node is being destroyed, the editor
   * sends out a DocumentWillBeDestroyed notification. We can use this to
   * clean up our references to the object, to allow it to be destroyed in a
   * timely fashion.
   */

  /*
   * Unhook ourselves when one of our state listeners has been called.
   * This can happen in 4 cases:
   *  1) The document the editor belongs to is navigated away from, and
   *     the document is not being cached
   *
   *  2) The document the editor belongs to is expired from the cache
   *
   *  3) The tab containing the owning document is closed
   *
   *  4) The <input> or <textarea> that owns the editor is explicitly
   *     removed from the DOM
   *
   * @param the listener that was invoked
   */
  _onEditorDestruction: function (aListener) {
    // First find the index of the editor the given listener listens to.
    // The listeners and editors arrays must always be in sync.
    // The listener will be in our array of cached listeners, as this
    // method could not have been called otherwise.
    let idx = 0;
    while (this._stateListeners[idx] != aListener)
      idx++;

    // Unhook both listeners
    this._unhookListenersAtIndex(idx);
  },

  /*
   * Creates a unique document state listener for an editor.
   *
   * It is not possible to simply have the findbar implement the
   * listener interface itself, as it wouldn't have sufficient information
   * to work out which editor was being destroyed. Therefore, we create new
   * listeners on the fly, and cache them in sync with the editors they
   * listen to.
   */
  _createStateListener: function () {
    return {
      findbar: this,

      QueryInterface: function(aIID) {
        if (aIID.equals(Ci.nsIDocumentStateListener) ||
            aIID.equals(Ci.nsISupports))
          return this;

        throw Components.results.NS_ERROR_NO_INTERFACE;
      },

      NotifyDocumentWillBeDestroyed: function() {
        this.findbar._onEditorDestruction(this);
      },

      // Unimplemented
      notifyDocumentCreated: function() {},
      notifyDocumentStateChanged: function(aDirty) {}
    };
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                         Ci.nsISupportsWeakReference])
};
