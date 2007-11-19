/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Johnathan Nightingale <johnath@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


var gDialog;
var gBundleBrand;
var gPKIBundle;
var gSSLStatus;
var gCert;
var gChecking;
var gBroken;
var gNeedReset;

function badCertListener() {}
badCertListener.prototype = {
  getInterface: function (aIID) {
    return this.QueryInterface(aIID);
  },
  QueryInterface: function(aIID) {
    if (aIID.equals(Components.interfaces.nsIBadCertListener2) ||
        aIID.equals(Components.interfaces.nsIInterfaceRequestor) ||
        aIID.equals(Components.interfaces.nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  },  
  handle_test_result: function () {
    if (gSSLStatus)
      gCert = gSSLStatus.QueryInterface(Components.interfaces.nsISSLStatus).serverCert;
  },
  notifyCertProblem: function MSR_notifyCertProblem(socketInfo, sslStatus, targetHost) {
    gBroken = true;
    gSSLStatus = sslStatus;
    this.handle_test_result();
    return true; // suppress error UI
  }
}

function initExceptionDialog() {
  gNeedReset = false;
  gDialog = document.documentElement;
  gBundleBrand = srGetStrBundle("chrome://branding/locale/brand.properties");
  gPKIBundle = srGetStrBundle("chrome://pippki/locale/pippki.properties");

  var brandName = gBundleBrand.GetStringFromName("brandShortName");
  
  setText("warningText", gPKIBundle.formatStringFromName("addExceptionBrandedWarning",
                                                         [brandName], 1));
  gDialog.getButton("extra1").disabled = true;
  
  if (window.arguments[0]
      && window.arguments[0].location) {
    // We were pre-seeded with a location.  Populate the location bar, and check
    // the cert
    document.getElementById("locationTextBox").value = window.arguments[0].location;
    checkCert();
  }
  
  // Set out parameter to false by default
  window.arguments[0].exceptionAdded = false;
}

// returns true if found and global status could be set
function findRecentBadCert(uri) {
  try {
    var recentCertsSvc = Components.classes["@mozilla.org/security/recentbadcerts;1"]
                         .getService(Components.interfaces.nsIRecentBadCertsService);
    if (!recentCertsSvc)
      return false;

    var hostWithPort = uri.host + ":" + uri.port;
    gSSLStatus = recentCertsSvc.getRecentBadCert(hostWithPort);
    if (!gSSLStatus)
      return false;

    gCert = gSSLStatus.QueryInterface(Components.interfaces.nsISSLStatus).serverCert;
    if (!gCert)
      return false;

    gBroken = true;
  }
  catch (e) {
    return false;
  }
  updateCertStatus();  
  return true;
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

  // Is the cert already known in the list of recently seen bad certs?
  if (findRecentBadCert(uri) == true)
    return;

  var req = new XMLHttpRequest();
  try {
    if(uri) {
      req.open('GET', uri.prePath, false);
      req.channel.notificationCallbacks = new badCertListener();
      req.send(null);
    }
  } catch (e) {
    // We *expect* exceptions if there are problems with the certificate
    // presented by the site.  Log it, just in case, but we can proceed here,
    // with appropriate sanity checks
    Components.utils.reportError(e);
  } finally {
    gChecking = false;
  }
      
  if(req.channel && req.channel.securityInfo) {
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
  // Use fixup service instead of just ioservice's newURI since it's quite likely
  // that the host will be supplied without a protocol prefix, resulting in malformed
  // uri exceptions being thrown.
  var fus = Components.classes["@mozilla.org/docshell/urifixup;1"]
                      .getService(Components.interfaces.nsIURIFixup);
  var uri = fus.createFixupURI(document.getElementById("locationTextBox").value, 0);
  
  if(!uri)
    return null;

  if(uri.scheme == "http")
    uri.scheme = "https";

  if (uri.port == -1)
    uri.port = 443;

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
  if(gCert) {
    if(gBroken) { 
      var mms = "addExceptionDomainMismatchShort";
      var mml = "addExceptionDomainMismatchLong";
      var exs = "addExceptionExpiredShort";
      var exl = "addExceptionExpiredLong";
      var uts = "addExceptionUnverifiedShort";
      var utl = "addExceptionUnverifiedLong";
      var use1 = false;
      if (gSSLStatus.isDomainMismatch) {
        use1 = true;
        shortDesc = mms;
        longDesc  = mml;
      }
      if (gSSLStatus.isNotValidAtThisTime) {
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
        if (!use1) {
          use1 = true;
          shortDesc = uts;
          longDesc  = utl;
        }
        else if (!use2) {
          use2 = true;
          shortDesc2 = uts;
          longDesc2  = utl;
        } 
        else {
          use3 = true;
          shortDesc3 = uts;
          longDesc3  = utl;
        }
      }
      
      // In these cases, we do want to enable the "Add Exception" button
      gDialog.getButton("extra1").disabled = false;
      document.getElementById("permanent").disabled = false;
      setText("headerDescription", gPKIBundle.GetStringFromName("addExceptionInvalidHeader"));
    }
    else {
      shortDesc = "addExceptionValidShort";
      longDesc  = "addExceptionValidLong";
      gDialog.getButton("extra1").disabled = true;
      document.getElementById("permanent").disabled = true;
    }
    
    document.getElementById("viewCertButton").disabled = false;
  }
  else if (gChecking) {
    shortDesc = "addExceptionCheckingShort";
    longDesc  = "addExceptionCheckingLong";
    document.getElementById("viewCertButton").disabled = true;
    gDialog.getButton("extra1").disabled = true;
    document.getElementById("permanent").disabled = true;
  }
  else {
    shortDesc = "addExceptionNoCertShort";
    longDesc  = "addExceptionNoCertLong";
    document.getElementById("viewCertButton").disabled = true;
    gDialog.getButton("extra1").disabled = true;
    document.getElementById("permanent").disabled = true;
  }
  
  setText("statusDescription", gPKIBundle.GetStringFromName(shortDesc));
  setText("statusLongDescription", gPKIBundle.GetStringFromName(longDesc));

  if (use2) {
    setText("status2Description", gPKIBundle.GetStringFromName(shortDesc2));
    setText("status2LongDescription", gPKIBundle.GetStringFromName(longDesc2));
  }

  if (use3) {
    setText("status3Description", gPKIBundle.GetStringFromName(shortDesc3));
    setText("status3LongDescription", gPKIBundle.GetStringFromName(longDesc3));
  }

  gNeedReset = true;
}

/**
 * Handle user request to display certificate details
 */
function viewCertButtonClick() {
  if (gCert)
    viewCertHelper(this, gCert);
    
}

/**
 * Handle user request to add an exception for the specified cert
 */
function addException() {
  if(!gCert || !gSSLStatus)
    return;

  var overrideService = Components.classes["@mozilla.org/security/certoverride;1"]
                                  .getService(Components.interfaces.nsICertOverrideService);
  var flags = 0;
  if(gSSLStatus.isUntrusted)
    flags |= overrideService.ERROR_UNTRUSTED;
  if(gSSLStatus.isDomainMismatch)
    flags |= overrideService.ERROR_MISMATCH;
  if(gSSLStatus.isNotValidAtThisTime)
    flags |= overrideService.ERROR_TIME;
  
  var permanentCheckbox = document.getElementById("permanent");

  overrideService.rememberValidityOverride(
    getURI().hostPort,
    gCert,
    flags,
    !permanentCheckbox.checked);
  
  window.arguments[0].exceptionAdded = true;
  gDialog.acceptDialog();
}
