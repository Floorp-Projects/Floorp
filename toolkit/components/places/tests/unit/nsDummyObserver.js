/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

const Cc = Components.classes;
const Ci = Components.interfaces;

// Dummy boomark/history observer
function DummyObserver() {
  Services.obs.notifyObservers(null, "dummy-observer-created", null);
}

DummyObserver.prototype = {
  // history observer
  onBeginUpdateBatch() {},
  onEndUpdateBatch() {},
  onVisit(aURI, aVisitID, aTime, aSessionID, aReferringID, aTransitionType) {
    Services.obs.notifyObservers(null, "dummy-observer-visited", null);
  },
  onTitleChanged() {},
  onDeleteURI() {},
  onClearHistory() {},
  onPageChanged() {},
  onDeleteVisits() {},

  // bookmark observer
  // onBeginUpdateBatch: function() {},
  // onEndUpdateBatch: function() {},
  onItemAdded(aItemId, aParentId, aIndex, aItemType, aURI) {
    Services.obs.notifyObservers(null, "dummy-observer-item-added", null);
  },
  onItemChanged() {},
  onItemRemoved() {},
  onItemVisited() {},
  onItemMoved() {},

  classID: Components.ID("62e221d3-68c3-4e1a-8943-a27beb5005fe"),

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsINavBookmarkObserver,
    Ci.nsINavHistoryObserver,
  ])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([DummyObserver]);
