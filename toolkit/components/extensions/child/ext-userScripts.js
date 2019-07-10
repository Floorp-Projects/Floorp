/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var USERSCRIPT_PREFNAME = "extensions.webextensions.userScripts.enabled";
var USERSCRIPT_DISABLED_ERRORMSG = `userScripts APIs are currently experimental and must be enabled with the ${USERSCRIPT_PREFNAME} preference.`;

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "userScriptsEnabled",
  USERSCRIPT_PREFNAME,
  false
);

// eslint-disable-next-line mozilla/reject-importGlobalProperties
Cu.importGlobalProperties(["crypto", "TextEncoder"]);

var { DefaultMap, ExtensionError, getUniqueId } = ExtensionUtils;

/**
 * Represents a registered userScript in the child extension process.
 *
 * @param {ExtensionPageContextChild} context
 *        The extension context which has registered the user script.
 * @param {string} scriptId
 *        An unique id that represents the registered user script
 *        (generated and used internally to identify it across the different processes).
 */
class UserScriptChild {
  constructor({ context, scriptId, onScriptUnregister }) {
    this.context = context;
    this.scriptId = scriptId;
    this.onScriptUnregister = onScriptUnregister;
    this.unregistered = false;
  }

  async unregister() {
    if (this.unregistered) {
      throw new ExtensionError("User script already unregistered");
    }

    this.unregistered = true;

    await this.context.childManager.callParentAsyncFunction(
      "userScripts.unregister",
      [this.scriptId]
    );

    this.context = null;

    this.onScriptUnregister();
  }

  api() {
    const { context } = this;

    // Returns the RegisteredUserScript API object.
    return {
      unregister: () => {
        return context.wrapPromise(this.unregister());
      },
    };
  }
}

this.userScripts = class extends ExtensionAPI {
  getAPI(context) {
    // Cache of the script code already converted into blob urls:
    //   Map<textHash, blobURLs>
    const blobURLsByHash = new Map();

    // Keep track of the userScript that are sharing the same blob urls,
    // so that we can revoke any blob url that is not used by a registered
    // userScripts:
    //   Map<blobURL, Set<scriptId>>
    const userScriptsByBlobURL = new DefaultMap(() => new Set());

    function revokeBlobURLs(scriptId, options) {
      let revokedUrls = new Set();

      for (let url of options.js) {
        if (userScriptsByBlobURL.has(url)) {
          let scriptIds = userScriptsByBlobURL.get(url);
          scriptIds.delete(scriptId);

          if (scriptIds.size === 0) {
            revokedUrls.add(url);
            userScriptsByBlobURL.delete(url);
            context.cloneScope.URL.revokeObjectURL(url);
          }
        }
      }

      // Remove all the removed urls from the map of known computed hashes.
      for (let [hash, url] of blobURLsByHash) {
        if (revokedUrls.has(url)) {
          blobURLsByHash.delete(hash);
        }
      }
    }

    // Convert a script code string into a blob URL (and use a cached one
    // if the script hash is already associated to a blob URL).
    const getBlobURL = async (text, scriptId) => {
      // Compute the hash of the js code string and reuse the blob url if we already have
      // for the same hash.
      const buffer = await crypto.subtle.digest(
        "SHA-1",
        new TextEncoder().encode(text)
      );
      const hash = String.fromCharCode(...new Uint16Array(buffer));

      let blobURL = blobURLsByHash.get(hash);

      if (blobURL) {
        userScriptsByBlobURL.get(blobURL).add(scriptId);
        return blobURL;
      }

      const blob = new context.cloneScope.Blob([text], {
        type: "text/javascript",
      });
      blobURL = context.cloneScope.URL.createObjectURL(blob);

      // Start to track this blob URL.
      userScriptsByBlobURL.get(blobURL).add(scriptId);

      blobURLsByHash.set(hash, blobURL);

      return blobURL;
    };

    function convertToAPIObject(scriptId, options) {
      const registeredScript = new UserScriptChild({
        context,
        scriptId,
        onScriptUnregister: () => revokeBlobURLs(scriptId, options),
      });

      const scriptAPI = Cu.cloneInto(
        registeredScript.api(),
        context.cloneScope,
        { cloneFunctions: true }
      );
      return scriptAPI;
    }

    // Revoke all the created blob urls once the context is destroyed.
    context.callOnClose({
      close() {
        if (!context.cloneScope) {
          return;
        }

        for (let blobURL of blobURLsByHash.values()) {
          context.cloneScope.URL.revokeObjectURL(blobURL);
        }
      },
    });

    return {
      userScripts: {
        register(options) {
          if (!userScriptsEnabled) {
            throw new ExtensionError(USERSCRIPT_DISABLED_ERRORMSG);
          }

          let scriptId = getUniqueId();
          return context.cloneScope.Promise.resolve().then(async () => {
            options.scriptId = scriptId;
            options.js = await Promise.all(
              options.js.map(js => {
                return js.file || getBlobURL(js.code, scriptId);
              })
            );

            await context.childManager.callParentAsyncFunction(
              "userScripts.register",
              [options]
            );

            return convertToAPIObject(scriptId, options);
          });
        },
      },
    };
  }
};
