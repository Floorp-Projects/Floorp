/* vim:set ts=2 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

function SimpleTest() {
}

SimpleTest.prototype = {
  classID: Components.ID("{4177e257-a0dc-49b9-a774-522a000a49fa}"),

  QueryInterface: function(iid) {
    if (iid.equals(Components.interfaces.nsISimpleTest) ||
        iid.equals(Components.interfaces.nsISupports))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  add: function(a, b) {
    dump("add(" + a + "," + b + ") from JS\n");
    return a + b;
  }
};

const NSGetFactory = XPCOMUtils.generateNSGetFactory([SimpleTest]);
