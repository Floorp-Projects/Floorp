/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */
/* eslint no-unused-vars: ["error", {args: "none"}] */
/* global sendAsyncMessage */

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(this, "AutoScrollController",
  "resource://gre/modules/AutoScrollController.jsm");
ChromeUtils.defineModuleGetter(this, "BrowserUtils",
  "resource://gre/modules/BrowserUtils.jsm");
ChromeUtils.defineModuleGetter(this, "SelectContentHelper",
  "resource://gre/modules/SelectContentHelper.jsm");
ChromeUtils.defineModuleGetter(this, "FindContent",
  "resource://gre/modules/FindContent.jsm");
ChromeUtils.defineModuleGetter(this, "PrintingContent",
  "resource://gre/modules/PrintingContent.jsm");
ChromeUtils.defineModuleGetter(this, "RemoteFinder",
  "resource://gre/modules/RemoteFinder.jsm");

XPCOMUtils.defineLazyProxy(this, "SelectionSourceContent",
  "resource://gre/modules/SelectionSourceContent.jsm");

XPCOMUtils.defineLazyProxy(this, "DateTimePickerContent", () => {
  let tmp = {};
  ChromeUtils.import("resource://gre/modules/DateTimePickerContent.jsm", tmp);
  return new tmp.DateTimePickerContent(this);
});

var global = this;


// Lazily load the finder code
addMessageListener("Finder:Initialize", function() {
  let {RemoteFinderListener} = ChromeUtils.import("resource://gre/modules/RemoteFinder.jsm", {});
  new RemoteFinderListener(global);
});

var AutoScrollListener = {
  handleEvent(event) {
    if (event.isTrusted &
        !event.defaultPrevented &&
        event.button == 1) {
      if (!this._controller) {
        this._controller = new AutoScrollController(global);
      }
      this._controller.handleEvent(event);
    }
  }
};
Services.els.addSystemEventListener(global, "mousedown", AutoScrollListener, true);

addEventListener("MozOpenDateTimePicker", DateTimePickerContent);

var PopupBlocking = {
  popupData: null,
  popupDataInternal: null,

  init() {
    addEventListener("DOMPopupBlocked", this, true);
    addEventListener("pageshow", this, true);
    addEventListener("pagehide", this, true);

    addMessageListener("PopupBlocking:UnblockPopup", this);
    addMessageListener("PopupBlocking:GetBlockedPopupList", this);
  },

  receiveMessage(msg) {
    switch (msg.name) {
      case "PopupBlocking:UnblockPopup": {
        let i = msg.data.index;
        if (this.popupData && this.popupData[i]) {
          let data = this.popupData[i];
          let internals = this.popupDataInternal[i];
          let dwi = internals.requestingWindow;

          // If we have a requesting window and the requesting document is
          // still the current document, open the popup.
          if (dwi && dwi.document == internals.requestingDocument) {
            dwi.open(data.popupWindowURIspec, data.popupWindowName, data.popupWindowFeatures);
          }
        }
        break;
      }

      case "PopupBlocking:GetBlockedPopupList": {
        let popupData = [];
        let length = this.popupData ? this.popupData.length : 0;

        // Limit 15 popup URLs to be reported through the UI
        length = Math.min(length, 15);

        for (let i = 0; i < length; i++) {
          let popupWindowURIspec = this.popupData[i].popupWindowURIspec;

          if (popupWindowURIspec == global.content.location.href) {
            popupWindowURIspec = "<self>";
          } else {
            // Limit 500 chars to be sent because the URI will be cropped
            // by the UI anyway, and data: URIs can be significantly larger.
            popupWindowURIspec = popupWindowURIspec.substring(0, 500);
          }

          popupData.push({popupWindowURIspec});
        }

        sendAsyncMessage("PopupBlocking:ReplyGetBlockedPopupList", {popupData});
        break;
      }
    }
  },

  handleEvent(ev) {
    switch (ev.type) {
      case "DOMPopupBlocked":
        return this.onPopupBlocked(ev);
      case "pageshow":
        return this._removeIrrelevantPopupData();
      case "pagehide":
        return this._removeIrrelevantPopupData(ev.target);
    }
    return undefined;
  },

  onPopupBlocked(ev) {
    if (!this.popupData) {
      this.popupData = [];
      this.popupDataInternal = [];
    }

    let obj = {
      popupWindowURIspec: ev.popupWindowURI ? ev.popupWindowURI.spec : "about:blank",
      popupWindowFeatures: ev.popupWindowFeatures,
      popupWindowName: ev.popupWindowName
    };

    let internals = {
      requestingWindow: ev.requestingWindow,
      requestingDocument: ev.requestingWindow.document,
    };

    this.popupData.push(obj);
    this.popupDataInternal.push(internals);
    this.updateBlockedPopups(true);
  },

  _removeIrrelevantPopupData(removedDoc = null) {
    if (this.popupData) {
      let i = 0;
      let oldLength = this.popupData.length;
      while (i < this.popupData.length) {
        let {requestingWindow, requestingDocument} = this.popupDataInternal[i];
        // Filter out irrelevant reports.
        if (requestingWindow && requestingWindow.document == requestingDocument &&
            requestingDocument != removedDoc) {
          i++;
        } else {
          this.popupData.splice(i, 1);
          this.popupDataInternal.splice(i, 1);
        }
      }
      if (this.popupData.length == 0) {
        this.popupData = null;
        this.popupDataInternal = null;
      }
      if (!this.popupData || oldLength > this.popupData.length) {
        this.updateBlockedPopups(false);
      }
    }
  },

  updateBlockedPopups(freshPopup) {
    sendAsyncMessage("PopupBlocking:UpdateBlockedPopups",
      {
        count: this.popupData ? this.popupData.length : 0,
        freshPopup
      });
  },
};
PopupBlocking.init();

