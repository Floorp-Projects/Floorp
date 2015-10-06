// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Wrap a remote fxa-content-server.
 *
 * An about:accounts tab loads and displays an fxa-content-server page,
 * depending on the current Android Account status and an optional 'action'
 * parameter.
 *
 * We show a spinner while the remote iframe is loading.  We expect the
 * WebChannel message listening to the fxa-content-server to send this tab's
 * <browser>'s messageManager a LOADED message when the remote iframe provides
 * the WebChannel LOADED message.  See the messageManager registration and the
 * |loadedDeferred| promise.  This loosely couples the WebChannel implementation
 * and about:accounts!  (We need this coupling in order to distinguish
 * WebChannel LOADED messages produced by multiple about:accounts tabs.)
 *
 * We capture error conditions by accessing the inner nsIWebNavigation of the
 * iframe directly.
 */

"use strict";

var {classes: Cc, interfaces: Ci, utils: Cu} = Components; /*global Components */

Cu.import("resource://gre/modules/Accounts.jsm"); /*global Accounts */
Cu.import("resource://gre/modules/PromiseUtils.jsm"); /*global PromiseUtils */
Cu.import("resource://gre/modules/Services.jsm"); /*global Services */
Cu.import("resource://gre/modules/XPCOMUtils.jsm"); /*global XPCOMUtils */

const ACTION_URL_PARAM = "action";

const COMMAND_LOADED = "fxaccounts:loaded";

const log = Cu.import("resource://gre/modules/AndroidLog.jsm", {}).AndroidLog.bind("FxAccounts");

XPCOMUtils.defineLazyServiceGetter(this, "ParentalControls",
  "@mozilla.org/parental-controls-service;1", "nsIParentalControlsService");

// Shows the toplevel element with |id| to be shown - all other top-level
// elements are hidden.
// If |id| is 'spinner', then 'remote' is also shown, with opacity 0.
function show(id) {
  let allTop = document.querySelectorAll(".toplevel");
  for (let elt of allTop) {
    if (elt.getAttribute("id") == id) {
      elt.style.display = 'block';
    } else {
      elt.style.display = 'none';
    }
  }
  if (id == 'spinner') {
    document.getElementById('remote').style.display = 'block';
    document.getElementById('remote').style.opacity = 0;
  }
}

// Each time we try to load the remote <iframe>, loadedDeferred is replaced.  It
// is resolved by a LOADED message, and rejected by a failure to load.
var loadedDeferred = null;

// We have a new load starting.  Replace the existing promise with a new one,
// and queue up the transition to remote content.
function deferTransitionToRemoteAfterLoaded(url) {
  log.d('Waiting for LOADED message.');

  // We are collecting data to understand the experience when using
  // about:accounts to connect to the specific fxa-content-server hosted by
  // Mozilla at accounts.firefox.com.  However, we don't want to observe what
  // alternate servers users might be using, so we can't collect the whole URL.
  // Here, we filter the data based on whether the user is /not/ using
  // accounts.firefox.com, and then record just the endpoint path.  Other
  // collected data could expose that the user is using Firefox Accounts, and
  // together, that leaks the number of users not using accounts.firefox.com.
  // We accept this leak: Mozilla already collects data about whether Sync (both
  // legacy and FxA) is using a custom server in various situations: see the
  // WEAVE_CUSTOM_* Telemetry histograms.
  let recordResultTelemetry; // Defined only when not customized.
  let isCustomized = Services.prefs.prefHasUserValue("identity.fxaccounts.remote.webchannel.uri");
  if (!isCustomized) {
    // Turn "https://accounts.firefox.com/settings?context=fx_fennec_v1&email=EMAIL" into "/settings".
    let key = Services.io.newURI(url, null, null).path.split("?")[0];

    let startTime = Cu.now();

    let start = Services.telemetry.getKeyedHistogramById('ABOUT_ACCOUNTS_CONTENT_SERVER_LOAD_STARTED_COUNT');
    start.add(key);

    recordResultTelemetry = function(success) {
      let rate = Services.telemetry.getKeyedHistogramById('ABOUT_ACCOUNTS_CONTENT_SERVER_LOADED_RATE');
      rate.add(key, success);

      // We would prefer to use TelemetryStopwatch, but it doesn't yet support
      // keyed histograms (see Bug 1205898).  So we measure and store ourselves.
      let delta = Cu.now() - startTime; // Floating point milliseconds, microsecond precision.
      let time = success ?
            Services.telemetry.getKeyedHistogramById('ABOUT_ACCOUNTS_CONTENT_SERVER_LOADED_TIME_MS') :
            Services.telemetry.getKeyedHistogramById('ABOUT_ACCOUNTS_CONTENT_SERVER_FAILURE_TIME_MS');
      time.add(key, Math.round(delta));
    };
  }

  loadedDeferred = PromiseUtils.defer();
  loadedDeferred.promise.then(() => {
    log.d('Got LOADED message!');
    document.getElementById("remote").style.opacity = 0;
    show("remote");
    document.getElementById("remote").style.opacity = 1;
    if (!isCustomized) {
      recordResultTelemetry(true);
    }
  })
  .catch((e) => {
    log.w('Did not get LOADED message: ' + e.toString());
    if (!isCustomized) {
      recordResultTelemetry(false);
    }
  });
}

