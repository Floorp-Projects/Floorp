// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

this.EXPORTED_SYMBOLS = ["ActionBarHandler"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  EventDispatcher: "resource://gre/modules/Messaging.jsm",
  GeckoViewUtils: "resource://gre/modules/GeckoViewUtils.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
  Snackbars: "resource://gre/modules/Snackbars.jsm",
  UITelemetry: "resource://gre/modules/UITelemetry.jsm",
});

XPCOMUtils.defineLazyServiceGetter(this, "ParentalControls",
  "@mozilla.org/parental-controls-service;1", "nsIParentalControlsService");

var Strings = {};

XPCOMUtils.defineLazyGetter(Strings, "browser", _ =>
        Services.strings.createBundle("chrome://browser/locale/browser.properties"));

const PHONE_REGEX = /^\+?[0-9\s,-.\(\)*#pw]{1,30}$/; // Are we a phone #?

/**
 * ActionBarHandler Object and methods. Interface between Gecko Text Selection code
 * (AccessibleCaret, etc) and the Mobile ActionBar UI.
 */
var ActionBarHandler = {
  // Error codes returned from _init().
  START_TOUCH_ERROR: {
    NO_CONTENT_WINDOW: "No valid content Window found.",
    NONE: "",
  },

  _nextSelectionID: 1, // Next available.
  _selectionID: null, // Unique Selection ID, assigned each time we _init().

  _boundingClientRect: null, // Current selections boundingClientRect.
  _actionBarActions: null, // Most-recent set of actions sent to ActionBar.

  /**
   * Receive and act on AccessibleCarets caret state-change
   * (mozcaretstatechanged) events.
   */
  handleEvent: function(e) {
    // Close an open ActionBar, if carets no longer logically visible.
    if (this._selectionID && !e.caretVisible) {
      this._uninit(false);
      return;
    }

    if (!this._selectionID && e.collapsed) {
      switch (e.reason) {
        case "longpressonemptycontent":
        case "taponcaret":
          // Show ActionBar when long pressing on an empty input or single
          // tapping on the caret.
          this._init(e.boundingClientRect);
          break;

        case "updateposition":
          // Do not show ActionBar when single tapping on an non-empty editable
          // input.
          break;

        default:
          break;
      }
      return;
    }

    // Open a closed ActionBar if carets actually visible.
    if (!this._selectionID && e.caretVisuallyVisible) {
      this._init(e.boundingClientRect);
      return;
    }

    // Else, update an open ActionBar.
    if (this._selectionID) {
      if (!this._selectionHasChanged()) {
        // Still the same active selection.
        if (e.reason == "presscaret" || e.reason == "scroll") {
          // boundingClientRect doesn't matter since we are hiding the floating
          // toolbar.
          this._updateVisibility();
        } else {
          // Selection changes update boundingClientRect.
          this._boundingClientRect = e.boundingClientRect;
          let forceUpdate = e.reason == "updateposition" || e.reason == "releasecaret";
          this._sendActionBarActions(forceUpdate);
        }
      } else {
        // We've started a new selection entirely.
        this._uninit(false);
        this._init(e.boundingClientRect);
      }
    }
  },

  /**
   * ActionBarHandler notification observers.
   */
  onEvent: function(event, data, callback) {
    switch (event) {
      // User click an ActionBar button.
      case "TextSelection:Action": {
        if (!this._selectionID) {
          break;
        }
        for (let type in this.actions) {
          let action = this.actions[type];
          if (action.id == data.id) {
            action.action(this._targetElement, this._contentWindow);
            break;
          }
        }
        break;
      }

      // Provide selected text to FindInPageBar on request.
      case "TextSelection:Get": {
        try {
          callback.onSuccess(this._getSelectedText());
        } catch (e) {
          callback.onError(e.toString());
        }
        this._uninit();
        break;
      }

      // User closed ActionBar by clicking "checkmark" button.
      case "TextSelection:End": {
        // End the requested selection only.
        if (this._selectionID == data.selectionID) {
          this._uninit();
        }
        break;
      }
    }
  },

  _getDispatcher: function(win) {
    try {
      return GeckoViewUtils.getDispatcherForWindow(win);
    } catch (e) {
      return null;
    }
  },

  /**
   * Called when Gecko AccessibleCaret becomes visible.
   */
  _init: function(boundingClientRect) {
    let [element, win] = this._getSelectionTargets();
    let dispatcher = this._getDispatcher(win);
    if (!win || !dispatcher) {
      return this.START_TOUCH_ERROR.NO_CONTENT_WINDOW;
    }

    // Hold the ActionBar ID provided by Gecko.
    this._selectionID = this._nextSelectionID++;
    [this._targetElement, this._contentWindow] = [element, win];
    this._boundingClientRect = boundingClientRect;

    // Open the ActionBar, send it's actions list.
    dispatcher.sendRequest({
      type: "TextSelection:ActionbarInit",
      selectionID: this._selectionID,
    });
    this._sendActionBarActions(true);

    return this.START_TOUCH_ERROR.NONE;
  },

  /**
   * Called when content is scrolled and handles are hidden.
   */
  _updateVisibility: function() {
    let win = this._contentWindow;
    let dispatcher = this._getDispatcher(win);
    if (!dispatcher) {
      return;
    }
    dispatcher.sendRequest({
      type: "TextSelection:Visibility",
      selectionID: this._selectionID,
    });
  },

  /**
   * Determines the window containing the selection, and its
   * editable element if present.
   */
  _getSelectionTargets: function() {
    let [element, win] = [Services.focus.focusedElement, Services.focus.focusedWindow];
    if (!element) {
      // No focused editable.
      return [null, win];
    }

    // Return focused editable text element and its window.
    if (((element instanceof Ci.nsIDOMHTMLInputElement) && element.mozIsTextField(false)) ||
        (element instanceof Ci.nsIDOMHTMLTextAreaElement) ||
        element.isContentEditable) {
      return [element, win];
    }

    // Focused element can't contain text.
    return [null, win];
  },

  /**
   * The active Selection has changed, if the current focused element / win,
   * pair, or state of the win's designMode changes.
   */
  _selectionHasChanged: function() {
    let [element, win] = this._getSelectionTargets();
    return (this._targetElement !== element ||
            this._contentWindow !== win ||
            this._isInDesignMode(this._contentWindow) !== this._isInDesignMode(win));
  },

  /**
   * Called when Gecko AccessibleCaret becomes hidden,
   * ActionBar is closed by user "close" request, or as a result of object
   * methods such as SELECT_ALL, PASTE, etc.
   */
  _uninit: function(clearSelection = true) {
    // Bail if there's no active selection.
    if (!this._selectionID) {
      return;
    }

    let win = this._contentWindow;
    let dispatcher = this._getDispatcher(win);
    if (dispatcher) {
      // Close the ActionBar.
      dispatcher.sendRequest({
        type: "TextSelection:ActionbarUninit",
      });
    }

    // Clear the selection ID to complete the uninit(), but leave our reference
    // to selectionTargets (_targetElement, _contentWindow) in case we need
    // a final clearSelection().
    this._selectionID = null;
    this._boundingClientRect = null;

    // Clear selection required if triggered by self, or TextSelection icon
    // actions. If called by Gecko CaretStateChangedEvent,
    // visibility state is already correct.
    if (clearSelection) {
      this._clearSelection();
    }
  },

  /**
   * Final UI cleanup when Actionbar is closed by icon click, or where
   * we terminate selection state after before/after actionbar actions
   * (Cut, Copy, Paste, Search, Share, Call).
   */
  _clearSelection: function(element = this._targetElement, win = this._contentWindow) {
    // Commit edit compositions, and clear focus from editables.
    if (element) {
      let editor = this._getEditor(element, win);
      if (editor.composing) {
        editor.forceCompositionEnd();
      }
      element.blur();
    }

    // Remove Selection from non-editables and now-unfocused contentEditables.
    if (!element || element.isContentEditable) {
      this._getSelection().removeAllRanges();
    }
  },

  /**
   * Called to determine current ActionBar actions and send to TextSelection
   * handler. By default we only send if current action state differs from
   * the previous.
   * @param By default we only send an ActionBarStatus update message if
   *        there is a change from the previous state. sendAlways can be
   *        set by init() for example, where we want to always send the
   *        current state.
   */
  _sendActionBarActions: function(sendAlways) {
    let actions = this._getActionBarActions();

    let actionCountUnchanged = this._actionBarActions &&
      actions.length === this._actionBarActions.length;
    let actionsMatch = actionCountUnchanged &&
      this._actionBarActions.every((e, i) => {
        return e.id === actions[i].id;
      });

    let win = this._contentWindow;
    let dispatcher = this._getDispatcher(win);
    if (!dispatcher) {
      return;
    }

    if (sendAlways || !actionsMatch) {
      dispatcher.sendRequest({
        type: "TextSelection:ActionbarStatus",
        selectionID: this._selectionID,
        actions: actions,
        x: this._boundingClientRect.x,
        y: this._boundingClientRect.y,
        width: this._boundingClientRect.width,
        height: this._boundingClientRect.height
      });
    }

    this._actionBarActions = actions;
  },

  /**
   * Determine and return current ActionBar state.
   */
  _getActionBarActions: function(element = this._targetElement, win = this._contentWindow) {
    let actions = [];

    for (let type in this.actions) {
      let action = this.actions[type];
      if (action.selector.matches(element, win)) {
        let a = {
          id: action.id,
          label: this._getActionValue(action, "label", "", element),
          icon: this._getActionValue(action, "icon", "drawable://ic_status_logo", element),
          order: this._getActionValue(action, "order", 0, element),
          floatingOrder: this._getActionValue(action, "floatingOrder", 9, element),
          showAsAction: this._getActionValue(action, "showAsAction", true, element),
        };
        actions.push(a);
      }
    }
    actions.sort((a, b) => b.order - a.order);

    return actions;
  },

  /**
   * Provides a value from an action. If the action defines the value as a function,
   * we return the result of calling the function. Otherwise, we return the value
   * itself. If the value isn't defined for this action, will return a default.
   */
  _getActionValue: function(obj, name, defaultValue, element) {
    if (!(name in obj))
      return defaultValue;

    if (typeof obj[name] == "function")
      return obj[name](element);

    return obj[name];
  },

  /**
   * Actionbar callback methods.
   */
  actions: {

    SELECT_ALL: {
      id: "selectall_action",
      label: () => Strings.browser.GetStringFromName("contextmenu.selectAll"),
      icon: "drawable://ab_select_all",
      order: 5,
      floatingOrder: 5,

      selector: {
        matches: function(element, win) {
          // For editable, check its length. For default contentWindow, assume
          // true, else there'd been nothing to long-press to open ActionBar.
          return (element) ? element.textLength != 0 : true;
        },
      },

      action: function(element, win) {
        // Some Mobile keyboards such as SwiftKeyboard, provide auto-suggest
        // style highlights via composition selections in editables.
        if (element) {
          // If we have an active composition string, commit it, and
          // ensure proper element focus.
          let editor = ActionBarHandler._getEditor(element, win);
          if (editor.composing) {
            element.blur();
            element.focus();
          }
        }

        // Close ActionBarHandler, then selectAll, and display handles.
        ActionBarHandler._getSelectAllController(element, win).selectAll();
        UITelemetry.addEvent("action.1", "actionbar", null, "select_all");
      },
    },

    CUT: {
      id: "cut_action",
      label: () => Strings.browser.GetStringFromName("contextmenu.cut"),
      icon: "drawable://ab_cut",
      order: 4,
      floatingOrder: 1,

      selector: {
        matches: function(element, win) {
          // Can cut from editable, or design-mode document.
          if (!element && !ActionBarHandler._isInDesignMode(win)) {
            return false;
          }
          // Don't allow "cut" from password fields.
          if (element instanceof Ci.nsIDOMHTMLInputElement &&
              !element.mozIsTextField(true)) {
            return false;
          }
          // Don't allow "cut" from disabled/readonly fields.
          if (element && (element.disabled || element.readOnly)) {
            return false;
          }
          // Allow if selected text exists.
          return (ActionBarHandler._getSelectedText().length > 0);
        },
      },

      action: function(element, win) {
        // First copy the selection text to the clipboard.
        let selectedText = ActionBarHandler._getSelectedText();
        let clipboard = Cc["@mozilla.org/widget/clipboardhelper;1"].
          getService(Ci.nsIClipboardHelper);
        clipboard.copyString(selectedText);

        let msg = Strings.browser.GetStringFromName("selectionHelper.textCopied");
        Snackbars.show(msg, Snackbars.LENGTH_LONG);

        // Then cut the selection text.
        ActionBarHandler._getSelection(element, win).deleteFromDocument();

        ActionBarHandler._uninit();
        UITelemetry.addEvent("action.1", "actionbar", null, "cut");
      },
    },

    COPY: {
      id: "copy_action",
      label: () => Strings.browser.GetStringFromName("contextmenu.copy"),
      icon: "drawable://ab_copy",
      order: 3,
      floatingOrder: 2,

      selector: {
        matches: function(element, win) {
          // Don't allow "copy" from password fields.
          if (element instanceof Ci.nsIDOMHTMLInputElement &&
              !element.mozIsTextField(true)) {
            return false;
          }
          // Allow if selected text exists.
          return (ActionBarHandler._getSelectedText().length > 0);
        },
      },

      action: function(element, win) {
        let selectedText = ActionBarHandler._getSelectedText();
        let clipboard = Cc["@mozilla.org/widget/clipboardhelper;1"].
          getService(Ci.nsIClipboardHelper);
        clipboard.copyString(selectedText);

        let msg = Strings.browser.GetStringFromName("selectionHelper.textCopied");
        Snackbars.show(msg, Snackbars.LENGTH_LONG);

        ActionBarHandler._uninit();
        UITelemetry.addEvent("action.1", "actionbar", null, "copy");
      },
    },

    PASTE: {
      id: "paste_action",
      label: () => Strings.browser.GetStringFromName("contextmenu.paste"),
      icon: "drawable://ab_paste",
      order: 2,
      floatingOrder: 3,

      selector: {
        matches: function(element, win) {
          // Can paste to editable, or design-mode document.
          if (!element && !ActionBarHandler._isInDesignMode(win)) {
            return false;
          }
          // Can't paste into disabled/readonly fields.
          if (element && (element.disabled || element.readOnly)) {
            return false;
          }
          // Can't paste if Clipboard empty.
          let flavors = ["text/unicode"];
          return Services.clipboard.hasDataMatchingFlavors(flavors, flavors.length,
            Ci.nsIClipboard.kGlobalClipboard);
        },
      },

      action: function(element, win) {
        // Paste the clipboard, then close the ActionBarHandler and ActionBar.
        ActionBarHandler._getEditor(element, win).
          paste(Ci.nsIClipboard.kGlobalClipboard);
        ActionBarHandler._uninit();
        UITelemetry.addEvent("action.1", "actionbar", null, "paste");
      },
    },

    CALL: {
      id: "call_action",
      label: () => Strings.browser.GetStringFromName("contextmenu.call"),
      icon: "drawable://phone",
      order: 1,
      floatingOrder: 0,

      selector: {
        matches: function(element, win) {
          return (ActionBarHandler._getSelectedPhoneNumber() != null);
        },
      },

      action: function(element, win) {
        let uri = "tel:" + ActionBarHandler._getSelectedPhoneNumber();
        let chrome = GeckoViewUtils.getChromeWindow(win);
        if (chrome.BrowserApp && chrome.BrowserApp.loadURI) {
          chrome.BrowserApp.loadURI(uri);
        } else {
          let bwin = chrome.QueryInterface(Ci.nsIDOMChromeWindow).browserDOMWindow;
          if (bwin) {
            bwin.openURI(Services.io.newURI(uri), win,
                         Ci.nsIBrowserDOMWindow.OPEN_NEWTAB,
                         Ci.nsIBrowserDOMWindow.OPEN_NEW,
                         win.document.nodePrincipal);
          }
        }

        ActionBarHandler._uninit();
        UITelemetry.addEvent("action.1", "actionbar", null, "call");
      },
    },

    SEARCH: {
      id: "search_action",
      label: () => Strings.browser.formatStringFromName("contextmenu.search",
        [Services.search.defaultEngine.name], 1),
      icon: "drawable://ab_search",
      order: 1,
      floatingOrder: 6,

      selector: {
        matches: function(element, win) {
          // Allow if selected text exists.
          return (ActionBarHandler._getSelectedText().length > 0);
        },
      },

      action: function(element, win) {
        let selectedText = ActionBarHandler._getSelectedText();
        ActionBarHandler._uninit();

        // Set current tab as parent of new tab,
        // and set new tab as private if the parent is.
        let searchSubmission = Services.search.defaultEngine.getSubmission(selectedText);
        let chrome = GeckoViewUtils.getChromeWindow(win);
        if (chrome.BrowserApp && chrome.BrowserApp.selectedTab && chrome.BrowserApp.addTab) {
          let parent = chrome.BrowserApp.selectedTab;
          let isPrivate = PrivateBrowsingUtils.isBrowserPrivate(parent.browser);
          chrome.BrowserApp.addTab(searchSubmission.uri.spec,
            { parentId: parent.id,
              selected: true,
              isPrivate: isPrivate,
            }
          );
        } else {
          let bwin = chrome.QueryInterface(Ci.nsIDOMChromeWindow).browserDOMWindow;
          if (bwin) {
            bwin.openURI(searchSubmission.uri, win,
                         Ci.nsIBrowserDOMWindow.OPEN_NEWTAB,
                         Ci.nsIBrowserDOMWindow.OPEN_NEW,
                         win.document.nodePrincipal);
          }
        }

        UITelemetry.addEvent("action.1", "actionbar", null, "search");
      },
    },

    SEARCH_ADD: {
      id: "search_add_action",
      label: () => Strings.browser.GetStringFromName("contextmenu.addSearchEngine3"),
      icon: "drawable://ab_add_search_engine",
      order: 0,
      floatingOrder: 8,

      selector: {
        matches: function(element, win) {
          let chrome = GeckoViewUtils.getChromeWindow(win);
          if (!chrome.SearchEngines) {
            return false;
          }
          if (!(element instanceof Ci.nsIDOMHTMLInputElement)) {
            return false;
          }
          let form = element.form;
          if (!form || element.type == "password") {
            return false;
          }

          let method = form.method.toUpperCase();
          let canAddEngine = (method == "GET") ||
            (method == "POST" && (form.enctype != "text/plain" && form.enctype != "multipart/form-data"));
          if (!canAddEngine) {
            return false;
          }

          // If SearchEngine query finds it, then we don't want action to add displayed.
          if (chrome.SearchEngines.visibleEngineExists(element)) {
            return false;
          }

          return true;
        },
      },

      action: function(element, win) {
        UITelemetry.addEvent("action.1", "actionbar", null, "add_search_engine");

        // Engines are added asynch. If required, update SelectionUI on callback.
        let chrome = GeckoViewUtils.getChromeWindow(win);
        chrome.SearchEngines.addEngine(element, (result) => {
          if (result) {
            ActionBarHandler._sendActionBarActions(true);
          }
        });
      },
    },

    SHARE: {
      id: "share_action",
      label: () => Strings.browser.GetStringFromName("contextmenu.share"),
      icon: "drawable://ic_menu_share",
      order: 0,
      floatingOrder: 4,

      selector: {
        matches: function(element, win) {
          if (!ParentalControls.isAllowed(ParentalControls.SHARE)) {
            return false;
          }
          // Allow if selected text exists.
          return (ActionBarHandler._getSelectedText().length > 0);
        },
      },

      action: function(element, win) {
        let title = win.document.title;
        if (title && title.length > 200) {
          let ellipsis = "\u2026";
          try {
            ellipsis = Services.prefs.getComplexValue(
                "intl.ellipsis", Ci.nsIPrefLocalizedString).data;
          } catch (e) {
          }
          title = title.slice(0, 200) + ellipsis; // Add ellipsis.
        } else if (!title) {
          title = win.location.href;
        }
        EventDispatcher.instance.sendRequest({
          type: "Share:Text",
          text: ActionBarHandler._getSelectedText(),
          title: title,
        });

        ActionBarHandler._uninit();
        UITelemetry.addEvent("action.1", "actionbar", null, "share");
      },
    },
  },

  /**
   * Provides UUID service for generating action ID's.
   */
  get _idService() {
    delete this._idService;
    return this._idService = Cc["@mozilla.org/uuid-generator;1"].
      getService(Ci.nsIUUIDGenerator);
  },

  /**
   * The targetElement holds an editable element containing a
   * selection or a caret.
   */
  get _targetElement() {
    if (this._targetElementRef)
      return this._targetElementRef.get();
    return null;
  },

  set _targetElement(element) {
    this._targetElementRef = Cu.getWeakReference(element);
  },

  /**
   * The contentWindow holds the selection, or the targetElement
   * if it's an editable.
   */
  get _contentWindow() {
    if (this._contentWindowRef)
      return this._contentWindowRef.get();
    return null;
  },

  set _contentWindow(aContentWindow) {
    this._contentWindowRef = Cu.getWeakReference(aContentWindow);
  },

  /**
   * If we have an active selection, is it part of a designMode document?
   */
  _isInDesignMode: function(win) {
    return this._selectionID && (win.document.designMode === "on");
  },

  /**
   * Provides the currently selected text, for either an editable,
   * or for the default contentWindow.
   */
  _getSelectedText: function() {
    // Can be called from FindInPageBar "TextSelection:Get", when there
    // is no active selection.
    if (!this._selectionID) {
      return "";
    }

    let selection = this._getSelection();

    // Textarea can contain LF, etc.
    if (this._targetElement &&
        this._targetElement instanceof Ci.nsIDOMHTMLTextAreaElement) {
      let flags = Ci.nsIDocumentEncoder.OutputPreformatted |
        Ci.nsIDocumentEncoder.OutputRaw;
      return selection.QueryInterface(Ci.nsISelectionPrivate).
        toStringWithFormat("text/plain", flags, 0);
    }

    // Return explicitly selected text.
    return selection.toString();
  },

  /**
   * Provides the nsISelection for either an editor, or from the
   * default window.
   */
  _getSelection: function(element = this._targetElement, win = this._contentWindow) {
    return (element instanceof Ci.nsIDOMNSEditableElement) ?
      this._getEditor(element).selection :
      win.getSelection();
  },

  /**
   * Returns an nsEditor or nsHTMLEditor.
   */
  _getEditor: function(element = this._targetElement, win = this._contentWindow) {
    if (element instanceof Ci.nsIDOMNSEditableElement) {
      return element.QueryInterface(Ci.nsIDOMNSEditableElement).editor;
    }

    return win.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIWebNavigation).
               QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIEditingSession).
               getEditorForWindow(win);
  },

  /**
   * Returns a selection controller.
   */
  _getSelectionController: function(element = this._targetElement, win = this._contentWindow) {
    if (element instanceof Ci.nsIDOMNSEditableElement) {
      return this._getEditor(element, win).selectionController;
    }

    return win.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIWebNavigation).
               QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsISelectionDisplay).
               QueryInterface(Ci.nsISelectionController);
  },

  /**
   * For selectAll(), provides the editor, or the default window selection Controller.
   */
  _getSelectAllController: function(element = this._targetElement, win = this._contentWindow) {
    let editor = this._getEditor(element, win);
    return (editor) ?
      editor : this._getSelectionController(element, win);
  },

  /**
   * Call / Phone Helper methods.
   */
  _getSelectedPhoneNumber: function() {
    let selectedText = this._getSelectedText().trim();
    return this._isPhoneNumber(selectedText) ?
      selectedText : null;
  },

  _isPhoneNumber: function(selectedText) {
    return (PHONE_REGEX.test(selectedText));
  },
};