var Printing = {
  MESSAGES: [
    "Printing:Preview:Enter",
    "Printing:Preview:Exit",
    "Printing:Preview:Navigate",
    "Printing:Preview:ParseDocument",
    "Printing:Print",
  ],

  init() {
    this.MESSAGES.forEach(msgName => addMessageListener(msgName, this));
    addEventListener("PrintingError", this, true);
    addEventListener("printPreviewUpdate", this, true);
  },

  handleEvent(event) {
    return PrintingContent.handleEvent(global, event);
  },

  receiveMessage(message) {
    return PrintingContent.receiveMessage(global, message);
  },
};
Printing.init();

function SwitchDocumentDirection(aWindow) {
 // document.dir can also be "auto", in which case it won't change
  if (aWindow.document.dir == "ltr" || aWindow.document.dir == "") {
    aWindow.document.dir = "rtl";
  } else if (aWindow.document.dir == "rtl") {
    aWindow.document.dir = "ltr";
  }
  for (let run = 0; run < aWindow.frames.length; run++) {
    SwitchDocumentDirection(aWindow.frames[run]);
  }
}

addMessageListener("SwitchDocumentDirection", () => {
  SwitchDocumentDirection(content.window);
});

var FindBar = {
  /* Please keep in sync with toolkit/content/widgets/findbar.xml */
  FIND_NORMAL: 0,
  FIND_TYPEAHEAD: 1,
  FIND_LINKS: 2,

  _findMode: 0,

  /**
   * _findKey and _findModifiers are used to determine whether a keypress
   * is a user attempting to use the find shortcut, after which we'll
   * route keypresses to the parent until we know the findbar has focus
   * there. To do this, we need shortcut data from the parent.
   */
  _findKey: null,
  _findModifiers: null,

  init() {
    addMessageListener("Findbar:UpdateState", this);
    Services.els.addSystemEventListener(global, "keypress", this, false);
    Services.els.addSystemEventListener(global, "mouseup", this, false);
    this._initShortcutData();
  },

  receiveMessage(msg) {
    switch (msg.name) {
      case "Findbar:UpdateState":
        this._findMode = msg.data.findMode;
        this._quickFindTimeout = msg.data.hasQuickFindTimeout;
        if (msg.data.isOpenAndFocused) {
          this._keepPassingUntilToldOtherwise = false;
        }
        break;
      case "Findbar:ShortcutData":
        // Set us up to never need this again for the lifetime of this process,
        // and remove the listener.
        Services.cpmm.initialProcessData.findBarShortcutData = msg.data;
        Services.cpmm.removeMessageListener("Findbar:ShortcutData", this);
        this._initShortcutData(msg.data);
        break;
    }
  },

  handleEvent(event) {
    switch (event.type) {
      case "keypress":
        this._onKeypress(event);
        break;
      case "mouseup":
        this._onMouseup(event);
        break;
    }
  },

  /**
   * Use initial process data for find key/modifier data if we have it.
   * Otherwise, add a listener so we get the data when the parent process has
   * it.
   */
  _initShortcutData(data = Services.cpmm.initialProcessData.findBarShortcutData) {
    if (data) {
      this._findKey = data.key;
      this._findModifiers = data.modifiers;
    } else {
      Services.cpmm.addMessageListener("Findbar:ShortcutData", this);
    }
  },

  /**
   * Check whether this key event will start the findbar in the parent,
   * in which case we should pass any further key events to the parent to avoid
   * them being lost.
   * @param aEvent the key event to check.
   */
  _eventMatchesFindShortcut(aEvent) {
    let modifiers = this._findModifiers;
    if (!modifiers) {
      return false;
    }
    return aEvent.ctrlKey == modifiers.ctrlKey && aEvent.altKey == modifiers.altKey &&
      aEvent.shiftKey == modifiers.shiftKey && aEvent.metaKey == modifiers.metaKey &&
      aEvent.key == this._findKey;
  },

  /**
   * Returns whether FAYT can be used for the given event in
   * the current content state.
   */
  _canAndShouldFastFind() {
    let should = false;
    let can = BrowserUtils.canFastFind(content);
    if (can) {
      // XXXgijs: why all these shenanigans? Why not use the event's target?
      let focusedWindow = {};
      let elt = Services.focus.getFocusedElementForWindow(content, true, focusedWindow);
      let win = focusedWindow.value;
      should = BrowserUtils.shouldFastFind(elt, win);
    }
    return { can, should };
  },

  _onKeypress(event) {
    const FAYT_LINKS_KEY = "'";
    const FAYT_TEXT_KEY = "/";
    if (this._eventMatchesFindShortcut(event)) {
      this._keepPassingUntilToldOtherwise = true;
    }
    // Useless keys:
    if (event.ctrlKey || event.altKey || event.metaKey || event.defaultPrevented) {
      return;
    }

    // Check the focused element etc.
    let fastFind = this._canAndShouldFastFind();

    // Can we even use find in this page at all?
    if (!fastFind.can) {
      return;
    }
    if (this._keepPassingUntilToldOtherwise) {
      this._passKeyToParent(event);
      return;
    }
    if (!fastFind.should) {
      return;
    }

    let charCode = event.charCode;
    // If the find bar is open and quick find is on, send the key to the parent.
    if (this._findMode != this.FIND_NORMAL && this._quickFindTimeout) {
      if (!charCode)
        return;
      this._passKeyToParent(event);
    } else {
      let key = charCode ? String.fromCharCode(charCode) : null;
      let manualstartFAYT = (key == FAYT_LINKS_KEY || key == FAYT_TEXT_KEY) && RemoteFinder._manualFAYT;
      let autostartFAYT = !manualstartFAYT && RemoteFinder._findAsYouType && key && key != " ";
      if (manualstartFAYT || autostartFAYT) {
        let mode = (key == FAYT_LINKS_KEY || (autostartFAYT && RemoteFinder._typeAheadLinksOnly)) ?
          this.FIND_LINKS : this.FIND_TYPEAHEAD;
        // Set _findMode immediately (without waiting for child->parent->child roundtrip)
        // to ensure we pass any further keypresses, too.
        this._findMode = mode;
        this._passKeyToParent(event);
      }
    }
  },

  _passKeyToParent(event) {
    event.preventDefault();
    // These are the properties required to dispatch another 'real' event
    // to the findbar in the parent in _dispatchKeypressEvent in findbar.xml .
    // If you make changes here, verify that that method can still do its job.
    const kRequiredProps = [
      "type", "bubbles", "cancelable", "ctrlKey", "altKey", "shiftKey",
      "metaKey", "keyCode", "charCode",
    ];
    let fakeEvent = {};
    for (let prop of kRequiredProps) {
      fakeEvent[prop] = event[prop];
    }
    sendAsyncMessage("Findbar:Keypress", fakeEvent);
  },

  _onMouseup(event) {
    if (this._findMode != this.FIND_NORMAL)
      sendAsyncMessage("Findbar:Mouseup");
  },
};
FindBar.init();

