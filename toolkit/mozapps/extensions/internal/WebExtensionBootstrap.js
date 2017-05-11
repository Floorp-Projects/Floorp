/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* exported startup, shutdown, install, uninstall */

Components.utils.import("resource://gre/modules/Extension.jsm");

var extension;

const BOOTSTRAP_REASON_TO_STRING_MAP = {
  [this.APP_STARTUP]: "APP_STARTUP",
  [this.APP_SHUTDOWN]: "APP_SHUTDOWN",
  [this.ADDON_ENABLE]: "ADDON_ENABLE",
  [this.ADDON_DISABLE]: "ADDON_DISABLE",
  [this.ADDON_INSTALL]: "ADDON_INSTALL",
  [this.ADDON_UNINSTALL]: "ADDON_UNINSTALL",
  [this.ADDON_UPGRADE]: "ADDON_UPGRADE",
  [this.ADDON_DOWNGRADE]: "ADDON_DOWNGRADE",
};

function install(data, reason) {
}

function startup(data, reason) {
  extension = new Extension(data, BOOTSTRAP_REASON_TO_STRING_MAP[reason]);
  extension.startup();
}

function shutdown(data, reason) {
  extension.shutdown(BOOTSTRAP_REASON_TO_STRING_MAP[reason]);
  extension = null;
}

function uninstall(data, reason) {
}
