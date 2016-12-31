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
                                  "resource://gre/modules/Console.jsm");

function section(number, url) {
  const baseURL = "https://developer.mozilla.org/en-US/Firefox/Multiprocess_Firefox/Limitations_of_chrome_scripts";
  return { number, url: baseURL + url };
}

var CompatWarning = {
  // Sometimes we want to generate a warning, but put off issuing it
  // until later. For example, if someone registers a listener, we
  // might only want to warn about it if the listener actually
  // fires. However, we want the warning to show a stack for the
  // registration site.
  delayedWarning(msg, addon, warning) {
    function isShimLayer(filename) {
      return filename.indexOf("CompatWarning.jsm") != -1 ||
        filename.indexOf("RemoteAddonsParent.jsm") != -1 ||
        filename.indexOf("RemoteAddonsChild.jsm") != -1 ||
        filename.indexOf("multiprocessShims.js") != -1;
    }

    let stack = Components.stack;
    while (stack && isShimLayer(stack.filename))
      stack = stack.caller;

    let alreadyWarned = false;

    return function() {
      if (alreadyWarned) {
        return;
      }
      alreadyWarned = true;

      if (addon) {
        let histogram = Services.telemetry.getKeyedHistogramById("ADDON_SHIM_USAGE");
        histogram.add(addon, warning ? warning.number : 0);
      }

      if (!Preferences.get("dom.ipc.shims.enabledWarnings", false))
        return;

      let error = Cc['@mozilla.org/scripterror;1'].createInstance(Ci.nsIScriptError);
      if (!error || !Services.console) {
        // Too late during shutdown to use the nsIConsole
        return;
      }

      let message = `Warning: ${msg}`;
      if (warning)
        message += `\nMore info at: ${warning.url}`;

      error.init(
                 /* message*/ message,
                 /* sourceName*/ stack ? stack.filename : "",
                 /* sourceLine*/ stack ? stack.sourceLine : "",
                 /* lineNumber*/ stack ? stack.lineNumber : 0,
                 /* columnNumber*/ 0,
                 /* flags*/ Ci.nsIScriptError.warningFlag,
                 /* category*/ "chrome javascript");
      Services.console.logMessage(error);

      if (Preferences.get("dom.ipc.shims.dumpWarnings", false)) {
        dump(message + "\n");
        while (stack) {
          dump(stack + "\n");
          stack = stack.caller;
        }
        dump("\n");
      }
    };
  },

  warn(msg, addon, warning) {
    let delayed = this.delayedWarning(msg, addon, warning);
    delayed();
  },

  warnings: {
    content: section(1, "#gBrowser.contentWindow.2C_window.content..."),
    limitations_of_CPOWs: section(2, "#Limitations_of_CPOWs"),
    nsIContentPolicy: section(3, "#nsIContentPolicy"),
    nsIWebProgressListener: section(4, "#nsIWebProgressListener"),
    observers: section(5, "#Observers_in_the_chrome_process"),
    DOM_events: section(6, "#DOM_Events"),
    sandboxes: section(7, "#Sandboxes"),
    JSMs: section(8, "#JavaScript_code_modules_(JSMs)"),
    nsIAboutModule: section(9, "#nsIAboutModule"),
    // If more than 14 values appear here, you need to change the
    // ADDON_SHIM_USAGE histogram definition in Histograms.json.
  },
};
