// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// vim: set ts=2 sw=2 sts=2 et tw=80: */
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

this.EXPORTED_SYMBOLS = ["RemoteFinder", "RemoteFinderListener"];

const { interfaces: Ci, classes: Cc, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Geometry.jsm");

XPCOMUtils.defineLazyGetter(this, "GetClipboardSearchString",
  () => Cu.import("resource://gre/modules/Finder.jsm", {}).GetClipboardSearchString
);
XPCOMUtils.defineLazyGetter(this, "Rect",
  () => Cu.import("resource://gre/modules/Geometry.jsm", {}).Rect
);

function RemoteFinder(browser) {
  this._listeners = new Set();
  this._searchString = null;

  this.swapBrowser(browser);
}

RemoteFinder.prototype = {
  destroy() {},

  swapBrowser(aBrowser) {
    if (this._messageManager) {
      this._messageManager.removeMessageListener("Finder:Result", this);
      this._messageManager.removeMessageListener("Finder:MatchesResult", this);
      this._messageManager.removeMessageListener("Finder:CurrentSelectionResult", this);
      this._messageManager.removeMessageListener("Finder:HighlightFinished", this);
    } else {
      aBrowser.messageManager.sendAsyncMessage("Finder:Initialize");
    }

    this._browser = aBrowser;
    this._messageManager = this._browser.messageManager;
    this._messageManager.addMessageListener("Finder:Result", this);
    this._messageManager.addMessageListener("Finder:MatchesResult", this);
    this._messageManager.addMessageListener("Finder:CurrentSelectionResult", this);
    this._messageManager.addMessageListener("Finder:HighlightFinished", this);

    // Ideally listeners would have removed themselves but that doesn't happen
    // right now
    this._listeners.clear();
  },

  addResultListener(aListener) {
    this._listeners.add(aListener);
  },

  removeResultListener(aListener) {
    this._listeners.delete(aListener);
  },

  receiveMessage(aMessage) {
    // Only Finder:Result messages have the searchString field.
    let callback;
    let params;
    switch (aMessage.name) {
      case "Finder:Result":
        this._searchString = aMessage.data.searchString;
        // The rect stops being a Geometry.jsm:Rect over IPC.
        if (aMessage.data.rect) {
          aMessage.data.rect = Rect.fromRect(aMessage.data.rect);
        }
        callback = "onFindResult";
        params = [ aMessage.data ];
        break;
      case "Finder:MatchesResult":
        callback = "onMatchesCountResult";
        params = [ aMessage.data ];
        break;
      case "Finder:CurrentSelectionResult":
        callback = "onCurrentSelection";
        params = [ aMessage.data.selection, aMessage.data.initial ];
        break;
      case "Finder:HighlightFinished":
        callback = "onHighlightFinished";
        params = [ aMessage.data ];
        break;
    }

    for (let l of this._listeners) {
      // Don't let one callback throwing stop us calling the rest
      try {
        l[callback].apply(l, params);
      } catch (e) {
        if (!l[callback]) {
          Cu.reportError(`Missing ${callback} callback on RemoteFinderListener`);
        }
        Cu.reportError(e);
      }
    }
  },

  get searchString() {
    return this._searchString;
  },

  get clipboardSearchString() {
    return GetClipboardSearchString(this._browser.loadContext);
  },

  setSearchStringToSelection() {
    this._browser.messageManager.sendAsyncMessage("Finder:SetSearchStringToSelection", {});
  },

  set caseSensitive(aSensitive) {
    this._browser.messageManager.sendAsyncMessage("Finder:CaseSensitive",
                                                  { caseSensitive: aSensitive });
  },

  set entireWord(aEntireWord) {
    this._browser.messageManager.sendAsyncMessage("Finder:EntireWord",
                                                  { entireWord: aEntireWord });
  },

  getInitialSelection() {
    this._browser.messageManager.sendAsyncMessage("Finder:GetInitialSelection", {});
  },

  fastFind(aSearchString, aLinksOnly, aDrawOutline) {
    this._browser.messageManager.sendAsyncMessage("Finder:FastFind",
                                                  { searchString: aSearchString,
                                                    linksOnly: aLinksOnly,
                                                    drawOutline: aDrawOutline });
  },

  findAgain(aFindBackwards, aLinksOnly, aDrawOutline) {
    this._browser.messageManager.sendAsyncMessage("Finder:FindAgain",
                                                  { findBackwards: aFindBackwards,
                                                    linksOnly: aLinksOnly,
                                                    drawOutline: aDrawOutline });
  },

  highlight(aHighlight, aWord, aLinksOnly) {
    this._browser.messageManager.sendAsyncMessage("Finder:Highlight",
                                                  { highlight: aHighlight,
                                                    linksOnly: aLinksOnly,
                                                    word: aWord });
  },

  enableSelection() {
    this._browser.messageManager.sendAsyncMessage("Finder:EnableSelection");
  },

  removeSelection() {
    this._browser.messageManager.sendAsyncMessage("Finder:RemoveSelection");
  },

  focusContent() {
    // Allow Finder listeners to cancel focusing the content.
    for (let l of this._listeners) {
      try {
        if ("shouldFocusContent" in l &&
            !l.shouldFocusContent())
          return;
      } catch (ex) {
        Cu.reportError(ex);
      }
    }

    this._browser.focus();
    this._browser.messageManager.sendAsyncMessage("Finder:FocusContent");
  },

  onFindbarClose() {
    this._browser.messageManager.sendAsyncMessage("Finder:FindbarClose");
  },

  onFindbarOpen() {
    this._browser.messageManager.sendAsyncMessage("Finder:FindbarOpen");
  },

  onModalHighlightChange(aUseModalHighlight) {
    this._browser.messageManager.sendAsyncMessage("Finder:ModalHighlightChange", {
      useModalHighlight: aUseModalHighlight
    });
  },

  onHighlightAllChange(aHighlightAll) {
    this._browser.messageManager.sendAsyncMessage("Finder:HighlightAllChange", {
      highlightAll: aHighlightAll
    });
  },

  keyPress(aEvent) {
    this._browser.messageManager.sendAsyncMessage("Finder:KeyPress",
                                                  { keyCode: aEvent.keyCode,
                                                    ctrlKey: aEvent.ctrlKey,
                                                    metaKey: aEvent.metaKey,
                                                    altKey: aEvent.altKey,
                                                    shiftKey: aEvent.shiftKey });
  },

  requestMatchesCount(aSearchString, aLinksOnly) {
    this._browser.messageManager.sendAsyncMessage("Finder:MatchesCount",
                                                  { searchString: aSearchString,
                                                    linksOnly: aLinksOnly });
  }
};

function RemoteFinderListener(global) {
  let {Finder} = Cu.import("resource://gre/modules/Finder.jsm", {});
  this._finder = new Finder(global.docShell);
  this._finder.addResultListener(this);
  this._global = global;

  for (let msg of this.MESSAGES) {
    global.addMessageListener(msg, this);
  }
}

RemoteFinderListener.prototype = {
  MESSAGES: [
    "Finder:CaseSensitive",
    "Finder:EntireWord",
    "Finder:FastFind",
    "Finder:FindAgain",
    "Finder:SetSearchStringToSelection",
    "Finder:GetInitialSelection",
    "Finder:Highlight",
    "Finder:HighlightAllChange",
    "Finder:EnableSelection",
    "Finder:RemoveSelection",
    "Finder:FocusContent",
    "Finder:FindbarClose",
    "Finder:FindbarOpen",
    "Finder:KeyPress",
    "Finder:MatchesCount",
    "Finder:ModalHighlightChange"
  ],

  onFindResult(aData) {
    this._global.sendAsyncMessage("Finder:Result", aData);
  },

  // When the child receives messages with results of requestMatchesCount,
  // it passes them forward to the parent.
  onMatchesCountResult(aData) {
    this._global.sendAsyncMessage("Finder:MatchesResult", aData);
  },

  onHighlightFinished(aData) {
    this._global.sendAsyncMessage("Finder:HighlightFinished", aData);
  },

  receiveMessage(aMessage) {
    let data = aMessage.data;

    switch (aMessage.name) {
      case "Finder:CaseSensitive":
        this._finder.caseSensitive = data.caseSensitive;
        break;

      case "Finder:EntireWord":
        this._finder.entireWord = data.entireWord;
        break;

      case "Finder:SetSearchStringToSelection": {
        let selection = this._finder.setSearchStringToSelection();
        this._global.sendAsyncMessage("Finder:CurrentSelectionResult",
                                      { selection,
                                        initial: false });
        break;
      }

      case "Finder:GetInitialSelection": {
        let selection = this._finder.getActiveSelectionText();
        this._global.sendAsyncMessage("Finder:CurrentSelectionResult",
                                      { selection,
                                        initial: true });
        break;
      }

      case "Finder:FastFind":
        this._finder.fastFind(data.searchString, data.linksOnly, data.drawOutline);
        break;

      case "Finder:FindAgain":
        this._finder.findAgain(data.findBackwards, data.linksOnly, data.drawOutline);
        break;

      case "Finder:Highlight":
        this._finder.highlight(data.highlight, data.word, data.linksOnly);
        break;

      case "Finder:HighlightAllChange":
        this._finder.onHighlightAllChange(data.highlightAll);
        break;

      case "Finder:EnableSelection":
        this._finder.enableSelection();
        break;

      case "Finder:RemoveSelection":
        this._finder.removeSelection();
        break;

      case "Finder:FocusContent":
        this._finder.focusContent();
        break;

      case "Finder:FindbarClose":
        this._finder.onFindbarClose();
        break;

      case "Finder:FindbarOpen":
        this._finder.onFindbarOpen();
        break;

      case "Finder:KeyPress":
        this._finder.keyPress(data);
        break;

      case "Finder:MatchesCount":
        this._finder.requestMatchesCount(data.searchString, data.linksOnly);
        break;

      case "Finder:ModalHighlightChange":
        this._finder.onModalHighlightChange(data.useModalHighlight);
        break;
    }
  }
};
