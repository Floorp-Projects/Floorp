// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "RuntimePermissions", "resource://gre/modules/RuntimePermissions.jsm");

add_task(function* test_snackbar_api() {
  RuntimePermissions.waitForPermissions([
    RuntimePermissions.CAMERA,
    RuntimePermissions.RECORD_AUDIO,
    RuntimePermissions.WRITE_EXTERNAL_STORAGE
  ]);
});

run_next_test();
