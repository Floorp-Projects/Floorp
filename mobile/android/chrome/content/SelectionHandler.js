// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Define elements that bound phone number containers.
const PHONE_NUMBER_CONTAINERS = "td,div";
const DEFER_CLOSE_TRIGGER_MS = 125; // Grace period delay before deferred _closeSelection()

var SelectionHandler = {

  // Successful startSelection() or attachCaret().
  ERROR_NONE: "",

  // Error codes returned during startSelection().
  START_ERROR_INVALID_MODE: "Invalid selection mode requested.",
  START_ERROR_NONTEXT_INPUT: "Target element by definition contains no text.",
  START_ERROR_NO_WORD_SELECTED: "No word selected at point.",
  START_ERROR_SELECT_WORD_FAILED: "Word selection at point failed.",
  START_ERROR_SELECT_ALL_PARAGRAPH_FAILED: "Select-All Paragraph failed.",
  START_ERROR_NO_SELECTION: "Selection performed, but nothing resulted.",
  START_ERROR_PROXIMITY: "Selection target and result seem unrelated.",

  // Error codes returned during attachCaret().
  ATTACH_ERROR_INCOMPATIBLE: "Element disabled, handled natively, or not editable.",

  HANDLE_TYPE_ANCHOR: "ANCHOR",
  HANDLE_TYPE_CARET: "CARET",
  HANDLE_TYPE_FOCUS: "FOCUS",

  TYPE_NONE: 0,
  TYPE_CURSOR: 1,
  TYPE_SELECTION: 2,

  SELECT_ALL: 0,
  SELECT_AT_POINT: 1,

  // Keeps track of data about the dimensions of the selection. Coordinates
  // stored here are relative to the _contentWindow window.
  _cache: { anchorPt: {}, focusPt: {} },
  _targetIsRTL: false,
  _anchorIsRTL: false,
  _focusIsRTL: false,

  _activeType: 0, // TYPE_NONE
  _selectionPrivate: null, // private selection reference
  _selectionID: null, // Unique Selection ID

  _draggingHandles: false, // True while user drags text selection handles
  _dragStartAnchorOffset: null, // Editables need initial pos during HandleMove events
  _dragStartFocusOffset: null, // Editables need initial pos during HandleMove events

  _ignoreCompositionChanges: false, // Persist caret during IME composition updates
  _prevHandlePositions: [], // Avoid issuing duplicate "TextSelection:Position" messages
  _deferCloseTimer: null, // Used to defer _closeSelection() actions during programmatic changes

  // TargetElement changes (text <--> no text) trigger actionbar UI update
  _prevTargetElementHasText: null,

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

  // Provides UUID service for selection ID's.
  get _idService() {
    delete this._idService;
    return this._idService = Cc["@mozilla.org/uuid-generator;1"].
      getService(Ci.nsIUUIDGenerator);
  },

  _addObservers: function sh_addObservers() {
    Services.obs.addObserver(this, "Gesture:SingleTap", false);
    Services.obs.addObserver(this, "Tab:Selected", false);
    Services.obs.addObserver(this, "TextSelection:Move", false);
    Services.obs.addObserver(this, "TextSelection:Position", false);
    Services.obs.addObserver(this, "TextSelection:End", false);
    Services.obs.addObserver(this, "TextSelection:Action", false);
    Services.obs.addObserver(this, "TextSelection:LayerReflow", false);

    BrowserApp.deck.addEventListener("pagehide", this, false);
    BrowserApp.deck.addEventListener("blur", this, true);
    BrowserApp.deck.addEventListener("scroll", this, true);
  },

  _removeObservers: function sh_removeObservers() {
    Services.obs.removeObserver(this, "Gesture:SingleTap");
    Services.obs.removeObserver(this, "Tab:Selected");
    Services.obs.removeObserver(this, "TextSelection:Move");
    Services.obs.removeObserver(this, "TextSelection:Position");
    Services.obs.removeObserver(this, "TextSelection:End");
    Services.obs.removeObserver(this, "TextSelection:Action");
    Services.obs.removeObserver(this, "TextSelection:LayerReflow");

    BrowserApp.deck.removeEventListener("pagehide", this, false);
    BrowserApp.deck.removeEventListener("blur", this, true);
    BrowserApp.deck.removeEventListener("scroll", this, true);
  },

  observe: function sh_observe(aSubject, aTopic, aData) {
    // Ignore all but selectionListener notifications during deferred _closeSelection().
    if (this._deferCloseTimer) {
      return;
    }

    switch (aTopic) {
      // Update selectionListener and handle/caret positions, on page reflow
      // (keyboard open/close, dynamic DOM changes, orientation updates, etc).
      case "TextSelection:LayerReflow": {
        if (this._activeType == this.TYPE_SELECTION) {
          this._updateSelectionListener();
        }
        if (this._activeType != this.TYPE_NONE) {
          this._positionHandlesOnChange();
        }
        break;
      }

      case "Gesture:SingleTap": {
        if (this._activeType == this.TYPE_CURSOR) {
          // attachCaret() is called in the "Gesture:SingleTap" handler in BrowserEventHandler
          // We're guaranteed to call this first, because this observer was added last
          this._deactivate();
        }
        break;
      }

      case "Tab:Selected":
        this._closeSelection();
        break;

      case "TextSelection:End":
        let data = JSON.parse(aData);
        // End the requested selection only.
        if (this._selectionID === data.selectionID) {
          this._closeSelection();
        }
        break;

      case "TextSelection:Action":
        for (let type in this.actions) {
          if (this.actions[type].id == aData) {
            this.actions[type].action(this._targetElement);
            break;
          }
        }
        break;

      case "TextSelection:Move": {
        let data = JSON.parse(aData);
        if (this._activeType == this.TYPE_SELECTION) {
          this._startDraggingHandles();
          this._moveSelection(data.handleType, new Point(data.x, data.y));

        } else if (this._activeType == this.TYPE_CURSOR) {
          this._startDraggingHandles();

          // Ignore IMM composition notifications when caret movement starts
          this._ignoreCompositionChanges = true;
          this._moveCaret(data.x, data.y);

          // Move the handle directly under the caret
          this._positionHandles();
        }
        break;
      }

      case "TextSelection:Position": {
        if (this._activeType == this.TYPE_SELECTION) {
          this._startDraggingHandles();
          this._ensureSelectionDirection();
          this._stopDraggingHandles();
          this._positionHandles();

          // Changes to handle position can affect selection context and actionbar display
          this._updateMenu();

        } else if (this._activeType == this.TYPE_CURSOR) {
          // Act on IMM composition notifications after caret movement ends
          this._ignoreCompositionChanges = false;
          this._stopDraggingHandles();
          this._positionHandles();

        } else {
          Cu.reportError("Ignored \"TextSelection:Position\" message during invalid selection status");
        }

        break;
      }

      case "TextSelection:Get":
        Messaging.sendRequest({
          type: "TextSelection:Data",
          requestId: aData,
          text: this._getSelectedText()
        });
        break;
    }
  },

  // Ignore selectionChange notifications during handle dragging, disable dynamic
  // IME text compositions (autoSuggest, autoCorrect, etc)
  _startDraggingHandles: function sh_startDraggingHandles() {
    if (!this._draggingHandles) {
      this._draggingHandles = true;
      let selection = this._getSelection();
      this._dragStartAnchorOffset = selection.anchorOffset;
      this._dragStartFocusOffset = selection.focusOffset;
      Messaging.sendRequest({ type: "TextSelection:DraggingHandle", dragging: true });
    }
  },

  // Act on selectionChange notifications when not dragging handles, allow dynamic
  // IME text compositions (autoSuggest, autoCorrect, etc)
  _stopDraggingHandles: function sh_stopDraggingHandles() {
    if (this._draggingHandles) {
      this._draggingHandles = false;
      this._dragStartAnchorOffset = null;
      this._dragStartFocusOffset = null;
      Messaging.sendRequest({ type: "TextSelection:DraggingHandle", dragging: false });
    }
  },

  handleEvent: function sh_handleEvent(aEvent) {
    // Ignore all but selectionListener notifications during deferred _closeSelection().
    if (this._deferCloseTimer) {
      return;
    }

    switch (aEvent.type) {
      case "scroll":
        // Maintain position when top-level document is scrolled
        this._positionHandlesOnChange();
        break;

      case "pagehide": {
        // We only care about events on the selected tab.
        let tab = BrowserApp.getTabForWindow(aEvent.originalTarget.defaultView);
        if (tab == BrowserApp.selectedTab) {
          this._closeSelection();
        }
        break;
      }

      case "blur":
        this._closeSelection();
        break;

      // Update caret position on keyboard activity
      case "keyup":
        // Not generated by Swiftkeyboard
      case "compositionupdate":
      case "compositionend":
        // Generated by SwiftKeyboard, et. al.
        if (!this._ignoreCompositionChanges) {
          this._positionHandles();
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

  /**
   * Add a selection listener to monitor for external selection changes.
   */
  _addSelectionListener: function(selection) {
    this._selectionPrivate = selection.QueryInterface(Ci.nsISelectionPrivate);
    this._selectionPrivate.addSelectionListener(this);
  },

  /**
   * The nsISelection object for an editable can change during DOM mutations,
   * causing us to stop receiving selectionChange notifications.
   *
   * We can detect that after a layer-reflow event, and dynamically update the
   * listener.
   */
  _updateSelectionListener: function() {
    if (!(this._targetElement instanceof Ci.nsIDOMNSEditableElement)) {
      return;
    }

    let selection = this._getSelection();
    if (this._selectionPrivate != selection.QueryInterface(Ci.nsISelectionPrivate)) {
      this._removeSelectionListener();
      this._addSelectionListener(selection);
    }
  },

  /**
   * Remove the selection listener.
   */
  _removeSelectionListener: function() {
    this._selectionPrivate.removeSelectionListener(this);
    this._selectionPrivate = null;
  },

  /**
   * Observe and react to programmatic SelectionChange notifications.
   */
  notifySelectionChanged: function sh_notifySelectionChanged(aDocument, aSelection, aReason) {
    // Cancel any in-progress / deferred _closeSelection() action.
    this._cancelDeferredCloseSelection();

    // Ignore selectionChange notifications during handle movements
    if (this._draggingHandles) {
      return;
    }

    // If the selection was collapsed to Start or to End, always close it
    if ((aReason & Ci.nsISelectionListener.COLLAPSETOSTART_REASON) ||
        (aReason & Ci.nsISelectionListener.COLLAPSETOEND_REASON)) {
      this._closeSelection();
      return;
    }

    // If selected text no longer exists, schedule a deferred close action.
    if (!aSelection.toString()) {
      this._deferCloseSelection();
      return;
    }

    // Update the selection handle positions.
    this._positionHandles();
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

    if (this._isNonTextInputElement(aElement)) {
      return this.START_ERROR_NONTEXT_INPUT;
    }

    const focus = Services.focus.focusedWindow;
    if (focus) {
      // Make sure any previous focus is cleared.
      Services.focus.clearFocus(focus);
    }

    this._initTargetInfo(aElement, this.TYPE_SELECTION);

    // Perform the appropriate selection method, if we can't determine method, or it fails, return
    let selectionResult = this._performSelection(aOptions);
    if (selectionResult !== this.ERROR_NONE) {
      this._deactivate();
      return selectionResult;
    }

    // Double check results of successful selection operation
    let selection = this._getSelection();
    if (!selection ||
        selection.rangeCount == 0 ||
        selection.getRangeAt(0).collapsed ||
        this._getSelectedText().length == 0) {
      this._deactivate();
      return this.START_ERROR_NO_SELECTION;
    }

    // Add a listener to end the selection if it's removed programatically
    this._addSelectionListener(selection);
    this._activeType = this.TYPE_SELECTION;

    // Figure out the distance between the selection and the click
    let scroll = this._getScrollPos();
    let positions = this._getHandlePositions(scroll);

    if (aOptions.mode == this.SELECT_AT_POINT &&
        !this._selectionNearClick(scroll.X + aOptions.x, scroll.Y + aOptions.y, positions)) {
        this._closeSelection();
        return this.START_ERROR_PROXIMITY;
    }

    // Determine position and show handles, open actionbar
    this._positionHandles(positions);
    Messaging.sendRequest({
      selectionID: this._selectionID,
      type: "TextSelection:ShowHandles",
      handles: [this.HANDLE_TYPE_ANCHOR, this.HANDLE_TYPE_FOCUS]
    });
    this._updateMenu();
    return this.ERROR_NONE;
  },

  /*
   * Called to perform a selection operation, given a target element, selection method, starting point etc.
   */
  _performSelection: function sh_performSelection(aOptions) {
    if (aOptions.mode == this.SELECT_AT_POINT) {
      // Clear any ranges selected outside SelectionHandler, by code such as Find-In-Page.
      this._contentWindow.getSelection().removeAllRanges();
      try {
        if (!this._domWinUtils.selectAtPoint(aOptions.x, aOptions.y, Ci.nsIDOMWindowUtils.SELECT_WORDNOSPACE)) {
          return this.START_ERROR_NO_WORD_SELECTED;
        }
      } catch (e) {
        return this.START_ERROR_SELECT_WORD_FAILED;
      }

      // Perform additional phone-number "smart selection".
      if (this._isPhoneNumber(this._getSelection().toString())) {
        this._selectSmartPhoneNumber();
      }

      return this.ERROR_NONE;
    }

    // Only selectAll() assumed from this point.
    if (aOptions.mode != this.SELECT_ALL) {
      return this.START_ERROR_INVALID_MODE;
    }

    // HTMLPreElement is a #text node, SELECT_ALL implies entire paragraph
    if (this._targetElement instanceof HTMLPreElement)  {
      try {
        this._domWinUtils.selectAtPoint(1, 1, Ci.nsIDOMWindowUtils.SELECT_PARAGRAPH);
        return this.ERROR_NONE;
      } catch (e) {
        return this.START_ERROR_SELECT_ALL_PARAGRAPH_FAILED;
      }
    }

    // Else default to selectALL Document
    let editor = this._getEditor();
    if (editor) {
      editor.selectAll();
    } else {
      this._getSelectionController().selectAll();
    }

    // Selection is entire HTMLHtmlElement, remove any trailing document whitespace
    let selection = this._getSelection();
    let lastNode = selection.focusNode;
    while (lastNode && lastNode.lastChild) {
      lastNode = lastNode.lastChild;
    }

    if (lastNode instanceof Text) {
      try {
        selection.extend(lastNode, lastNode.length);
      } catch (e) {
        Cu.reportError("SelectionHandler.js: _performSelection() whitespace trim fails: lastNode[" + lastNode +
          "] lastNode.length[" + lastNode.length + "]");
      }
    }

    return this.ERROR_NONE;
  },

  /*
   * Called to expand a selection that appears to represent a phone number. This enhances the basic
   * SELECT_WORDNOSPACE logic employed in performSelection() in response to long-tap / selecting text.
   */
  _selectSmartPhoneNumber: function() {
    this._extendPhoneNumberSelection("forward");
    this._reversePhoneNumberSelectionDir();

    this._extendPhoneNumberSelection("backward");
    this._reversePhoneNumberSelectionDir();
  },

  /*
   * Extend the current phone number selection in the requested direction.
   */
  _extendPhoneNumberSelection: function(direction) {
    let selection = this._getSelection();

    // Extend the phone number selection until we find a boundry.
    while (true) {
      // Save current focus position, and extend the selection.
      let focusNode = selection.focusNode;
      let focusOffset = selection.focusOffset;
      selection.modify("extend", direction, "character");

      // If the selection doesn't change, (can't extend further), we're done.
      if (selection.focusNode == focusNode && selection.focusOffset == focusOffset) {
        return;
      }

      // Don't extend past a valid phone number.
      if (!this._isPhoneNumber(selection.toString().trim())) {
        // Backout the undesired selection extend, and we're done.
        selection.collapse(selection.anchorNode, selection.anchorOffset);
        selection.extend(focusNode, focusOffset);
        return;
      }

      // Don't extend the selection into a new container.
      if (selection.focusNode != focusNode) {
        let nextContainer = (selection.focusNode instanceof Text) ?
          selection.focusNode.parentNode : selection.focusNode;
        if (nextContainer.matches &&
            nextContainer.matches(PHONE_NUMBER_CONTAINERS)) {
          // Backout the undesired selection extend, and we're done.
          selection.collapse(selection.anchorNode, selection.anchorOffset);
          selection.extend(focusNode, focusOffset);
          return
        }
      }
    }
  },

  /*
   * Reverse the the selection direction, swapping anchorNode <-+-> focusNode.
   */
  _reversePhoneNumberSelectionDir: function(direction) {
    let selection = this._getSelection();

    let anchorNode = selection.anchorNode;
    let anchorOffset = selection.anchorOffset;
    selection.collapse(selection.focusNode, selection.focusOffset);
    selection.extend(anchorNode, anchorOffset);
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

  addAction: function(action) {
    if (!action.id)
      action.id = uuidgen.generateUUID().toString()

    if (this.actions[action.id])
      throw "Action with id " + action.id + " already added";

    // Update actions list and actionbar UI if active.
    this.actions[action.id] = action;
    this._updateMenu();
    return action.id;
  },

  removeAction: function(id) {
    // Update actions list and actionbar UI if active.
    delete this.actions[id];
    this._updateMenu();
  },

  _updateMenu: function() {
    if (this._activeType == this.TYPE_NONE) {
      return;
    }

    // Update actionbar UI.
    let actions = [];
    for (let type in this.actions) {
      let action = this.actions[type];
      if (action.selector.matches(this._targetElement)) {
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

    Messaging.sendRequest({
      type: "TextSelection:Update",
      actions: actions
    });
  },

  /*
   * Actionbar methods.
   */
  actions: {
    SELECT_ALL: {
      label: Strings.browser.GetStringFromName("contextmenu.selectAll"),
      id: "selectall_action",
      icon: "drawable://ab_select_all",
      action: function(aElement) {
        SelectionHandler.startSelection(aElement);
        UITelemetry.addEvent("action.1", "actionbar", null, "select_all");
      },
      order: 5,
      selector: {
        matches: function(aElement) {
          return (aElement.textLength != 0);
        }
      }
    },

    CUT: {
      label: Strings.browser.GetStringFromName("contextmenu.cut"),
      id: "cut_action",
      icon: "drawable://ab_cut",
      action: function(aElement) {
        let start = aElement.selectionStart;
        let end   = aElement.selectionEnd;

        SelectionHandler.copySelection();
        aElement.value = aElement.value.substring(0, start) + aElement.value.substring(end)

        // copySelection closes the selection. Show a caret where we just cut the text.
        SelectionHandler.attachCaret(aElement);
        UITelemetry.addEvent("action.1", "actionbar", null, "cut");
      },
      order: 4,
      selector: {
        matches: function(aElement) {
          // Disallow cut for contentEditable elements (until Bug 1112276 is fixed).
          return !aElement.isContentEditable && SelectionHandler.isElementEditableText(aElement) ?
            SelectionHandler.isSelectionActive() : false;
        }
      }
    },

    COPY: {
      label: Strings.browser.GetStringFromName("contextmenu.copy"),
      id: "copy_action",
      icon: "drawable://ab_copy",
      action: function() {
        SelectionHandler.copySelection();
        UITelemetry.addEvent("action.1", "actionbar", null, "copy");
      },
      order: 3,
      selector: {
        matches: function(aElement) {
          // Don't include "copy" for password fields.
          // mozIsTextField(true) tests for only non-password fields.
          if (aElement instanceof Ci.nsIDOMHTMLInputElement && !aElement.mozIsTextField(true)) {
            return false;
          }
          return SelectionHandler.isSelectionActive();
        }
      }
    },

    PASTE: {
      label: Strings.browser.GetStringFromName("contextmenu.paste"),
      id: "paste_action",
      icon: "drawable://ab_paste",
      action: function(aElement) {
        if (aElement) {
          let target = SelectionHandler._getEditor();
          aElement.focus();
          target.paste(Ci.nsIClipboard.kGlobalClipboard);
          SelectionHandler._closeSelection();
          UITelemetry.addEvent("action.1", "actionbar", null, "paste");
        }
      },
      order: 2,
      selector: {
        matches: function(aElement) {
          if (SelectionHandler.isElementEditableText(aElement)) {
            let flavors = ["text/unicode"];
            return Services.clipboard.hasDataMatchingFlavors(flavors, flavors.length, Ci.nsIClipboard.kGlobalClipboard);
          }
          return false;
        }
      }
    },

    SHARE: {
      label: Strings.browser.GetStringFromName("contextmenu.share"),
      id: "share_action",
      icon: "drawable://ic_menu_share",
      action: function() {
        SelectionHandler.shareSelection();
        UITelemetry.addEvent("action.1", "actionbar", null, "share");
      },
      selector: {
        matches: function() {
          if (!ParentalControls.isAllowed(ParentalControls.SHARE)) {
            return false;
          }

          return SelectionHandler.isSelectionActive();
        }
      }
    },

    SEARCH_ADD: {
      id: "search_add_action",
      label: Strings.browser.GetStringFromName("contextmenu.addSearchEngine2"),
      icon: "drawable://ab_add_search_engine",

      selector: {
        matches: function(element) {
          if(!(element instanceof HTMLInputElement)) {
            return false;
          }
          let form = element.form;
          if (!form || element.type == "password") {
            return false;
          }

          // These are the following types of forms we can create keywords for:
          //
          // method    encoding type        can create keyword
          // GET       *                                   YES
          //           *                                   YES
          // POST      *                                   YES
          // POST      application/x-www-form-urlencoded   YES
          // POST      text/plain                          NO ( a little tricky to do)
          // POST      multipart/form-data                 NO
          // POST      everything else                     YES
          let method = form.method.toUpperCase();
          return (method == "GET" || method == "") ||
                 (form.enctype != "text/plain") && (form.enctype != "multipart/form-data");
        },
      },

      action: function(element) {
        UITelemetry.addEvent("action.1", "actionbar", null, "add_search_engine");
        SearchEngines.addEngine(element);
      },
    },

    SEARCH: {
      label: function() {
        return Strings.browser.formatStringFromName("contextmenu.search", [Services.search.defaultEngine.name], 1);
      },
      id: "search_action",
      icon: "drawable://ab_search",
      action: function() {
        SelectionHandler.searchSelection();
        SelectionHandler._closeSelection();
        UITelemetry.addEvent("action.1", "actionbar", null, "search");
      },
      order: 1,
      selector: {
        matches: function() {
          return SelectionHandler.isSelectionActive();
        }
      }
    },

    CALL: {
      label: Strings.browser.GetStringFromName("contextmenu.call"),
      id: "call_action",
      icon: "drawable://phone",
      action: function() {
        SelectionHandler.callSelection();
        UITelemetry.addEvent("action.1", "actionbar", null, "call");
      },
      order: 1,
      selector: {
        matches: function () {
          return SelectionHandler._getSelectedPhoneNumber() != null;
        }
      }
    }
  },

  /*
   * Called by BrowserEventHandler when the user taps in a form input.
   * Initializes SelectionHandler and positions the caret handle.
   *
   * @param aX, aY tap location in client coordinates.
   */
  attachCaret: function sh_attachCaret(aElement) {
    // Clear out any existing active selection
    this._closeSelection();

    // Ensure it isn't disabled, isn't handled by Android native dialog, and is editable text element
    if (aElement.disabled || InputWidgetHelper.hasInputWidget(aElement) || !this.isElementEditableText(aElement)) {
      return this.ATTACH_ERROR_INCOMPATIBLE;
    }

    this._initTargetInfo(aElement, this.TYPE_CURSOR);

    // Caret-specific observer/listeners
    BrowserApp.deck.addEventListener("keyup", this, false);
    BrowserApp.deck.addEventListener("compositionupdate", this, false);
    BrowserApp.deck.addEventListener("compositionend", this, false);

    this._activeType = this.TYPE_CURSOR;

    // Determine position and show caret, open actionbar
    this._positionHandles();
    Messaging.sendRequest({
      selectionID: this._selectionID,
      type: "TextSelection:ShowHandles",
      handles: [this.HANDLE_TYPE_CARET]
    });
    this._updateMenu();

    return this.ERROR_NONE;
  },

  // Target initialization for both TYPE_CURSOR and TYPE_SELECTION
  _initTargetInfo: function sh_initTargetInfo(aElement, aSelectionType) {
    this._targetElement = aElement;
    if (aElement instanceof Ci.nsIDOMNSEditableElement) {
      if (aSelectionType === this.TYPE_SELECTION) {
        // Blur the targetElement to force IME code to undo previous style compositions
        // (visible underlines / etc generated by autoCorrection, autoSuggestion)
        aElement.blur();
      }
      // Ensure targetElement is now focused normally
      aElement.focus();
    }

    this._selectionID = this._idService.generateUUID().toString();
    this._stopDraggingHandles();
    this._contentWindow = aElement.ownerDocument.defaultView;
    this._targetIsRTL = (this._contentWindow.getComputedStyle(aElement, "").direction == "rtl");

    this._addObservers();
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

  _getEditor: function sh_getEditor() {
    if (this._targetElement instanceof Ci.nsIDOMNSEditableElement) {
      return this._targetElement.QueryInterface(Ci.nsIDOMNSEditableElement).editor;
    }
    return this._contentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIWebNavigation)
                              .QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIEditingSession)
                              .getEditorForWindow(this._contentWindow);
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

  isElementEditableText: function (aElement) {
    return (((aElement instanceof HTMLInputElement && aElement.mozIsTextField(false)) ||
            (aElement instanceof HTMLTextAreaElement)) && !aElement.readOnly) ||
            aElement.isContentEditable;
  },

  _isNonTextInputElement: function(aElement) {
    return (aElement instanceof HTMLInputElement && !aElement.mozIsTextField(false));
  },

  /*
   * Moves the selection as the user drags a handle.
   * @param handleType: Specifies either the anchor or the focus handle.
   * @param handlePt: selection point in client coordinates.
   */
  _moveSelection: function sh_moveSelection(handleType, handlePt) {
    let isAnchorHandle = (handleType == this.HANDLE_TYPE_ANCHOR);

    // Determine new caret position from handlePt, exit if user
    // moved it offscreen.
    let viewOffset = this._getViewOffset();
    let ptX = handlePt.x - viewOffset.x;
    let ptY = handlePt.y - viewOffset.y;
    let cwd = this._contentWindow.document;
    let caretPos = cwd.caretPositionFromPoint(ptX, ptY);
    if (!caretPos) {
      return;
    }

    // Constrain text selection within editable elements.
    let targetIsEditable = this._targetElement instanceof Ci.nsIDOMNSEditableElement;
    if (targetIsEditable && (caretPos.offsetNode != this._targetElement)) {
      return;
    }

    // Update the Selection for editable elements. Selection Change
    // logic is the same, regardless of RTL/LTR. Selection direction is
    // maintained always forward (startOffset <= endOffset).
    if (targetIsEditable) {
      let start = this._dragStartAnchorOffset;
      let end = this._dragStartFocusOffset;
      if (isAnchorHandle) {
        start = caretPos.offset;
      } else {
        end = caretPos.offset;
      }
      if (start > end) {
        [start, end] = [end, start];
      }
      this._targetElement.setSelectionRange(start, end);
      return;
    }

    // Update the Selection for non-editable elements. Selection Change
    // logic is the same, regardless of RTL/LTR. Selection direction internally
    // can finish reversed by user drag. ie: Forward is (a,o ---> f,o),
    // and reversed is (a,o <--- f,o).
    let selection = this._getSelection();
    if (isAnchorHandle) {
      let focusNode = selection.focusNode;
      let focusOffset = selection.focusOffset;
      selection.collapse(caretPos.offsetNode, caretPos.offset);
      selection.extend(focusNode, focusOffset);
    } else {
      selection.extend(caretPos.offsetNode, caretPos.offset);
    }
  },

  _moveCaret: function sh_moveCaret(aX, aY) {
    // Get rect of text inside element
    let range = document.createRange();
    range.selectNodeContents(this._getEditor().rootElement);
    let textBounds = range.getBoundingClientRect();

    // Get rect of editor
    let editorBounds = this._domWinUtils.sendQueryContentEvent(this._domWinUtils.QUERY_EDITOR_RECT, 0, 0, 0, 0,
                                                               this._domWinUtils.QUERY_CONTENT_FLAG_USE_XP_LINE_BREAK);
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

    this._domWinUtils.sendMouseEventToWindow("mousedown", aX, aY, 0, 0, 0, true);
    this._domWinUtils.sendMouseEventToWindow("mouseup", aX, aY, 0, 0, 0, true);
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
      Messaging.sendRequest({
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
      let isPrivate = PrivateBrowsingUtils.isBrowserPrivate(parent.browser);
      // Set current tab as parent of new tab, and set new tab as private if the parent is.
      BrowserApp.addTab(req.uri.spec, {parentId: parent.id,
                                       selected: true,
                                       isPrivate: isPrivate});
    }
    this._closeSelection();
  },

  _phoneRegex: /^\+?[0-9\s,-.\(\)*#pw]{1,30}$/,

  _getSelectedPhoneNumber: function sh_getSelectedPhoneNumber() {
    return this._isPhoneNumber(this._getSelectedText().trim());
  },

  _isPhoneNumber: function sh_isPhoneNumber(selectedText) {
    return (this._phoneRegex.test(selectedText) ? selectedText : null);
  },

  callSelection: function sh_callSelection() {
    let selectedText = this._getSelectedPhoneNumber();
    if (selectedText) {
      BrowserApp.loadURI("tel:" + selectedText);
    }
    this._closeSelection();
  },

  /**
   * Deferred _closeSelection() actions allow for brief periods where programmatic
   * selection changes have effectively closed the selection, but we anticipate further
   * activity that may restore it.
   *
   * At this point, we hide the UI handles, and stop responding to messages until
   * either the final _closeSelection() is triggered, or until our Gecko selectionListener
   * notices a subsequent programmatic selection that results in a new selection.
   */
  _deferCloseSelection: function() {
    // Schedule the deferred _closeSelection() action.
    this._deferCloseTimer = setTimeout((function() {
      // Time is up! Close the selection.
      this._deferCloseTimer = null;
      this._closeSelection();
    }).bind(this), DEFER_CLOSE_TRIGGER_MS);

    // Hide any handles while deferClosed.
    if (this._prevHandlePositions.length) {
      let positions = this._prevHandlePositions;
      for (let i in positions) {
        positions[i].hidden = true;
      }

      Messaging.sendRequest({
        type: "TextSelection:PositionHandles",
        positions: positions,
      });
    }
  },

  /**
   * Cancel any current deferred _closeSelection() action.
   */
  _cancelDeferredCloseSelection: function() {
    if (this._deferCloseTimer) {
      clearTimeout(this._deferCloseTimer);
      this._deferCloseTimer = null;
    }
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
    // Cancel any in-progress / deferred _closeSelection() process.
    this._cancelDeferredCloseSelection();

    let selection = this._getSelection();
    if (selection) {
      // Remove our listener before we clear the selection
      this._removeSelectionListener();

      // Remove the selection. For editables, we clear selection without losing
      // element focus. For non-editables, just clear all.
      if (selection.rangeCount != 0) {
        if (this.isElementEditableText(this._targetElement)) {
          selection.collapseToStart();
        } else {
          selection.removeAllRanges();
        }
      }
    }
  },

  _deactivate: function sh_deactivate() {
    this._stopDraggingHandles();
    // Hide handle/caret, close actionbar
    Messaging.sendRequest({ type: "TextSelection:HideHandles" });

    this._removeObservers();

    // Only observed for caret positioning
    if (this._activeType == this.TYPE_CURSOR) {
      BrowserApp.deck.removeEventListener("keyup", this);
      BrowserApp.deck.removeEventListener("compositionupdate", this);
      BrowserApp.deck.removeEventListener("compositionend", this);
    }

    this._contentWindow = null;
    this._targetElement = null;
    this._targetIsRTL = false;
    this._ignoreCompositionChanges = false;
    this._prevHandlePositions = [];
    this._prevTargetElementHasText = null;

    this._activeType = this.TYPE_NONE;
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

  /*
   * The direction of the Selection is ensured for editables while the user drags
   * the handles (per "TextSelection:Move" event). For non-editables, we just let
   * the user change direction, but fix it up at the end of handle movement (final
   * "TextSelection:Position" event).
   */
  _ensureSelectionDirection: function() {
    // Never needed at this time.
    if (this._targetElement instanceof Ci.nsIDOMNSEditableElement) {
      return;
    }

    // Nothing needed if not reversed.
    let qcEventResult = this._domWinUtils.sendQueryContentEvent(
      this._domWinUtils.QUERY_SELECTED_TEXT, 0, 0, 0, 0);
    if (!qcEventResult.reversed) {
      return;
    }

    // Reverse the Selection.
    let selection = this._getSelection();
    let newFocusNode = selection.anchorNode;
    let newFocusOffset = selection.anchorOffset;

    selection.collapse(selection.focusNode, selection.focusOffset);
    selection.extend(newFocusNode, newFocusOffset);
  },

  /*
   * Updates the TYPE_SELECTION cache, with the handle anchor/focus point values
   * of the current selection. Passed to Java for UI positioning only.
   *
   * Note that the anchor handle and focus handle can reference text in nodes
   * with mixed direction. (ie a.direction = "rtl" while f.direction = "ltr").
   */
  _updateCacheForSelection: function() {
    let selection = this._getSelection();
    let rects = selection.getRangeAt(0).getClientRects();
    if (rects.length == 0) {
      // nsISelection object exists, but there's nothing actually selected
      throw "Failed to update cache for invalid selection";
    }

    // Right-to-Left (ie: Hebrew) anchorPt is on right,
    // Left-to-Right (ie: English) anchorPt is on left.
    this._anchorIsRTL = this._isNodeRTL(selection.anchorNode);
    let anchorIdx = 0;
    this._cache.anchorPt = (this._anchorIsRTL) ?
      new Point(rects[anchorIdx].right, rects[anchorIdx].bottom) :
      new Point(rects[anchorIdx].left, rects[anchorIdx].bottom);

    // Right-to-Left (ie: Hebrew) focusPt is on left,
    // Left-to-Right (ie: English) focusPt is on right.
    this._focusIsRTL = this._isNodeRTL(selection.focusNode);
    let focusIdx = rects.length - 1;
    this._cache.focusPt = (this._focusIsRTL) ?
      new Point(rects[focusIdx].left, rects[focusIdx].bottom) :
      new Point(rects[focusIdx].right, rects[focusIdx].bottom);
  },

  /*
   * Return true if text associated with a node is RTL.
   */
  _isNodeRTL: function(node) {
    // Find containing node that supports .direction attribute (needed
    // when target node is #text for example).
    while (node && !(node instanceof Element)) {
      node = node.parentNode;
    }

    // Worst case, use original direction from _targetElement.
    if (!node) {
      return this._targetIsRTL;
    }

    let nodeWin = node.ownerDocument.defaultView;
    let nodeStyle = nodeWin.getComputedStyle(node, "");
    return (nodeStyle.direction == "rtl");
  },

  _getHandlePositions: function(scroll = this._getScrollPos()) {
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

    if (this._activeType == this.TYPE_CURSOR) {
      // The left and top properties returned are relative to the client area
      // of the window, so we don't need to account for a sub-frame offset.
      let cursor = this._domWinUtils.sendQueryContentEvent(this._domWinUtils.QUERY_CARET_RECT, this._targetElement.selectionEnd, 0, 0, 0,
                                                           this._domWinUtils.QUERY_CONTENT_FLAG_USE_XP_LINE_BREAK);
      // the return value from sendQueryContentEvent is in LayoutDevice pixels and we want CSS pixels, so
      // divide by the pixel ratio
      let x = cursor.left / window.devicePixelRatio;
      let y = (cursor.top + cursor.height) / window.devicePixelRatio;
      return [{ handle: this.HANDLE_TYPE_CARET,
                left: x + scroll.X,
                top: y + scroll.Y,
                rtl: this._targetIsRTL,
                hidden: checkHidden(x, y) }];
    }

    // Determine the handle screen coords
    this._updateCacheForSelection();
    let offset = this._getViewOffset();
    return  [{ handle: this.HANDLE_TYPE_ANCHOR,
               left: this._cache.anchorPt.x + offset.x + scroll.X,
               top: this._cache.anchorPt.y + offset.y + scroll.Y,
               rtl: this._anchorIsRTL,
               hidden: checkHidden(this._cache.anchorPt.x, this._cache.anchorPt.y) },
             { handle: this.HANDLE_TYPE_FOCUS,
               left: this._cache.focusPt.x + offset.x + scroll.X,
               top: this._cache.focusPt.y + offset.y + scroll.Y,
               rtl: this._focusIsRTL,
               hidden: checkHidden(this._cache.focusPt.x, this._cache.focusPt.y) }];
  },

  // Position handles, but avoid superfluous re-positioning (helps during
  // "TextSelection:LayerReflow", "scroll" of top-level document, etc).
  _positionHandlesOnChange: function() {
    // Helper function to compare position messages
    let samePositions = function(aPrev, aCurr) {
      if (aPrev.length != aCurr.length) {
        return false;
      }
      for (let i = 0; i < aPrev.length; i++) {
        if (aPrev[i].left != aCurr[i].left ||
            aPrev[i].top != aCurr[i].top ||
            aPrev[i].rtl != aCurr[i].rtl ||
            aPrev[i].hidden != aCurr[i].hidden) {
          return false;
        }
      }
      return true;
    }

    let positions = this._getHandlePositions();
    if (!samePositions(this._prevHandlePositions, positions)) {
      this._positionHandles(positions);
    }
  },

  // Position handles, allow for re-position, in case user drags handle
  // to invalid position, then releases, we can put it back where it started
  // positions is an array of objects with data about handle positions,
  // which we get from _getHandlePositions.
  _positionHandles: function(positions = this._getHandlePositions()) {
    Messaging.sendRequest({
      type: "TextSelection:PositionHandles",
      positions: positions,
    });
    this._prevHandlePositions = positions;

    // Text state transitions (text <--> no text) will affect selection context and actionbar display
    let currTargetElementHasText = (this._targetElement.textLength > 0);
    if (currTargetElementHasText != this._prevTargetElementHasText) {
      this._prevTargetElementHasText = currTargetElementHasText;
      this._updateMenu();
    }
  },

  subdocumentScrolled: function sh_subdocumentScrolled(aElement) {
    // Ignore all but selectionListener notifications during deferred _closeSelection().
    if (this._deferCloseTimer) {
      return;
    }

    if (this._activeType == this.TYPE_NONE) {
      return;
    }
    let scrollView = aElement.ownerDocument.defaultView;
    let view = this._contentWindow;
    while (true) {
      if (view == scrollView) {
        // The selection is in a view (or sub-view) of the view that scrolled.
        // So we need to reposition the handles.
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
