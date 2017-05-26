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

XPCOMUtils.defineLazyGetter(this, "UUIDMap", () => {
  let {UUIDMap} = Cu.import("resource://gre/modules/Extension.jsm", {});
  return UUIDMap;
});

/*
 * This file should be kept short and simple since it's loaded even
 * when no extensions are running.
 */

function parseScriptOptions(options) {
  return {
    allFrames: options.all_frames,
    matchAboutBlank: options.match_about_blank,
    frameID: options.frame_id,
    runAt: options.run_at,

    matches: new MatchPatternSet(options.matches),
    excludeMatches: new MatchPatternSet(options.exclude_matches || []),
    includeGlobs: options.include_globs && options.include_globs.map(glob => new MatchGlob(glob)),
    excludeGlobs: options.include_globs && options.exclude_globs.map(glob => new MatchGlob(glob)),

    jsPaths: options.js || [],
    cssPaths: options.css || [],
  };
}

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

let cacheInvalidated = 0;
function onCacheInvalidate() {
  cacheInvalidated++;
}
Services.obs.addObserver(onCacheInvalidate, "startupcache-invalidate");

var ExtensionManagement = {
  get cacheInvalidated() {
    return cacheInvalidated;
  },

  get isExtensionProcess() {
    return WebExtensionPolicy.isExtensionProcess;
  },

  // Called when a new extension is loaded.
  startupExtension(uuid, uri, extension) {
    let policy = new WebExtensionPolicy({
      id: extension.id,
      mozExtensionHostname: uuid,
      baseURL: uri.spec,

      permissions: Array.from(extension.permissions),
      allowedOrigins: extension.whiteListedHosts,
      webAccessibleResources: extension.webAccessibleResources || [],

      contentSecurityPolicy: extension.manifest.content_security_policy,

      localizeCallback: extension.localize.bind(extension),

      backgroundScripts: (extension.manifest.background &&
                          extension.manifest.background.scripts),

      contentScripts: (extension.manifest.content_scripts || []).map(parseScriptOptions),
    });

    extension.policy = policy;
    policy.active = true;
  },

  // Called when an extension is unloaded.
  shutdownExtension(extension) {
    extension.policy.active = false;
  },

  registerAPI: APIs.register.bind(APIs),
  unregisterAPI: APIs.unregister.bind(APIs),

  getURLForExtension,

  APIs,
};

XPCOMUtils.defineLazyPreferenceGetter(ExtensionManagement, "useRemoteWebExtensions",
                                      "extensions.webextensions.remote", false);
