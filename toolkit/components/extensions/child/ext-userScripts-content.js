/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

var USERSCRIPT_PREFNAME = "extensions.webextensions.userScripts.enabled";
var USERSCRIPT_DISABLED_ERRORMSG = `userScripts APIs are currently experimental and must be enabled with the ${USERSCRIPT_PREFNAME} preference.`;

XPCOMUtils.defineLazyPreferenceGetter(this, "userScriptsEnabled", USERSCRIPT_PREFNAME, false);

var {
  ExtensionError,
} = ExtensionUtils;

this.userScriptsContent = class extends ExtensionAPI {
  getAPI(context) {
    return {
      userScripts: {
        setScriptAPIs(exportedAPIMethods) {
          if (!userScriptsEnabled) {
            throw new ExtensionError(USERSCRIPT_DISABLED_ERRORMSG);
          }

          context.setUserScriptAPIs(exportedAPIMethods);
        },
      },
    };
  }
};
