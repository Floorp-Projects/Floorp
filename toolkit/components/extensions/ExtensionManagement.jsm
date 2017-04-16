/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["ExtensionManagement"];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "E10SUtils",
                                  "resource:///modules/E10SUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ExtensionUtils",
                                  "resource://gre/modules/ExtensionUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "console", () => ExtensionUtils.getConsole());

XPCOMUtils.defineLazyGetter(this, "UUIDMap", () => {
  let {UUIDMap} = Cu.import("resource://gre/modules/Extension.jsm", {});
  return UUIDMap;
});

const {appinfo} = Services;
const isParentProcess = appinfo.processType === appinfo.PROCESS_TYPE_DEFAULT;

var ExtensionManagement;

/*
 * This file should be kept short and simple since it's loaded even
 * when no extensions are running.
 */

var APIs = {
  apis: new Map(),

  register(namespace, schema, script) {
    if (this.apis.has(namespace)) {
      throw new Error(`API namespace already exists: ${namespace}`);
    }

    this.apis.set(namespace, {schema, script});
  },

  unregister(namespace) {
    if (!this.apis.has(namespace)) {
      throw new Error(`API namespace does not exist: ${namespace}`);
    }

    this.apis.delete(namespace);
  },
};

function getURLForExtension(id, path = "") {
  let uuid = UUIDMap.get(id, false);
  if (!uuid) {
    Cu.reportError(`Called getURLForExtension on unmapped extension ${id}`);
    return null;
  }
  return `moz-extension://${uuid}/${path}`;
}

// This object manages various platform-level issues related to
// moz-extension:// URIs. It lives here so that it can be used in both
// the parent and child processes.
//
// moz-extension URIs have the form moz-extension://uuid/path. Each
// extension has its own UUID, unique to the machine it's installed
// on. This is easier and more secure than using the extension ID,
// since it makes it slightly harder to fingerprint for extensions if
// each user uses different URIs for the extension.
var Service = {
  initialized: false,

  // Map[uuid -> extension].
  // extension can be an Extension (parent process) or BrowserExtensionContent (child process).
  uuidMap: new Map(),

  init() {
    let aps = Cc["@mozilla.org/addons/policy-service;1"].getService(Ci.nsIAddonPolicyService);
    aps = aps.wrappedJSObject;
    this.aps = aps;
    aps.setExtensionURILoadCallback(this.extensionURILoadableByAnyone.bind(this));
    aps.setExtensionURIToAddonIdCallback(this.extensionURIToAddonID.bind(this));
  },

  // Called when a new extension is loaded.
  startupExtension(uuid, uri, extension) {
    if (!this.initialized) {
      this.initialized = true;
      this.init();
    }

    // Create the moz-extension://uuid mapping.
    let handler = Services.io.getProtocolHandler("moz-extension");
    handler.QueryInterface(Ci.nsISubstitutingProtocolHandler);
    handler.setSubstitution(uuid, uri);

    this.uuidMap.set(uuid, extension);
    this.aps.setAddonHasPermissionCallback(extension.id, extension.hasPermission.bind(extension));
    this.aps.setAddonLoadURICallback(extension.id, this.checkAddonMayLoad.bind(this, extension));
    this.aps.setAddonLocalizeCallback(extension.id, extension.localize.bind(extension));
    this.aps.setAddonCSP(extension.id, extension.manifest.content_security_policy);
    this.aps.setBackgroundPageUrlCallback(uuid, this.generateBackgroundPageUrl.bind(this, extension));
  },

  // Called when an extension is unloaded.
  shutdownExtension(uuid) {
    let extension = this.uuidMap.get(uuid);
    this.uuidMap.delete(uuid);
    this.aps.setAddonHasPermissionCallback(extension.id, null);
    this.aps.setAddonLoadURICallback(extension.id, null);
    this.aps.setAddonLocalizeCallback(extension.id, null);
    this.aps.setAddonCSP(extension.id, null);
    this.aps.setBackgroundPageUrlCallback(uuid, null);

    let handler = Services.io.getProtocolHandler("moz-extension");
    handler.QueryInterface(Ci.nsISubstitutingProtocolHandler);
    handler.setSubstitution(uuid, null);
  },

  // Return true if the given URI can be loaded from arbitrary web
  // content. The manifest.json |web_accessible_resources| directive
  // determines this.
  extensionURILoadableByAnyone(uri) {
    let uuid = uri.host;
    let extension = this.uuidMap.get(uuid);
    if (!extension || !extension.webAccessibleResources) {
      return false;
    }

    let path = uri.QueryInterface(Ci.nsIURL).filePath;
    if (path.length > 0 && path[0] == "/") {
      path = path.substr(1);
    }
    return extension.webAccessibleResources.matches(path);
  },

  // Checks whether a given extension can load this URI (typically via
  // an XML HTTP request). The manifest.json |permissions| directive
  // determines this.
  checkAddonMayLoad(extension, uri, explicit = false) {
    return extension.whiteListedHosts.matchesIgnoringPath(uri, explicit);
  },

  generateBackgroundPageUrl(extension) {
    let background_scripts = (extension.manifest.background &&
                              extension.manifest.background.scripts);

    if (!background_scripts) {
      return;
    }

    let html = "<!DOCTYPE html>\n<html>\n<body>\n";
    for (let script of background_scripts) {
      script = script.replace(/"/g, "&quot;");
      html += `<script src="${script}"></script>\n`;
    }
    html += "</body>\n</html>\n";

    return "data:text/html;charset=utf-8," + encodeURIComponent(html);
  },

  // Finds the add-on ID associated with a given moz-extension:// URI.
  // This is used to set the addonId on the for the nsIPrincipal
  // attached to the URI.
  extensionURIToAddonID(uri) {
    let uuid = uri.host;
    let extension = this.uuidMap.get(uuid);
    return extension ? extension.id : undefined;
  },
};

let cacheInvalidated = 0;
function onCacheInvalidate() {
  cacheInvalidated++;
}
Services.obs.addObserver(onCacheInvalidate, "startupcache-invalidate");

ExtensionManagement = {
  get cacheInvalidated() {
    return cacheInvalidated;
  },

  get isExtensionProcess() {
    if (this.useRemoteWebExtensions) {
      return appinfo.remoteType === E10SUtils.EXTENSION_REMOTE_TYPE;
    }
    return isParentProcess;
  },

  startupExtension: Service.startupExtension.bind(Service),
  shutdownExtension: Service.shutdownExtension.bind(Service),

  registerAPI: APIs.register.bind(APIs),
  unregisterAPI: APIs.unregister.bind(APIs),

  getURLForExtension,

  APIs,
};

XPCOMUtils.defineLazyPreferenceGetter(ExtensionManagement, "useRemoteWebExtensions",
                                      "extensions.webextensions.remote", false);
