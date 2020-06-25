/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["AboutCertViewerParent"];

const TYPE_UNKNOWN = 0;
const TYPE_CA = 1;
const TYPE_USER = 2;
const TYPE_EMAIL = 4;
const TYPE_SERVER = 8;

class AboutCertViewerParent extends JSWindowActorParent {
  getCertificates() {
    let certs = {
      [TYPE_UNKNOWN]: [],
      [TYPE_CA]: [],
      [TYPE_USER]: [],
      [TYPE_EMAIL]: [],
      [TYPE_SERVER]: [],
    };
    let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
      Ci.nsIX509CertDB
    );
    let certcache = certdb.getCerts();
    for (let cert of certcache) {
      for (let certType of Object.keys(certs).map(Number)) {
        if (certType & cert.certType) {
          certs[certType].push({
            displayName: cert.displayName,
            derb64: cert.getBase64DERString(),
          });
        }
      }
    }
    return certs;
  }

  receiveMessage(aMessage) {
    switch (aMessage.name) {
      case "getCertificates":
        return this.getCertificates();
    }

    return undefined;
  }
}
