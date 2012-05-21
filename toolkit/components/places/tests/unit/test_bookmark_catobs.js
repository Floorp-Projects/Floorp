/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

// Get services.
let bs = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
         getService(Ci.nsINavBookmarksService);
let os = Cc["@mozilla.org/observer-service;1"].
         getService(Ci.nsIObserverService);

let gDummyCreated = false;
let gDummyAdded = false;

let observer = {
  observe: function(subject, topic, data) {
    if (topic == "dummy-observer-created")
      gDummyCreated = true;
    else if (topic == "dummy-observer-item-added")
      gDummyAdded = true;
  },

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIObserver,
    Ci.nsISupportsWeakReference,
  ])
};

function verify() {
  do_check_true(gDummyCreated);
  do_check_true(gDummyAdded);
  do_test_finished();
}

// main
function run_test() {
  do_load_manifest("nsDummyObserver.manifest");

  os.addObserver(observer, "dummy-observer-created", true);
  os.addObserver(observer, "dummy-observer-item-added", true);

  // Add a bookmark
  bs.insertBookmark(bs.unfiledBookmarksFolder, uri("http://typed.mozilla.org"),
                    bs.DEFAULT_INDEX, "bookmark");

  do_test_pending();
  do_timeout(1000, verify);
}
