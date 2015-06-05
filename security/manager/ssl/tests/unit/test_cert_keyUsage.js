/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

do_get_profile(); // must be called before getting nsIX509CertDB
let certdb = Cc["@mozilla.org/security/x509certdb;1"]
               .getService(Ci.nsIX509CertDB);

let caList = ["ca-no-keyUsage-extension", "ca-missing-keyCertSign",
              "ca-all-usages"];
let eeList = ["ee-no-keyUsage-extension", "ee-keyCertSign-only",
              "ee-keyEncipherment-only", "ee-keyCertSign-and-keyEncipherment"];

let caUsage = "SSL CA";
let allEEUsages = "Client,Server,Sign,Encrypt,Object Signer";
let serverEEUsages = "Server,Encrypt";

let expectedUsagesMap = {
  "ca-no-keyUsage-extension": caUsage,
  "ca-missing-keyCertSign": "",
  "ca-all-usages": caUsage,

  "ee-no-keyUsage-extension-ca-no-keyUsage-extension": allEEUsages,
  "ee-no-keyUsage-extension-ca-missing-keyCertSign": "",
  "ee-no-keyUsage-extension-ca-all-usages": allEEUsages,

  "ee-keyCertSign-only-ca-no-keyUsage-extension": "",
  "ee-keyCertSign-only-ca-missing-keyCertSign": "",
  "ee-keyCertSign-only-ca-all-usages": "",

  "ee-keyEncipherment-only-ca-no-keyUsage-extension": serverEEUsages,
  "ee-keyEncipherment-only-ca-missing-keyCertSign": "",
  "ee-keyEncipherment-only-ca-all-usages": serverEEUsages,

  "ee-keyCertSign-and-keyEncipherment-ca-no-keyUsage-extension": serverEEUsages,
  "ee-keyCertSign-and-keyEncipherment-ca-missing-keyCertSign": "",
  "ee-keyCertSign-and-keyEncipherment-ca-all-usages": serverEEUsages,
};

function run_test() {
  caList.forEach(function(ca) {
    addCertFromFile(certdb, "test_cert_keyUsage/" + ca + ".pem",
                    "CTu,CTu,CTu");
    let caCert = certdb.findCertByNickname(null, ca);
    let usages = {};
    caCert.getUsagesString(true, {}, usages); // true indicates local-only
    equal(usages.value, expectedUsagesMap[ca],
          "Actual and expected CA usages should match");
    eeList.forEach(function(ee) {
      let eeFullName = ee + "-" + ca;
      let cert = constructCertFromFile(
        "test_cert_keyUsage/" + eeFullName + ".pem");
      cert.getUsagesString(true, {}, usages); // true indicates local-only
      equal(usages.value, expectedUsagesMap[eeFullName],
            "Actual and expected EE usages should match");
    });
  });
}
