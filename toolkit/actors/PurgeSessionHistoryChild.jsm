/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["PurgeSessionHistoryChild"];

class PurgeSessionHistoryChild extends JSWindowActorChild {
  receiveMessage(message) {
    if (message.name != "Browser:PurgeSessionHistory") {
      return;
    }
    let sessionHistory = this.docShell.QueryInterface(Ci.nsIWebNavigation)
      .sessionHistory;
    if (!sessionHistory) {
      return;
    }

    // place the entry at current index at the end of the history list, so it won't get removed
    if (sessionHistory.index < sessionHistory.count - 1) {
      let legacy = sessionHistory.legacySHistory;
      let indexEntry = legacy.getEntryAtIndex(sessionHistory.index);
      indexEntry.QueryInterface(Ci.nsISHEntry);
      legacy.addEntry(indexEntry, true);
    }

    let purge = sessionHistory.count;
    if (this.document.location.href != "about:blank") {
      --purge; // Don't remove the page the user's staring at from shistory
    }

    if (purge > 0) {
      sessionHistory.legacySHistory.PurgeHistory(purge);
    }
  }
}