let WebChannelMessageToChromeListener = {
  // Preference containing the list (space separated) of origins that are
  // allowed to send non-string values through a WebChannel, mainly for
  // backwards compatability. See bug 1238128 for more information.
  URL_WHITELIST_PREF: "webchannel.allowObject.urlWhitelist",

  // Cached list of whitelisted principals, we avoid constructing this if the
  // value in `_lastWhitelistValue` hasn't changed since we constructed it last.
  _cachedWhitelist: [],
  _lastWhitelistValue: "",

  init() {
    addEventListener("WebChannelMessageToChrome", e => {
      this._onMessageToChrome(e);
    }, true, true);
  },

  _getWhitelistedPrincipals() {
    let whitelist = Services.prefs.getCharPref(this.URL_WHITELIST_PREF);
    if (whitelist != this._lastWhitelistValue) {
      let urls = whitelist.split(/\s+/);
      this._cachedWhitelist = urls.map(origin =>
        Services.scriptSecurityManager.createCodebasePrincipalFromOrigin(origin));
    }
    return this._cachedWhitelist;
  },

  _onMessageToChrome(e) {
    // If target is window then we want the document principal, otherwise fallback to target itself.
    let principal = e.target.nodePrincipal ? e.target.nodePrincipal : e.target.document.nodePrincipal;

    if (e.detail) {
      if (typeof e.detail != "string") {
        // Check if the principal is one of the ones that's allowed to send
        // non-string values for e.detail.  They're whitelisted by site origin,
        // so we compare on originNoSuffix in order to avoid other origin attributes
        // that are not relevant here, such as containers or private browsing.
        let objectsAllowed = this._getWhitelistedPrincipals().some(whitelisted =>
          principal.originNoSuffix == whitelisted.originNoSuffix);
        if (!objectsAllowed) {
          Cu.reportError("WebChannelMessageToChrome sent with an object from a non-whitelisted principal");
          return;
        }
      }
      sendAsyncMessage("WebChannelMessageToChrome", e.detail, { eventTarget: e.target }, principal);
    } else {
      Cu.reportError("WebChannel message failed. No message detail.");
    }
  }
};

