/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* exported startup, shutdown, install, uninstall */

Components.utils.import("resource://gre/modules/Extension.jsm");

var extension;

const BOOTSTRAP_REASON_TO_STRING_MAP = {
  1: "APP_STARTUP",
  2: "APP_SHUTDOWN",
  3: "ADDON_ENABLE",
  4: "ADDON_DISABLE",
  5: "ADDON_INSTALL",
  6: "ADDON_UNINSTALL",
  7: "ADDON_UPGRADE",
  8: "ADDON_DOWNGRADE",
}

function install(data, reason)
{
}

function startup(data, reason)
{
  extension = new Extension(data, BOOTSTRAP_REASON_TO_STRING_MAP[reason]);
  extension.startup();
}

function shutdown(data, reason)
{
  extension.shutdown();
}

function uninstall(data, reason)
{
}
