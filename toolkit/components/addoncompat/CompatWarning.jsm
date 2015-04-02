// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

this.EXPORTED_SYMBOLS = ["CompatWarning"];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "console",
                                  "resource://gre/modules/devtools/Console.jsm");

let CompatWarning = {
  warn: function CompatWarning_warn(msg, url) {

    if (!Preferences.get("dom.ipc.shims.enabledWarnings", false))
      return;

    function isShimLayer(filename) {
      return filename.indexOf("CompatWarning.jsm") != -1 ||
        filename.indexOf("RemoteAddonsParent.jsm") != -1 ||
        filename.indexOf("RemoteAddonsChild.jsm") != -1 ||
        filename.indexOf("multiprocessShims.js") != -1;
    };

    let stack = Components.stack;
    while (stack && isShimLayer(stack.filename))
      stack = stack.caller;

    let error = Cc['@mozilla.org/scripterror;1'].createInstance(Ci.nsIScriptError);
    if (!error || !Services.console) {
      // Too late during shutdown to use the nsIConsole
      return;
    }

    let message = `Warning: ${msg}`;
    if (url)
      message += `\nMore info at: ${url}`;

    error.init(
               /*message*/ message,
               /*sourceName*/ stack ? stack.filename : "",
               /*sourceLine*/ stack ? stack.sourceLine : "",
               /*lineNumber*/ stack ? stack.lineNumber : 0,
               /*columnNumber*/ 0,
               /*flags*/ Ci.nsIScriptError.warningFlag,
               /*category*/ "chrome javascript");
    Services.console.logMessage(error);
  },

  chromeScriptSections: ((baseURL) => {
    return {
      content: baseURL + "#gBrowser.contentWindow.2C_window.content...",
      limitations_of_CPOWs: baseURL + "#Limitations_of_CPOWs",
      nsIContentPolicy : baseURL + "#nsIContentPolicy",
      nsIWebProgressListener: baseURL + "#nsIWebProgressListener",
      observers : baseURL + "#Observers_in_the_chrome_process",
      DOM_events : baseURL + "#DOM_Events",
      sandboxes : baseURL + "#Sandboxes",
      JSMs : baseURL + "#JavaScript_code_modules_(JSMs)",
      nsIAboutModule: baseURL + "#nsIAboutModule"
    };
  })("https://developer.mozilla.org/en-US/Firefox/Multiprocess_Firefox/Limitations_of_chrome_scripts"),

  frameScriptSections: ((baseURL) => {
    return {
      file_IO : baseURL + "#File_I.2FO",
      XUL_and_browser_UI: baseURL + "#XUL_and_browser_UI",
      chrome_windows: baseURL + "#Chrome_windows",
      places_API: baseURL + "#Places_API",
      observers: baseURL + "#Observers_in_the_content_process",
      QI: baseURL + "#QI_from_content_window_to_chrome_window",
      nsIAboutModule: baseURL + "#nsIAboutModule",
      JSMs: baseURL + "#JavaScript_code_modules_(JSMs)"
    };
  })("https://developer.mozilla.org/en-US/Firefox/Multiprocess_Firefox/Limitations_of_frame_scripts")
};
