/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* import-globals-from pippki.js */
"use strict";

var gDialog;
var gBundleBrand;
var gPKIBundle;
var gSSLStatus;
var gCert;
var gChecking;
var gBroken;
var gNeedReset;
var gSecHistogram;
var gNsISecTel;

Components.utils.import("resource://gre/modules/PrivateBrowsingUtils.jsm");

function badCertListener() {}
badCertListener.prototype = {
  getInterface: function (aIID) {
    return this.QueryInterface(aIID);
  },
  QueryInterface: function(aIID) {
    if (aIID.equals(Components.interfaces.nsIBadCertListener2) ||
        aIID.equals(Components.interfaces.nsIInterfaceRequestor) ||
        aIID.equals(Components.interfaces.nsISupports)) {
      return this;
    }

    throw new Error(Components.results.NS_ERROR_NO_INTERFACE);
  },
  handle_test_result: function () {
    if (gSSLStatus) {
      gCert = gSSLStatus.QueryInterface(Components.interfaces.nsISSLStatus).serverCert;
    }
  },
  notifyCertProblem: function MSR_notifyCertProblem(socketInfo, sslStatus, targetHost) {
    gBroken = true;
    gSSLStatus = sslStatus;
    this.handle_test_result();
    return true; // suppress error UI
  }
};

function initExceptionDialog() {
  gNeedReset = false;
  gDialog = document.documentElement;
  gBundleBrand = document.getElementById("brand_bundle");
  gPKIBundle = document.getElementById("pippki_bundle");
  gSecHistogram = Components.classes["@mozilla.org/base/telemetry;1"].
                    getService(Components.interfaces.nsITelemetry).
                    getHistogramById("SECURITY_UI");
  gNsISecTel = Components.interfaces.nsISecurityUITelemetry;

  var brandName = gBundleBrand.getString("brandShortName");
  setText("warningText", gPKIBundle.getFormattedString("addExceptionBrandedWarning2", [brandName]));
  gDialog.getButton("extra1").disabled = true;

  var args = window.arguments;
  if (args && args[0]) {
    if (args[0].location) {
      // We were pre-seeded with a location.
      document.getElementById("locationTextBox").value = args[0].location;
      document.getElementById('checkCertButton').disabled = false;

      if (args[0].sslStatus) {
        gSSLStatus = args[0].sslStatus;
        gCert = gSSLStatus.serverCert;
        gBroken = true;
        updateCertStatus();
      } else if (args[0].prefetchCert) {
        // We can optionally pre-fetch the certificate too.  Don't do this
        // synchronously, since it would prevent the window from appearing
        // until the fetch is completed, which could be multiple seconds.
        // Instead, let's use a timer to spawn the actual fetch, but update
        // the dialog to "checking..." state right away, so that the UI
        // is appropriately responsive.  Bug 453855
        document.getElementById("checkCertButton").disabled = true;
        gChecking = true;
        updateCertStatus();

        window.setTimeout(checkCert, 0);
      }
    }

    // Set out parameter to false by default
    args[0].exceptionAdded = false;
  }
}

/**
 * Attempt to download the certificate for the location specified, and populate
 * the Certificate Status section with the result.
 */
function checkCert() {
  gCert = null;
  gSSLStatus = null;
  gChecking = true;
  gBroken = false;
  updateCertStatus();

  var uri = getURI();

  var req = new XMLHttpRequest();
  try {
    if (uri) {
      req.open('GET', uri.prePath, false);
      req.channel.notificationCallbacks = new badCertListener();
      req.send(null);
    }
  } catch (e) {
    // We *expect* exceptions if there are problems with the certificate
    // presented by the site.  Log it, just in case, but we can proceed here,
    // with appropriate sanity checks
    Components.utils.reportError("Attempted to connect to a site with a bad certificate in the add exception dialog. " +
                                 "This results in a (mostly harmless) exception being thrown. " +
                                 "Logged for information purposes only: " + e);
  } finally {
    gChecking = false;
  }

  if (req.channel && req.channel.securityInfo) {
    const Ci = Components.interfaces;
    gSSLStatus = req.channel.securityInfo
                    .QueryInterface(Ci.nsISSLStatusProvider).SSLStatus;
    gCert = gSSLStatus.QueryInterface(Ci.nsISSLStatus).serverCert;
  }

  updateCertStatus();
}

