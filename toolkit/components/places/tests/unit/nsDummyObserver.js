/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Places unit test code.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mark Finkle <mfinkle@mozilla.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

const Cc = Components.classes;
const Ci = Components.interfaces;

// Dummy boomark/history observer
function DummyObserver() {
  let os = Cc["@mozilla.org/observer-service;1"].
           getService(Ci.nsIObserverService);
  os.notifyObservers(null, "dummy-observer-created", null);
}

DummyObserver.prototype = {
  // history observer
  onBeginUpdateBatch: function() {},
  onEndUpdateBatch: function() {},
  onVisit: function(aURI, aVisitID, aTime, aSessionID, aReferringID, aTransitionType) {
    let os = Cc["@mozilla.org/observer-service;1"].
             getService(Ci.nsIObserverService);
    os.notifyObservers(null, "dummy-observer-visited", null);
  },
  onTitleChanged: function(aURI, aPageTitle) {},
  onDeleteURI: function(aURI) {},
  onClearHistory: function() {},
  onPageChanged: function(aURI, aWhat, aValue) {},
  onPageExpired: function(aURI, aVisitTime, aWholeEntry) {},

  // bookmark observer
  //onBeginUpdateBatch: function() {},
  //onEndUpdateBatch: function() {},
  onItemAdded: function(aItemId, aParentId, aIndex) {
    let os = Cc["@mozilla.org/observer-service;1"].
             getService(Ci.nsIObserverService);
    os.notifyObservers(null, "dummy-observer-item-added", null);
  },
  onItemChanged: function (aItemId, aProperty, aIsAnnotationProperty, aValue) {},
  onBeforeItemRemoved: function() {},
  onItemRemoved: function() {},
  onItemVisited: function() {},
  onItemMoved: function() {},

  classDescription: "Dummy observer used to test category observers",
  classID: Components.ID("62e221d3-68c3-4e1a-8943-a27beb5005fe"),
  contractID: "@mozilla.org/places/test/dummy-observer;1",

  // Registering in these categories makes us get initialized when either of
  // those listeners would be notified.
  _xpcom_categories: [
    { category: "bookmark-observers" },
    { category: "history-observers" }
  ],

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsINavBookmarkObserver,
    Ci.nsINavHistoryObserver,
  ])
};

function NSGetModule(compMgr, fileSpec) {
  return XPCOMUtils.generateModule([DummyObserver]);
}
