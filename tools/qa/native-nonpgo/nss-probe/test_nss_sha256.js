/* SPDX-License-Identifier: MPL-2.0 */
"use strict";

add_task(function nss_random_and_sha256_known_answer() {
  const random = Cc["@mozilla.org/security/random-generator;1"].createInstance(
    Ci.nsIRandomGenerator
  );
  Assert.strictEqual(
    random.generateRandomBytes(32).length,
    32,
    "The same built xpcshell initializes NSS and uses its internal PKCS11 slot"
  );
  const hash = Cc["@mozilla.org/security/hash;1"].createInstance(Ci.nsICryptoHash);
  hash.initWithString("sha256");
  hash.update([97, 98, 99], 3);
  const actual = Array.from(hash.finish(false), character =>
    character.charCodeAt(0).toString(16).padStart(2, "0")
  ).join("");
  Assert.strictEqual(
    actual,
    "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
    "The independent Rust SHA-256 implementation computes its known answer"
  );
});
