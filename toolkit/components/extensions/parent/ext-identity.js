/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "Services",
  "resource://gre/modules/Services.jsm"
);

XPCOMUtils.defineLazyGlobalGetters(this, ["XMLHttpRequest", "ChannelWrapper"]);

var { promiseDocumentLoaded } = ExtensionUtils;

const checkRedirected = (url, redirectURI) => {
  return new Promise((resolve, reject) => {
    let xhr = new XMLHttpRequest();
    xhr.open("GET", url);
    // We expect this if the user has not authenticated.
    xhr.onload = () => {
      reject(0);
    };
    // An unexpected error happened, log for extension authors.
    xhr.onerror = () => {
      reject(xhr.status);
    };
    // Catch redirect to our redirect_uri before a new request is made.
    xhr.channel.notificationCallbacks = {
      QueryInterface: ChromeUtils.generateQI([
        "nsIInterfaceRequestor",
        "nsIChannelEventSync",
      ]),

      getInterface: ChromeUtils.generateQI(["nsIChannelEventSink"]),

      asyncOnChannelRedirect(oldChannel, newChannel, flags, callback) {
        let responseURL = newChannel.URI.spec;
        if (responseURL.startsWith(redirectURI)) {
          resolve(responseURL);
          // Cancel the redirect.
          callback.onRedirectVerifyCallback(Cr.NS_BINDING_ABORTED);
          return;
        }
        callback.onRedirectVerifyCallback(Cr.NS_OK);
      },
    };
    xhr.send();
  });
};

const openOAuthWindow = (details, redirectURI) => {
  let args = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
  let supportsStringPrefURL = Cc[
    "@mozilla.org/supports-string;1"
  ].createInstance(Ci.nsISupportsString);
  supportsStringPrefURL.data = details.url;
  args.appendElement(supportsStringPrefURL);

  let window = Services.ww.openWindow(
    null,
    AppConstants.BROWSER_CHROME_URL,
    "launchWebAuthFlow_dialog",
    "chrome,location=yes,centerscreen,dialog=no,resizable=yes,scrollbars=yes",
    args
  );

  return new Promise((resolve, reject) => {
    let httpActivityDistributor = Cc[
      "@mozilla.org/network/http-activity-distributor;1"
    ].getService(Ci.nsIHttpActivityDistributor);

    let unloadListener;
    let httpObserver;

    const resolveIfRedirectURI = channel => {
      const url = channel.URI && channel.URI.spec;
      if (!url || !url.startsWith(redirectURI)) {
        return;
      }

      // Early exit if channel isn't related to the oauth dialog.
      let wrapper = ChannelWrapper.get(channel);
      if (
        !wrapper.browserElement &&
        wrapper.browserElement !== window.gBrowser.selectedBrowser
      ) {
        return;
      }

      wrapper.cancel(Cr.NS_ERROR_ABORT, Ci.nsILoadInfo.BLOCKING_REASON_NONE);
      window.gBrowser.webNavigation.stop(Ci.nsIWebNavigation.STOP_ALL);
      window.removeEventListener("unload", unloadListener);
      httpActivityDistributor.removeObserver(httpObserver);
      window.close();
      resolve(url);
    };

    httpObserver = {
      observeActivity(channel, type, subtype, timestamp, sizeData, stringData) {
        try {
          channel.QueryInterface(Ci.nsIChannel);
        } catch {
          // Ignore activities for channels that doesn't implement nsIChannel
          // (e.g. a NullHttpChannel).
          return;
        }

        resolveIfRedirectURI(channel);
      },
    };

    httpActivityDistributor.addObserver(httpObserver);

    // If the user just closes the window we need to reject
    unloadListener = () => {
      window.removeEventListener("unload", unloadListener);
      httpActivityDistributor.removeObserver(httpObserver);
      reject({ message: "User cancelled or denied access." });
    };

    promiseDocumentLoaded(window.document).then(() => {
      window.addEventListener("unload", unloadListener);
    });
  });
};

this.identity = class extends ExtensionAPI {
  getAPI(context) {
    return {
      identity: {
        launchWebAuthFlowInParent: function(details, redirectURI) {
          // If the request is automatically redirected the user has already
          // authorized and we do not want to show the window.
          return checkRedirected(details.url, redirectURI).catch(
            requestError => {
              // requestError is zero or xhr.status
              if (requestError !== 0) {
                Cu.reportError(
                  `browser.identity auth check failed with ${requestError}`
                );
                return Promise.reject({ message: "Invalid request" });
              }
              if (!details.interactive) {
                return Promise.reject({ message: `Requires user interaction` });
              }

              return openOAuthWindow(details, redirectURI);
            }
          );
        },
      },
    };
  }
};
