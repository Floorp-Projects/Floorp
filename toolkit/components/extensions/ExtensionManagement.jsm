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
if (isParentProcess) {
  Services.ppmm.loadProcessScript("chrome://extensions/content/extension-process-script.js", true);
}

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

// API Levels Helpers

// Find the add-on associated with this document via the
// principal's addonId attribute. This value is computed by
// extensionURIToAddonID, which ensures that we don't inject our
// API into webAccessibleResources or remote web pages.
function getAddonIdForWindow(window) {
  return Cu.getObjectPrincipal(window).addonId;
}

const API_LEVELS = Object.freeze({
  NO_PRIVILEGES: 0,
  CONTENTSCRIPT_PRIVILEGES: 1,
  FULL_PRIVILEGES: 2,
});

// Finds the API Level ("FULL_PRIVILEGES", "CONTENTSCRIPT_PRIVILEGES", "NO_PRIVILEGES")
// with a given a window object.
function getAPILevelForWindow(window, addonId) {
  const {NO_PRIVILEGES, CONTENTSCRIPT_PRIVILEGES, FULL_PRIVILEGES} = API_LEVELS;

  // Non WebExtension URLs and WebExtension URLs from a different extension
  // has no access to APIs.
  if (!addonId || getAddonIdForWindow(window) != addonId) {
    return NO_PRIVILEGES;
  }

  if (!ExtensionManagement.isExtensionProcess) {
    return CONTENTSCRIPT_PRIVILEGES;
  }

  let docShell = window.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIDocShell);

  // Handling of ExtensionPages running inside sub-frames.
  if (docShell.sameTypeParent) {
    let parentWindow = docShell.sameTypeParent.QueryInterface(Ci.nsIInterfaceRequestor)
                               .getInterface(Ci.nsIDOMWindow);

    // The option page iframe embedded in the about:addons tab should have
    // full API level privileges. (see Bug 1256282 for rationale)
    let parentDocument = parentWindow.document;
    let parentIsSystemPrincipal = Services.scriptSecurityManager
                                          .isSystemPrincipal(parentDocument.nodePrincipal);

    if (parentDocument.location.href == "about:addons" && parentIsSystemPrincipal) {
      return FULL_PRIVILEGES;
    }

    // NOTE: Special handling for devtools panels using a chrome iframe here
    // for the devtools panel, it is needed because a content iframe breaks
    // switching between docked and undocked mode (see bug 1075490).
    let devtoolsBrowser = parentDocument.querySelector(
      "browser[webextension-view-type='devtools_panel']");
    if (devtoolsBrowser && devtoolsBrowser.contentWindow === window &&
        parentIsSystemPrincipal) {
      return FULL_PRIVILEGES;
    }

    // The addon iframes embedded in a addon page from with the same addonId
    // should have the same privileges of the sameTypeParent.
    // (see Bug 1258347 for rationale)
    let parentSameAddonPrivileges = getAPILevelForWindow(parentWindow, addonId);
    if (parentSameAddonPrivileges > NO_PRIVILEGES) {
      return parentSameAddonPrivileges;
    }

    // In all the other cases, WebExtension URLs loaded into sub-frame UI
    // will have "content script API level privileges".
    // (see Bug 1214658 for rationale)
    return CONTENTSCRIPT_PRIVILEGES;
  }

  // WebExtension URLs loaded into top frames UI could have full API level privileges.
  return FULL_PRIVILEGES;
}

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

  // exported API Level Helpers
  getAddonIdForWindow,
  getAPILevelForWindow,
  API_LEVELS,

  APIs,
};

XPCOMUtils.defineLazyPreferenceGetter(ExtensionManagement, "useRemoteWebExtensions",
                                      "extensions.webextensions.remote", false);
