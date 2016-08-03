/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["ExtensionManagement"];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/AppConstants.jsm");

/*
 * This file should be kept short and simple since it's loaded even
 * when no extensions are running.
 */

// Keep track of frame IDs for content windows. Mostly we can just use
// the outer window ID as the frame ID. However, the API specifies
// that top-level windows have a frame ID of 0. So we need to keep
// track of which windows are top-level. This code listens to messages
// from ExtensionContent to do that.
var Frames = {
  // Window IDs of top-level content windows.
  topWindowIds: new Set(),

  init() {
    if (Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT) {
      return;
    }

    Services.mm.addMessageListener("Extension:TopWindowID", this);
    Services.mm.addMessageListener("Extension:RemoveTopWindowID", this, true);
  },

  isTopWindowId(windowId) {
    return this.topWindowIds.has(windowId);
  },

  // Convert an outer window ID to a frame ID. An outer window ID of 0
  // is invalid.
  getId(windowId) {
    if (this.isTopWindowId(windowId)) {
      return 0;
    }
    if (windowId == 0) {
      return -1;
    }
    return windowId;
  },

  // Convert an outer window ID for a parent window to a frame
  // ID. Outer window IDs follow the same convention that
  // |window.top.parent === window.top|. The API works differently,
  // giving a frame ID of -1 for the the parent of a top-level
  // window. This function handles the conversion.
  getParentId(parentWindowId, windowId) {
    if (parentWindowId == windowId) {
      // We have a top-level window.
      return -1;
    }

    // Not a top-level window. Just return the ID as normal.
    return this.getId(parentWindowId);
  },

  receiveMessage({name, data}) {
    switch (name) {
      case "Extension:TopWindowID":
        // FIXME: Need to handle the case where the content process
        // crashes. Right now we leak its top window IDs.
        this.topWindowIds.add(data.windowId);
        break;

      case "Extension:RemoveTopWindowID":
        this.topWindowIds.delete(data.windowId);
        break;
    }
  },
};
Frames.init();

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
    // On b2g, in content processes we can't load jar:file:/// content, so we
    // switch to jar:remoteopenfile:/// instead
    // This is mostly exercised by generated extensions in tests. Installed
    // extensions in b2g get an app: uri that also maps to the right jar: uri.
    if (AppConstants.MOZ_B2G &&
        Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT &&
        uri.spec.startsWith("jar:file://")) {
      uri = Services.io.newURI("jar:remoteopen" + uri.spec.substr("jar:".length), null, null);
    }

    let handler = Services.io.getProtocolHandler("moz-extension");
    handler.QueryInterface(Ci.nsISubstitutingProtocolHandler);
    handler.setSubstitution(uuid, uri);

    this.uuidMap.set(uuid, extension);
    this.aps.setAddonLoadURICallback(extension.id, this.checkAddonMayLoad.bind(this, extension));
    this.aps.setAddonLocalizeCallback(extension.id, extension.localize.bind(extension));
    this.aps.setAddonCSP(extension.id, extension.manifest.content_security_policy);
    this.aps.setBackgroundPageUrlCallback(uuid, this.generateBackgroundPageUrl.bind(this, extension));
  },

  // Called when an extension is unloaded.
  shutdownExtension(uuid) {
    let extension = this.uuidMap.get(uuid);
    this.uuidMap.delete(uuid);
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
  checkAddonMayLoad(extension, uri) {
    return extension.whiteListedHosts.matchesIgnoringPath(uri);
  },

  generateBackgroundPageUrl(extension) {
    let background_scripts = extension.manifest.background &&
      extension.manifest.background.scripts;
    if (!background_scripts) {
      return;
    }
    let html = "<!DOCTYPE html>\n<body>\n";
    for (let script of background_scripts) {
      script = script.replace(/"/g, "&quot;");
      html += `<script src="${script}"></script>\n`;
    }
    html += "</body>\n</html>\n";
    return "data:text/html;charset=utf-8," + encodeURIComponent(html);
  },

  // Finds the add-on ID associated with a given moz-extension:// URI.
  // This is used to set the addonId on the originAttributes for the
  // nsIPrincipal attached to the URI.
  extensionURIToAddonID(uri) {
    let uuid = uri.host;
    let extension = this.uuidMap.get(uuid);
    return extension ? extension.id : undefined;
  },
};

// API Levels Helpers

// Find the add-on associated with this document via the
// principal's originAttributes. This value is computed by
// extensionURIToAddonID, which ensures that we don't inject our
// API into webAccessibleResources or remote web pages.
function getAddonIdForWindow(window) {
  let principal = window.document.nodePrincipal;
  return principal.originAttributes.addonId;
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

  // Extension pages running in the content process always defaults to
  // "content script API level privileges".
  if (Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT) {
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

this.ExtensionManagement = {
  startupExtension: Service.startupExtension.bind(Service),
  shutdownExtension: Service.shutdownExtension.bind(Service),

  getFrameId: Frames.getId.bind(Frames),
  getParentFrameId: Frames.getParentId.bind(Frames),

  // exported API Level Helpers
  getAddonIdForWindow,
  getAPILevelForWindow,
  API_LEVELS,
};
