/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
Cu.importGlobalProperties(["fetch"]);

var EXPORTED_SYMBOLS = ["AboutHttpsOnlyErrorChild"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { RemotePageChild } = ChromeUtils.import(
  "resource://gre/actors/RemotePageChild.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "@mozilla.org/network/serialization-helper;1",
  "nsISerializationHelper"
);

class AboutHttpsOnlyErrorChild extends RemotePageChild {
  actorCreated() {
    super.actorCreated();

    // If you add a new function, remember to add it to RemotePageAccessManager.jsm
    // to allow content-privileged about:httpsonlyerror to use it.
    const exportableFunctions = [
      "RPMTryPingSecureWWWLink",
      "RPMOpenSecureWWWLink",
    ];
    this.exportFunctions(exportableFunctions);
  }

  RPMTryPingSecureWWWLink() {
    // try if the page can be reached with www prefix
    // if so send message to the parent to send message to the error page to display
    // suggestion button for www

    const httpsOnlySuggestionPref = Services.prefs.getBoolPref(
      "dom.security.https_only_mode_error_page_user_suggestions"
    );

    // only check if pref is true otherwise return
    if (!httpsOnlySuggestionPref) {
      return;
    }

    // get the host url without the path with www in front
    const wwwURL = "https://www." + this.contentWindow.location.host;
    fetch(wwwURL, {
      credentials: "omit",
      cache: "no-store",
    })
      .then(data => {
        if (data.status === 200) {
          this.contentWindow.dispatchEvent(
            new this.contentWindow.CustomEvent("pingSecureWWWLinkSuccess")
          );
        }
      })
      .catch(() => {
        dump("No secure www suggestion possible for " + wwwURL);
      });
  }

  RPMOpenSecureWWWLink() {
    // if user wants to visit suggested secure www page: visit page with www prefix and delete errorpage from history
    const context = this.manager.browsingContext;
    const docShell = context.docShell;
    const httpChannel = docShell.failedChannel.QueryInterface(
      Ci.nsIHttpChannel
    );
    const webNav = docShell.QueryInterface(Ci.nsIWebNavigation);
    const triggeringPrincipal =
      docShell.failedChannel.loadInfo.triggeringPrincipal;
    const oldURI = httpChannel.URI;
    const newWWWURI = oldURI
      .mutate()
      .setHost("www." + oldURI.host)
      .finalize();

    webNav.loadURI(newWWWURI.spec, {
      triggeringPrincipal,
      loadFlags: Ci.nsIWebNavigation.LOAD_FLAGS_REPLACE_HISTORY,
    });
  }
}
