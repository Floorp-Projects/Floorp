"use strict";

this.backgroundPage = class extends ExtensionAPI {
  getAPI(context) {
    function getBackgroundPage() {
      for (let view of context.extension.views) {
        if (view.viewType == "background" && context.principal.subsumes(view.principal)) {
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
          return context.cloneScope.Promise.resolve(getBackgroundPage());
        },
      },
    };
  }
};