function handleLoadedMessage(message) {
  loadedDeferred.resolve();
};

var wrapper = {
  iframe: null,

  url: null,

  init: function (url) {
    this.url = url;
    deferTransitionToRemoteAfterLoaded(this.url);

    let iframe = document.getElementById("remote");
    this.iframe = iframe;
    this.iframe.QueryInterface(Ci.nsIFrameLoaderOwner);
    let docShell = this.iframe.frameLoader.docShell;
    docShell.QueryInterface(Ci.nsIWebProgress);
    docShell.addProgressListener(this.iframeListener, Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT);

    // Set the iframe's location with loadURI/LOAD_FLAGS_BYPASS_HISTORY to
    // avoid having a new history entry being added.
    let webNav = iframe.frameLoader.docShell.QueryInterface(Ci.nsIWebNavigation);
    webNav.loadURI(url, Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_HISTORY, null, null, null);
  },

  retry: function () {
    deferTransitionToRemoteAfterLoaded(this.url);

    let webNav = this.iframe.frameLoader.docShell.QueryInterface(Ci.nsIWebNavigation);
    webNav.loadURI(this.url, Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_HISTORY, null, null, null);
  },

  iframeListener: {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                         Ci.nsISupportsWeakReference,
                                         Ci.nsISupports]),

    onStateChange: function(aWebProgress, aRequest, aState, aStatus) {
      let failure = false;

      // Captive portals sometimes redirect users
      if ((aState & Ci.nsIWebProgressListener.STATE_REDIRECTING)) {
        failure = true;
      } else if ((aState & Ci.nsIWebProgressListener.STATE_STOP)) {
        if (aRequest instanceof Ci.nsIHttpChannel) {
          try {
            failure = aRequest.responseStatus != 200;
          } catch (e) {
            failure = aStatus != Components.results.NS_OK;
          }
        }
      }

      // Calling cancel() will raise some OnStateChange notifications by itself,
      // so avoid doing that more than once
      if (failure && aStatus != Components.results.NS_BINDING_ABORTED) {
        aRequest.cancel(Components.results.NS_BINDING_ABORTED);
        // Since after a promise is fulfilled, subsequent fulfillments are
        // treated as no-ops, we don't care that we might see multiple failures
        // due to multiple listener callbacks.  (It's not easy to extract this
        // from the Promises spec, but it is widely quoted.  Start with
        // http://stackoverflow.com/a/18218542.)
        loadedDeferred.reject(new Error("Failed in onStateChange!"));
        show("networkError");
      }
    },

    onLocationChange: function(aWebProgress, aRequest, aLocation, aFlags) {
      if (aRequest && aFlags & Ci.nsIWebProgressListener.LOCATION_CHANGE_ERROR_PAGE) {
        aRequest.cancel(Components.results.NS_BINDING_ABORTED);
        // As above, we're not concerned by multiple listener callbacks.
        loadedDeferred.reject(new Error("Failed in onLocationChange!"));
        show("networkError");
      }
    },

    onProgressChange: function() {},
    onStatusChange: function() {},
    onSecurityChange: function() {},
  },
};


function retry() {
  log.i("Retrying.");
  show("spinner");
  wrapper.retry();
}

function openPrefs() {
  log.i("Opening Sync preferences.");
  // If an Android Account exists, this will open the Status Activity.
  // Otherwise, it will begin the Get Started flow.  This should only be shown
  // when an Account actually exists.
  Accounts.launchSetup();
}

function getURLForAction(action, urlParams) {
  let url = Services.urlFormatter.formatURLPref("identity.fxaccounts.remote.webchannel.uri");
  url = url + (url.endsWith("/") ? "" : "/") + action;
  const CONTEXT = "fx_fennec_v1";
  // The only service managed by Fennec, to date, is Firefox Sync.
  const SERVICE = "sync";
  urlParams = urlParams || new URLSearchParams("");
  urlParams.set('service', SERVICE);
  urlParams.set('context', CONTEXT);
  // Ideally we'd just merge urlParams with new URL(url).searchParams, but our
  // URLSearchParams implementation doesn't support iteration (bug 1085284).
  let urlParamStr = urlParams.toString();
  if (urlParamStr) {
    url += (url.includes("?") ? "&" : "?") + urlParamStr;
  }
  return url;
}

