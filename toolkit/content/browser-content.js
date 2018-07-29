/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */
/* eslint no-unused-vars: ["error", {args: "none"}] */
/* global sendAsyncMessage */

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(this, "AutoCompletePopup",
  "resource://gre/modules/AutoCompletePopupContent.jsm");
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

XPCOMUtils.defineLazyServiceGetter(this, "formFill",
                                   "@mozilla.org/satchel/form-fill-controller;1",
                                   "nsIFormFillController");

var global = this;

XPCOMUtils.defineLazyProxy(this, "PopupBlocking", () => {
  let tmp = {};
  ChromeUtils.import("resource://gre/modules/PopupBlocking.jsm", tmp);
  return new tmp.PopupBlocking(global);
});

XPCOMUtils.defineLazyProxy(this, "SelectionSourceContent",
  "resource://gre/modules/SelectionSourceContent.jsm");

XPCOMUtils.defineLazyProxy(this, "WebChannelContent",
  "resource://gre/modules/WebChannelContent.jsm");

XPCOMUtils.defineLazyProxy(this, "DateTimePickerContent", () => {
  let tmp = {};
  ChromeUtils.import("resource://gre/modules/DateTimePickerContent.jsm", tmp);
  return new tmp.DateTimePickerContent(this);
});

XPCOMUtils.defineLazyProxy(this, "FindBarChild", () => {
  let tmp = {};
  ChromeUtils.import("resource://gre/modules/FindBarChild.jsm", tmp);
  return new tmp.FindBarChild(this);
}, {inQuickFind: false, inPassThrough: false});


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

addEventListener("DOMPopupBlocked", PopupBlocking, true);

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
    this.init = null;
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
  /**
   * _findKey and _findModifiers are used to determine whether a keypress
   * is a user attempting to use the find shortcut, after which we'll
   * route keypresses to the parent until we know the findbar has focus
   * there. To do this, we need shortcut data from the parent.
   */
  _findKey: null,

  init() {
    Services.els.addSystemEventListener(global, "keypress",
                                        this.onKeypress.bind(this), false);
    this.init = null;
  },

  /**
   * Check whether this key event will start the findbar in the parent,
   * in which case we should pass any further key events to the parent to avoid
   * them being lost.
   * @param aEvent the key event to check.
   */
  eventMatchesFindShortcut(aEvent) {
    if (!this._findKey) {
      this._findKey = Services.cpmm.sharedData.get("Findbar:Shortcut");
      if (!this._findKey) {
          return false;
      }
    }
    for (let k in this._findKey) {
      if (this._findKey[k] != aEvent[k]) {
        return false;
      }
    }
    return true;
  },

  onKeypress(event) {
    if (!FindBarChild.inPassThrough &&
        this.eventMatchesFindShortcut(event)) {
      return FindBarChild.start(event);
    }

    if (event.ctrlKey || event.altKey || event.metaKey || event.defaultPrevented ||
        !BrowserUtils.canFastFind(content)) {
      return null;
    }

    if (FindBarChild.inPassThrough || FindBarChild.inQuickFind) {
      return FindBarChild.onKeypress(event);
    }

    if (event.charCode && BrowserUtils.shouldFastFind(event.target)) {
      let key = String.fromCharCode(event.charCode);
      if ((key == "/" || key == "'") && RemoteFinder._manualFAYT) {
        return FindBarChild.startQuickFind(event);
      }
      if (key != " " && RemoteFinder._findAsYouType) {
        return FindBarChild.startQuickFind(event, true);
      }
    }
    return null;
  },
};
FindBar.init();

addEventListener("WebChannelMessageToChrome", WebChannelContent,
                 true, true);
addMessageListener("WebChannelMessageToContent", WebChannelContent);

var AudioPlaybackListener = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver]),

  init() {
    Services.obs.addObserver(this, "audio-playback");

    addMessageListener("AudioPlayback", this);
    addEventListener("unload", () => {
      AudioPlaybackListener.uninit();
    });
    this.init = null;
  },

  uninit() {
    Services.obs.removeObserver(this, "audio-playback");

    removeMessageListener("AudioPlayback", this);
  },

  handleMediaControlMessage(msg) {
    let utils = global.content.windowUtils;
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
    this.init = null;
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


var AudibleAutoplayObserver = {
  init() {
    addEventListener("AudibleAutoplayMediaOccurred", this);
  },
  handleEvent(event) {
    sendAsyncMessage("AudibleAutoplayMediaOccurred");
  }
};
AudibleAutoplayObserver.init();

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

let AutoComplete = {
  _connected: false,

  init() {
    addEventListener("unload", this, {once: true});
    addEventListener("DOMContentLoaded", this, {once: true});
    // WebExtension browserAction is preloaded and does not receive DCL, wait
    // on pageshow so we can hookup the formfill controller.
    addEventListener("pageshow", this, {capture: true, once: true});

    XPCOMUtils.defineLazyProxy(this, "popup", () => new AutoCompletePopup(global),
                               {QueryInterface: null});
    this.init = null;
  },

  handleEvent(event) {
    switch (event.type) {
    case "DOMContentLoaded":
    case "pageshow":
      // We need to wait for a content viewer to be available
      // before we can attach our AutoCompletePopup handler,
      // since nsFormFillController assumes one will exist
      // when we call attachToBrowser.
      if (!this._connected) {
        formFill.attachToBrowser(docShell, this.popup);
        this._connected = true;
      }
      break;

    case "unload":
      if (this._connected) {
        formFill.detachFromBrowser(docShell);
        this._connected = false;
      }
      break;
    }
  },
};

AutoComplete.init();

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
    this.init = null;
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
