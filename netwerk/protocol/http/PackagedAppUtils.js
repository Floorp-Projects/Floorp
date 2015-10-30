/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const PACKAGEDAPPUTILS_CONTRACTID = "@mozilla.org/network/packaged-app-utils;1";
const PACKAGEDAPPUTILS_CID = Components.ID("{fe8f1c2e-3c13-11e5-9a3f-bbf47d1e6697}");

function PackagedAppUtils() {
  this.packageIdentifier = '';
  this.packageOrigin = '';
}

var DEBUG = 0
function debug(s) {
  if (DEBUG) {
    dump("-*- PackagedAppUtils: " + s + "\n");
  }
}

PackagedAppUtils.prototype = {
  classID: PACKAGEDAPPUTILS_CID,
  contractID: PACKAGEDAPPUTILS_CONTRACTID,
  classDescription: "Packaged App Utils",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPackagedAppUtils]),

  verifyManifest: function(aHeader, aManifest, aCallback, aDeveloperMode) {
    debug("Manifest: " + aManifest);

    // parse signature from header
    let signature;
    const signatureField = "manifest-signature: ";
    for (let item of aHeader.split('\r\n')) {
      if (item.substr(0, signatureField.length) == signatureField) {
        signature = item.substr(signatureField.length);
        break;
      }
    }
    if (!signature) {
      debug("No signature in header");
      aCallback.fireVerifiedEvent(true, false);
      return;
    }
    debug("Signature: " + signature);

    try {
      // Remove header
      let manifestBody = aManifest.substr(aManifest.indexOf('\r\n\r\n') + 4);
      debug("manifestBody: " + manifestBody);

      // Parse manifest, store resource hashes
      let manifestObj = JSON.parse(manifestBody);
      this.packageIdentifier = manifestObj["package-identifier"];
      this.packageOrigin = manifestObj["moz-package-origin"];
      this.resources = manifestObj["moz-resources"];

      // Base64 decode
      signature = atob(signature);
    } catch (e) {
      debug("Manifest parsing failure");
      aCallback.fireVerifiedEvent(true, false);
      return;
    }

    let manifestStream = Cc["@mozilla.org/io/string-input-stream;1"]
                           .createInstance(Ci.nsIStringInputStream);
    let signatureStream = Cc["@mozilla.org/io/string-input-stream;1"]
                            .createInstance(Ci.nsIStringInputStream);
    manifestStream.setData(aManifest, aManifest.length);
    signatureStream.setData(signature, signature.length);

    let certDb;
    try {
      certDb = Cc["@mozilla.org/security/x509certdb;1"]
                 .getService(Ci.nsIX509CertDB);
    } catch (e) {
      debug("nsIX509CertDB error: " + e);
      // unrecoverable error, don't bug the user
      throw "CERTDB_ERROR";
    }

    let trustedRoot = aDeveloperMode ? Ci.nsIX509CertDB.DeveloperImportedRoot
                                     : Ci.nsIX509CertDB.PrivilegedPackageRoot;

    certDb.verifySignedManifestAsync(trustedRoot, manifestStream, signatureStream,
      function(aRv, aCert) {
        aCallback.fireVerifiedEvent(true, Components.isSuccessCode(aRv));
      });
  },

  checkIntegrity: function(aFileName, aHashValue, aCallback) {
    debug("checkIntegrity() " + aFileName + ": " + aHashValue + "\n");
    if (!this.resources) {
      debug("resource hashes not found");
      aCallback.fireVerifiedEvent(false, false);
      return;
    }
    for (let r of this.resources) {
      if (r.src === aFileName) {
        debug("found integrity = " + r.integrity);
        aCallback.fireVerifiedEvent(false, r.integrity === aHashValue);
        return;
      }
    }
    aCallback.fireVerifiedEvent(false, false);
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([PackagedAppUtils]);
