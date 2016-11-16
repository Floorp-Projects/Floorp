/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* exported startup, shutdown, install, uninstall */

Components.utils.import("resource://gre/modules/ExtensionManagement.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

var namespace;
var resource;
var resProto;

function install(data, reason) {
}

function startup(data, reason) {
  namespace = data.id.replace(/@.*/, "");
  resource = `extension-${namespace}-api`;

  resProto = Services.io.getProtocolHandler("resource")
                     .QueryInterface(Components.interfaces.nsIResProtocolHandler);

  resProto.setSubstitution(resource, data.resourceURI);

  ExtensionManagement.registerAPI(
    namespace,
    `resource://${resource}/schema.json`,
    `resource://${resource}/api.js`);
}

function shutdown(data, reason) {
  resProto.setSubstitution(resource, null);

  ExtensionManagement.unregisterAPI(namespace);
}

function uninstall(data, reason) {
}
