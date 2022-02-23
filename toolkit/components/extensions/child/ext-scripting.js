/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { ExtensionError } = ExtensionUtils;

this.scripting = class extends ExtensionAPI {
  getAPI(context) {
    return {
      scripting: {
        executeScript: async details => {
          if (
            (details.files !== null && details.func !== null) ||
            (!details.files && !details.func)
          ) {
            throw new ExtensionError(
              "Exactly one of files and func must be specified."
            );
          }

          let { func, args, ...parentDetails } = details;

          if (details.files) {
            if (details.args) {
              throw new ExtensionError(
                "'args' may not be used with file injections."
              );
            }
          } else {
            try {
              const serializedArgs = args
                ? JSON.stringify(args).slice(1, -1)
                : "";
              // This is a prop that we compute here and pass to the parent.
              parentDetails.codeToExecute = `(${func.toString()})(${serializedArgs});`;
            } catch (e) {
              throw new ExtensionError("Unserializable arguments.");
            }
          }

          return context.childManager.callParentAsyncFunction(
            "scripting.executeScriptInternal",
            [parentDetails]
          );
        },
      },
    };
  }
};
