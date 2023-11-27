/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* global crypto */

"use strict";

const { FxAccountsOAuth } = ChromeUtils.importESModule(
  "resource://gre/modules/FxAccountsOAuth.sys.mjs"
);

const { SCOPE_PROFILE, FX_OAUTH_CLIENT_ID } = ChromeUtils.importESModule(
  "resource://gre/modules/FxAccountsCommon.sys.mjs"
);

ChromeUtils.defineESModuleGetters(this, {
  jwcrypto: "resource://services-crypto/jwcrypto.sys.mjs",
});

initTestLogging("Trace");

add_task(function test_begin_oauth_flow() {
  const oauth = new FxAccountsOAuth();
  add_task(async function test_begin_oauth_flow_invalid_scopes() {
    try {
      await oauth.beginOAuthFlow("foo,fi,fum", "foo");
      Assert.fail("Should have thrown error, scopes must be an array");
    } catch {
      // OK
    }
    try {
      await oauth.beginOAuthFlow(["not-a-real-scope", SCOPE_PROFILE]);
      Assert.fail("Should have thrown an error, must use a valid scope");
    } catch {
      // OK
    }
  });
  add_task(async function test_begin_oauth_flow_ok() {
    const scopes = [SCOPE_PROFILE, SCOPE_OLD_SYNC];
    const queryParams = await oauth.beginOAuthFlow(scopes);

    // First verify default query parameters
    Assert.equal(queryParams.client_id, FX_OAUTH_CLIENT_ID);
    Assert.equal(queryParams.action, "email");
    Assert.equal(queryParams.response_type, "code");
    Assert.equal(queryParams.access_type, "offline");
    Assert.equal(queryParams.scope, [SCOPE_PROFILE, SCOPE_OLD_SYNC].join(" "));

    // Then, we verify that the state is a valid Base64 value
    const state = queryParams.state;
    ChromeUtils.base64URLDecode(state, { padding: "reject" });

    // Then, we verify that the codeVerifier, can be used to verify the code_challenge
    const code_challenge = queryParams.code_challenge;
    Assert.equal(queryParams.code_challenge_method, "S256");
    const oauthFlow = oauth.getFlow(state);
    const codeVerifierB64 = oauthFlow.verifier;
    const expectedChallenge = await crypto.subtle.digest(
      "SHA-256",
      new TextEncoder().encode(codeVerifierB64)
    );
    const expectedChallengeB64 = ChromeUtils.base64URLEncode(
      expectedChallenge,
      { pad: false }
    );
    Assert.equal(expectedChallengeB64, code_challenge);

    // Then, we verify that something encrypted with the `keys_jwk`, can be decrypted using the private key
    const keysJwk = queryParams.keys_jwk;
    const decodedKeysJwk = JSON.parse(
      new TextDecoder().decode(
        ChromeUtils.base64URLDecode(keysJwk, { padding: "reject" })
      )
    );
    const plaintext = "text to be encrypted and decrypted!";
    delete decodedKeysJwk.key_ops;
    const jwe = await jwcrypto.generateJWE(
      decodedKeysJwk,
      new TextEncoder().encode(plaintext)
    );
    const privateKey = oauthFlow.key;
    const decrypted = await jwcrypto.decryptJWE(jwe, privateKey);
    Assert.equal(new TextDecoder().decode(decrypted), plaintext);

    // Finally, we verify that we stored the requested scopes
    Assert.deepEqual(oauthFlow.requestedScopes, scopes);
  });
});
