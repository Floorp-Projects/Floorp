/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { ExtensionError, getUniqueId, parseMatchPatterns } = ExtensionUtils;

// Map<Extension, Map<string, number>> - For each extension, we keep a map
// where the key is a user-provided script ID, the value is an internal
// generated integer.
const gScriptIdsMap = new Map();

/**
 * Inserts a script or style in the given tab, and returns a promise which
 * resolves when the operation has completed.
 *
 * @param {BaseContext} context
 *        The extension context for which to perform the injection.
 * @param {Object} details
 *        The details object, specifying what to inject, where, and when.
 *        Derived from the ScriptInjection or CSSInjection types.
 * @param {string} kind
 *        The kind of data being injected. Possible choices: "js" or "css".
 * @param {string} method
 *        The name of the method which was called to trigger the injection.
 *        Used to generate appropriate error messages on failure.
 *
 * @returns {Promise}
 *        Resolves to the result of the execution, once it has completed.
 */
const execute = (context, details, kind, method) => {
  const { tabManager } = context.extension;

  let options = {
    jsPaths: [],
    cssPaths: [],
    removeCSS: method == "removeCSS",
    extensionId: context.extension.id,
  };

  const { tabId, frameIds, allFrames } = details.target;
  const tab = tabManager.get(tabId);

  // TODO: Bug 1750765 - Add test coverage for this option.
  options.hasActiveTabPermission = tab.hasActiveTabPermission;
  options.matches = tab.extension.allowedOrigins.patterns.map(
    host => host.pattern
  );

  const codeKey = kind === "js" ? "func" : "css";
  if ((details.files === null) == (details[codeKey] === null)) {
    throw new ExtensionError(
      `Exactly one of files and ${codeKey} must be specified.`
    );
  }

  if (details[codeKey]) {
    options[`${kind}Code`] = details[codeKey];
  }

  if (details.files) {
    for (const file of details.files) {
      let url = context.uri.resolve(file);
      if (!tab.extension.isExtensionURL(url)) {
        throw new ExtensionError(
          "Files to be injected must be within the extension"
        );
      }
      options[`${kind}Paths`].push(url);
    }
  }

  if (allFrames && frameIds) {
    throw new ExtensionError("Cannot specify both 'allFrames' and 'frameIds'.");
  }

  if (allFrames) {
    options.allFrames = allFrames;
  } else if (frameIds) {
    options.frameIds = frameIds;
  } else {
    options.frameIds = [0];
  }

  options.runAt = "document_idle";
  options.matchAboutBlank = true;
  options.wantReturnValue = true;
  // With this option set to `true`, we'll receive executeScript() results with
  // `frameId/result` properties and an `error` property will also be returned
  // in case of an error.
  options.returnResultsWithFrameIds = kind === "js";

  if (details.origin) {
    options.cssOrigin = details.origin.toLowerCase();
  } else {
    options.cssOrigin = "author";
  }

  // There is no need to execute anything when we have an empty list of frame
  // IDs because (1) it isn't invalid and (2) nothing will get executed.
  if (options.frameIds && options.frameIds.length === 0) {
    return [];
  }

  // This function is derived from `_execute()` in `parent/ext-tabs-base.js`,
  // make sure to keep both in sync when relevant.
  return tab.queryContent("Execute", options);
};

const makeInternalContentScript = details => {
  return {
    scriptId: getUniqueId(),
    options: {
      allFrames: details.allFrames || false,
      // TODO: Bug 1755976 - Add support for `details.css`.
      cssPaths: [],
      excludeMatches: details.excludeMatches,
      jsPaths: details.js || [],
      matchAboutBlank: true,
      matches: details.matches,
      originAttributesPatterns: null,
      runAt: details.runAt,
    },
  };
};

const ensureValidScriptId = id => {
  if (!id.length || id.startsWith("_")) {
    throw new ExtensionError("Invalid content script id.");
  }
};

