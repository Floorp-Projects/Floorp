/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals __URI__ */

// Module used by test_import_module.js

const EXPORTED_SYMBOLS = [ "MODULE_IMPORTED", "MODULE_URI", "SUBMODULE_IMPORTED", "same_scope", "SUBMODULE_IMPORTED_TO_SCOPE" ];

const MODULE_IMPORTED = true;
const MODULE_URI = __URI__;

// Will import SUBMODULE_IMPORTED into scope.
Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.importRelative(this, "import_sub_module.jsm");

// Prepare two scopes that we can import the submodule into.
var scope1 = { __URI__ };
var scope2 = { __URI__ };
// First one is the regular path.
XPCOMUtils.importRelative(scope1, "import_sub_module.jsm");
scope1.test_obj.i++;
// Second one is with a different path (leads to the same file).
XPCOMUtils.importRelative(scope2, "duh/../import_sub_module.jsm");
// test_obj belongs to import_sub_module.jsm and has a mutable field name i, if
// the two modules are actually the same, then they'll share the same value.
// We'll leave it up to test_import_module.js to check that this variable is
// true.
var same_scope = (scope1.test_obj.i == scope2.test_obj.i);

// Check that importRelative can also import into a given scope
var testScope = {};
XPCOMUtils.importRelative(this, "import_sub_module.jsm", testScope);
var SUBMODULE_IMPORTED_TO_SCOPE = testScope.SUBMODULE_IMPORTED;

