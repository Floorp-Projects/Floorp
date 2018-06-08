/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

ChromeUtils.defineModuleGetter(this, "Services",
                               "resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGlobalGetters(this, ["URL", "XMLHttpRequest"]);

var {
  promiseDocumentLoaded,
} = ExtensionUtils;

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
      QueryInterface: ChromeUtils.generateQI([Ci.nsIInterfaceRequestor, Ci.nsIChannelEventSync]),

      getInterface: ChromeUtils.generateQI([Ci.nsIChannelEventSink]),

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
  let supportsStringPrefURL = Cc["@mozilla.org/supports-string;1"]
                                .createInstance(Ci.nsISupportsString);
  supportsStringPrefURL.data = details.url;
  args.appendElement(supportsStringPrefURL);

  let window = Services.ww.openWindow(null,
                                      Services.prefs.getCharPref("browser.chromeURL"),
                                      "launchWebAuthFlow_dialog",
                                      "chrome,location=yes,centerscreen,dialog=no,resizable=yes,scrollbars=yes",
                                      args);

  return new Promise((resolve, reject) => {
    let wpl;

    // If the user just closes the window we need to reject
    function unloadlistener() {
      window.removeEventListener("unload", unloadlistener);
      window.gBrowser.removeProgressListener(wpl);
      reject({message: "User cancelled or denied access."});
    }

    wpl = {
      onStateChange(progress, request, flags, status) {
        // "request" is now a RemoteWebProgressRequest and is not cancelable
        // using request.cancel.  We can however, stop everything using
        // webNavigation.
        if (request && request.URI &&
          request.URI.spec.startsWith(redirectURI)) {
          window.gBrowser.webNavigation.stop(Ci.nsIWebNavigation.STOP_ALL);
          window.removeEventListener("unload", unloadlistener);
          window.gBrowser.removeProgressListener(wpl);
          window.close();
          resolve(request.URI.spec);
        }
      },
    };

    promiseDocumentLoaded(window.document).then(() => {
      window.gBrowser.addProgressListener(wpl);
      window.addEventListener("unload", unloadlistener);
    });
  });
};

this.identity = class extends ExtensionAPI {
  getAPI(context) {
    return {
      identity: {
        launchWebAuthFlow: function(details) {
          // In OAuth2 the url should have a redirect_uri param, parse the url and grab it
          let url, redirectURI;
          try {
            url = new URL(details.url);
          } catch (e) {
            return Promise.reject({message: "details.url is invalid"});
          }
          try {
            redirectURI = new URL(url.searchParams.get("redirect_uri"));
            if (!redirectURI) {
              return Promise.reject({message: "redirect_uri is missing"});
            }
          } catch (e) {
            return Promise.reject({message: "redirect_uri is invalid"});
          }

          // If the request is automatically redirected the user has already
          // authorized and we do not want to show the window.
          return checkRedirected(details.url, redirectURI).catch((requestError) => {
            // requestError is zero or xhr.status
            if (requestError !== 0) {
              Cu.reportError(`browser.identity auth check failed with ${requestError}`);
              return Promise.reject({message: "Invalid request"});
            }
            if (!details.interactive) {
              return Promise.reject({message: `Requires user interaction`});
            }

            return openOAuthWindow(details, redirectURI);
          });
        },
      },
    };
  }
};
