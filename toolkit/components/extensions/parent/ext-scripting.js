/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { ExtensionError } = ExtensionUtils;

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

  // This function is derived from `_execute()` in `parent/ext-tabs-base.js`,
  // make sure to keep both in sync when relevant.
  return tab.queryContent("Execute", options);
};

this.scripting = class extends ExtensionAPI {
  getAPI(context) {
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
      },
    };
  }
};
