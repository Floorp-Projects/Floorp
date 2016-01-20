/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

this.EXPORTED_SYMBOLS = ["RuntimePermissions"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Messaging", "resource://gre/modules/Messaging.jsm");

// See: http://developer.android.com/reference/android/Manifest.permission.html
const CAMERA = "android.permission.CAMERA";
const WRITE_EXTERNAL_STORAGE = "android.permission.WRITE_EXTERNAL_STORAGE";
const RECORD_AUDIO = "android.permission.RECORD_AUDIO";

var RuntimePermissions = {
  CAMERA: CAMERA,
  RECORD_AUDIO: RECORD_AUDIO,
  WRITE_EXTERNAL_STORAGE: WRITE_EXTERNAL_STORAGE,

  /**
   * Check whether the permissions have been granted or not. If needed prompt the user to accept the permissions.
   *
   * @returns A promise resolving to true if all the permissions have been granted or false if any of the
   *          permissions have been denied.
   */
  waitForPermissions: function(permission) {
    let permissions = [].concat(permission);

    let msg = {
      type: 'RuntimePermissions:Prompt',
      permissions: permissions
    };

    return Messaging.sendRequestForResult(msg);
  }
};