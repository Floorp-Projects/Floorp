/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
Components.utils.import("resource:///modules/XPCOMUtils.jsm");

function TestProcessDirective() {}
TestProcessDirective.prototype = {

  /* Boilerplate */
  QueryInterface: XPCOMUtils.generateQI([Components.interfaces.nsISupportsString]),
  contractID: "@mozilla.org/xpcom/tests/ChildProcessDirectiveTest;1",
  classID: Components.ID("{4bd1ba60-45c4-11e4-916c-0800200c9a66}"),

  type: Components.interfaces.nsISupportsString.TYPE_STRING,
  data: "child process",
  toString: function() {
    return this.data;
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([TestProcessDirective]);
