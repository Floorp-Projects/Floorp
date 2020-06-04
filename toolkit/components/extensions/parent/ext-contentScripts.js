/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* exported registerContentScript, unregisterContentScript */
/* global registerContentScript, unregisterContentScript */

var { ExtensionUtils } = ChromeUtils.import(
  "resource://gre/modules/ExtensionUtils.jsm"
);

var { ExtensionError, getUniqueId } = ExtensionUtils;

/**
 * Represents (in the main browser process) a content script registered
 * programmatically (instead of being included in the addon manifest).
 *
 * @param {ProxyContextParent} context
 *        The parent proxy context related to the extension context which
 *        has registered the content script.
 * @param {RegisteredContentScriptOptions} details
 *        The options object related to the registered content script
 *        (which has the properties described in the content_scripts.json
 *        JSON API schema file).
 */
class ContentScriptParent {
  constructor({ context, details }) {
    this.context = context;
    this.scriptId = getUniqueId();
    this.blobURLs = new Set();

    this.options = this._convertOptions(details);

    context.callOnClose(this);
  }

  close() {
    this.destroy();
  }

  destroy() {
    if (this.destroyed) {
      throw new Error("Unable to destroy ContentScriptParent twice");
    }

    this.destroyed = true;

    this.context.forgetOnClose(this);

    for (const blobURL of this.blobURLs) {
      this.context.cloneScope.URL.revokeObjectURL(blobURL);
    }

    this.blobURLs.clear();

    this.context = null;
    this.options = null;
  }

  _convertOptions(details) {
    const { context } = this;

    const options = {
      matches: details.matches,
      excludeMatches: details.excludeMatches,
      includeGlobs: details.includeGlobs,
      excludeGlobs: details.excludeGlobs,
      allFrames: details.allFrames,
      matchAboutBlank: details.matchAboutBlank,
      runAt: details.runAt || "document_idle",
      jsPaths: [],
      cssPaths: [],
    };

    const convertCodeToURL = (data, mime) => {
      const blob = new context.cloneScope.Blob(data, { type: mime });
      const blobURL = context.cloneScope.URL.createObjectURL(blob);

      this.blobURLs.add(blobURL);

      return blobURL;
    };

    if (details.js && details.js.length) {
      options.jsPaths = details.js.map(data => {
        if (data.file) {
          return data.file;
        }

        return convertCodeToURL([data.code], "text/javascript");
      });
    }

    if (details.css && details.css.length) {
      options.cssPaths = details.css.map(data => {
        if (data.file) {
          return data.file;
        }

        return convertCodeToURL([data.code], "text/css");
      });
    }

    return options;
  }

  serialize() {
    return this.options;
  }
}

this.contentScripts = class extends ExtensionAPI {
  getAPI(context) {
    const { extension } = context;

    // Map of the content script registered from the extension context.
    //
    // Map<scriptId -> ContentScriptParent>
    const parentScriptsMap = new Map();

    // Unregister all the scriptId related to a context when it is closed.
    context.callOnClose({
      close() {
        if (parentScriptsMap.size === 0) {
          return;
        }

        const scriptIds = Array.from(parentScriptsMap.keys());

        for (let scriptId of scriptIds) {
          extension.registeredContentScripts.delete(scriptId);
        }
        extension.updateContentScripts();

        extension.broadcast("Extension:UnregisterContentScripts", {
          id: extension.id,
          scriptIds,
        });
      },
    });

    return {
      contentScripts: {
        async register(details) {
          for (let origin of details.matches) {
            if (!extension.allowedOrigins.subsumes(new MatchPattern(origin))) {
              throw new ExtensionError(
                `Permission denied to register a content script for ${origin}`
              );
            }
          }

          const contentScript = new ContentScriptParent({ context, details });
          const { scriptId } = contentScript;

          parentScriptsMap.set(scriptId, contentScript);

          const scriptOptions = contentScript.serialize();

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
        async unregister(scriptId) {
          const contentScript = parentScriptsMap.get(scriptId);
          if (!contentScript) {
            Cu.reportError(new Error(`No such content script ID: ${scriptId}`));

            return;
          }

          parentScriptsMap.delete(scriptId);
          extension.registeredContentScripts.delete(scriptId);
          extension.updateContentScripts();

          contentScript.destroy();

          await extension.broadcast("Extension:UnregisterContentScripts", {
            id: extension.id,
            scriptIds: [scriptId],
          });
        },
      },
    };
  }
};
