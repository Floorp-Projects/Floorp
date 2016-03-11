// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// vim: set ts=2 sw=2 sts=2 et tw=80: */
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

this.EXPORTED_SYMBOLS = ["RemoteFinder", "RemoteFinderListener"];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "GetClipboardSearchString",
  () => Cu.import("resource://gre/modules/Finder.jsm", {}).GetClipboardSearchString
);

function RemoteFinder(browser) {
  this._listeners = new Set();
  this._searchString = null;

  this.swapBrowser(browser);
}

RemoteFinder.prototype = {
  swapBrowser: function(aBrowser) {
    if (this._messageManager) {
      this._messageManager.removeMessageListener("Finder:Result", this);
      this._messageManager.removeMessageListener("Finder:MatchesResult", this);
      this._messageManager.removeMessageListener("Finder:CurrentSelectionResult",this);
      this._messageManager.removeMessageListener("Finder:HighlightFinished",this);
    }
    else {
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

  addResultListener: function (aListener) {
    this._listeners.add(aListener);
  },

  removeResultListener: function (aListener) {
    this._listeners.delete(aListener);
  },

  receiveMessage: function (aMessage) {
    // Only Finder:Result messages have the searchString field.
    let callback;
    let params;
    switch (aMessage.name) {
      case "Finder:Result":
        this._searchString = aMessage.data.searchString;
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
      }
      catch (e) {
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

  getInitialSelection: function() {
    this._browser.messageManager.sendAsyncMessage("Finder:GetInitialSelection", {});
  },

  fastFind: function (aSearchString, aLinksOnly, aDrawOutline) {
    this._browser.messageManager.sendAsyncMessage("Finder:FastFind",
                                                  { searchString: aSearchString,
                                                    linksOnly: aLinksOnly,
                                                    drawOutline: aDrawOutline });
  },

  findAgain: function (aFindBackwards, aLinksOnly, aDrawOutline) {
    this._browser.messageManager.sendAsyncMessage("Finder:FindAgain",
                                                  { findBackwards: aFindBackwards,
                                                    linksOnly: aLinksOnly,
                                                    drawOutline: aDrawOutline });
  },

  highlight: function (aHighlight, aWord) {
    this._browser.messageManager.sendAsyncMessage("Finder:Highlight",
                                                  { highlight: aHighlight,
                                                    word: aWord });
  },

  enableSelection: function () {
    this._browser.messageManager.sendAsyncMessage("Finder:EnableSelection");
  },

  removeSelection: function () {
    this._browser.messageManager.sendAsyncMessage("Finder:RemoveSelection");
  },

  focusContent: function () {
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

  keyPress: function (aEvent) {
    this._browser.messageManager.sendAsyncMessage("Finder:KeyPress",
                                                  { keyCode: aEvent.keyCode,
                                                    ctrlKey: aEvent.ctrlKey,
                                                    metaKey: aEvent.metaKey,
                                                    altKey: aEvent.altKey,
                                                    shiftKey: aEvent.shiftKey });
  },

  requestMatchesCount: function (aSearchString, aMatchLimit, aLinksOnly) {
    this._browser.messageManager.sendAsyncMessage("Finder:MatchesCount",
                                                  { searchString: aSearchString,
                                                    matchLimit: aMatchLimit,
                                                    linksOnly: aLinksOnly });
  }
}

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
    "Finder:FastFind",
    "Finder:FindAgain",
    "Finder:SetSearchStringToSelection",
    "Finder:GetInitialSelection",
    "Finder:Highlight",
    "Finder:EnableSelection",
    "Finder:RemoveSelection",
    "Finder:FocusContent",
    "Finder:KeyPress",
    "Finder:MatchesCount"
  ],

  onFindResult: function (aData) {
    this._global.sendAsyncMessage("Finder:Result", aData);
  },

  // When the child receives messages with results of requestMatchesCount,
  // it passes them forward to the parent.
  onMatchesCountResult: function (aData) {
    this._global.sendAsyncMessage("Finder:MatchesResult", aData);
  },

  onHighlightFinished: function(aData) {
    this._global.sendAsyncMessage("Finder:HighlightFinished", aData);
  },

  receiveMessage: function (aMessage) {
    let data = aMessage.data;

    switch (aMessage.name) {
      case "Finder:CaseSensitive":
        this._finder.caseSensitive = data.caseSensitive;
        break;

      case "Finder:SetSearchStringToSelection": {
        let selection = this._finder.setSearchStringToSelection();
        this._global.sendAsyncMessage("Finder:CurrentSelectionResult",
                                      { selection: selection,
                                        initial: false });
        break;
      }

      case "Finder:GetInitialSelection": {
        let selection = this._finder.getActiveSelectionText();
        this._global.sendAsyncMessage("Finder:CurrentSelectionResult",
                                      { selection: selection,
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
        this._finder.highlight(data.highlight, data.word);
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

      case "Finder:KeyPress":
        this._finder.keyPress(data);
        break;

      case "Finder:MatchesCount":
        this._finder.requestMatchesCount(data.searchString, data.matchLimit, data.linksOnly);
        break;
    }
  }
};
