/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file wraps XPIDatabase, XPIInstall, and XPIProvider modules in order to
 * allow testing the shutdown+restart situation in AddonTestUtils.sys.mjs.
 */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

// A shared `lazy` object for exports from XPIDatabase, XPIInternal, and
// XPIProvider modules.
//
// Consumers shouldn't store those property values to global variables, except
// for registering XPIProvider.
//
// The list of lazy getters should be in sync with resetXPIExports in
// AddonTestUtils.sys.mjs.
const lazy = {};
XPCOMUtils.defineLazyModuleGetters(lazy, {
  // XPIDatabase.jsm
  AddonInternal: "resource://gre/modules/addons/XPIDatabase.jsm",
  BuiltInThemesHelpers: "resource://gre/modules/addons/XPIDatabase.jsm",
  XPIDatabase: "resource://gre/modules/addons/XPIDatabase.jsm",
  XPIDatabaseReconcile: "resource://gre/modules/addons/XPIDatabase.jsm",

  // XPIInstall.jsm
  UpdateChecker: "resource://gre/modules/addons/XPIInstall.jsm",
  XPIInstall: "resource://gre/modules/addons/XPIInstall.jsm",
  verifyBundleSignedState: "resource://gre/modules/addons/XPIInstall.jsm",

  // XPIProvider.jsm
  XPIProvider: "resource://gre/modules/addons/XPIProvider.jsm",
  XPIInternal: "resource://gre/modules/addons/XPIProvider.jsm",
});

export { lazy as XPIExports };
