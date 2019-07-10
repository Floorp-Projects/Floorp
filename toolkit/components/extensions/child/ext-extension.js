/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.extension = class extends ExtensionAPI {
  getAPI(context) {
    let api = {
      getURL(url) {
        return context.extension.baseURI.resolve(url);
      },

      get lastError() {
        return context.lastError;
      },

      get inIncognitoContext() {
        return context.incognito;
      },
    };

    if (context.envType === "addon_child") {
      api.getViews = function(fetchProperties) {
        let result = Cu.cloneInto([], context.cloneScope);

        for (let view of context.extension.views) {
          if (!view.active) {
            continue;
          }
          if (!context.principal.subsumes(view.principal)) {
            continue;
          }

          if (fetchProperties !== null) {
            if (
              fetchProperties.type !== null &&
              view.viewType != fetchProperties.type
            ) {
              continue;
            }

            if (
              fetchProperties.windowId !== null &&
              view.windowId != fetchProperties.windowId
            ) {
              continue;
            }

            if (
              fetchProperties.tabId !== null &&
              view.tabId != fetchProperties.tabId
            ) {
              continue;
            }
          }

          result.push(view.contentWindow);
        }

        return result;
      };
    }

    return { extension: api };
  }
};
