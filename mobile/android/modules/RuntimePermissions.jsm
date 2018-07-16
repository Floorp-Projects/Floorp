/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["RuntimePermissions"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(this, "EventDispatcher",
                               "resource://gre/modules/Messaging.jsm");

// See: http://developer.android.com/reference/android/Manifest.permission.html
const ACCESS_COARSE_LOCATION = "android.permission.ACCESS_COARSE_LOCATION";
const ACCESS_FINE_LOCATION = "android.permission.ACCESS_FINE_LOCATION";
const CAMERA = "android.permission.CAMERA";
const RECORD_AUDIO = "android.permission.RECORD_AUDIO";
const WRITE_EXTERNAL_STORAGE = "android.permission.WRITE_EXTERNAL_STORAGE";
const READ_EXTERNAL_STORAGE = "android.permission.READ_EXTERNAL_STORAGE";

var RuntimePermissions = {
  ACCESS_COARSE_LOCATION: ACCESS_COARSE_LOCATION,
  ACCESS_FINE_LOCATION: ACCESS_FINE_LOCATION,
  CAMERA: CAMERA,
  RECORD_AUDIO: RECORD_AUDIO,
  WRITE_EXTERNAL_STORAGE: WRITE_EXTERNAL_STORAGE,
  READ_EXTERNAL_STORAGE: READ_EXTERNAL_STORAGE,

  /**
   * Check whether the permissions have been granted or not. If needed prompt the user to accept the permissions.
   *
   * @returns A promise resolving to true if all the permissions have been granted or false if any of the
   *          permissions have been denied.
   */
  waitForPermissions: function(permission) {
    let permissions = [].concat(permission);

    let msg = {
      type: "RuntimePermissions:Check",
      permissions: permissions,
      shouldPrompt: true
    };

    return EventDispatcher.instance.sendRequestForResult(msg);
  },

  /**
    * Check whether the specified permissions have already been granted or not.
    *
    * @returns A promise resolving to true if all the permissions are already granted or false if any of the
    *          permissions are not granted.
    */
  checkPermissions: function(permission) {
    let permissions = [].concat(permission);

    let msg = {
      type: "RuntimePermissions:Check",
      permissions: permissions,
      shouldPrompt: false
    };

    return EventDispatcher.instance.sendRequestForResult(msg);
  }
};