WebChannelMessageToChromeListener.init();

// This should be kept in sync with /browser/base/content.js.
// Add message listener for "WebChannelMessageToContent" messages from chrome scripts.
addMessageListener("WebChannelMessageToContent", function(e) {
  if (e.data) {
    // e.objects.eventTarget will be defined if sending a response to
    // a WebChannelMessageToChrome event. An unsolicited send
    // may not have an eventTarget defined, in this case send to the
    // main content window.
    let eventTarget = e.objects.eventTarget || content;

    // Use nodePrincipal if available, otherwise fallback to document principal.
    let targetPrincipal = eventTarget instanceof Ci.nsIDOMWindow ? eventTarget.document.nodePrincipal : eventTarget.nodePrincipal;

    if (e.principal.subsumes(targetPrincipal)) {
      // If eventTarget is a window, use it as the targetWindow, otherwise
      // find the window that owns the eventTarget.
      let targetWindow = eventTarget instanceof Ci.nsIDOMWindow ? eventTarget : eventTarget.ownerGlobal;

      eventTarget.dispatchEvent(new targetWindow.CustomEvent("WebChannelMessageToContent", {
        detail: Cu.cloneInto({
          id: e.data.id,
          message: e.data.message,
        }, targetWindow),
      }));
    } else {
      Cu.reportError("WebChannel message failed. Principal mismatch.");
    }
  } else {
    Cu.reportError("WebChannel message failed. No message data.");
  }
});

