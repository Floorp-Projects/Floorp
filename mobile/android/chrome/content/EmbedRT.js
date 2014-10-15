/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "ConsoleAPI",
                                  "resource://gre/modules/devtools/Console.jsm");

/*
 * Collection of methods and features specific to using a GeckoView instance.
 * The code is isolated from browser.js for code size and performance reasons.
 */
var EmbedRT = {
  _scopes: {},

  observe: function(subject, topic, data) {
    switch(topic) {
      case "GeckoView:ImportScript":
        this.importScript(data);
        break;
    }
  },

  /*
   * Loads a script file into a sandbox and calls an optional load function
   */
  importScript: function(scriptURL) {
    if (scriptURL in this._scopes) {
      return;
    }

    let principal = Cc["@mozilla.org/systemprincipal;1"].createInstance(Ci.nsIPrincipal);

    let sandbox = new Cu.Sandbox(principal,
      {
        sandboxName: scriptURL,
        wantGlobalProperties: ["indexedDB"]
      }
    );

    sandbox["console"] = new ConsoleAPI({ consoleID: "script/" + scriptURL });

    // As we don't want our caller to control the JS version used for the
    // script file, we run loadSubScript within the context of the
    // sandbox with the latest JS version set explicitly.
    sandbox.__SCRIPT_URI_SPEC__ = scriptURL;
    Cu.evalInSandbox("Components.classes['@mozilla.org/moz/jssubscript-loader;1'].createInstance(Components.interfaces.mozIJSSubScriptLoader).loadSubScript(__SCRIPT_URI_SPEC__);", sandbox, "ECMAv5");

    this._scopes[scriptURL] = sandbox;

    if ("load" in sandbox) {
      let params = {
        window: window,
        resourceURI: scriptURL,
      };

      try {
        sandbox["load"](params);
      } catch(e) {
        dump("Exception calling 'load' method in script: " + scriptURL + "\n" + e);
      }
    }
  }
};