function updateDisplayedEmail(user) {
  let emailDiv = document.getElementById("email");
  if (emailDiv && user) {
    emailDiv.textContent = user.email;
  }
}

function init() {
  // Test for restrictions before getFirefoxAccount(), since that will fail if
  // we are restricted.
  if (!ParentalControls.isAllowed(ParentalControls.MODIFY_ACCOUNTS)) {
    // It's better to log and show an error message than to invite user
    // confusion by removing about:accounts entirely.  That is, if the user is
    // restricted, this way they'll discover as much and may be able to get
    // out of their restricted profile.  If we remove about:accounts entirely,
    // it will look like Fennec is buggy, and the user will be very confused.
    log.e("This profile cannot connect to Firefox Accounts: showing restricted error.");
    show("restrictedError");
    return;
  }

  Accounts.getFirefoxAccount().then(user => {
    // It's possible for the window to start closing before getting the user
    // completes.  Tests in particular can cause this.
    if (window.closed) {
      return;
    }

    updateDisplayedEmail(user);

    // Ideally we'd use new URL(document.URL).searchParams, but for about: URIs,
    // searchParams is empty.
    let urlParams = new URLSearchParams(document.URL.split("?")[1] || "");
    let action = urlParams.get(ACTION_URL_PARAM);
    urlParams.delete(ACTION_URL_PARAM);

    switch (action) {
    case "signup":
      if (user) {
        // Asking to sign-up when already signed in just shows prefs.
        show("prefs");
      } else {
        show("spinner");
        wrapper.init(getURLForAction("signup", urlParams));
      }
      break;
    case "signin":
      if (user) {
        // Asking to sign-in when already signed in just shows prefs.
        show("prefs");
      } else {
        show("spinner");
        wrapper.init(getURLForAction("signin", urlParams));
      }
      break;
    case "force_auth":
      if (user) {
        show("spinner");
        urlParams.set("email", user.email); // In future, pin using the UID.
        wrapper.init(getURLForAction("force_auth", urlParams));
      } else {
        show("spinner");
        wrapper.init(getURLForAction("signup", urlParams));
      }
      break;
    case "manage":
      if (user) {
        show("spinner");
        urlParams.set("email", user.email); // In future, pin using the UID.
        wrapper.init(getURLForAction("settings", urlParams));
      } else {
        show("spinner");
        wrapper.init(getURLForAction("signup", urlParams));
      }
      break;
    case "avatar":
      if (user) {
        show("spinner");
        urlParams.set("email", user.email); // In future, pin using the UID.
        wrapper.init(getURLForAction("settings/avatar/change", urlParams));
      } else {
        show("spinner");
        wrapper.init(getURLForAction("signup", urlParams));
      }
      break;
    default:
      // Unrecognized or no action specified.
      if (action) {
        log.w("Ignoring unrecognized action: " + action);
      }
      if (user) {
        show("prefs");
      } else {
        show("spinner");
        wrapper.init(getURLForAction("signup", urlParams));
      }
      break;
    }
  }).catch(e => {
    log.e("Failed to get the signed in user: " + e.toString());
  });
}

document.addEventListener("DOMContentLoaded", function onload() {
  document.removeEventListener("DOMContentLoaded", onload, true);
  init();
  var buttonRetry = document.getElementById('buttonRetry');
  buttonRetry.addEventListener('click', retry);

  var buttonOpenPrefs = document.getElementById('buttonOpenPrefs');
  buttonOpenPrefs.addEventListener('click', openPrefs);
}, true);

// This window is contained in a XUL <browser> element.  Return the
// messageManager of that <browser> element, or null.
function getBrowserMessageManager() {
  let browser = window
        .QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIWebNavigation)
        .QueryInterface(Ci.nsIDocShellTreeItem)
        .rootTreeItem
        .QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIDOMWindow)
        .QueryInterface(Ci.nsIDOMChromeWindow)
        .BrowserApp
        .getBrowserForDocument(document);
  if (browser) {
    return browser.messageManager;
  }
  return null;
}

// Add a single listener for 'loaded' messages from the iframe in this
// <browser>.  These 'loaded' messages are ferried from the WebChannel to just
// this <browser>.
var mm = getBrowserMessageManager();
if (mm) {
  mm.addMessageListener(COMMAND_LOADED, handleLoadedMessage);
} else {
  log.e('No messageManager, not listening for LOADED message!');
}

window.addEventListener("unload", function(event) {
  try {
    let mm = getBrowserMessageManager();
    if (mm) {
      mm.removeMessageListener(COMMAND_LOADED, handleLoadedMessage);
    }
  } catch (e) {
    // This could fail if the page is being torn down, the tab is being
    // destroyed, etc.
    log.w('Not removing listener for LOADED message: ' + e.toString());
  }
});
