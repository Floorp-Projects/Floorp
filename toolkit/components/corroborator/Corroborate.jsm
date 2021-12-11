/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyServiceGetters(this, {
  gCertDB: ["@mozilla.org/security/x509certdb;1", "nsIX509CertDB"],
});

var EXPORTED_SYMBOLS = ["Corroborate"];

/**
 * Tools for verifying internal files in Mozilla products.
 */
this.Corroborate = {
  async init() {
    // Check whether libxul's build ID matches the one in the GRE omni jar.
    // As above, Firefox could be running with an omni jar unpacked, in which
    // case we're really just checking that the version in the unpacked
    // AppConstants.jsm matches libxul.
    let mismatchedOmnijar =
      Services.appinfo.platformBuildID != AppConstants.MOZ_BUILDID;

    Services.telemetry.scalarSet(
      "corroborate.omnijar_mismatch",
      mismatchedOmnijar
    );
  },

  /**
   * Verify signed state of arbitrary JAR file. Currently only JAR files signed
   * with Mozilla-internal keys are supported.
   *
   * @argument file - an nsIFile pointing to the JAR to verify.
   *
   * @returns {Promise} - resolves true if file exists and is valid, false otherwise.
   *                      Never rejects.
   */
  verifyJar(file) {
    let root = Ci.nsIX509CertDB.AddonsPublicRoot;
    let expectedOrganizationalUnit = "Mozilla Components";

    return new Promise(resolve => {
      gCertDB.openSignedAppFileAsync(root, file, (rv, _zipReader, cert) => {
        resolve(
          Components.isSuccessCode(rv) &&
            cert.organizationalUnit === expectedOrganizationalUnit
        );
      });
    });
  },
};
