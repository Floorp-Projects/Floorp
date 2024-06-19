/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { FxAccountsKeys } = ChromeUtils.importESModule(
  "resource://gre/modules/FxAccountsKeys.sys.mjs"
);

// Ref https://github.com/mozilla/fxa-crypto-relier/ for the details
// of these test vectors.

add_task(async function test_derive_scoped_key_test_vector() {
  const keys = new FxAccountsKeys(null);
  const uid = "aeaa1725c7a24ff983c6295725d5fc9b";
  const kB = "8b2e1303e21eee06a945683b8d495b9bf079ca30baa37eb8392d9ffa4767be45";
  const scopedKeyMetadata = {
    identifier: "app_key:https%3A//example.com",
    keyRotationTimestamp: 1510726317000,
    keyRotationSecret:
      "517d478cb4f994aa69930416648a416fdaa1762c5abf401a2acf11a0f185e98d",
  };

  const scopedKey = await keys._deriveScopedKey(
    uid,
    CommonUtils.hexToBytes(kB),
    "app_key",
    scopedKeyMetadata
  );

  Assert.deepEqual(scopedKey, {
    kty: "oct",
    kid: "1510726317-Voc-Eb9IpoTINuo9ll7bjA",
    k: "Kkbk1_Q0oCcTmggeDH6880bQrxin2RLu5D00NcJazdQ",
  });
});

add_task(async function test_derive_legacy_sync_key_test_vector() {
  const keys = new FxAccountsKeys(null);
  const uid = "aeaa1725c7a24ff983c6295725d5fc9b";
  const kB = "eaf9570b7219a4187d3d6bf3cec2770c2e0719b7cc0dfbb38243d6f1881675e9";
  const scopedKeyMetadata = {
    identifier: SCOPE_APP_SYNC,
    keyRotationTimestamp: 1510726317123,
    keyRotationSecret:
      "0000000000000000000000000000000000000000000000000000000000000000",
  };

  const scopedKey = await keys._deriveLegacyScopedKey(
    uid,
    CommonUtils.hexToBytes(kB),
    SCOPE_APP_SYNC,
    scopedKeyMetadata
  );

  Assert.deepEqual(scopedKey, {
    kty: "oct",
    kid: "1510726317123-IqQv4onc7VcVE1kTQkyyOw",
    k: "DW_ll5GwX6SJ5GPqJVAuMUP2t6kDqhUulc2cbt26xbTcaKGQl-9l29FHAQ7kUiJETma4s9fIpEHrt909zgFang",
  });
});

add_task(async function test_derive_multiple_keys_at_once() {
  const keys = new FxAccountsKeys(null);
  const uid = "aeaa1725c7a24ff983c6295725d5fc9b";
  const kB = "eaf9570b7219a4187d3d6bf3cec2770c2e0719b7cc0dfbb38243d6f1881675e9";
  const scopedKeysMetadata = {
    app_key: {
      identifier: "app_key:https%3A//example.com",
      keyRotationTimestamp: 1510726317000,
      keyRotationSecret:
        "517d478cb4f994aa69930416648a416fdaa1762c5abf401a2acf11a0f185e98d",
    },
    [SCOPE_APP_SYNC]: {
      identifier: SCOPE_APP_SYNC,
      keyRotationTimestamp: 1510726318123,
      keyRotationSecret:
        "0000000000000000000000000000000000000000000000000000000000000000",
    },
  };

  const scopedKeys = await keys._deriveScopedKeys(
    uid,
    CommonUtils.hexToBytes(kB),
    scopedKeysMetadata
  );

  Assert.deepEqual(scopedKeys, {
    app_key: {
      kty: "oct",
      kid: "1510726317-tUkxiR1lTlFrTgkF0tJidA",
      k: "TYK6Hmj86PfKiqsk9DZmX61nxk9VsExGrwo94HP-0wU",
    },
    [SCOPE_APP_SYNC]: {
      kty: "oct",
      kid: "1510726318123-IqQv4onc7VcVE1kTQkyyOw",
      k: "DW_ll5GwX6SJ5GPqJVAuMUP2t6kDqhUulc2cbt26xbTcaKGQl-9l29FHAQ7kUiJETma4s9fIpEHrt909zgFang",
    },
  });
});

