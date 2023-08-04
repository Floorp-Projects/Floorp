/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.backgroundPage = class extends ExtensionAPI {
  getAPI(context) {
    function getBackgroundPage() {
      for (let view of context.extension.views) {
        if (
          // To find the (top-level) background context, this logic relies on
          // the order of views, implied by the fact that the top-level context
          // is created before child contexts. If this assumption ever becomes
          // invalid, add a check for view.isBackgroundContext.
          view.viewType == "background" &&
          context.principal.subsumes(view.principal)
        ) {
          return view.contentWindow;
        }
      }
      return null;
    }
    return {
      extension: {
        getBackgroundPage,
      },

      runtime: {
        getBackgroundPage() {
          return context.childManager
            .callParentAsyncFunction("runtime.internalWakeupBackground", [])
            .then(() => {
              return context.cloneScope.Promise.resolve(getBackgroundPage());
            });
        },
      },
    };
  }
};