/**
 * Build and return a URI, based on the information supplied in the
 * Certificate Location fields
 */
function getURI() {
  // Use fixup service instead of just ioservice's newURI since it's quite
  // likely that the host will be supplied without a protocol prefix, resulting
  // in malformed uri exceptions being thrown.
  let fus = Components.classes["@mozilla.org/docshell/urifixup;1"]
                      .getService(Components.interfaces.nsIURIFixup);
  let locationTextBox = document.getElementById("locationTextBox");
  let uri = fus.createFixupURI(locationTextBox.value, 0);

  if (!uri) {
    return null;
  }

  if (uri.scheme == "http") {
    uri.scheme = "https";
  }

  if (uri.port == -1) {
    uri.port = 443;
  }

  return uri;
}

function resetDialog() {
  document.getElementById("viewCertButton").disabled = true;
  document.getElementById("permanent").disabled = true;
  gDialog.getButton("extra1").disabled = true;
  setText("headerDescription", "");
  setText("statusDescription", "");
  setText("statusLongDescription", "");
  setText("status2Description", "");
  setText("status2LongDescription", "");
  setText("status3Description", "");
  setText("status3LongDescription", "");
}

/**
 * Called by input textboxes to manage UI state
 */
function handleTextChange() {
  var checkCertButton = document.getElementById('checkCertButton');
  checkCertButton.disabled = !(document.getElementById("locationTextBox").value);
  if (gNeedReset) {
    gNeedReset = false;
    resetDialog();
  }
}

function updateCertStatus() {
  var shortDesc, longDesc;
  var shortDesc2, longDesc2;
  var shortDesc3, longDesc3;
  var use2 = false;
  var use3 = false;
  let bucketId = gNsISecTel.WARNING_BAD_CERT_TOP_ADD_EXCEPTION_BASE;
  if (gCert) {
    if (gBroken) {
      var mms = "addExceptionDomainMismatchShort";
      var mml = "addExceptionDomainMismatchLong2";
      var exs = "addExceptionExpiredShort";
      var exl = "addExceptionExpiredLong2";
      var uts = "addExceptionUnverifiedOrBadSignatureShort";
      var utl = "addExceptionUnverifiedOrBadSignatureLong2";
      var use1 = false;
      if (gSSLStatus.isDomainMismatch) {
        bucketId += gNsISecTel.WARNING_BAD_CERT_TOP_ADD_EXCEPTION_FLAG_DOMAIN;
        use1 = true;
        shortDesc = mms;
        longDesc  = mml;
      }
      if (gSSLStatus.isNotValidAtThisTime) {
        bucketId += gNsISecTel.WARNING_BAD_CERT_TOP_ADD_EXCEPTION_FLAG_TIME;
        if (!use1) {
          use1 = true;
          shortDesc = exs;
          longDesc  = exl;
        }
        else {
          use2 = true;
          shortDesc2 = exs;
          longDesc2  = exl;
        }
      }
      if (gSSLStatus.isUntrusted) {
        bucketId += gNsISecTel.WARNING_BAD_CERT_TOP_ADD_EXCEPTION_FLAG_UNTRUSTED;
        if (!use1) {
          use1 = true;
          shortDesc = uts;
          longDesc  = utl;
        } else if (!use2) {
          use2 = true;
          shortDesc2 = uts;
          longDesc2  = utl;
        } else {
          use3 = true;
          shortDesc3 = uts;
          longDesc3  = utl;
        }
      }
      gSecHistogram.add(bucketId);

      // In these cases, we do want to enable the "Add Exception" button
      gDialog.getButton("extra1").disabled = false;

      // If the Private Browsing service is available and the mode is active,
      // don't store permanent exceptions, since they would persist after
      // private browsing mode was disabled.
      var inPrivateBrowsing = inPrivateBrowsingMode();
      var pe = document.getElementById("permanent");
      pe.disabled = inPrivateBrowsing;
      pe.checked = !inPrivateBrowsing;

      setText("headerDescription", gPKIBundle.getString("addExceptionInvalidHeader"));
    }
    else {
      shortDesc = "addExceptionValidShort";
      longDesc  = "addExceptionValidLong";
      gDialog.getButton("extra1").disabled = true;
      document.getElementById("permanent").disabled = true;
    }

    // We're done checking the certificate, so allow the user to check it again.
    document.getElementById("checkCertButton").disabled = false;
    document.getElementById("viewCertButton").disabled = false;

    // Notify observers about the availability of the certificate
    Components.classes["@mozilla.org/observer-service;1"]
              .getService(Components.interfaces.nsIObserverService)
              .notifyObservers(null, "cert-exception-ui-ready", null);
  }
  else if (gChecking) {
    shortDesc = "addExceptionCheckingShort";
    longDesc  = "addExceptionCheckingLong2";
    // We're checking the certificate, so we disable the Get Certificate
    // button to make sure that the user can't interrupt the process and
    // trigger another certificate fetch.
    document.getElementById("checkCertButton").disabled = true;
    document.getElementById("viewCertButton").disabled = true;
    gDialog.getButton("extra1").disabled = true;
    document.getElementById("permanent").disabled = true;
  }
  else {
    shortDesc = "addExceptionNoCertShort";
    longDesc  = "addExceptionNoCertLong2";
    // We're done checking the certificate, so allow the user to check it again.
    document.getElementById("checkCertButton").disabled = false;
    document.getElementById("viewCertButton").disabled = true;
    gDialog.getButton("extra1").disabled = true;
    document.getElementById("permanent").disabled = true;
  }

  setText("statusDescription", gPKIBundle.getString(shortDesc));
  setText("statusLongDescription", gPKIBundle.getString(longDesc));

  if (use2) {
    setText("status2Description", gPKIBundle.getString(shortDesc2));
    setText("status2LongDescription", gPKIBundle.getString(longDesc2));
  }

  if (use3) {
    setText("status3Description", gPKIBundle.getString(shortDesc3));
    setText("status3LongDescription", gPKIBundle.getString(longDesc3));
  }

  gNeedReset = true;
}