add_task(function test_check_valid_scoped_keys() {
  const keys = new FxAccountsKeys(null);
  add_task(function test_missing_key_data() {
    const scopedKeys = {
      [SCOPE_APP_SYNC]: {
        kty: "oct",
        kid: "1510726318123-IqQv4onc7VcVE1kTQkyyOw",
        scope: SCOPE_APP_SYNC,
      },
    };
    Assert.equal(keys.validScopedKeys(scopedKeys), false);
  });
  add_task(function test_unexpected_scope() {
    const scopedKeys = {
      [SCOPE_APP_SYNC]: {
        kty: "oct",
        kid: "1510726318123-IqQv4onc7VcVE1kTQkyyOw",
        k: "DW_ll5GwX6SJ5GPqJVAuMUP2t6kDqhUulc2cbt26xbTcaKGQl-9l29FHAQ7kUiJETma4s9fIpEHrt909zgFang",
        scope: "UnexpectedScope",
      },
    };
    Assert.equal(keys.validScopedKeys(scopedKeys), false);
  });
  add_task(function test_not_oct_key() {
    const scopedKeys = {
      [SCOPE_APP_SYNC]: {
        // Should be "oct"!
        kty: "EC",
        kid: "1510726318123-IqQv4onc7VcVE1kTQkyyOw",
        k: "DW_ll5GwX6SJ5GPqJVAuMUP2t6kDqhUulc2cbt26xbTcaKGQl-9l29FHAQ7kUiJETma4s9fIpEHrt909zgFang",
        scope: SCOPE_APP_SYNC,
      },
    };
    Assert.equal(keys.validScopedKeys(scopedKeys), false);
  });
  add_task(function test_invalid_kid_not_timestamp() {
    const scopedKeys = {
      [SCOPE_APP_SYNC]: {
        kty: "oct",
        // Does not have the timestamp!
        kid: "IqQv4onc7VcVE1kTQkyyOw",
        k: "DW_ll5GwX6SJ5GPqJVAuMUP2t6kDqhUulc2cbt26xbTcaKGQl-9l29FHAQ7kUiJETma4s9fIpEHrt909zgFang",
        scope: SCOPE_APP_SYNC,
      },
    };
    Assert.equal(keys.validScopedKeys(scopedKeys), false);
  });
  add_task(function test_invalid_kid_not_valid_timestamp() {
    const scopedKeys = {
      [SCOPE_APP_SYNC]: {
        kty: "oct",
        // foo is not a valid timestamp!
        kid: "foo-IqQv4onc7VcVE1kTQkyyOw",
        k: "DW_ll5GwX6SJ5GPqJVAuMUP2t6kDqhUulc2cbt26xbTcaKGQl-9l29FHAQ7kUiJETma4s9fIpEHrt909zgFang",
        scope: SCOPE_APP_SYNC,
      },
    };
    Assert.equal(keys.validScopedKeys(scopedKeys), false);
  });
  add_task(function test_invalid_kid_not_b64_fingerprint() {
    const scopedKeys = {
      [SCOPE_APP_SYNC]: {
        kty: "oct",
        // fingerprint not a valid base64 encoded string.
        kid: "1510726318123-notvalidb64][",
        k: "DW_ll5GwX6SJ5GPqJVAuMUP2t6kDqhUulc2cbt26xbTcaKGQl-9l29FHAQ7kUiJETma4s9fIpEHrt909zgFang",
        scope: SCOPE_APP_SYNC,
      },
    };
    Assert.equal(keys.validScopedKeys(scopedKeys), false);
  });
  add_task(function test_invalid_k_not_base64() {
    const scopedKeys = {
      [SCOPE_APP_SYNC]: {
        kty: "oct",
        kid: "1510726318123-IqQv4onc7VcVE1kTQkyyOw",
        k: "notavalidb64[]",
        scope: SCOPE_APP_SYNC,
      },
    };
    Assert.equal(keys.validScopedKeys(scopedKeys), false);
  });

  add_task(function test_multiple_scoped_keys_one_invalid() {
    const scopedKeys = {
      // Valid
      "https://identity.mozilla.com/apps/otherscope": {
        kty: "oct",
        kid: "1510726318123-IqQv4onc7VcVE1kTQkyyOw",
        k: "DW_ll5GwX6SJ5GPqJVAuMUP2t6kDqhUulc2cbt26xbTcaKGQl-9l29FHAQ7kUiJETma4s9fIpEHrt909zgFang",
        scope: "https://identity.mozilla.com/apps/otherscope",
      },
      // Invalid
      [SCOPE_APP_SYNC]: {
        kty: "oct",
        kid: "1510726318123-IqQv4onc7VcVE1kTQkyyOw",
        k: "notavalidb64[]",
        scope: SCOPE_APP_SYNC,
      },
    };
    Assert.equal(keys.validScopedKeys(scopedKeys), false);
  });

  add_task(function test_valid_scopedkeys() {
    const scopedKeys = {
      [SCOPE_APP_SYNC]: {
        kty: "oct",
        kid: "1510726318123-IqQv4onc7VcVE1kTQkyyOw",
        k: "DW_ll5GwX6SJ5GPqJVAuMUP2t6kDqhUulc2cbt26xbTcaKGQl-9l29FHAQ7kUiJETma4s9fIpEHrt909zgFang",
        scope: SCOPE_APP_SYNC,
      },
      "https://identity.mozilla.com/apps/otherscope": {
        kty: "oct",
        kid: "1510726318123-IqQv4onc7VcVE1kTQkyyOw",
        k: "DW_ll5GwX6SJ5GPqJVAuMUP2t6kDqhUulc2cbt26xbTcaKGQl-9l29FHAQ7kUiJETma4s9fIpEHrt909zgFang",
        scope: "https://identity.mozilla.com/apps/otherscope",
      },
    };
    Assert.equal(keys.validScopedKeys(scopedKeys), true);
  });
  add_task(function test_valid_kid_with_dash() {
    const scopedKeys = {
      "https://identity.mozilla.com/apps/oldsync": {
        kty: "oct",
        // kid contains another dash. The fingerprint must not be truncated.
        kid: "1510726318123-I-Qv4onc7VcVE1kTQkyyOw",
        k: "DW_ll5GwX6SJ5GPqJVAuMUP2t6kDqhUulc2cbt26xbTcaKGQl-9l29FHAQ7kUiJETma4s9fIpEHrt909zgFang",
        scope: "https://identity.mozilla.com/apps/oldsync",
      },
    };
    Assert.equal(keys.validScopedKeys(scopedKeys), true);
  });
});

