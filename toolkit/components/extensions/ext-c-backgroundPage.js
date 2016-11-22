"use strict";

global.initializeBackgroundPage = (contentWindow) => {
  // Override the `alert()` method inside background windows;
  // we alias it to console.log().
  // See: https://bugzilla.mozilla.org/show_bug.cgi?id=1203394
  let alertDisplayedWarning = false;
  let alertOverwrite = text => {
    if (!alertDisplayedWarning) {
      require("devtools/client/framework/devtools-browser");

      let hudservice = require("devtools/client/webconsole/hudservice");
      hudservice.openBrowserConsoleOrFocus();

      contentWindow.console.warn("alert() is not supported in background windows; please use console.log instead.");

      alertDisplayedWarning = true;
    }

    contentWindow.console.log(text);
  };
  Cu.exportFunction(alertOverwrite, contentWindow, {defineAs: "alert"});
};

extensions.registerSchemaAPI("extension", "addon_child", context => {
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
});
