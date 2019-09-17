/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["ExtFindChild"];

ChromeUtils.defineModuleGetter(
  this,
  "FindContent",
  "resource://gre/modules/FindContent.jsm"
);

class ExtFindChild extends JSWindowActorChild {
  receiveMessage(message) {
    if (!this._findContent) {
      this._findContent = new FindContent(this.docShell);
    }

    switch (message.name) {
      case "ext-Finder:CollectResults":
        this.finderInited = true;
        return this._findContent.findRanges(message.data);
      case "ext-Finder:HighlightResults":
        return this._findContent.highlightResults(message.data);
      case "ext-Finder:ClearHighlighting":
        this._findContent.highlighter.highlight(false);
        break;
    }

    return null;
  }
}
