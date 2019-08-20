/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { FileUtils } = ChromeUtils.import(
  "resource://gre/modules/FileUtils.jsm"
);
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
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
    const appOmniJar = FileUtils.getFile("XCurProcD", [
      AppConstants.OMNIJAR_NAME,
    ]);
    const greOmniJar = FileUtils.getFile("GreD", [AppConstants.OMNIJAR_NAME]);

    let corruptOmnijar = true;
    // If an omni jar is missing, we consider that corrupt. Firefox could be running with
    // an omni jar unpacked, but it would never be signed correctly in that case so there
    // isn't a point checking further.
    if (
      (await OS.File.exists(appOmniJar.path)) &&
      (await OS.File.exists(greOmniJar.path))
    ) {
      corruptOmnijar = !(
        (await this.verifyJar(appOmniJar)) && (await this.verifyJar(greOmniJar))
      );
    }

    Services.telemetry.scalarSet(
      "corroborate.omnijar_corrupted",
      corruptOmnijar
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
