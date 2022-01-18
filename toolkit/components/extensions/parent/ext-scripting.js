/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { ExtensionError } = ExtensionUtils;

/**
 * Inserts a script in the given tab, and returns a promise which resolves when
 * the operation has completed.
 *
 * @param {TabBase} tab
 *        The tab in which to perform the injection.
 * @param {BaseContext} context
 *        The extension context for which to perform the injection.
 * @param {Object} details
 *        The details object, specifying what to inject, where, and when.
 *        Derived from the ScriptInjection type.
 * @param {string} kind
 *        The kind of data being injected. Possible choices: "js".
 * @param {string} method
 *        The name of the method which was called to trigger the injection.
 *        Used to generate appropriate error messages on failure.
 *
 * @returns {Promise}
 *        Resolves to the result of the execution, once it has completed.
 */
const execute = (tab, context, details, kind, method) => {
  let options = {
    jsPaths: [],
    extensionId: context.extension.id,
  };

  // TODO: Bug 1750765 - Add test coverage for this option.
  options.hasActiveTabPermission = tab.hasActiveTabPermission;
  options.matches = tab.extension.allowedOrigins.patterns.map(
    host => host.pattern
  );

  if (details.code) {
    options[`${kind}Code`] = details.code;
  }

  if (details.files) {
    for (const file of details.files) {
      let url = context.uri.resolve(file);
      if (!tab.extension.isExtensionURL(url)) {
        return Promise.reject({
          message: "Files to be injected must be within the extension",
        });
      }
      options[`${kind}Paths`].push(url);
    }
  }

  // TODO: Bug 1736574 - Add support for multiple frame IDs and `allFrames`.
  if (details.frameId) {
    options.frameID = details.frameId;
  }

  options.runAt = "document_idle";
  options.wantReturnValue = true;

  // TODO: Bug 1736579 - Configure options for CSS injection, e.g., `cssPaths`
  // and `cssOrigin`.

  // This function is derived from `_execute()` in `parent/ext-tabs-base.js`,
  // make sure to keep both in sync when relevant.
  return tab.queryContent("Execute", options);
};

this.scripting = class extends ExtensionAPI {
  getAPI(context) {
    const { extension } = context;
    const { tabManager } = extension;

    return {
      scripting: {
        executeScriptInternal: async details => {
          let { tabId, frameIds } = details.target;

          let tab = tabManager.get(tabId);

          let executeDetails = {
            // Defined in `child/ext-scripting.js`.
            code: details.codeToExecute,
            files: details.files,
          };

          const promises = [];

          if (!frameIds) {
            // We use the top-level frame by default.
            frameIds = [0];
          }

          for (const frameId of frameIds) {
            const details = { ...executeDetails, frameId };
            promises.push(
              execute(tab, context, details, "js", "executeScript")
                // We return `null` when the result value is falsey.
                .then(results => ({ frameId, result: results[0] || null }))
                .catch(error => ({ frameId, result: null, error }))
            );
          }

          const results = await Promise.all(promises);

          return results.map(({ frameId, result, error }) => {
            if (error) {
              // TODO Bug 1740608: we re-throw extension errors coming from
              // `tab.executeScript()` and only log runtime errors, but this
              // might change because error handling needs to be more
              // well-defined.
              if (error instanceof ExtensionError) {
                throw error;
              }

              Cu.reportError(error.message || error);
            }

            return { frameId, result };
          });
        },
      },
    };
  }
};