var AudioPlaybackListener = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver]),

  init() {
    Services.obs.addObserver(this, "audio-playback");

    addMessageListener("AudioPlayback", this);
    addEventListener("unload", () => {
      AudioPlaybackListener.uninit();
    });
  },

  uninit() {
    Services.obs.removeObserver(this, "audio-playback");

    removeMessageListener("AudioPlayback", this);
  },

  handleMediaControlMessage(msg) {
    let utils = global.content.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIDOMWindowUtils);
    let suspendTypes = Ci.nsISuspendedTypes;
    switch (msg) {
      case "mute":
        utils.audioMuted = true;
        break;
      case "unmute":
        utils.audioMuted = false;
        break;
      case "lostAudioFocus":
        utils.mediaSuspend = suspendTypes.SUSPENDED_PAUSE_DISPOSABLE;
        break;
      case "lostAudioFocusTransiently":
        utils.mediaSuspend = suspendTypes.SUSPENDED_PAUSE;
        break;
      case "gainAudioFocus":
        utils.mediaSuspend = suspendTypes.NONE_SUSPENDED;
        break;
      case "mediaControlPaused":
        utils.mediaSuspend = suspendTypes.SUSPENDED_PAUSE_DISPOSABLE;
        break;
      case "mediaControlStopped":
        utils.mediaSuspend = suspendTypes.SUSPENDED_STOP_DISPOSABLE;
        break;
      case "resumeMedia":
        // User has clicked the tab audio indicator to play a delayed
        // media. That's clear user intent to play, so gesture activate
        // the content document tree so that the block-autoplay logic
        // allows the media to autoplay.
        content.document.notifyUserGestureActivation();
        utils.mediaSuspend = suspendTypes.NONE_SUSPENDED;
        break;
      default:
        dump("Error : wrong media control msg!\n");
        break;
    }
  },

  observe(subject, topic, data) {
    if (topic === "audio-playback") {
      if (subject && subject.top == global.content) {
        let name = "AudioPlayback:";
        if (data === "activeMediaBlockStart") {
          name += "ActiveMediaBlockStart";
        } else if (data === "activeMediaBlockStop") {
          name += "ActiveMediaBlockStop";
        } else {
          name += (data === "active") ? "Start" : "Stop";
        }
        sendAsyncMessage(name);
      }
    }
  },

  receiveMessage(msg) {
    if (msg.name == "AudioPlayback") {
      this.handleMediaControlMessage(msg.data.type);
    }
  },
};
AudioPlaybackListener.init();

var UnselectedTabHoverObserver = {
  init() {
    addMessageListener("Browser:UnselectedTabHover", this);
    addEventListener("UnselectedTabHover:Enable", this);
    addEventListener("UnselectedTabHover:Disable", this);
  },
  receiveMessage(message) {
    Services.obs.notifyObservers(content.window, "unselected-tab-hover",
                                 message.data.hovered);
  },
  handleEvent(event) {
    sendAsyncMessage("UnselectedTabHover:Toggle",
                     { enable: event.type == "UnselectedTabHover:Enable" });
  }
};
UnselectedTabHoverObserver.init();

