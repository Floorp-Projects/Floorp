/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var SelectionHandler = {
  HANDLE_TYPE_START: "START",
  HANDLE_TYPE_MIDDLE: "MIDDLE",
  HANDLE_TYPE_END: "END",

  TYPE_NONE: 0,
  TYPE_CURSOR: 1,
  TYPE_SELECTION: 2,

  // Keeps track of data about the dimensions of the selection. Coordinates
  // stored here are relative to the _contentWindow window.
  _cache: null,
  _activeType: 0, // TYPE_NONE

  // The window that holds the selection (can be a sub-frame)
  get _contentWindow() {
    if (this._contentWindowRef)
      return this._contentWindowRef.get();
    return null;
  },

  set _contentWindow(aContentWindow) {
    this._contentWindowRef = Cu.getWeakReference(aContentWindow);
  },

  get _targetElement() {
    if (this._targetElementRef)
      return this._targetElementRef.get();
    return null;
  },

  set _targetElement(aTargetElement) {
    this._targetElementRef = Cu.getWeakReference(aTargetElement);
  },

  get _domWinUtils() {
    return BrowserApp.selectedBrowser.contentWindow.QueryInterface(Ci.nsIInterfaceRequestor).
                                                    getInterface(Ci.nsIDOMWindowUtils);
  },

  _isRTL: false,

  _addObservers: function sh_addObservers() {
    Services.obs.addObserver(this, "Gesture:SingleTap", false);
    Services.obs.addObserver(this, "Window:Resize", false);
    Services.obs.addObserver(this, "Tab:Selected", false);
    Services.obs.addObserver(this, "after-viewport-change", false);
    Services.obs.addObserver(this, "TextSelection:Move", false);
    Services.obs.addObserver(this, "TextSelection:Position", false);
    BrowserApp.deck.addEventListener("compositionend", this, false);
  },

  _removeObservers: function sh_removeObservers() {
    Services.obs.removeObserver(this, "Gesture:SingleTap");
    Services.obs.removeObserver(this, "Window:Resize");
    Services.obs.removeObserver(this, "Tab:Selected");
    Services.obs.removeObserver(this, "after-viewport-change");
    Services.obs.removeObserver(this, "TextSelection:Move");
    Services.obs.removeObserver(this, "TextSelection:Position");
    BrowserApp.deck.removeEventListener("compositionend", this);
  },

  observe: function sh_observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "Gesture:SingleTap": {
        if (this._activeType == this.TYPE_SELECTION) {
          let data = JSON.parse(aData);
          if (this._pointInSelection(data.x, data.y))
            this.copySelection();
          else
            this._closeSelection();
        }
        break;
      }
      case "Tab:Selected":
        this._closeSelection();
        break;
  
      case "Window:Resize": {
        if (this._activeType == this.TYPE_SELECTION) {
          // Knowing when the page is done drawing is hard, so let's just cancel
          // the selection when the window changes. We should fix this later.
          this._closeSelection();
        }
        break;
      }
      case "after-viewport-change": {
        if (this._activeType == this.TYPE_SELECTION) {
          // Update the cache after the viewport changes (e.g. panning, zooming).
          this._updateCacheForSelection();
        }
        break;
      }
      case "TextSelection:Move": {
        let data = JSON.parse(aData);
        if (this._activeType == this.TYPE_SELECTION)
          this._moveSelection(data.handleType == this.HANDLE_TYPE_START, data.x, data.y);
        else if (this._activeType == this.TYPE_CURSOR) {
          // Send a click event to the text box, which positions the caret
          this._sendMouseEvents(data.x, data.y);

          // Move the handle directly under the caret
          this._positionHandles();
        }
        break;
      }
      case "TextSelection:Position": {
        if (this._activeType == this.TYPE_SELECTION) {
          // Check to see if the handles should be reversed.
          let isStartHandle = JSON.parse(aData).handleType == this.HANDLE_TYPE_START;
          let selectionReversed = this._updateCacheForSelection(isStartHandle);
          if (selectionReversed) {
            // Reverse the anchor and focus to correspond to the new start and end handles.
            let selection = this._getSelection();
            let anchorNode = selection.anchorNode;
            let anchorOffset = selection.anchorOffset;
            selection.collapse(selection.focusNode, selection.focusOffset);
            selection.extend(anchorNode, anchorOffset);
          }
        }
        this._positionHandles();
        break;
      }
    }
  },

  handleEvent: function sh_handleEvent(aEvent) {
    switch (aEvent.type) {
      case "pagehide":
      // We only add keydown and blur listeners for TYPE_CURSOR
      case "keydown":
      case "blur":
        this._closeSelection();
        break;

      case "compositionend":
        if (this._activeType == this.TYPE_CURSOR) {
          this._closeSelection();
        }
        break;
    }
  },

  /** Returns true if the provided element can be selected in text selection, false otherwise. */
  canSelect: function sh_canSelect(aElement) {
    return !(aElement instanceof Ci.nsIDOMHTMLButtonElement ||
             aElement instanceof Ci.nsIDOMHTMLEmbedElement ||
             aElement instanceof Ci.nsIDOMHTMLImageElement ||
             aElement instanceof Ci.nsIDOMHTMLMediaElement) &&
             aElement.style.MozUserSelect != 'none';
  },

  /*
   * Called from browser.js when the user long taps on text or chooses
   * the "Select Word" context menu item. Initializes SelectionHandler,
   * starts a selection, and positions the text selection handles.
   *
   * @param aX, aY tap location in client coordinates.
   */
  startSelection: function sh_startSelection(aElement, aX, aY) {
    // Clear out any existing active selection
    this._closeSelection();

    this._initTargetInfo(aElement);

    // Clear any existing selection from the document
    this._contentWindow.getSelection().removeAllRanges();

    if (!this._domWinUtils.selectAtPoint(aX, aY, Ci.nsIDOMWindowUtils.SELECT_WORDNOSPACE)) {
      this._onFail("failed to set selection at point");
      return;
    }

    let selection = this._getSelection();
    // If the range didn't have any text, let's bail
    if (!selection) {
      this._onFail("no selection was present");
      return;
    }

    // Initialize the cache
    this._cache = { start: {}, end: {}};
    this._updateCacheForSelection();

    this._activeType = this.TYPE_SELECTION;
    this._positionHandles();

    sendMessageToJava({
      type: "TextSelection:ShowHandles",
      handles: [this.HANDLE_TYPE_START, this.HANDLE_TYPE_END]
    });
  },

  /*
   * Called by BrowserEventHandler when the user taps in a form input.
   * Initializes SelectionHandler and positions the caret handle.
   *
   * @param aX, aY tap location in client coordinates.
   */
  attachCaret: function sh_attachCaret(aElement) {
    this._initTargetInfo(aElement);

    this._contentWindow.addEventListener("keydown", this, false);
    this._contentWindow.addEventListener("blur", this, false);

    this._activeType = this.TYPE_CURSOR;
    this._positionHandles();

    sendMessageToJava({
      type: "TextSelection:ShowHandles",
      handles: [this.HANDLE_TYPE_MIDDLE]
    });
  },

  _initTargetInfo: function sh_initTargetInfo(aElement) {
    this._targetElement = aElement;
    if (aElement instanceof Ci.nsIDOMNSEditableElement) {
      aElement.focus();
    }

    this._contentWindow = aElement.ownerDocument.defaultView;
    this._isRTL = (this._contentWindow.getComputedStyle(aElement, "").direction == "rtl");

    this._addObservers();
    this._contentWindow.addEventListener("pagehide", this, false);
  },

  _getSelection: function sh_getSelection() {
    if (this._targetElement instanceof Ci.nsIDOMNSEditableElement)
      return this._targetElement.QueryInterface(Ci.nsIDOMNSEditableElement).editor.selection;
    else
      return this._contentWindow.getSelection();
  },

  _getSelectedText: function sh_getSelectedText() {
    let selection = this._getSelection();
    if (selection)
      return selection.toString().trim();
    return "";
  },

  _getSelectionController: function sh_getSelectionController() {
    if (this._targetElement instanceof Ci.nsIDOMNSEditableElement)
      return this._targetElement.QueryInterface(Ci.nsIDOMNSEditableElement).editor.selectionController;
    else
      return this._contentWindow.QueryInterface(Ci.nsIInterfaceRequestor).
                                 getInterface(Ci.nsIWebNavigation).
                                 QueryInterface(Ci.nsIInterfaceRequestor).
                                 getInterface(Ci.nsISelectionDisplay).
                                 QueryInterface(Ci.nsISelectionController);
  },

  // Used by the contextmenu "matches" functions in ClipboardHelper
  shouldShowContextMenu: function sh_shouldShowContextMenu(aX, aY) {
    return (this._activeType == this.TYPE_SELECTION) && this._pointInSelection(aX, aY);
  },

  selectAll: function sh_selectAll(aElement, aX, aY) {
    if (this._activeType != this.TYPE_SELECTION)
      this.startSelection(aElement, aX, aY);

    let selectionController = this._getSelectionController();
    selectionController.selectAll();
    this._updateCacheForSelection();
    this._positionHandles();
  },

  /*
   * Moves the selection as the user drags a selection handle.
   *
   * @param aIsStartHandle whether the user is moving the start handle (as opposed to the end handle)
   * @param aX, aY selection point in client coordinates
   */
  _moveSelection: function sh_moveSelection(aIsStartHandle, aX, aY) {
    // XXX We should be smarter about the coordinates we pass to caretPositionFromPoint, especially
    // in editable targets. We should factor out the logic that's currently in _sendMouseEvents.
    let viewOffset = this._getViewOffset();
    let caretPos = this._contentWindow.document.caretPositionFromPoint(aX - viewOffset.x, aY - viewOffset.y);

    // Constrain text selection within editable elements.
    let targetIsEditable = this._targetElement instanceof Ci.nsIDOMNSEditableElement;
    if (targetIsEditable && (caretPos.offsetNode != this._targetElement)) {
      return;
    }

    // Update the cache as the handle is dragged (keep the cache in client coordinates).
    if (aIsStartHandle) {
      this._cache.start.x = aX;
      this._cache.start.y = aY;
    } else {
      this._cache.end.x = aX;
      this._cache.end.y = aY;
    }

    let selection = this._getSelection();

    // The handles work the same on both LTR and RTL pages, but the anchor/focus nodes
    // are reversed, so we need to reverse the logic to extend the selection.
    if ((aIsStartHandle && !this._isRTL) || (!aIsStartHandle && this._isRTL)) {
      if (targetIsEditable) {
        // XXX This will just collapse the selection if the start handle goes past the end handle.
        this._targetElement.selectionStart = caretPos.offset;
      } else {
        let focusNode = selection.focusNode;
        let focusOffset = selection.focusOffset;
        selection.collapse(caretPos.offsetNode, caretPos.offset);
        selection.extend(focusNode, focusOffset);
      }
    } else {
      if (targetIsEditable) {
        // XXX This will just collapse the selection if the end handle goes past the start handle.
        this._targetElement.selectionEnd = caretPos.offset;
      } else {
        selection.extend(caretPos.offsetNode, caretPos.offset);
      }
    }
  },

  _sendMouseEvents: function sh_sendMouseEvents(aX, aY, useShift) {
    // If we're positioning a cursor in an input field, make sure the handle
    // stays within the bounds of the field
    if (this._activeType == this.TYPE_CURSOR) {
      // Get rect of text inside element
      let range = document.createRange();
      range.selectNodeContents(this._targetElement.QueryInterface(Ci.nsIDOMNSEditableElement).editor.rootElement);
      let textBounds = range.getBoundingClientRect();

      // Get rect of editor
      let editorBounds = this._domWinUtils.sendQueryContentEvent(this._domWinUtils.QUERY_EDITOR_RECT, 0, 0, 0, 0);
      let editorRect = new Rect(editorBounds.left, editorBounds.top, editorBounds.width, editorBounds.height);

      // Use intersection of the text rect and the editor rect
      let rect = new Rect(textBounds.left, textBounds.top, textBounds.width, textBounds.height);
      rect.restrictTo(editorRect);

      // Clamp vertically and scroll if handle is at bounds. The top and bottom
      // must be restricted by an additional pixel since clicking on the top
      // edge of an input field moves the cursor to the beginning of that
      // field's text (and clicking the bottom moves the cursor to the end).
      if (aY < rect.y + 1) {
        aY = rect.y + 1;
        this._getSelectionController().scrollLine(false);
      } else if (aY > rect.y + rect.height - 1) {
        aY = rect.y + rect.height - 1;
        this._getSelectionController().scrollLine(true);
      }

      // Clamp horizontally and scroll if handle is at bounds
      if (aX < rect.x) {
        aX = rect.x;
        this._getSelectionController().scrollCharacter(false);
      } else if (aX > rect.x + rect.width) {
        aX = rect.x + rect.width;
        this._getSelectionController().scrollCharacter(true);
      }
    } else if (this._activeType == this.TYPE_SELECTION) {
      // Send mouse event 1px too high to prevent selection from entering the line below where it should be
      aY -= 1;
    }

    this._domWinUtils.sendMouseEventToWindow("mousedown", aX, aY, 0, 0, useShift ? Ci.nsIDOMNSEvent.SHIFT_MASK : 0, true);
    this._domWinUtils.sendMouseEventToWindow("mouseup", aX, aY, 0, 0, useShift ? Ci.nsIDOMNSEvent.SHIFT_MASK : 0, true);
  },

  copySelection: function sh_copySelection() {
    let selectedText = this._getSelectedText();
    if (selectedText.length) {
      let clipboard = Cc["@mozilla.org/widget/clipboardhelper;1"].getService(Ci.nsIClipboardHelper);
      clipboard.copyString(selectedText, this._contentWindow.document);
      NativeWindow.toast.show(Strings.browser.GetStringFromName("selectionHelper.textCopied"), "short");
    }
    this._closeSelection();
  },

  shareSelection: function sh_shareSelection() {
    let selectedText = this._getSelectedText();
    if (selectedText.length) {
      sendMessageToJava({
        type: "Share:Text",
        text: selectedText
      });
    }
    this._closeSelection();
  },

  /*
   * Called if for any reason we fail during the selection
   * process. Cancels the selection.
   */
  _onFail: function sh_onFail(aDbgMessage) {
    if (aDbgMessage && aDbgMessage.length > 0)
      Cu.reportError("SelectionHandler - " + aDbgMessage);
    this._closeSelection();
  },

  /*
   * Shuts SelectionHandler down.
   */
  _closeSelection: function sh_closeSelection() {
    // Bail if there's no active selection
    if (this._activeType == this.TYPE_NONE)
      return;

    if (this._activeType == this.TYPE_SELECTION) {
      let selection = this._getSelection();
      if (selection) {
        selection.removeAllRanges();
      }
    }

    this._activeType = this.TYPE_NONE;

    sendMessageToJava({ type: "TextSelection:HideHandles" });

    this._removeObservers();
    this._contentWindow.removeEventListener("pagehide", this, false);
    this._contentWindow.removeEventListener("keydown", this, false);
    this._contentWindow.removeEventListener("blur", this, true);
    this._contentWindow = null;
    this._targetElement = null;
    this._isRTL = false;
    this._cache = null;
  },

  _getViewOffset: function sh_getViewOffset() {
    let offset = { x: 0, y: 0 };
    let win = this._contentWindow;

    // Recursively look through frames to compute the total position offset.
    while (win.frameElement) {
      let rect = win.frameElement.getBoundingClientRect();
      offset.x += rect.left;
      offset.y += rect.top;

      win = win.parent;
    }

    return offset;
  },

  _pointInSelection: function sh_pointInSelection(aX, aY) {
    let offset = this._getViewOffset();
    let rangeRect = this._getSelection().getRangeAt(0).getBoundingClientRect();
    let radius = ElementTouchHelper.getTouchRadius();
    return (aX - offset.x > rangeRect.left - radius.left &&
            aX - offset.x < rangeRect.right + radius.right &&
            aY - offset.y > rangeRect.top - radius.top &&
            aY - offset.y < rangeRect.bottom + radius.bottom);
  },

  // Returns true if the selection has been reversed. Takes optional aIsStartHandle
  // param to decide whether the selection has been reversed.
  _updateCacheForSelection: function sh_updateCacheForSelection(aIsStartHandle) {
    let selection = this._getSelection();
    let rects = selection.getRangeAt(0).getClientRects();
    let start = { x: this._isRTL ? rects[0].right : rects[0].left, y: rects[0].bottom };
    let end = { x: this._isRTL ? rects[rects.length - 1].left : rects[rects.length - 1].right, y: rects[rects.length - 1].bottom };

    let selectionReversed = false;
    if (this._cache.start) {
      // If the end moved past the old end, but we're dragging the start handle, then that handle should become the end handle (and vice versa)
      selectionReversed = (aIsStartHandle && (end.y > this._cache.end.y || (end.y == this._cache.end.y && end.x > this._cache.end.x))) ||
                          (!aIsStartHandle && (start.y < this._cache.start.y || (start.y == this._cache.start.y && start.x < this._cache.start.x)));
    }

    this._cache.start = start;
    this._cache.end = end;

    return selectionReversed;
  },

  _positionHandles: function sh_positionHandles() {
    let scrollX = {}, scrollY = {};
    this._contentWindow.top.QueryInterface(Ci.nsIInterfaceRequestor).
                            getInterface(Ci.nsIDOMWindowUtils).getScrollXY(false, scrollX, scrollY);

    // the checkHidden function tests to see if the given point is hidden inside an
    // iframe/subdocument. this is so that if we select some text inside an iframe and
    // scroll the iframe so the selection is out of view, we hide the handles rather
    // than having them float on top of the main page content.
    let checkHidden = function(x, y) {
      return false;
    };
    if (this._contentWindow.frameElement) {
      let bounds = this._contentWindow.frameElement.getBoundingClientRect();
      checkHidden = function(x, y) {
        return x < 0 || y < 0 || x > bounds.width || y > bounds.height;
      }
    }

    let positions = null;
    if (this._activeType == this.TYPE_CURSOR) {
      // The left and top properties returned are relative to the client area
      // of the window, so we don't need to account for a sub-frame offset.
      let cursor = this._domWinUtils.sendQueryContentEvent(this._domWinUtils.QUERY_CARET_RECT, this._targetElement.selectionEnd, 0, 0, 0);
      let x = cursor.left;
      let y = cursor.top + cursor.height;
      positions = [{ handle: this.HANDLE_TYPE_MIDDLE,
                     left: x + scrollX.value,
                     top: y + scrollY.value,
                     hidden: checkHidden(x, y) }];
    } else {
      let sx = this._cache.start.x;
      let sy = this._cache.start.y;
      let ex = this._cache.end.x;
      let ey = this._cache.end.y;

      // Translate coordinates to account for selections in sub-frames. We can't cache
      // this because the top-level page may have scrolled since selection started.
      let offset = this._getViewOffset();

      positions = [{ handle: this.HANDLE_TYPE_START,
                     left: sx + offset.x + scrollX.value,
                     top: sy + offset.y + scrollY.value,
                     hidden: checkHidden(sx, sy) },
                   { handle: this.HANDLE_TYPE_END,
                     left: ex + offset.x + scrollX.value,
                     top: ey + offset.y + scrollY.value,
                     hidden: checkHidden(ex, ey) }];
    }

    sendMessageToJava({
      type: "TextSelection:PositionHandles",
      positions: positions,
      rtl: this._isRTL
    });
  },

  subdocumentScrolled: function sh_subdocumentScrolled(aElement) {
    if (this._activeType == this.TYPE_NONE) {
      return;
    }
    let scrollView = aElement.ownerDocument.defaultView;
    let view = this._contentWindow;
    while (true) {
      if (view == scrollView) {
        // The selection is in a view (or sub-view) of the view that scrolled.
        // So we need to reposition the handles.
        if (this._activeType == this.TYPE_SELECTION) {
          this._updateCacheForSelection();
        }
        this._positionHandles();
        break;
      }
      if (view == view.parent) {
        break;
      }
      view = view.parent;
    }
  }
};
