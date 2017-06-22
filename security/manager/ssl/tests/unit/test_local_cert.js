/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const certService = Cc["@mozilla.org/security/local-cert-service;1"]
                    .getService(Ci.nsILocalCertService);

const gNickname = "local-cert-test";

function run_test() {
  // Need profile dir to store the key / cert
  do_get_profile();
  // Ensure PSM is initialized
  Cc["@mozilla.org/psm;1"].getService(Ci.nsISupports);
  run_next_test();
}

function getOrCreateCert(nickname) {
  return new Promise((resolve, reject) => {
    certService.getOrCreateCert(nickname, {
      handleCert(c, rv) {
        if (rv) {
          reject(rv);
          return;
        }
        resolve(c);
      }
    });
  });
}

function removeCert(nickname) {
  return new Promise((resolve, reject) => {
    certService.removeCert(nickname, {
      handleResult(rv) {
        if (rv) {
          reject(rv);
          return;
        }
        resolve();
      }
    });
  });
}

add_task(async function() {
  // No master password, so no prompt required here
  ok(!certService.loginPromptRequired);

  let certA = await getOrCreateCert(gNickname);
  // The local cert service implementation takes the given nickname and uses it
  // as the common name for the certificate it creates. nsIX509Cert.displayName
  // uses the common name if it is present, so these should match. Should either
  // implementation change to do something else, this won't necessarily work.
  equal(certA.displayName, gNickname);

  // Getting again should give the same cert
  let certB = await getOrCreateCert(gNickname);
  equal(certB.displayName, gNickname);

  // Should be matching instances
  ok(certA.equals(certB));

  // Check a few expected attributes
  ok(certA.isSelfSigned);
  equal(certA.certType, Ci.nsIX509Cert.USER_CERT);

  // New nickname should give a different cert
  let diffNameCert = await getOrCreateCert("cool-stuff");
  ok(!diffNameCert.equals(certA));

  // Remove the cert, and get a new one again
  await removeCert(gNickname);
  let newCert = await getOrCreateCert(gNickname);
  ok(!newCert.equals(certA));

  // Drop all cert references and GC
  let serial = newCert.serialNumber;
  certA = certB = diffNameCert = newCert = null;
  Cu.forceGC();
  Cu.forceCC();

  // Should still get the same cert back
  let certAfterGC = await getOrCreateCert(gNickname);
  equal(certAfterGC.serialNumber, serial);
});
