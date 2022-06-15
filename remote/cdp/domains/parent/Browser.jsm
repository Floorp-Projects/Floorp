/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Browser"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const { Domain } = ChromeUtils.import(
  "chrome://remote/content/cdp/domains/Domain.jsm"
);

class Browser extends Domain {
  getVersion() {
    const { isHeadless } = Cc["@mozilla.org/gfx/info;1"].getService(
      Ci.nsIGfxInfo
    );
    const { userAgent } = Cc[
      "@mozilla.org/network/protocol;1?name=http"
    ].getService(Ci.nsIHttpProtocolHandler);
    return {
      protocolVersion: "1.3",
      product:
        (isHeadless ? "Headless" : "") +
        `${Services.appinfo.name}/${Services.appinfo.version}`,
      revision: Services.appinfo.sourceURL.split("/").pop(),
      userAgent,
      jsVersion: "1.8.5",
    };
  }

  close() {
    // Notify all windows that an application quit has been requested.
    const cancelQuit = Cc["@mozilla.org/supports-PRBool;1"].createInstance(
      Ci.nsISupportsPRBool
    );
    Services.obs.notifyObservers(cancelQuit, "quit-application-requested");

    // If the shutdown of the application is prevented force quit it instead.
    const mode = cancelQuit.data
      ? Ci.nsIAppStartup.eForceQuit
      : Ci.nsIAppStartup.eAttemptQuit;

    Services.startup.quit(mode);
  }
}
