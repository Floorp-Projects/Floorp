/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

XPCOMUtils.defineLazyServiceGetters(lazy, {
  gCertDB: ["@mozilla.org/security/x509certdb;1", "nsIX509CertDB"],
});

/**
 * Tools for verifying internal files in Mozilla products.
 */
export const Corroborate = {
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
        (rv, _zipReader, signatureInfos) => {
          // aSignatureInfos is an array of nsIAppSignatureInfo.
          // This implementation could be modified to iterate through the array to
          // determine if one or all of the verified signatures used a satisfactory
          // algorithm and signing certificate.
          // For now, though, it maintains existing behavior by inspecting the
          // first signing certificate encountered.
          resolve(
            Components.isSuccessCode(rv) &&
              signatureInfos.length &&
              signatureInfos[0].signerCert.organizationalUnit ==
                expectedOrganizationalUnit
          );
        }
      );
    });
  },
};
