/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var Cc = Components.classes;
var Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/Services.jsm");

const manifest = do_get_file('bug725015.manifest');
const contract = "@bug725015.test.contract";
const observerTopic = "xpcom-category-entry-added";
const category = "bug725015-test-category";
const entry = "bug725015-category-entry";
const cid = Components.ID("{05070380-6e6e-42ba-aaa5-3289fc55ca5a}");

function observe_category(subj, topic, data) {
  try {
    do_check_eq(topic, observerTopic);
    if (data != category)
      return;
  
    var thisentry = subj.QueryInterface(Ci.nsISupportsCString).data;
    do_check_eq(thisentry, entry);
  
    do_check_eq(Cc["@mozilla.org/categorymanager;1"].getService(Ci.nsICategoryManager).getCategoryEntry(category, entry), contract);
    do_check_true(Cc[contract].equals(cid));
  }
  catch (e) {
    do_throw(e);
  }
  do_test_finished();
}

function run_test() {
  do_test_pending();
  Services.obs.addObserver(observe_category, observerTopic);
  Components.manager.QueryInterface(Ci.nsIComponentRegistrar).autoRegister(manifest);
}
