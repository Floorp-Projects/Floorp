/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

const Cc = Components.classes;
const Ci = Components.interfaces;

// Dummy boomark/history observer
function DummyObserver() {
  Services.obs.notifyObservers(null, "dummy-observer-created");
}

DummyObserver.prototype = {
  // history observer
  onBeginUpdateBatch() {},
  onEndUpdateBatch() {},
  onVisits(aVisits) {
    Services.obs.notifyObservers(null, "dummy-observer-visited");
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
    Services.obs.notifyObservers(null, "dummy-observer-item-added");
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
