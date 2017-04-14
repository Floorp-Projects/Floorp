/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  var ps = Cc["@mozilla.org/preferences-service;1"].
            getService(Ci.nsIPrefService);

  var pb = Cc["@mozilla.org/preferences-service;1"].
            getService(Ci.nsIPrefBranch);

  var observer = {
    QueryInterface: function QueryInterface(aIID) {
      if (aIID.equals(Ci.nsIObserver) ||
          aIID.equals(Ci.nsISupports))
         return this;
      throw Components.results.NS_NOINTERFACE;
    },

    observe: function observe(aSubject, aTopic, aState) {
      // Don't do anything.
    }
  }

  /* Set the same pref twice.  This shouldn't leak. */
  pb.addObserver("UserPref.nonexistent.setIntPref", observer);
  pb.addObserver("UserPref.nonexistent.setIntPref", observer);
}
