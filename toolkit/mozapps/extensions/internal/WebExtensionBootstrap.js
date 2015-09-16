/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Components.utils.import("resource://gre/modules/Extension.jsm");

var extension;

function install(data, reason)
{
}

function startup(data, reason)
{
  extension = new Extension(data);
  extension.startup();
}

function shutdown(data, reason)
{
  extension.shutdown();
}

function uninstall(data, reason)
{
}
