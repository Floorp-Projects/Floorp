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

  SELECT_ALL: 0,
  SELECT_AT_POINT: 1,

  // Keeps track of data about the dimensions of the selection. Coordinates
  // stored here are relative to the _contentWindow window.
  _cache: null,
  _activeType: 0, // TYPE_NONE
  _ignoreSelectionChanges: false, // True while user drags text selection handles
  _ignoreCompositionChanges: false, // Persist caret during IME composition updates

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
    Services.obs.addObserver(this, "Tab:Selected", false);
    Services.obs.addObserver(this, "after-viewport-change", false);
    Services.obs.addObserver(this, "TextSelection:Move", false);
    Services.obs.addObserver(this, "TextSelection:Position", false);
    Services.obs.addObserver(this, "TextSelection:End", false);
    Services.obs.addObserver(this, "TextSelection:Action", false);
    BrowserApp.deck.addEventListener("compositionend", this, false);
  },

  _removeObservers: function sh_removeObservers() {
    Services.obs.removeObserver(this, "Gesture:SingleTap");
    Services.obs.removeObserver(this, "Tab:Selected");
    Services.obs.removeObserver(this, "after-viewport-change");
    Services.obs.removeObserver(this, "TextSelection:Move");
    Services.obs.removeObserver(this, "TextSelection:Position");
    Services.obs.removeObserver(this, "TextSelection:End");
    Services.obs.removeObserver(this, "TextSelection:Action");
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
        } else if (this._activeType == this.TYPE_CURSOR) {
          // attachCaret() is called in the "Gesture:SingleTap" handler in BrowserEventHandler
          // We're guaranteed to call this first, because this observer was added last
          this._deactivate();
        }
        break;
      }
      case "Tab:Selected":
      case "TextSelection:End":
        this._closeSelection();
        break;
      case "TextSelection:Action":
        for (let type in this.actions) {
          if (this.actions[type].id == aData) {
            this.actions[type].action(this._targetElement);
            break;
          }
        }
        break;
      case "after-viewport-change": {
        if (this._activeType == this.TYPE_SELECTION) {
          // Update the cache after the viewport changes (e.g. panning, zooming).
          this._updateCacheForSelection();
        }
        break;
      }
      case "TextSelection:Move": {
        let data = JSON.parse(aData);
        if (this._activeType == this.TYPE_SELECTION) {
          // Ignore selectionChange notifications when handle movement starts
          this._ignoreSelectionChanges = true;
          this._moveSelection(data.handleType == this.HANDLE_TYPE_START, data.x, data.y);
        } else if (this._activeType == this.TYPE_CURSOR) {
          // Ignore IMM composition notifications when caret movement starts
          this._ignoreCompositionChanges = true;

          // Send a click event to the text box, which positions the caret
          this._sendMouseEvents(data.x, data.y);

          // Move the handle directly under the caret
          this._positionHandles();
        }
        break;
      }
      case "TextSelection:Position": {
        if (this._activeType == this.TYPE_SELECTION) {
          // Ignore selectionChange notifications when handle movement starts
          this._ignoreSelectionChanges = true;
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
          // Act on selectionChange notifications after handle movement ends
          this._ignoreSelectionChanges = false;

        } else if (this._activeType == this.TYPE_CURSOR) {
          // Act on IMM composition notifications after caret movement ends
          this._ignoreCompositionChanges = false;
        }
        this._positionHandles();
        break;
      }

      case "TextSelection:Get":
        sendMessageToJava({
          type: "TextSelection:Data",
          requestId: aData,
          text: this._getSelectedText()
        });
        break;
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
        // compositionend messages normally terminate caret display
        if (this._activeType == this.TYPE_CURSOR && !this._ignoreCompositionChanges) {
          this._deactivate();
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

  _getScrollPos: function sh_getScrollPos() {
    // Get the current display position
    let scrollX = {}, scrollY = {};
    this._contentWindow.top.QueryInterface(Ci.nsIInterfaceRequestor).
                            getInterface(Ci.nsIDOMWindowUtils).getScrollXY(false, scrollX, scrollY);
    return {
      X: scrollX.value,
      Y: scrollY.value
    };
  },

  notifySelectionChanged: function sh_notifySelectionChanged(aDocument, aSelection, aReason) {
    // Ignore selectionChange notifications during handle movements
    if (this._ignoreSelectionChanges) {
      return;
    }

    // If the selection was collapsed to Start or to End, always close it
    if ((aReason & Ci.nsISelectionListener.COLLAPSETOSTART_REASON) ||
        (aReason & Ci.nsISelectionListener.COLLAPSETOEND_REASON)) {
      this._closeSelection();
      return;
    }

    // If selected text no longer exists, close
    if (!aSelection.toString()) {
      this._closeSelection();
    }
  },

  /*
   * Called from browser.js when the user long taps on text or chooses
   * the "Select Word" context menu item. Initializes SelectionHandler,
   * starts a selection, and positions the text selection handles.
   *
   * @param aOptions list of options describing how to start selection
   *                 Options include:
   *                   mode - SELECT_ALL to select everything in the target
   *                          element, or SELECT_AT_POINT to select a word.
   *                   x    - The x-coordinate for SELECT_AT_POINT.
   *                   y    - The y-coordinate for SELECT_AT_POINT.
   */
  startSelection: function sh_startSelection(aElement, aOptions = { mode: SelectionHandler.SELECT_ALL }) {
    // Clear out any existing active selection
    this._closeSelection();

    this._initTargetInfo(aElement);

    // Clear any existing selection from the document
    this._contentWindow.getSelection().removeAllRanges();

    // Perform the appropriate selection method, if we can't determine method, or it fails, return
    if (!this._performSelection(aOptions)) {
      this._deactivate();
      return false;
    }

    // Double check results of successful selection operation
    let selection = this._getSelection();
    if (!selection || selection.rangeCount == 0 || selection.getRangeAt(0).collapsed) {
      this._deactivate();
      return false;
    }

    // Add a listener to end the selection if it's removed programatically
    selection.QueryInterface(Ci.nsISelectionPrivate).addSelectionListener(this);
    this._activeType = this.TYPE_SELECTION;

    // Initialize the cache
    this._cache = { start: {}, end: {}};
    this._updateCacheForSelection();

    let scroll = this._getScrollPos();
    // Figure out the distance between the selection and the click
    let positions = this._getHandlePositions(scroll);

    if (aOptions.mode == this.SELECT_AT_POINT && !this._selectionNearClick(scroll.X + aOptions.x,
                                                                      scroll.Y + aOptions.y,
                                                                      positions)) {
        this._closeSelection();
        return false;
    }

    this._positionHandles(positions);
    this._sendMessage("TextSelection:ShowHandles", [this.HANDLE_TYPE_START, this.HANDLE_TYPE_END], aOptions.x, aOptions.y);
    return true;
  },

  /*
   * Called to perform a selection operation, given a target element, selection method, starting point etc.
   */
  _performSelection: function sh_performSelection(aOptions) {
    if (aOptions.mode == this.SELECT_ALL) {
      if (this._targetElement instanceof HTMLPreElement)  {
        // Use SELECT_PARAGRAPH else we default to entire page including trailing whitespace
        return this._domWinUtils.selectAtPoint(1, 1, Ci.nsIDOMWindowUtils.SELECT_PARAGRAPH);
      } else {
        // Else default to selectALL Document
        this._getSelectionController().selectAll();
        return true;
      }
    }

    if (aOptions.mode == this.SELECT_AT_POINT) {
      return this._domWinUtils.selectAtPoint(aOptions.x, aOptions.y, Ci.nsIDOMWindowUtils.SELECT_WORDNOSPACE);
    }

    Services.console.logStringMessage("Invalid selection mode " + aOptions.mode);
    return false;
  },

  /* Return true if the current selection (given by aPositions) is near to where the coordinates passed in */
  _selectionNearClick: function(aX, aY, aPositions) {
      let distance = 0;

      // Check if the click was in the bounding box of the selection handles
      if (aPositions[0].left < aX && aX < aPositions[1].left
          && aPositions[0].top < aY && aY < aPositions[1].top) {
        distance = 0;
      } else {
        // If it was outside, check the distance to the center of the selection
        let selectposX = (aPositions[0].left + aPositions[1].left) / 2;
        let selectposY = (aPositions[0].top + aPositions[1].top) / 2;

        let dx = Math.abs(selectposX - aX);
        let dy = Math.abs(selectposY - aY);
        distance = dx + dy;
      }

      let maxSelectionDistance = Services.prefs.getIntPref("browser.ui.selection.distance");
      return (distance < maxSelectionDistance);
  },

  /* Reads a value from an action. If the action defines the value as a function, will return the result of calling
     the function. Otherwise, will return the value itself. If the value isn't defined for this action, will return a default */
  _getValue: function(obj, name, defaultValue) {
    if (!(name in obj))
      return defaultValue;

    if (typeof obj[name] == "function")
      return obj[name](this._targetElement);

    return obj[name];
  },

  _sendMessage: function(type, handles, aX, aY) {
    let actions = [];
    for (let type in this.actions) {
      let action = this.actions[type];
      if (action.selector.matches(this._targetElement, aX, aY)) {
        let a = {
          id: action.id,
          label: this._getValue(action, "label", ""),
          icon: this._getValue(action, "icon", "drawable://ic_status_logo"),
          showAsAction: this._getValue(action, "showAsAction", true),
          order: this._getValue(action, "order", 0)
        };
        actions.push(a);
      }
    }

    actions.sort((a, b) => b.order - a.order);

    sendMessageToJava({
      type: type,
      handles: handles,
      actions: actions,
    });
  },

  _updateMenu: function() {
    this._sendMessage("TextSelection:Update");
  },

  addAction: function(action) {
    if (!action.id)
      action.id = uuidgen.generateUUID().toString()

    if (this.actions[action.id])
      throw "Action with id " + action.id + " already added";

    this.actions[action.id] = action;
    return action.id;
  },

  removeAction: function(id) {
    delete this.actions[id];
  },

  actions: {
    SELECT_ALL: {
      label: Strings.browser.GetStringFromName("contextmenu.selectAll"),
      id: "selectall_action",
      icon: "drawable://select_all",
      action: function(aElement) {
        SelectionHandler.selectAll(aElement);
      },
      selector: ClipboardHelper.selectAllContext,
      order: 1,
    },

    CUT: {
      label: Strings.browser.GetStringFromName("contextmenu.cut"),
      id: "cut_action",
      icon: "drawable://cut",
      action: function(aElement) {
        let start = aElement.selectionStart;
        let end   = aElement.selectionEnd;

        SelectionHandler.copySelection();
        aElement.value = aElement.value.substring(0, start) + aElement.value.substring(end)

        // copySelection closes the selection. Show a caret where we just cut the text.
        SelectionHandler.attachCaret(aElement);
      },
      order: 1,
      selector: ClipboardHelper.cutContext,
    },

    COPY: {
      label: Strings.browser.GetStringFromName("contextmenu.copy"),
      id: "copy_action",
      icon: "drawable://copy",
      action: function() {
        SelectionHandler.copySelection();
      },
      order: 1,
      selector: ClipboardHelper.getCopyContext(false)
    },

    PASTE: {
      label: Strings.browser.GetStringFromName("contextmenu.paste"),
      id: "paste_action",
      icon: "drawable://paste",
      action: function(aElement) {
        ClipboardHelper.paste(aElement);
        SelectionHandler._positionHandles();
        SelectionHandler._updateMenu();
      },
      order: 1,
      selector: ClipboardHelper.pasteContext,
    },

    SHARE: {
      label: Strings.browser.GetStringFromName("contextmenu.share"),
      id: "share_action",
      icon: "drawable://ic_menu_share",
      action: function() {
        SelectionHandler.shareSelection();
      },
      selector: ClipboardHelper.shareContext,
    },

    SEARCH: {
      label: function() {
        return Strings.browser.formatStringFromName("contextmenu.search", [Services.search.defaultEngine.name], 1);
      },
      id: "search_action",
      icon: "drawable://ic_url_bar_search",
      action: function() {
        SelectionHandler.searchSelection();
        SelectionHandler._closeSelection();
      },
      selector: ClipboardHelper.searchWithContext,
    },

  },

  /*
   * Called by BrowserEventHandler when the user taps in a form input.
   * Initializes SelectionHandler and positions the caret handle.
   *
   * @param aX, aY tap location in client coordinates.
   */
  attachCaret: function sh_attachCaret(aElement) {
    // See if its an input element, and it isn't disabled, nor handled by Android native dialog
    if (aElement.disabled ||
        InputWidgetHelper.hasInputWidget(aElement) ||
        !((aElement instanceof HTMLInputElement && aElement.mozIsTextField(false)) ||
          (aElement instanceof HTMLTextAreaElement)))
      return;
    this._initTargetInfo(aElement);

    this._contentWindow.addEventListener("keydown", this, false);
    this._contentWindow.addEventListener("blur", this, true);

    this._activeType = this.TYPE_CURSOR;
    this._positionHandles();

    this._sendMessage("TextSelection:ShowHandles", [this.HANDLE_TYPE_MIDDLE]);
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
    if (!this._contentWindow)
      return "";

    let selection = this._getSelection();
    if (!selection)
      return "";

    if (this._targetElement instanceof Ci.nsIDOMHTMLTextAreaElement) {
      return selection.QueryInterface(Ci.nsISelectionPrivate).
        toStringWithFormat("text/plain", Ci.nsIDocumentEncoder.OutputPreformatted | Ci.nsIDocumentEncoder.OutputRaw, 0);
    }

    return selection.toString().trim();
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
  isSelectionActive: function sh_isSelectionActive() {
    return (this._activeType == this.TYPE_SELECTION);
  },

  selectAll: function sh_selectAll(aElement) {
    this.startSelection(aElement, { mode : this.SELECT_ALL });
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
      // the return value from sendQueryContentEvent is in LayoutDevice pixels and we want CSS pixels, so
      // divide by the pixel ratio
      let editorRect = new Rect(editorBounds.left / window.devicePixelRatio,
                                editorBounds.top / window.devicePixelRatio,
                                editorBounds.width / window.devicePixelRatio,
                                editorBounds.height / window.devicePixelRatio);

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

  searchSelection: function sh_searchSelection() {
    let selectedText = this._getSelectedText();
    if (selectedText.length) {
      let req = Services.search.defaultEngine.getSubmission(selectedText);
      let parent = BrowserApp.selectedTab;
      let isPrivate = PrivateBrowsingUtils.isWindowPrivate(parent.browser.contentWindow);
      // Set current tab as parent of new tab, and set new tab as private if the parent is.
      BrowserApp.addTab(req.uri.spec, {parentId: parent.id,
                                       selected: true,
                                       isPrivate: isPrivate});
    }
    this._closeSelection();
  },

  /*
   * Shuts SelectionHandler down.
   */
  _closeSelection: function sh_closeSelection() {
    // Bail if there's no active selection
    if (this._activeType == this.TYPE_NONE)
      return;

    if (this._activeType == this.TYPE_SELECTION)
      this._clearSelection();

    this._deactivate();
  },

  _clearSelection: function sh_clearSelection() {
    let selection = this._getSelection();
    if (selection) {
      // Remove our listener before we clear the selection
      selection.QueryInterface(Ci.nsISelectionPrivate).removeSelectionListener(this);
      // Clear selection without clearing the anchorNode or focusNode
      if (selection.rangeCount != 0) {
        selection.collapseToStart();
      }
    }
  },

  _deactivate: function sh_deactivate() {
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
    this._ignoreSelectionChanges = false;
    this._ignoreCompositionChanges = false;
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

  _getHandlePositions: function sh_getHandlePositions(scroll) {
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
      };
    }

    let positions = null;
    if (this._activeType == this.TYPE_CURSOR) {
      // The left and top properties returned are relative to the client area
      // of the window, so we don't need to account for a sub-frame offset.
      let cursor = this._domWinUtils.sendQueryContentEvent(this._domWinUtils.QUERY_CARET_RECT, this._targetElement.selectionEnd, 0, 0, 0);
      // the return value from sendQueryContentEvent is in LayoutDevice pixels and we want CSS pixels, so
      // divide by the pixel ratio
      let x = cursor.left / window.devicePixelRatio;
      let y = (cursor.top + cursor.height) / window.devicePixelRatio;
      return [{ handle: this.HANDLE_TYPE_MIDDLE,
                left: x + scroll.X,
                top: y + scroll.Y,
                hidden: checkHidden(x, y) }];
    } else {
      let sx = this._cache.start.x;
      let sy = this._cache.start.y;
      let ex = this._cache.end.x;
      let ey = this._cache.end.y;

      // Translate coordinates to account for selections in sub-frames. We can't cache
      // this because the top-level page may have scrolled since selection started.
      let offset = this._getViewOffset();

      return  [{ handle: this.HANDLE_TYPE_START,
                 left: sx + offset.x + scroll.X,
                 top: sy + offset.y + scroll.Y,
                 hidden: checkHidden(sx, sy) },
               { handle: this.HANDLE_TYPE_END,
                 left: ex + offset.x + scroll.X,
                 top: ey + offset.y + scroll.Y,
                 hidden: checkHidden(ex, ey) }];
    }
  },

  // positions is an array of objects with data about handle positions,
  // which we get from _getHandlePositions.
  _positionHandles: function sh_positionHandles(positions) {
    if (!positions) {
      positions = this._getHandlePositions(this._getScrollPos());
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
