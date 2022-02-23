/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { ExtensionError } = ExtensionUtils;

this.scripting = class extends ExtensionAPI {
  getAPI(context) {
    const { extension } = context;
    const { tabManager } = extension;

    return {
      scripting: {
        executeScriptInternal: async details => {
          let { tabId, frameIds } = details.target;

          let tab = tabManager.get(tabId);

          let executeScriptDetails = {
            code: null,
            file: null,
            runAt: "document_idle",
          };

          if (details.files) {
            // TODO bug 1736576: Support more than one file.
            executeScriptDetails.file = details.files[0];
          } else {
            // Defined in `child/ext-scripting.js`.
            executeScriptDetails.code = details.codeToExecute;
          }

          const promises = [];

          if (!frameIds) {
            // We use the top-level frame by default.
            frameIds = [0];
          }

          for (const frameId of frameIds) {
            promises.push(
              tab
                .executeScript(context, { ...executeScriptDetails, frameId })
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
