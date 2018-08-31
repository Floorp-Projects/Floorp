// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// vim: set ts=2 sw=2 sts=2 et tw=80: */
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

var EXPORTED_SYMBOLS = ["FinderChild"];

ChromeUtils.import("resource://gre/modules/ActorChild.jsm");

ChromeUtils.defineModuleGetter(this, "Finder",
                               "resource://gre/modules/Finder.jsm");

const MESSAGES = [
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
  "Finder:ModalHighlightChange",
];

class FinderChild extends ActorChild {
  constructor(mm) {
    super(mm);

    this._finder = new Finder(mm.docShell);
    this._finder.addResultListener(this);

    for (let msg of MESSAGES) {
      mm.addMessageListener(msg, this);
    }
  }

  onFindResult(aData) {
    this.mm.sendAsyncMessage("Finder:Result", aData);
  }

  // When the child receives messages with results of requestMatchesCount,
  // it passes them forward to the parent.
  onMatchesCountResult(aData) {
    this.mm.sendAsyncMessage("Finder:MatchesResult", aData);
  }

  onHighlightFinished(aData) {
    this.mm.sendAsyncMessage("Finder:HighlightFinished", aData);
  }

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
        this.mm.sendAsyncMessage("Finder:CurrentSelectionResult",
                                 { selection,
                                   initial: false });
        break;
      }

      case "Finder:GetInitialSelection": {
        let selection = this._finder.getActiveSelectionText();
        this.mm.sendAsyncMessage("Finder:CurrentSelectionResult",
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
        var KeyboardEvent = this._finder._getWindow().KeyboardEvent;
        this._finder.keyPress(new KeyboardEvent("keypress", data));
        break;

      case "Finder:MatchesCount":
        this._finder.requestMatchesCount(data.searchString, data.linksOnly);
        break;

      case "Finder:ModalHighlightChange":
        this._finder.onModalHighlightChange(data.useModalHighlight);
        break;
    }
  }
}
