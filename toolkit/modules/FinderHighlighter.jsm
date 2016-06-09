/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["FinderHighlighter"];

const { interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Task.jsm");

const kHighlightIterationSizeMax = 100;

/**
 * FinderHighlighter class that is used by Finder.jsm to take care of the
 * 'Highlight All' feature, which can highlight all find occurrences in a page.
 *
 * @param {Finder} finder Finder.jsm instance
 */
function FinderHighlighter(finder) {
  this.finder = finder;
}

FinderHighlighter.prototype = {
  /**
   * Notify all registered listeners that the 'Highlight All' operation finished.
   *
   * @param  {Boolean} highlight Whether highlighting was turned on
   */
  notifyFinished(highlight) {
    for (let l of this.finder._listeners) {
      try {
        l.onHighlightFinished(highlight);
      } catch (ex) {}
    }
  },

  /**
   * Whilst the iterator is running, it's possible to abort it. This may be useful
   * if the word to highlight was updated in the meantime.
   */
  maybeAbort() {
    if (!this._abortHighlight) {
      return;
    }
    this._abortHighlight();
  },

  /**
   * Uses the iterator in Finder.jsm to find all the words to highlight and makes
   * sure not to block the thread whilst running.
   *
   * @param {String}       word    Needle to search for and highlight when found
   * @param {nsIDOMWindow} window  Window object, whose DOM tree should be traversed
   * @param {Function}     onFind  Callback invoked for each found occurrence
   * @yield {Promise} that resolves once the iterator has finished
   */
  iterator: Task.async(function* (word, window, onFind) {
    let count = 0;
    for (let range of this.finder._findIterator(word, window)) {
      onFind(range);
      if (++count >= kHighlightIterationSizeMax) {
        count = 0;
        // Sleep for the rest of this cycle.
        yield new Promise(resolve => resolve());
      }
    }
  }),

  /**
   * Toggle highlighting all occurrences of a word in a page. This method will
   * be called recursively for each (i)frame inside a page.
   *
   * @param {Booolean}     highlight Whether highlighting should be turned on
   * @param {String}       word      Needle to search for and highlight when found
   * @param {nsIDOMWindow} window    Window object, whose DOM tree should be traversed
   * @yield {Promise} that resolves once the operation has finished
   */
  highlight: Task.async(function* (highlight, word, window) {
    window = window || this.finder._getWindow();
    let found = false;
    for (let i = 0; window.frames && i < window.frames.length; i++) {
      if (yield this.highlight(highlight, word, window.frames[i])) {
        found = true;
      }
    }

    let controller = this.finder._getSelectionController(window);
    let doc = window.document;
    if (!controller || !doc || !doc.documentElement) {
      // Without the selection controller,
      // we are unable to (un)highlight any matches
      return found;
    }

    if (highlight) {
      yield this.iterator(word, window, range => {
        this.highlightRange(range, controller);
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
  }),

  /**
   * Add a range to the find selection, i.e. highlight it, and if it's inside an
   * editable node, track it.
   *
   * @param  {nsIDOMRange}            range      Range object to be highlighted
   * @param  {nsISelectionController} controller Selection controller of the
   *                                             document that the range belongs
   *                                             to
   */
  highlightRange(range, controller) {
    let node = range.startContainer;
    let editableNode = this._getEditableNode(node);
    if (editableNode) {
      controller = editableNode.editor.selectionController;
    }

    let findSelection = controller.getSelection(Ci.nsISelectionController.SELECTION_FIND);
    findSelection.addRange(range);

    if (editableNode) {
      // Highlighting added, so cache this editor, and hook up listeners
      // to ensure we deal properly with edits within the highlighting
      this._addEditorListeners(editableNode.editor);
    }
  },

  /**
   * For a given node returns its editable parent or null if there is none.
   * It's enough to check if node is a text node and its parent's parent is
   * instance of nsIDOMNSEditableElement.
   *
   * @param node the node we want to check
   * @returns the first node in the parent chain that is editable,
   *          null if there is no such node
   */
  _getEditableNode(node) {
    if (node.nodeType === node.TEXT_NODE && node.parentNode && node.parentNode.parentNode &&
        node.parentNode.parentNode instanceof Ci.nsIDOMNSEditableElement) {
      return node.parentNode.parentNode;
    }
    return null;
  },

  /**
   * Add ourselves as an nsIEditActionListener and nsIDocumentStateListener for
   * a given editor
   *
   * @param editor the editor we'd like to listen to
   */
  _addEditorListeners(editor) {
    if (!this._editors) {
      this._editors = [];
      this._stateListeners = [];
    }

    let existingIndex = this._editors.indexOf(editor);
    if (existingIndex == -1) {
      let x = this._editors.length;
      this._editors[x] = editor;
      this._stateListeners[x] = this._createStateListener();
      this._editors[x].addEditActionListener(this);
      this._editors[x].addDocumentStateListener(this._stateListeners[x]);
    }
  },

  /**
   * Helper method to unhook listeners, remove cached editors
   * and keep the relevant arrays in sync
   *
   * @param idx the index into the array of editors/state listeners
   *        we wish to remove
   */
  _unhookListenersAtIndex(idx) {
    this._editors[idx].removeEditActionListener(this);
    this._editors[idx]
        .removeDocumentStateListener(this._stateListeners[idx]);
    this._editors.splice(idx, 1);
    this._stateListeners.splice(idx, 1);
    if (!this._editors.length) {
      delete this._editors;
      delete this._stateListeners;
    }
  },

  /**
   * Remove ourselves as an nsIEditActionListener and
   * nsIDocumentStateListener from a given cached editor
   *
   * @param editor the editor we no longer wish to listen to
   */
  _removeEditorListeners(editor) {
    // editor is an editor that we listen to, so therefore must be
    // cached. Find the index of this editor
    let idx = this._editors.indexOf(editor);
    if (idx == -1) {
      return;
    }
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

  /**
   * Helper method used to check whether a selection intersects with
   * some highlighting
   *
   * @param selectionRange the range from the selection to check
   * @param findRange the highlighted range to check against
   * @returns true if they intersect, false otherwise
   */
  _checkOverlap(selectionRange, findRange) {
    // The ranges overlap if one of the following is true:
    // 1) At least one of the endpoints of the deleted selection
    //    is in the find selection
    // 2) At least one of the endpoints of the find selection
    //    is in the deleted selection
    if (findRange.isPointInRange(selectionRange.startContainer,
                                 selectionRange.startOffset))
      return true;
    if (findRange.isPointInRange(selectionRange.endContainer,
                                 selectionRange.endOffset))
      return true;
    if (selectionRange.isPointInRange(findRange.startContainer,
                                      findRange.startOffset))
      return true;
    if (selectionRange.isPointInRange(findRange.endContainer,
                                      findRange.endOffset))
      return true;

    return false;
  },

  /**
   * Helper method to determine if an edit occurred within a highlight
   *
   * @param selection the selection we wish to check
   * @param node the node we want to check is contained in selection
   * @param offset the offset into node that we want to check
   * @returns the range containing (node, offset) or null if no ranges
   *          in the selection contain it
   */
  _findRange(selection, node, offset) {
    let rangeCount = selection.rangeCount;
    let rangeidx = 0;
    let foundContainingRange = false;
    let range = null;

    // Check to see if this node is inside one of the selection's ranges
    while (!foundContainingRange && rangeidx < rangeCount) {
      range = selection.getRangeAt(rangeidx);
      if (range.isPointInRange(node, offset)) {
        foundContainingRange = true;
        break;
      }
      rangeidx++;
    }

    if (foundContainingRange) {
      return range;
    }

    return null;
  },

  // Start of nsIEditActionListener implementations

  WillDeleteText(textNode, offset, length) {
    let editor = this._getEditableNode(textNode).editor;
    let controller = editor.selectionController;
    let fSelection = controller.getSelection(Ci.nsISelectionController.SELECTION_FIND);
    let range = this._findRange(fSelection, textNode, offset);

    if (range) {
      // Don't remove the highlighting if the deleted text is at the
      // end of the range
      if (textNode != range.endContainer ||
          offset != range.endOffset) {
        // Text within the highlight is being removed - the text can
        // no longer be a match, so remove the highlighting
        fSelection.removeRange(range);
        if (fSelection.rangeCount == 0) {
          this._removeEditorListeners(editor);
        }
      }
    }
  },

  DidInsertText(textNode, offset, aString) {
    let editor = this._getEditableNode(textNode).editor;
    let controller = editor.selectionController;
    let fSelection = controller.getSelection(Ci.nsISelectionController.SELECTION_FIND);
    let range = this._findRange(fSelection, textNode, offset);

    if (range) {
      // If the text was inserted before the highlight
      // adjust the highlight's bounds accordingly
      if (textNode == range.startContainer &&
          offset == range.startOffset) {
        range.setStart(range.startContainer,
                       range.startOffset+aString.length);
      } else if (textNode != range.endContainer ||
                 offset != range.endOffset) {
        // The edit occurred within the highlight - any addition of text
        // will result in the text no longer being a match,
        // so remove the highlighting
        fSelection.removeRange(range);
        if (fSelection.rangeCount == 0) {
          this._removeEditorListeners(editor);
        }
      }
    }
  },

  WillDeleteSelection(selection) {
    let editor = this._getEditableNode(selection.getRangeAt(0)
                                                 .startContainer).editor;
    let controller = editor.selectionController;
    let fSelection = controller.getSelection(Ci.nsISelectionController.SELECTION_FIND);

    let selectionIndex = 0;
    let findSelectionIndex = 0;
    let shouldDelete = {};
    let numberOfDeletedSelections = 0;
    let numberOfMatches = fSelection.rangeCount;

    // We need to test if any ranges in the deleted selection (selection)
    // are in any of the ranges of the find selection
    // Usually both selections will only contain one range, however
    // either may contain more than one.

    for (let fIndex = 0; fIndex < numberOfMatches; fIndex++) {
      shouldDelete[fIndex] = false;
      let fRange = fSelection.getRangeAt(fIndex);

      for (let index = 0; index < selection.rangeCount; index++) {
        if (shouldDelete[fIndex]) {
          continue;
        }

        let selRange = selection.getRangeAt(index);
        let doesOverlap = this._checkOverlap(selRange, fRange);
        if (doesOverlap) {
          shouldDelete[fIndex] = true;
          numberOfDeletedSelections++;
        }
      }
    }

    // OK, so now we know what matches (if any) are in the selection
    // that is being deleted. Time to remove them.
    if (!numberOfDeletedSelections) {
      return;
    }

    for (let i = numberOfMatches - 1; i >= 0; i--) {
      if (shouldDelete[i])
        fSelection.removeRange(fSelection.getRangeAt(i));
    }

    // Remove listeners if no more highlights left
    if (!fSelection.rangeCount) {
      this._removeEditorListeners(editor);
    }
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

  /**
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
  _onEditorDestruction(aListener) {
    // First find the index of the editor the given listener listens to.
    // The listeners and editors arrays must always be in sync.
    // The listener will be in our array of cached listeners, as this
    // method could not have been called otherwise.
    let idx = 0;
    while (this._stateListeners[idx] != aListener) {
      idx++;
    }

    // Unhook both listeners
    this._unhookListenersAtIndex(idx);
  },

  /**
   * Creates a unique document state listener for an editor.
   *
   * It is not possible to simply have the findbar implement the
   * listener interface itself, as it wouldn't have sufficient information
   * to work out which editor was being destroyed. Therefore, we create new
   * listeners on the fly, and cache them in sync with the editors they
   * listen to.
   */
  _createStateListener() {
    return {
      findbar: this,

      QueryInterface: function(iid) {
        if (iid.equals(Ci.nsIDocumentStateListener) ||
            iid.equals(Ci.nsISupports))
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
  }
};