addMessageListener("Browser:PurgeSessionHistory", function BrowserPurgeHistory() {
  let sessionHistory = docShell.QueryInterface(Ci.nsIWebNavigation).sessionHistory;
  if (!sessionHistory) {
    return;
  }

  // place the entry at current index at the end of the history list, so it won't get removed
  if (sessionHistory.index < sessionHistory.count - 1) {
    let legacy = sessionHistory.legacySHistory;
    legacy.QueryInterface(Ci.nsISHistoryInternal);
    let indexEntry = legacy.getEntryAtIndex(sessionHistory.index, false);
    indexEntry.QueryInterface(Ci.nsISHEntry);
    legacy.addEntry(indexEntry, true);
  }

  let purge = sessionHistory.count;
  if (global.content.location.href != "about:blank") {
    --purge; // Don't remove the page the user's staring at from shistory
  }

  if (purge > 0) {
    sessionHistory.legacySHistory.PurgeHistory(purge);
  }
});

addMessageListener("ViewSource:GetSelection", SelectionSourceContent);

addEventListener("MozApplicationManifest", function(e) {
  let doc = e.target;
  let info = {
    uri: doc.documentURI,
    characterSet: doc.characterSet,
    manifest: doc.documentElement.getAttribute("manifest"),
    principal: doc.nodePrincipal,
  };
  sendAsyncMessage("MozApplicationManifest", info);
}, false);

let AutoCompletePopup = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIAutoCompletePopup]),

  _connected: false,

  MESSAGES: [
    "FormAutoComplete:HandleEnter",
    "FormAutoComplete:PopupClosed",
    "FormAutoComplete:PopupOpened",
    "FormAutoComplete:RequestFocus",
  ],

  init() {
    addEventListener("unload", this);
    addEventListener("DOMContentLoaded", this);
    // WebExtension browserAction is preloaded and does not receive DCL, wait
    // on pageshow so we can hookup the formfill controller.
    addEventListener("pageshow", this, true);

    for (let messageName of this.MESSAGES) {
      addMessageListener(messageName, this);
    }

    this._input = null;
    this._popupOpen = false;
  },

  destroy() {
    if (this._connected) {
      let controller = Cc["@mozilla.org/satchel/form-fill-controller;1"]
                         .getService(Ci.nsIFormFillController);
      controller.detachFromBrowser(docShell);
      this._connected = false;
    }

    removeEventListener("pageshow", this);
    removeEventListener("unload", this);
    removeEventListener("DOMContentLoaded", this);

    for (let messageName of this.MESSAGES) {
      removeMessageListener(messageName, this);
    }
  },

  connect() {
    if (this._connected) {
      return;
    }
    // We need to wait for a content viewer to be available
    // before we can attach our AutoCompletePopup handler,
    // since nsFormFillController assumes one will exist
    // when we call attachToBrowser.

    // Hook up the form fill autocomplete controller.
    let controller = Cc["@mozilla.org/satchel/form-fill-controller;1"]
                       .getService(Ci.nsIFormFillController);
    controller.attachToBrowser(docShell,
                               this.QueryInterface(Ci.nsIAutoCompletePopup));
    this._connected = true;
  },

  handleEvent(event) {
    switch (event.type) {
      case "pageshow": {
        removeEventListener("pageshow", this);
        this.connect();
        break;
      }

      case "DOMContentLoaded": {
        removeEventListener("DOMContentLoaded", this);
        this.connect();
        break;
      }

      case "unload": {
        this.destroy();
        break;
      }
    }
  },

  receiveMessage(message) {
    switch (message.name) {
      case "FormAutoComplete:HandleEnter": {
        this.selectedIndex = message.data.selectedIndex;

        let controller = Cc["@mozilla.org/autocomplete/controller;1"]
                           .getService(Ci.nsIAutoCompleteController);
        controller.handleEnter(message.data.isPopupSelection);
        break;
      }

      case "FormAutoComplete:PopupClosed": {
        this._popupOpen = false;
        break;
      }

      case "FormAutoComplete:PopupOpened": {
        this._popupOpen = true;
        break;
      }

      case "FormAutoComplete:RequestFocus": {
        if (this._input) {
          this._input.focus();
        }
        break;
      }
    }
  },

  get input() { return this._input; },
  get overrideValue() { return null; },
  set selectedIndex(index) {
    sendAsyncMessage("FormAutoComplete:SetSelectedIndex", { index });
  },
  get selectedIndex() {
    // selectedIndex getter must be synchronous because we need the
    // correct value when the controller is in controller::HandleEnter.
    // We can't easily just let the parent inform us the new value every
    // time it changes because not every action that can change the
    // selectedIndex is trivial to catch (e.g. moving the mouse over the
    // list).
    return sendSyncMessage("FormAutoComplete:GetSelectedIndex", {});
  },
  get popupOpen() {
    return this._popupOpen;
  },

  openAutocompletePopup(input, element) {
    if (this._popupOpen || !input) {
      return;
    }

    let rect = BrowserUtils.getElementBoundingScreenRect(element);
    let window = element.ownerGlobal;
    let dir = window.getComputedStyle(element).direction;
    let results = this.getResultsFromController(input);

    sendAsyncMessage("FormAutoComplete:MaybeOpenPopup",
                     { results, rect, dir });
    this._input = input;
  },

  closePopup() {
    // We set this here instead of just waiting for the
    // PopupClosed message to do it so that we don't end
    // up in a state where the content thinks that a popup
    // is open when it isn't (or soon won't be).
    this._popupOpen = false;
    sendAsyncMessage("FormAutoComplete:ClosePopup", {});
  },

  invalidate() {
    if (this._popupOpen) {
      let results = this.getResultsFromController(this._input);
      sendAsyncMessage("FormAutoComplete:Invalidate", { results });
    }
  },

  selectBy(reverse, page) {
    this._index = sendSyncMessage("FormAutoComplete:SelectBy", {
      reverse,
      page
    });
  },

  getResultsFromController(inputField) {
    let results = [];

    if (!inputField) {
      return results;
    }

    let controller = inputField.controller;
    if (!(controller instanceof Ci.nsIAutoCompleteController)) {
      return results;
    }

    for (let i = 0; i < controller.matchCount; ++i) {
      let result = {};
      result.value = controller.getValueAt(i);
      result.label = controller.getLabelAt(i);
      result.comment = controller.getCommentAt(i);
      result.style = controller.getStyleAt(i);
      result.image = controller.getImageAt(i);
      results.push(result);
    }

    return results;
  },
};

