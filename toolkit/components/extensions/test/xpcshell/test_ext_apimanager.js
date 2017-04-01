/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Cu.import("resource://gre/modules/ExtensionCommon.jsm");

const {
  SchemaAPIManager,
} = ExtensionCommon;

this.unknownvar = "Some module-global var";

var gUniqueId = 0;

// SchemaAPIManager's loadScript uses loadSubScript to load a script. This
// requires a local (resource://) URL. So create such a temporary URL for
// testing.
function toLocalURI(code) {
  let dataUrl = `data:charset=utf-8,${encodeURIComponent(code)}`;
  let uniqueResPart = `need-a-local-uri-for-subscript-loading-${++gUniqueId}`;
  Services.io.getProtocolHandler("resource")
    .QueryInterface(Ci.nsIResProtocolHandler)
    .setSubstitution(uniqueResPart, Services.io.newURI(dataUrl));
  return `resource://${uniqueResPart}`;
}

add_task(function* test_global_isolation() {
  let manA = new SchemaAPIManager("procA");
  let manB = new SchemaAPIManager("procB");

  // The "global" variable should be persistent and shared.
  manA.loadScript(toLocalURI`global.globalVar = 1;`);
  do_check_eq(manA.global.globalVar, 1);
  do_check_eq(manA.global.unknownvar, undefined);
  manA.loadScript(toLocalURI`global.canSeeGlobal = global.globalVar;`);
  do_check_eq(manA.global.canSeeGlobal, 1);

  // Each loadScript call should have their own scope, and global is shared.
  manA.loadScript(toLocalURI`this.aVar = 1; global.thisScopeVar = aVar`);
  do_check_eq(manA.global.aVar, undefined);
  do_check_eq(manA.global.thisScopeVar, 1);
  manA.loadScript(toLocalURI`global.differentScopeVar = this.aVar;`);
  do_check_eq(manA.global.differentScopeVar, undefined);
  manA.loadScript(toLocalURI`global.cantSeeOtherScope = typeof aVar;`);
  do_check_eq(manA.global.cantSeeOtherScope, "undefined");

  manB.loadScript(toLocalURI`global.tryReadOtherGlobal = global.tryagain;`);
  do_check_eq(manA.global.tryReadOtherGlobal, undefined);

  // Cu.import without second argument exports to the caller's global. Let's
  // verify that it does not leak to the SchemaAPIManager's global.
  do_check_eq(typeof ExtensionUtils, "undefined");  // Sanity check #1.
  manA.loadScript(toLocalURI`global.hasExtUtils = typeof ExtensionUtils;`);
  do_check_eq(manA.global.hasExtUtils, "undefined");  // Sanity check #2

  Cu.import("resource://gre/modules/ExtensionUtils.jsm");
  do_check_eq(typeof ExtensionUtils, "object");  // Sanity check #3.

  manA.loadScript(toLocalURI`global.hasExtUtils = typeof ExtensionUtils;`);
  do_check_eq(manA.global.hasExtUtils, "undefined");
  manB.loadScript(toLocalURI`global.hasExtUtils = typeof ExtensionUtils;`);
  do_check_eq(manB.global.hasExtUtils, "undefined");

  // Confirm that Cu.import does not leak between SchemaAPIManager globals.
  manA.loadScript(toLocalURI`
      Cu.import("resource://gre/modules/ExtensionUtils.jsm");
      global.hasExtUtils = typeof ExtensionUtils;
  `);
  do_check_eq(manA.global.hasExtUtils, "object");  // Sanity check.
  manB.loadScript(toLocalURI`global.hasExtUtils = typeof ExtensionUtils;`);
  do_check_eq(manB.global.hasExtUtils, "undefined");

  // Prototype modifications should be isolated.
  manA.loadScript(toLocalURI`
      Object.prototype.modifiedByA = "Prrft";
      global.fromA = {};
  `);
  manA.loadScript(toLocalURI`
      global.fromAagain = {};
  `);
  manB.loadScript(toLocalURI`
      global.fromB = {};
  `);
  do_check_eq(manA.global.modifiedByA, "Prrft");
  do_check_eq(manA.global.fromA.modifiedByA, "Prrft");
  do_check_eq(manA.global.fromAagain.modifiedByA, "Prrft");
  do_check_eq(manB.global.modifiedByA, undefined);
  do_check_eq(manB.global.fromB.modifiedByA, undefined);
});
