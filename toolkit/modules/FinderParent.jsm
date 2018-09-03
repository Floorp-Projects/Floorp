// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// vim: set ts=2 sw=2 sts=2 et tw=80: */
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

var EXPORTED_SYMBOLS = ["FinderParent"];

ChromeUtils.defineModuleGetter(this, "GetClipboardSearchString",
                               "resource://gre/modules/Finder.jsm");

ChromeUtils.defineModuleGetter(this, "Rect",
                               "resource://gre/modules/Geometry.jsm");

function FinderParent(browser) {
  this._listeners = new Set();
  this._searchString = null;

  this.swapBrowser(browser);
}

FinderParent.prototype = {
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
        } else {
          Cu.reportError(e);
        }
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
      useModalHighlight: aUseModalHighlight,
    });
  },

  onHighlightAllChange(aHighlightAll) {
    this._browser.messageManager.sendAsyncMessage("Finder:HighlightAllChange", {
      highlightAll: aHighlightAll,
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
  },
};
