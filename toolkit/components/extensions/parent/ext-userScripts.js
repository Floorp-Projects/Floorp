/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { ExtensionUtils } = ChromeUtils.import(
  "resource://gre/modules/ExtensionUtils.jsm"
);

var { ExtensionError } = ExtensionUtils;

/**
 * Represents (in the main browser process) a user script.
 *
 * @param {UserScriptOptions} details
 *        The options object related to the user script
 *        (which has the properties described in the user_scripts.json
 *        JSON API schema file).
 */
class UserScriptParent {
  constructor(details) {
    this.scriptId = details.scriptId;
    this.options = this._convertOptions(details);
  }

  destroy() {
    if (this.destroyed) {
      throw new Error("Unable to destroy UserScriptParent twice");
    }

    this.destroyed = true;
    this.options = null;
  }

  _convertOptions(details) {
    const options = {
      matches: details.matches,
      excludeMatches: details.excludeMatches,
      includeGlobs: details.includeGlobs,
      excludeGlobs: details.excludeGlobs,
      allFrames: details.allFrames,
      matchAboutBlank: details.matchAboutBlank,
      runAt: details.runAt || "document_idle",
      jsPaths: details.js,
      userScriptOptions: {
        scriptMetadata: details.scriptMetadata,
      },
    };

    return options;
  }

  serialize() {
    return this.options;
  }
}

this.userScripts = class extends ExtensionAPI {
  constructor(...args) {
    super(...args);

    // Map<scriptId -> UserScriptParent>
    this.userScriptsMap = new Map();
  }

  getAPI(context) {
    const { extension } = context;

    // Set of the scriptIds registered from this context.
    const registeredScriptIds = new Set();

    const unregisterContentScripts = scriptIds => {
      if (scriptIds.length === 0) {
        return Promise.resolve();
      }

      for (let scriptId of scriptIds) {
        registeredScriptIds.delete(scriptId);
        extension.registeredContentScripts.delete(scriptId);
        this.userScriptsMap.delete(scriptId);
      }
      extension.updateContentScripts();

      return context.extension.broadcast("Extension:UnregisterContentScripts", {
        id: context.extension.id,
        scriptIds,
      });
    };

    // Unregister all the scriptId related to a context when it is closed,
    // and revoke all the created blob urls once the context is destroyed.
    context.callOnClose({
      close() {
        unregisterContentScripts(Array.from(registeredScriptIds));
      },
    });

    return {
      userScripts: {
        register: async details => {
          for (let origin of details.matches) {
            if (!extension.allowedOrigins.subsumes(new MatchPattern(origin))) {
              throw new ExtensionError(
                `Permission denied to register a user script for ${origin}`
              );
            }
          }

          const userScript = new UserScriptParent(details);
          const { scriptId } = userScript;

          this.userScriptsMap.set(scriptId, userScript);

          const scriptOptions = userScript.serialize();

          await extension.broadcast("Extension:RegisterContentScript", {
            id: extension.id,
            options: scriptOptions,
            scriptId,
          });

          extension.registeredContentScripts.set(scriptId, scriptOptions);
          extension.updateContentScripts();

          return scriptId;
        },

        // This method is not available to the extension code, the extension code
        // doesn't have access to the internally used scriptId, on the contrary
        // the extension code will call script.unregister on the script API object
        // that is resolved from the register API method returned promise.
        unregister: async scriptId => {
          const userScript = this.userScriptsMap.get(scriptId);
          if (!userScript) {
            throw new Error(`No such user script ID: ${scriptId}`);
          }

          userScript.destroy();

          await unregisterContentScripts([scriptId]);
        },
      },
    };
  }
};
