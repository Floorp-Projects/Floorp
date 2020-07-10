/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
const { ComponentUtils } = ChromeUtils.import(
  "resource://gre/modules/ComponentUtils.jsm"
);

function TestProcessDirective() {}
TestProcessDirective.prototype = {
  /* Boilerplate */
  QueryInterface: ChromeUtils.generateQI(["nsIProperty"]),
  contractID: "@mozilla.org/xpcom/tests/ChildProcessDirectiveTest;1",
  classID: Components.ID("{4bd1ba60-45c4-11e4-916c-0800200c9a66}"),

  name: "child process",
  value: "some value",
};

this.NSGetFactory = ComponentUtils.generateNSGetFactory([TestProcessDirective]);