AutoCompletePopup.init();

addEventListener("mozshowdropdown", event => {
  if (!event.isTrusted)
    return;

  if (!SelectContentHelper.open) {
    new SelectContentHelper(event.target, {isOpenedViaTouch: false}, this);
  }
});

addEventListener("mozshowdropdown-sourcetouch", event => {
  if (!event.isTrusted)
    return;

  if (!SelectContentHelper.open) {
    new SelectContentHelper(event.target, {isOpenedViaTouch: true}, this);
  }
});

let ExtFind = {
  init() {
    addMessageListener("ext-Finder:CollectResults", this);
    addMessageListener("ext-Finder:HighlightResults", this);
    addMessageListener("ext-Finder:clearHighlighting", this);
  },

  _findContent: null,

  async receiveMessage(message) {
    if (!this._findContent) {
      this._findContent = new FindContent(docShell);
    }

    let data;
    switch (message.name) {
      case "ext-Finder:CollectResults":
        this.finderInited = true;
        data = await this._findContent.findRanges(message.data);
        sendAsyncMessage("ext-Finder:CollectResultsFinished", data);
        break;
      case "ext-Finder:HighlightResults":
        data = this._findContent.highlightResults(message.data);
        sendAsyncMessage("ext-Finder:HighlightResultsFinished", data);
        break;
      case "ext-Finder:clearHighlighting":
        this._findContent.highlighter.highlight(false);
        break;
    }
  },
};

ExtFind.init();
