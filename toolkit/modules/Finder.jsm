// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

this.EXPORTED_SYMBOLS = ["Finder"];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
const Services = Cu.import("resource://gre/modules/Services.jsm").Services;

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

  _notify: function (aResult, aFindBackwards, aDrawOutline) {
    this._outlineLink(aDrawOutline);

    let foundLink = this._fastFind.foundLink;
    let linkURL = null;
    if (foundLink) {
      let docCharset = null;
      let ownerDoc = foundLink.ownerDocument;
      if (ownerDoc)
        docCharset = ownerDoc.characterSet;

      if (!this._textToSubURIService) {
        this._textToSubURIService = Cc["@mozilla.org/intl/texttosuburi;1"]
                                      .getService(Ci.nsITextToSubURI);
      }

      linkURL = this._textToSubURIService.unEscapeURIForUI(docCharset, foundLink.href);
    }

    for (let l of this._listeners) {
      l.onFindResult(aResult, aFindBackwards, linkURL);
    }
  },

  get searchString() {
    return this._fastFind.searchString;
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
    this._notify(result, false, aDrawOutline);
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
    this._notify(result, aFindBackwards, aDrawOutline);
  },

  highlight: function (aHighlight, aWord) {
    this._searchString = aWord;
    let found = this._highlight(aHighlight, aWord, null);
    if (found)
      this._notify(Ci.nsITypeAheadFind.FIND_FOUND, false, false);
    else
      this._notify(Ci.nsITypeAheadFind.FIND_NOTFOUND, false, false);
  },

  removeSelection: function() {
    let fastFind = this._fastFind;

    fastFind.collapseSelection();
    fastFind.setSelectionModeAndRepaint(Ci.nsISelectionController.SELECTION_ON);

    this._restoreOriginalOutline();
  },

  focusContent: function() {
    let fastFind = this._fastFind;

    try {
      // Try to find the best possible match that should receive focus.
      if (fastFind.foundLink) {
        fastFind.foundLink.focus();
      } else if (fastFind.foundEditable) {
        fastFind.foundEditable.focus();
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
        if (this._fastFind.foundLink) // Todo: Handle ctrl click.
          this._fastFind.foundLink.click();
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

  _getWindow: function () {
    return this._docShell.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindow);
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

    let body = (doc instanceof Ci.nsIDOMHTMLDocument && doc.body) ?
               doc.body : doc.documentElement;

    if (aHighlight) {
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

      while ((retRange = finder.Find(aWord, searchRange,
                                     startPt, endPt))) {
        this._highlightRange(retRange, controller);
        startPt = retRange.cloneRange();
        startPt.collapse(false);

        found = true;
      }
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

      //Removing the highlighting always succeeds, so return true.
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
