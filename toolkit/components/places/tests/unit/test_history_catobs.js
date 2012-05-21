/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

// Get services.
let hs = Cc["@mozilla.org/browser/nav-history-service;1"].
         getService(Ci.nsINavHistoryService);
let os = Cc["@mozilla.org/observer-service;1"].
         getService(Ci.nsIObserverService);

let gDummyCreated = false;
let gDummyVisited = false;

let observer = {
  observe: function(subject, topic, data) {
    if (topic == "dummy-observer-created")
      gDummyCreated = true;
    else if (topic == "dummy-observer-visited")
      gDummyVisited = true;
  },

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIObserver,
    Ci.nsISupportsWeakReference,
  ])
};

function verify() {
  do_check_true(gDummyCreated);
  do_check_true(gDummyVisited);
  do_test_finished();
}

// main
function run_test() {
  do_load_manifest("nsDummyObserver.manifest");

  os.addObserver(observer, "dummy-observer-created", true);
  os.addObserver(observer, "dummy-observer-visited", true);

  // Add a visit
  hs.addVisit(uri("http://typed.mozilla.org"), Date.now(), null,
              hs.TRANSITION_TYPED, false, 0);

  do_test_pending();
  do_timeout(1000, verify);
}
