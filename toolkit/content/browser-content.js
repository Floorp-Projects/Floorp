/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */
/* eslint no-unused-vars: ["error", {args: "none"}] */
/* global sendAsyncMessage */

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/ActorManagerChild.jsm");

ActorManagerChild.attach(this);

ChromeUtils.defineModuleGetter(this, "AutoCompletePopup",
  "resource://gre/modules/AutoCompletePopupContent.jsm");
ChromeUtils.defineModuleGetter(this, "AutoScrollController",
  "resource://gre/modules/AutoScrollController.jsm");
ChromeUtils.defineModuleGetter(this, "FindContent",
  "resource://gre/modules/FindContent.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "formFill",
                                   "@mozilla.org/satchel/form-fill-controller;1",
                                   "nsIFormFillController");

var global = this;

XPCOMUtils.defineLazyProxy(this, "ShieldFrameListener", () => {
  let tmp = {};
  ChromeUtils.import("resource://normandy-content/ShieldFrameListener.jsm", tmp);
  return new tmp.ShieldFrameListener(global);
});

XPCOMUtils.defineLazyProxy(this, "UITourListener", () => {
  let tmp = {};
  ChromeUtils.import("resource:///modules/ContentUITour.jsm", tmp);
  return new tmp.UITourListener(global);
});

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

addEventListener("ShieldPageEvent", ShieldFrameListener, false, true);

addEventListener("mozUITour", UITourListener, false, true);
