/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { Credentials } = ChromeUtils.import(
  "resource://gre/modules/Credentials.jsm"
);
const { CryptoUtils } = ChromeUtils.import(
  "resource://services-crypto/utils.js"
);

var { hexToBytes: h2b, hexAsString: h2s, bytesAsHex: b2h } = CommonUtils;

// Test vectors for the "onepw" protocol:
// https://github.com/mozilla/fxa-auth-server/wiki/onepw-protocol#wiki-test-vectors
var vectors = {
  "client stretch-KDF": {
    email: h("616e6472c3a94065 78616d706c652e6f 7267"),
    password: h("70c3a4737377c3b6 7264"),
    quickStretchedPW: h(
      "e4e8889bd8bd61ad 6de6b95c059d56e7 b50dacdaf62bd846 44af7e2add84345d"
    ),
    authPW: h(
      "247b675ffb4c4631 0bc87e26d712153a be5e1c90ef00a478 4594f97ef54f2375"
    ),
    authSalt: h(
      "00f0000000000000 0000000000000000 0000000000000000 0000000000000000"
    ),
  },
};

// A simple test suite with no utf8 encoding madness.
add_task(async function test_onepw_setup_credentials() {
  let email = "francine@example.org";
  let password = CommonUtils.encodeUTF8("i like pie");

  let pbkdf2 = CryptoUtils.pbkdf2Generate;
  let hkdf = CryptoUtils.hkdfLegacy;

  // quickStretch the email
  let saltyEmail = Credentials.keyWordExtended("quickStretch", email);

  Assert.equal(
    b2h(saltyEmail),
    "6964656e746974792e6d6f7a696c6c612e636f6d2f7069636c2f76312f717569636b537472657463683a6672616e63696e65406578616d706c652e6f7267"
  );

  let pbkdf2Rounds = 1000;
  let pbkdf2Len = 32;

  let quickStretchedPW = await pbkdf2(
    password,
    saltyEmail,
    pbkdf2Rounds,
    pbkdf2Len
  );
  let quickStretchedActual =
    "6b88094c1c73bbf133223f300d101ed70837af48d9d2c1b6e7d38804b20cdde4";
  Assert.equal(b2h(quickStretchedPW), quickStretchedActual);

  // obtain hkdf info
  let authKeyInfo = Credentials.keyWord("authPW");
  Assert.equal(
    b2h(authKeyInfo),
    "6964656e746974792e6d6f7a696c6c612e636f6d2f7069636c2f76312f617574685057"
  );

  // derive auth password
  let hkdfSalt = h2b("00");
  let hkdfLen = 32;
  let authPW = await hkdf(quickStretchedPW, hkdfSalt, authKeyInfo, hkdfLen);

  Assert.equal(
    b2h(authPW),
    "4b8dec7f48e7852658163601ff766124c312f9392af6c3d4e1a247eb439be342"
  );

  // derive unwrap key
  let unwrapKeyInfo = Credentials.keyWord("unwrapBkey");
  let unwrapKey = await hkdf(
    quickStretchedPW,
    hkdfSalt,
    unwrapKeyInfo,
    hkdfLen
  );

  Assert.equal(
    b2h(unwrapKey),
    "8ff58975be391338e4ec5d7138b5ed7b65c7d1bfd1f3a4f93e05aa47d5b72be9"
  );
});

add_task(async function test_client_stretch_kdf() {
  let expected = vectors["client stretch-KDF"];

  let email = h2s(expected.email);
  let password = h2s(expected.password);

  // Intermediate value from sjcl implementation in fxa-js-client
  // The key thing is the c3a9 sequence in "andré"
  let salt = Credentials.keyWordExtended("quickStretch", email);
  Assert.equal(
    b2h(salt),
    "6964656e746974792e6d6f7a696c6c612e636f6d2f7069636c2f76312f717569636b537472657463683a616e6472c3a9406578616d706c652e6f7267"
  );

  let options = {
    stretchedPassLength: 32,
    pbkdf2Rounds: 1000,
    hkdfSalt: h2b("00"),
    hkdfLength: 32,
  };

  let results = await Credentials.setup(email, password, options);

  Assert.equal(
    expected.quickStretchedPW,
    b2h(results.quickStretchedPW),
    "quickStretchedPW is wrong"
  );

  Assert.equal(expected.authPW, b2h(results.authPW), "authPW is wrong");
});

// End of tests
// Utility functions follow

// turn formatted test vectors into normal hex strings
function h(hexStr) {
  return hexStr.replace(/\s+/g, "");
}
