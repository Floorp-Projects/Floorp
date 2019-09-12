/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { FxAccountsPairingFlow } = ChromeUtils.import(
  "resource://gre/modules/FxAccountsPairing.jsm",
  {}
);
const { EventEmitter } = ChromeUtils.import(
  "resource://gre/modules/EventEmitter.jsm",
  {}
);
const { PromiseUtils } = ChromeUtils.import(
  "resource://gre/modules/PromiseUtils.jsm",
  {}
);
const { CryptoUtils } = ChromeUtils.import(
  "resource://services-crypto/utils.js",
  {}
);
XPCOMUtils.defineLazyModuleGetters(this, {
  jwcrypto: "resource://services-crypto/jwcrypto.jsm",
});
XPCOMUtils.defineLazyGlobalGetters(this, ["URL", "crypto"]);

const CHANNEL_ID = "sW-UA97Q6Dljqen7XRlYPw";
const CHANNEL_KEY = crypto.getRandomValues(new Uint8Array(32));

const SENDER_SUPP = {
  ua: "Firefox Supp",
  city: "Nice",
  region: "PACA",
  country: "France",
  remote: "127.0.0.1",
};
const UID = "abcd";
const EMAIL = "foo@bar.com";
const AVATAR = "https://foo.bar/avatar";
const DISPLAY_NAME = "Foo bar";
const DEVICE_NAME = "Foo's computer";

const PAIR_URI = "https://foo.bar/pair";
const OAUTH_URI = "https://foo.bar/oauth";
const KSYNC = "myksync";
const fxaConfig = {
  promisePairingURI() {
    return PAIR_URI;
  },
  promiseOAuthURI() {
    return OAUTH_URI;
  },
};
const fxAccounts = {
  keys: {
    getScopedKeys(scope) {
      return {
        [scope]: {
          kid: "123456",
          k: KSYNC,
          kty: "oct",
        },
      };
    },
  },
  authorizeOAuthCode() {
    return { code: "mycode", state: "mystate" };
  },
  getSignedInUserProfile() {
    return {
      uid: UID,
      email: EMAIL,
      avatar: AVATAR,
      displayName: DISPLAY_NAME,
    };
  },
};
const weave = {
  Service: { clientsEngine: { localName: DEVICE_NAME } },
};

class MockPairingChannel extends EventTarget {
  get channelId() {
    return CHANNEL_ID;
  }

  get channelKey() {
    return CHANNEL_KEY;
  }

  send(data) {
    this.dispatchEvent(
      new CustomEvent("send", {
        detail: { data },
      })
    );
  }

  simulateIncoming(data) {
    this.dispatchEvent(
      new CustomEvent("message", {
        detail: { data, sender: SENDER_SUPP },
      })
    );
  }

  close() {
    this.closed = true;
  }
}

add_task(async function testFullFlow() {
  const emitter = new EventEmitter();
  const pairingChannel = new MockPairingChannel();
  const pairingUri = await FxAccountsPairingFlow.start({
    emitter,
    pairingChannel,
    fxAccounts,
    fxaConfig,
    weave,
  });
  Assert.equal(
    pairingUri,
    `${PAIR_URI}#channel_id=${CHANNEL_ID}&channel_key=${ChromeUtils.base64URLEncode(
      CHANNEL_KEY,
      { pad: false }
    )}`
  );

  const flow = FxAccountsPairingFlow.get(CHANNEL_ID);

  const promiseSwitchToWebContent = emitter.once("view:SwitchToWebContent");
  const promiseMetadataSent = promiseOutgoingMessage(pairingChannel);
  const epk = await generateEphemeralKeypair();
  pairingChannel.simulateIncoming({
    message: "pair:supp:request",
    data: {
      client_id: "client_id_1",
      state: "mystate",
      keys_jwk: ChromeUtils.base64URLEncode(
        new TextEncoder().encode(JSON.stringify(epk.publicJWK)),
        { pad: false }
      ),
      scope: "profile https://identity.mozilla.com/apps/oldsync",
      code_challenge: "chal",
      code_challenge_method: "S256",
    },
  });
  const sentAuthMetadata = await promiseMetadataSent;
  Assert.deepEqual(sentAuthMetadata, {
    message: "pair:auth:metadata",
    data: {
      email: EMAIL,
      avatar: AVATAR,
      displayName: DISPLAY_NAME,
      deviceName: DEVICE_NAME,
    },
  });
  const oauthUrl = await promiseSwitchToWebContent;
  Assert.equal(
    oauthUrl,
    `${OAUTH_URI}?client_id=client_id_1&scope=profile+https%3A%2F%2Fidentity.mozilla.com%2Fapps%2Foldsync&email=foo%40bar.com&uid=abcd&channel_id=${CHANNEL_ID}&redirect_uri=urn%3Aietf%3Awg%3Aoauth%3A2.0%3Aoob%3Apair-auth-webchannel`
  );

  let pairSuppMetadata = await simulateIncomingWebChannel(
    flow,
    "fxaccounts:pair_supplicant_metadata"
  );
  Assert.deepEqual(
    {
      ua: "Firefox Supp",
      city: "Nice",
      region: "PACA",
      country: "France",
      ipAddress: "127.0.0.1",
    },
    pairSuppMetadata
  );

  const authorizeOAuthCode = sinon.spy(fxAccounts, "authorizeOAuthCode");
  const promiseOAuthParamsMsg = promiseOutgoingMessage(pairingChannel);
  await simulateIncomingWebChannel(flow, "fxaccounts:pair_authorize");
  Assert.ok(authorizeOAuthCode.calledOnce);
  const oauthCodeArgs = authorizeOAuthCode.firstCall.args[0];
  Assert.equal(
    oauthCodeArgs.keys_jwk,
    ChromeUtils.base64URLEncode(
      new TextEncoder().encode(JSON.stringify(epk.publicJWK)),
      { pad: false }
    )
  );
  Assert.equal(oauthCodeArgs.client_id, "client_id_1");
  Assert.equal(oauthCodeArgs.access_type, "offline");
  Assert.equal(oauthCodeArgs.state, "mystate");
  Assert.equal(
    oauthCodeArgs.scope,
    "profile https://identity.mozilla.com/apps/oldsync"
  );
  Assert.equal(oauthCodeArgs.code_challenge, "chal");
  Assert.equal(oauthCodeArgs.code_challenge_method, "S256");
  const oAuthParams = await promiseOAuthParamsMsg;
  Assert.deepEqual(oAuthParams, {
    message: "pair:auth:authorize",
    data: { code: "mycode", state: "mystate" },
  });
  let heartbeat = await simulateIncomingWebChannel(
    flow,
    "fxaccounts:pair_heartbeat"
  );
  Assert.ok(!heartbeat.suppAuthorized);
  await pairingChannel.simulateIncoming({
    message: "pair:supp:authorize",
  });
  heartbeat = await simulateIncomingWebChannel(
    flow,
    "fxaccounts:pair_heartbeat"
  );
  Assert.ok(heartbeat.suppAuthorized);
  await simulateIncomingWebChannel(flow, "fxaccounts:pair_complete");
  // The flow should have been destroyed!
  Assert.ok(!FxAccountsPairingFlow.get(CHANNEL_ID));
  Assert.ok(pairingChannel.closed);
  fxAccounts.authorizeOAuthCode.restore();
});