add_task(async function test_rejects_bad_scoped_key_data() {
  const keys = new FxAccountsKeys(null);
  const uid = "aeaa1725c7a24ff983c6295725d5fc9b";
  const kB = "8b2e1303e21eee06a945683b8d495b9bf079ca30baa37eb8392d9ffa4767be45";
  const scopedKeyMetadata = {
    identifier: "app_key:https%3A//example.com",
    keyRotationTimestamp: 1510726317000,
    keyRotationSecret:
      "517d478cb4f994aa69930416648a416fdaa1762c5abf401a2acf11a0f185e98d",
  };

  await Assert.rejects(
    keys._deriveScopedKey(
      uid.slice(0, -1),
      CommonUtils.hexToBytes(kB),
      "app_key",
      scopedKeyMetadata
    ),
    /uid must be a 32-character hex string/
  );
  await Assert.rejects(
    keys._deriveScopedKey(
      uid.slice(0, -1) + "Q",
      CommonUtils.hexToBytes(kB),
      "app_key",
      scopedKeyMetadata
    ),
    /uid must be a 32-character hex string/
  );
  await Assert.rejects(
    keys._deriveScopedKey(
      uid,
      CommonUtils.hexToBytes(kB).slice(0, -1),
      "app_key",
      scopedKeyMetadata
    ),
    /kBbytes must be exactly 32 bytes/
  );
  await Assert.rejects(
    keys._deriveScopedKey(uid, CommonUtils.hexToBytes(kB), "app_key", {
      ...scopedKeyMetadata,
      identifier: "foo",
    }),
    /identifier must be a string of length >= 10/
  );
  await Assert.rejects(
    keys._deriveScopedKey(uid, CommonUtils.hexToBytes(kB), "app_key", {
      ...scopedKeyMetadata,
      identifier: {},
    }),
    /identifier must be a string of length >= 10/
  );
  await Assert.rejects(
    keys._deriveScopedKey(uid, CommonUtils.hexToBytes(kB), "app_key", {
      ...scopedKeyMetadata,
      keyRotationTimestamp: "xyz",
    }),
    /keyRotationTimestamp must be a number/
  );
  await Assert.rejects(
    keys._deriveScopedKey(uid, CommonUtils.hexToBytes(kB), "app_key", {
      ...scopedKeyMetadata,
      keyRotationTimestamp: 12345,
    }),
    /keyRotationTimestamp must round to a 10-digit number/
  );
  await Assert.rejects(
    keys._deriveScopedKey(uid, CommonUtils.hexToBytes(kB), "app_key", {
      ...scopedKeyMetadata,
      keyRotationSecret: scopedKeyMetadata.keyRotationSecret.slice(0, -1),
    }),
    /keyRotationSecret must be a 64-character hex string/
  );
  await Assert.rejects(
    keys._deriveScopedKey(uid, CommonUtils.hexToBytes(kB), "app_key", {
      ...scopedKeyMetadata,
      keyRotationSecret: scopedKeyMetadata.keyRotationSecret.slice(0, -1) + "z",
    }),
    /keyRotationSecret must be a 64-character hex string/
  );
});
