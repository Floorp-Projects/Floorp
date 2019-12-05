// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// vim: set ts=2 sw=2 sts=2 et tw=80: */
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

var EXPORTED_SYMBOLS = ["FinderChild"];

ChromeUtils.defineModuleGetter(
  this,
  "Finder",
  "resource://gre/modules/Finder.jsm"
);

class FinderChild extends JSWindowActorChild {
  get finder() {
    if (!this._finder) {
      this._finder = new Finder(this.docShell);
    }
    return this._finder;
  }

  receiveMessage(aMessage) {
    let data = aMessage.data;

    switch (aMessage.name) {
      case "Finder:CaseSensitive":
        this.finder.caseSensitive = data.caseSensitive;
        break;

      case "Finder:EntireWord":
        this.finder.entireWord = data.entireWord;
        break;

      case "Finder:SetSearchStringToSelection": {
        return new Promise(resolve => {
          resolve(this.finder.setSearchStringToSelection());
        });
      }

      case "Finder:GetInitialSelection": {
        return new Promise(resolve => {
          resolve(this.finder.getActiveSelectionText());
        });
      }

      case "Finder:Find":
        return this.finder.find(data);

      case "Finder:Highlight":
        return this.finder
          .highlight(
            data.highlight,
            data.searchString,
            data.linksOnly,
            data.useSubFrames
          )
          .then(result => {
            if (result) {
              result.browsingContextId = this.browsingContext.id;
            }
            return result;
          });

      case "Finder:UpdateHighlightAndMatchCount":
        return this.finder.updateHighlightAndMatchCount(data).then(result => {
          if (result) {
            result.browsingContextId = this.browsingContext.id;
          }
          return result;
        });

      case "Finder:HighlightAllChange":
        this.finder.onHighlightAllChange(data.highlightAll);
        break;

      case "Finder:EnableSelection":
        this.finder.enableSelection();
        break;

      case "Finder:RemoveSelection":
        this.finder.removeSelection(data.keepHighlight);
        break;

      case "Finder:FocusContent":
        this.finder.focusContent();
        break;

      case "Finder:FindbarClose":
        this.finder.onFindbarClose();
        break;

      case "Finder:FindbarOpen":
        this.finder.onFindbarOpen();
        break;

      case "Finder:KeyPress":
        var KeyboardEvent = this.finder._getWindow().KeyboardEvent;
        this.finder.keyPress(new KeyboardEvent("keypress", data));
        break;

      case "Finder:MatchesCount":
        return this.finder
          .requestMatchesCount(
            data.searchString,
            data.linksOnly,
            data.useSubFrames
          )
          .then(result => {
            if (result) {
              result.browsingContextId = this.browsingContext.id;
            }
            return result;
          });

      case "Finder:ModalHighlightChange":
        this.finder.onModalHighlightChange(data.useModalHighlight);
        break;
    }

    return null;
  }
}