/**
 * Handle user request to display certificate details
 */
function viewCertButtonClick() {
  gSecHistogram.add(gNsISecTel.WARNING_BAD_CERT_TOP_CLICK_VIEW_CERT);
  if (gCert) {
    viewCertHelper(this, gCert);
  }
}

/**
 * Handle user request to add an exception for the specified cert
 */
function addException() {
  if (!gCert || !gSSLStatus) {
    return;
  }

  var overrideService = Components.classes["@mozilla.org/security/certoverride;1"]
                                  .getService(Components.interfaces.nsICertOverrideService);
  var flags = 0;
  let confirmBucketId = gNsISecTel.WARNING_BAD_CERT_TOP_CONFIRM_ADD_EXCEPTION_BASE;
  if (gSSLStatus.isUntrusted) {
    flags |= overrideService.ERROR_UNTRUSTED;
    confirmBucketId += gNsISecTel.WARNING_BAD_CERT_TOP_CONFIRM_ADD_EXCEPTION_FLAG_UNTRUSTED;
  }
  if (gSSLStatus.isDomainMismatch) {
    flags |= overrideService.ERROR_MISMATCH;
    confirmBucketId += gNsISecTel.WARNING_BAD_CERT_TOP_CONFIRM_ADD_EXCEPTION_FLAG_DOMAIN;
  }
  if (gSSLStatus.isNotValidAtThisTime) {
    flags |= overrideService.ERROR_TIME;
    confirmBucketId += gNsISecTel.WARNING_BAD_CERT_TOP_CONFIRM_ADD_EXCEPTION_FLAG_TIME;
  }

  var permanentCheckbox = document.getElementById("permanent");
  var shouldStorePermanently = permanentCheckbox.checked && !inPrivateBrowsingMode();
  if (!permanentCheckbox.checked) {
    gSecHistogram.add(gNsISecTel.WARNING_BAD_CERT_TOP_DONT_REMEMBER_EXCEPTION);
  }

  gSecHistogram.add(confirmBucketId);
  var uri = getURI();
  overrideService.rememberValidityOverride(
    uri.asciiHost, uri.port,
    gCert,
    flags,
    !shouldStorePermanently);

  let args = window.arguments;
  if (args && args[0]) {
    args[0].exceptionAdded = true;
  }

  gDialog.acceptDialog();
}

/**
 * Returns true if this dialog is in private browsing mode.
 */
function inPrivateBrowsingMode() {
  return PrivateBrowsingUtils.isWindowPrivate(window);
}
