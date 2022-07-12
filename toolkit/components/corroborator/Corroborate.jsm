/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyServiceGetters(lazy, {
  gCertDB: ["@mozilla.org/security/x509certdb;1", "nsIX509CertDB"],
});

var EXPORTED_SYMBOLS = ["Corroborate"];

/**
 * Tools for verifying internal files in Mozilla products.
 */
const Corroborate = {
  async init() {},

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
      lazy.gCertDB.openSignedAppFileAsync(
        root,
        file,
        (rv, _zipReader, cert) => {
          resolve(
            Components.isSuccessCode(rv) &&
              cert.organizationalUnit === expectedOrganizationalUnit
          );
        }
      );
    });
  },
};