add_task(async function testUnknownPairingMessage() {
  const emitter = new EventEmitter();
  const pairingChannel = new MockPairingChannel();
  await FxAccountsPairingFlow.start({
    emitter,
    pairingChannel,
    fxAccounts,
    fxaConfig,
    weave,
  });
  const flow = FxAccountsPairingFlow.get(CHANNEL_ID);
  const viewErrorObserved = emitter.once("view:Error");
  pairingChannel.simulateIncoming({
    message: "pair:boom",
  });
  await viewErrorObserved;
  let heartbeat = await simulateIncomingWebChannel(
    flow,
    "fxaccounts:pair_heartbeat"
  );
  Assert.ok(heartbeat.err);
});

add_task(async function testUnknownWebChannelCommand() {
  const emitter = new EventEmitter();
  const pairingChannel = new MockPairingChannel();
  await FxAccountsPairingFlow.start({
    emitter,
    pairingChannel,
    fxAccounts,
    fxaConfig,
    weave,
  });
  const flow = FxAccountsPairingFlow.get(CHANNEL_ID);
  const viewErrorObserved = emitter.once("view:Error");
  await simulateIncomingWebChannel(flow, "fxaccounts:boom");
  await viewErrorObserved;
  let heartbeat = await simulateIncomingWebChannel(
    flow,
    "fxaccounts:pair_heartbeat"
  );
  Assert.ok(heartbeat.err);
});

add_task(async function testPairingChannelFailure() {
  const emitter = new EventEmitter();
  const pairingChannel = new MockPairingChannel();
  await FxAccountsPairingFlow.start({
    emitter,
    pairingChannel,
    fxAccounts,
    fxaConfig,
    weave,
  });
  const flow = FxAccountsPairingFlow.get(CHANNEL_ID);
  const viewErrorObserved = emitter.once("view:Error");
  sinon.stub(pairingChannel, "send").callsFake(() => {
    throw new Error("Boom!");
  });
  pairingChannel.simulateIncoming({
    message: "pair:supp:request",
    data: {
      client_id: "client_id_1",
      state: "mystate",
      scope: "profile https://identity.mozilla.com/apps/oldsync",
      code_challenge: "chal",
      code_challenge_method: "S256",
    },
  });
  await viewErrorObserved;

  let heartbeat = await simulateIncomingWebChannel(
    flow,
    "fxaccounts:pair_heartbeat"
  );
  Assert.ok(heartbeat.err);
});

add_task(async function testFlowTimeout() {
  const emitter = new EventEmitter();
  const pairingChannel = new MockPairingChannel();
  const viewErrorObserved = emitter.once("view:Error");
  await FxAccountsPairingFlow.start({
    emitter,
    pairingChannel,
    fxAccounts,
    fxaConfig,
    weave,
    flowTimeout: 1,
  });
  const flow = FxAccountsPairingFlow.get(CHANNEL_ID);
  await viewErrorObserved;

  let heartbeat = await simulateIncomingWebChannel(
    flow,
    "fxaccounts:pair_heartbeat"
  );
  Assert.ok(heartbeat.err.match(/Timeout/));
});

async function simulateIncomingWebChannel(flow, command) {
  return flow.onWebChannelMessage(command);
}

async function promiseOutgoingMessage(pairingChannel) {
  return new Promise(res => {
    const onMessage = event => {
      pairingChannel.removeEventListener("send", onMessage);
      res(event.detail.data);
    };
    pairingChannel.addEventListener("send", onMessage);
  });
}

async function generateEphemeralKeypair() {
  const keypair = await crypto.subtle.generateKey(
    { name: "ECDH", namedCurve: "P-256" },
    true,
    ["deriveKey"]
  );
  const publicJWK = await crypto.subtle.exportKey("jwk", keypair.publicKey);
  const privateJWK = await crypto.subtle.exportKey("jwk", keypair.privateKey);
  delete publicJWK.key_ops;
  return {
    publicJWK,
    privateJWK,
  };
}