this.scripting = class extends ExtensionAPI {
  constructor(extension) {
    super(extension);

    gScriptIdsMap.set(extension, new Map());
  }

  onShutdown() {
    // When the extension is unloaded, the following happens:
    //
    // 1. The shared memory is cleared in the parent, see [1]
    // 2. The policy is marked as invalid, see [2]
    //
    // The following are not explicitly cleaned up:
    //
    // - `extension.registeredContentScripts
    // - `ExtensionProcessScript.registeredContentScripts` +
    //   `policy.contentScripts` (via `policy.unregisterContentScripts`)
    //
    // This means the script won't run again, but there is still potential for
    // memory leaks if there is a reference to `extension` or `policy`
    // somewhere.
    //
    // [1]: https://searchfox.org/mozilla-central/rev/211649f071259c4c733b4cafa94c44481c5caacc/toolkit/components/extensions/Extension.jsm#2974-2976
    // [2]: https://searchfox.org/mozilla-central/rev/211649f071259c4c733b4cafa94c44481c5caacc/toolkit/components/extensions/ExtensionProcessScript.jsm#239

    gScriptIdsMap.delete(this.extension);
  }

  getAPI(context) {
    const { extension } = context;

    return {
      scripting: {
        executeScriptInternal: async details => {
          return execute(context, details, "js", "executeScript");
        },

        insertCSS: async details => {
          return execute(context, details, "css", "insertCSS").then(() => {});
        },

        removeCSS: async details => {
          return execute(context, details, "css", "removeCSS").then(() => {});
        },

        registerContentScripts: async scripts => {
          // Map<string, number>
          const scriptIdsMap = gScriptIdsMap.get(extension);
          // Map<string, { scriptId: number, options: Object }>
          const scriptsToRegister = new Map();

          for (const script of scripts) {
            ensureValidScriptId(script.id);

            if (scriptIdsMap.has(script.id)) {
              throw new ExtensionError(
                `Content script with id "${script.id}" is already registered.`
              );
            }

            if (scriptsToRegister.has(script.id)) {
              throw new ExtensionError(
                `Attempt to register content script with id "${script.id}" more than once.`
              );
            }

            if (!script.js?.length) {
              throw new ExtensionError("js must be specified.");
            }

            if (!script.matches?.length) {
              throw new ExtensionError("matches must be specified.");
            }

            // This will throw if a match pattern is invalid.
            parseMatchPatterns(script.matches);

            if (script.excludeMatches) {
              // This will throw if a match pattern is invalid.
              parseMatchPatterns(script.excludeMatches);
            }

            scriptsToRegister.set(script.id, makeInternalContentScript(script));
          }

          for (const [id, { scriptId, options }] of scriptsToRegister) {
            scriptIdsMap.set(id, scriptId);
            extension.registeredContentScripts.set(scriptId, options);
          }
          extension.updateContentScripts();

          await extension.broadcast("Extension:RegisterContentScripts", {
            id: extension.id,
            scripts: Array.from(scriptsToRegister.values()),
          });
        },

        getRegisteredContentScripts: async details => {
          // Map<string, number>
          const scriptIdsMap = gScriptIdsMap.get(extension);

          return Array.from(scriptIdsMap.entries())
            .filter(
              ([id, scriptId]) => !details?.ids || details.ids.includes(id)
            )
            .map(([id, scriptId]) => {
              const options = extension.registeredContentScripts.get(scriptId);

              return {
                id,
                allFrames: options.allFrames,
                excludeMatches: options.excludeMatches || undefined,
                js: options.jsPaths.map(jsPath =>
                  jsPath.replace(extension.baseURL, "")
                ),
                matches: options.matches,
                runAt: options.runAt,
              };
            });
        },

        unregisterContentScripts: async details => {
          // Map<string, number>
          const scriptIdsMap = gScriptIdsMap.get(extension);

          let ids = [];

          if (details?.ids) {
            for (const id of details.ids) {
              ensureValidScriptId(id);

              if (!scriptIdsMap.has(id)) {
                throw new ExtensionError(
                  `Content script with id "${id}" does not exist.`
                );
              }
            }

            ids = details.ids;
          } else {
            ids = Array.from(scriptIdsMap.keys());
          }

          if (ids.length === 0) {
            return;
          }

          const scriptIds = [];
          for (const id of ids) {
            const scriptId = scriptIdsMap.get(id);

            extension.registeredContentScripts.delete(scriptId);
            scriptIdsMap.delete(id);
            scriptIds.push(scriptId);
          }
          extension.updateContentScripts();

          await extension.broadcast("Extension:UnregisterContentScripts", {
            id: extension.id,
            scriptIds,
          });
        },
      },
    };
  }
};
