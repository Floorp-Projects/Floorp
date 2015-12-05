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
    Services.mm.addMessageListener("Extension:RemoveTopWindowID", this);
  },

  isTopWindowId(windowId) {
    return this.topWindowIds.has(windowId);
  },

  // Convert an outer window ID to a frame ID. An outer window ID of 0
  // is invalid.
  getId(windowId) {
    if (this.isTopWindowId(windowId)) {
      return 0;
    } else if (windowId == 0) {
      return -1;
    } else {
      return windowId;
    }
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

// Manage the collection of ext-*.js scripts that define the extension API.
var Scripts = {
  scripts: new Set(),

  register(script) {
    this.scripts.add(script);
  },

  getScripts() {
    return this.scripts;
  },
};

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
    this.aps.setAddonLoadURICallback(extension.id, this.checkAddonMayLoad.bind(this, extension));
    this.aps.setAddonLocalizeCallback(extension.id, extension.localize.bind(extension));
  },

  // Called when an extension is unloaded.
  shutdownExtension(uuid) {
    let extension = this.uuidMap.get(uuid);
    this.uuidMap.delete(uuid);
    this.aps.setAddonLoadURICallback(extension.id, null);
    this.aps.setAddonLocalizeCallback(extension.id, null);

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
    if (!extension) {
      return false;
    }

    let path = uri.path;
    if (path.length > 0 && path[0] == "/") {
      path = path.substr(1);
    }
    return extension.webAccessibleResources.has(path);
  },

  // Checks whether a given extension can load this URI (typically via
  // an XML HTTP request). The manifest.json |permissions| directive
  // determines this.
  checkAddonMayLoad(extension, uri) {
    return extension.whiteListedHosts.matchesIgnoringPath(uri);
  },

  // Finds the add-on ID associated with a given moz-extension:// URI.
  // This is used to set the addonId on the originAttributes for the
  // nsIPrincipal attached to the URI.
  extensionURIToAddonID(uri) {
    if (this.extensionURILoadableByAnyone(uri)) {
      // We don't want webAccessibleResources to be associated with
      // the add-on. That way they don't get any special privileges.
      return null;
    }

    let uuid = uri.host;
    let extension = this.uuidMap.get(uuid);
    return extension ? extension.id : undefined;
  },
};

this.ExtensionManagement = {
  startupExtension: Service.startupExtension.bind(Service),
  shutdownExtension: Service.shutdownExtension.bind(Service),

  registerScript: Scripts.register.bind(Scripts),
  getScripts: Scripts.getScripts.bind(Scripts),

  getFrameId: Frames.getId.bind(Frames),
  getParentFrameId: Frames.getParentId.bind(Frames),
};
