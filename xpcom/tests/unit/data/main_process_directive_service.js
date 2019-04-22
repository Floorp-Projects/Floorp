/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

function TestProcessDirective() {}
TestProcessDirective.prototype = {

  /* Boilerplate */
  QueryInterface: ChromeUtils.generateQI([Ci.nsIProperty]),
  contractID: "@mozilla.org/xpcom/tests/MainProcessDirectiveTest;1",
  classID: Components.ID("{9b6f4160-45be-11e4-916c-0800200c9a66}"),

  name: "main process",
  value: "some value",
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([TestProcessDirective]);
