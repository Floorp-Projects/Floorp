/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from head.js */

"use strict";

/* import-globals-from dynamicfpi_head.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/components/antitracking/test/browser/dynamicfpi_head.js",
  this);

/* import-globals-from storageprincipal_head.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/components/antitracking/test/browser/storageprincipal_head.js",
  this);

this.PartitionedStorageHelper = {
  runTest(name, callback, cleanupFunction, extraPrefs) {
    DynamicFPIHelper.runTest(name, callback, cleanupFunction, extraPrefs);
    StoragePrincipalHelper.runTest(name, callback, cleanupFunction, extraPrefs);
  },
};
