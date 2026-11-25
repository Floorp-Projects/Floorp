// SPDX-License-Identifier: MPL-2.0
/// <reference lib="deno.ns" />

import { assert, assertEquals } from "@std/assert";
import {
  FEATURE_PACKAGE_DOMAIN,
  FEATURE_PACKAGE_MAX_BYTES,
  FEATURE_PACKAGE_SOURCE_MAX_BYTES,
  FeaturePackageError,
  signFeaturePackage,
  verifySignedFeaturePackage,
} from "../../browser-features/modules/modules/feature-host/package/FeaturePackageVerifier.ts";
import type {
  FeaturePackageAuthoringMetadata,
  FeaturePackageManifest,
  FeaturePackagePolicy,
} from "../../browser-features/modules/modules/feature-host/package/types.ts";

const encoder = new TextEncoder();
const alphabet =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
const keyPair = await crypto.subtle.generateKey({ name: "Ed25519" }, true, [
  "sign",
  "verify",
]) as CryptoKeyPair;
const publicBytes = new Uint8Array(
  await crypto.subtle.exportKey("raw", keyPair.publicKey),
);
const publicKey = encodeBase64Url(publicBytes);
const policy: FeaturePackagePolicy = {
  featureId: "hub-panel-menu",
  hostSchema: 2,
  hostBuildId: "floorp-host-1",
  runtimeBuildId: "runtime-1",
  allowedCapabilities: ["hub-panel"],
  minimumReleaseSequence: 1,
  trustedKeys: [{ keyId: "test-key", publicKey }],
};

function encodeBase64Url(bytes: Uint8Array): string {
  let binary = "";
  for (const byte of bytes) binary += String.fromCharCode(byte);
  return btoa(binary).replaceAll("+", "-").replaceAll("/", "_").replace(
    /=+$/,
    "",
  );
}

function hex(bytes: Uint8Array): string {
  return Array.from(bytes, (byte) => byte.toString(16).padStart(2, "0")).join(
    "",
  );
}

function arrayBuffer(bytes: Uint8Array): ArrayBuffer {
  const buffer = new ArrayBuffer(bytes.byteLength);
  new Uint8Array(buffer).set(bytes);
  return buffer;
}

function metadata(
  overrides: Partial<FeaturePackageAuthoringMetadata> = {},
): FeaturePackageAuthoringMetadata {
  return {
    featureId: "hub-panel-menu",
    version: "v1.0",
    releaseSequence: 3,
    compatibility: {
      hostSchema: 2,
      hostBuildId: "floorp-host-1",
      runtimeBuildIds: ["runtime-1"],
      capabilities: ["hub-panel"],
    },
    ...overrides,
  };
}

function policyWith(
  overrides: Partial<FeaturePackagePolicy> = {},
): FeaturePackagePolicy {
  return { ...policy, ...overrides };
}

async function signed(
  source = "const exact = 'quoted';\r\n",
  authoring = metadata(),
  keyId = "test-key",
): Promise<Uint8Array> {
  return await signFeaturePackage(authoring, source, keyId, keyPair.privateKey);
}

async function signedManifest(
  source: string,
  overrides: Partial<FeaturePackageManifest> = {},
  keyId = "test-key",
): Promise<Uint8Array> {
  const sourceBytes = encoder.encode(source);
  const compatibility = overrides.compatibility ?? metadata().compatibility;
  const manifest = {
    featureId: "hub-panel-menu",
    version: "v1.0",
    releaseSequence: 3,
    sourceBytes: sourceBytes.length,
    sourceSha256: hex(
      new Uint8Array(
        await crypto.subtle.digest("SHA-256", arrayBuffer(sourceBytes)),
      ),
    ),
    compatibility,
    ...overrides,
  };
  const header = JSON.stringify({ formatVersion: 1, keyId, manifest });
  const signingBytes = encoder.encode(FEATURE_PACKAGE_DOMAIN + header);
  const signature = encodeBase64Url(
    new Uint8Array(
      await crypto.subtle.sign(
        { name: "Ed25519" },
        keyPair.privateKey,
        arrayBuffer(signingBytes),
      ),
    ),
  );
  return encoder.encode(
    JSON.stringify({ formatVersion: 1, keyId, manifest, source, signature }),
  );
}

async function rejectsWith(
  operation: Promise<unknown>,
  expectedCode: string,
): Promise<void> {
  let caught: unknown;
  try {
    await operation;
  } catch (error: unknown) {
    caught = error;
  }
  assert(
    caught instanceof FeaturePackageError,
    "operation rejects with bounded package error",
  );
  assertEquals(caught.code, expectedCode);
  assert(caught.message.length < 128, "error message is bounded");
}

async function rejectsWithFeaturePackageError(
  operation: Promise<unknown>,
): Promise<FeaturePackageError> {
  let caught: unknown;
  try {
    await operation;
  } catch (error: unknown) {
    caught = error;
  }
  assert(
    caught instanceof FeaturePackageError,
    "operation rejects with bounded package error",
  );
  assert(caught.message.length < 128, "error message is bounded");
  return caught;
}

async function withUnavailableCrypto(
  operation: () => Promise<void>,
): Promise<void> {
  const descriptor = Object.getOwnPropertyDescriptor(globalThis, "crypto");
  try {
    Object.defineProperty(globalThis, "crypto", {
      configurable: true,
      value: undefined,
    });
    await operation();
  } finally {
    if (descriptor) Object.defineProperty(globalThis, "crypto", descriptor);
    else Reflect.deleteProperty(globalThis, "crypto");
  }
}

function deferred<T>(): {
  promise: Promise<T>;
  resolve(value: T): void;
  reject(reason?: unknown): void;
} {
  let resolve!: (value: T) => void;
  let reject!: (reason?: unknown) => void;
  const promise = new Promise<T>((resolvePromise, rejectPromise) => {
    resolve = resolvePromise;
    reject = rejectPromise;
  });
  return { promise, resolve, reject };
}

async function eventLoopTurn(): Promise<void> {
  await new Promise<void>((resolve) => setTimeout(resolve, 0));
}

Deno.test("signed package accepts exact UTF-8 source and returns detached immutable metadata", async () => {
  const source =
    'globalThis.__packageSourceWasRun = true;\r\nπ\r\nconst value = `quote: \\" slash: \\\\ control: \u0001`;\n';
  const bytes = await signed(source);
  const result = await verifySignedFeaturePackage(bytes, policy);
  assertEquals(result.source, source);
  assertEquals(result.manifest.sourceBytes, encoder.encode(source).length);
  assertEquals(result.keyId, "test-key");
  assertEquals(result.packageSha256.length, 64);
  assert(Object.isFrozen(result));
  assert(Object.isFrozen(result.manifest));
  assert(Object.isFrozen(result.manifest.compatibility));
  assert(Object.isFrozen(result.manifest.compatibility.runtimeBuildIds));
  assert(Object.isFrozen(result.manifest.compatibility.capabilities));
  assertEquals(
    (globalThis as Record<string, unknown>).__packageSourceWasRun,
    undefined,
  );
});

Deno.test("signer and verifier bound unavailable and failing crypto", async () => {
  const bytes = await signed();
  await withUnavailableCrypto(async () => {
    await rejectsWith(
      signFeaturePackage(metadata(), "source", "test-key", keyPair.privateKey),
      "unsupported-crypto",
    );
    await rejectsWith(
      verifySignedFeaturePackage(bytes, policy),
      "unsupported-crypto",
    );
  });

  const subtle = globalThis.crypto.subtle;
  const digestDescriptor = Object.getOwnPropertyDescriptor(subtle, "digest");
  const failingDigest: SubtleCrypto["digest"] = () =>
    Promise.reject(new Error("unbounded crypto failure"));
  try {
    Object.defineProperty(subtle, "digest", {
      configurable: true,
      value: failingDigest,
    });
    await rejectsWith(
      signFeaturePackage(metadata(), "source", "test-key", keyPair.privateKey),
      "unsupported-crypto",
    );
    await rejectsWith(
      verifySignedFeaturePackage(bytes, policy),
      "unsupported-crypto",
    );
  } finally {
    if (digestDescriptor) {
      Object.defineProperty(subtle, "digest", digestDescriptor);
    } else {
      Reflect.deleteProperty(subtle, "digest");
    }
  }
});

Deno.test("verifier drains pending crypto siblings after early rejection", async () => {
  const bytes = await signed();
  const subtle = globalThis.crypto.subtle;
  const digestDescriptor = Object.getOwnPropertyDescriptor(subtle, "digest");
  const importKeyDescriptor = Object.getOwnPropertyDescriptor(
    subtle,
    "importKey",
  );
  const pendingDigest = deferred<ArrayBuffer>();
  const pendingImport = deferred<CryptoKey>();
  let digestCalls = 0;
  const patchedDigest: SubtleCrypto["digest"] = () => {
    digestCalls++;
    return digestCalls === 1
      ? Promise.reject(new Error("early digest rejection"))
      : pendingDigest.promise;
  };
  const patchedImport: SubtleCrypto["importKey"] = () => pendingImport.promise;
  try {
    Object.defineProperty(subtle, "digest", {
      configurable: true,
      value: patchedDigest,
    });
    Object.defineProperty(subtle, "importKey", {
      configurable: true,
      value: patchedImport,
    });
    const pending = verifySignedFeaturePackage(bytes, policy);
    let settled = false;
    void pending.then(
      () => settled = true,
      () => settled = true,
    );
    await eventLoopTurn();
    assertEquals(digestCalls, 2);
    assertEquals(settled, false);
    pendingDigest.resolve(new ArrayBuffer(32));
    await eventLoopTurn();
    assertEquals(settled, false);
    pendingImport.resolve(keyPair.publicKey);
    await rejectsWith(pending, "unsupported-crypto");
    assertEquals(settled, true);
  } finally {
    if (digestDescriptor) {
      Object.defineProperty(subtle, "digest", digestDescriptor);
    } else {
      Reflect.deleteProperty(subtle, "digest");
    }
    if (importKeyDescriptor) {
      Object.defineProperty(subtle, "importKey", importKeyDescriptor);
    } else {
      Reflect.deleteProperty(subtle, "importKey");
    }
  }
});

Deno.test("verifier drains started crypto when a later launch throws synchronously", async () => {
  const bytes = await signed();
  const subtle = globalThis.crypto.subtle;
  const digestDescriptor = Object.getOwnPropertyDescriptor(subtle, "digest");
  const importKeyDescriptor = Object.getOwnPropertyDescriptor(
    subtle,
    "importKey",
  );
  const pendingDigests = [deferred<ArrayBuffer>(), deferred<ArrayBuffer>()];
  let digestCalls = 0;
  const patchedDigest: SubtleCrypto["digest"] = () => {
    const deferredDigest = pendingDigests[digestCalls++];
    assert(deferredDigest, "both digest operations started before importKey");
    return deferredDigest.promise;
  };
  const patchedImport: SubtleCrypto["importKey"] = () => {
    throw new Error("synchronous import failure");
  };
  try {
    Object.defineProperty(subtle, "digest", {
      configurable: true,
      value: patchedDigest,
    });
    Object.defineProperty(subtle, "importKey", {
      configurable: true,
      value: patchedImport,
    });
    const pending = verifySignedFeaturePackage(bytes, policy);
    let settled = false;
    void pending.then(
      () => settled = true,
      () => settled = true,
    );
    assertEquals(digestCalls, 2);
    await eventLoopTurn();
    assertEquals(settled, false);
    pendingDigests[0].resolve(new ArrayBuffer(32));
    await eventLoopTurn();
    assertEquals(settled, false);
    pendingDigests[1].resolve(new ArrayBuffer(32));
    await rejectsWith(pending, "unsupported-crypto");
    assertEquals(settled, true);
  } finally {
    if (digestDescriptor) {
      Object.defineProperty(subtle, "digest", digestDescriptor);
    } else {
      Reflect.deleteProperty(subtle, "digest");
    }
    if (importKeyDescriptor) {
      Object.defineProperty(subtle, "importKey", importKeyDescriptor);
    } else {
      Reflect.deleteProperty(subtle, "importKey");
    }
  }
});

Deno.test("verifier maps detached and throwing byte views to bounded errors", async () => {
  const bytes = await signed();
  const detachedBuffer = new ArrayBuffer(bytes.byteLength);
  const detachedView = new Uint8Array(detachedBuffer);
  detachedView.set(bytes);
  structuredClone(detachedBuffer, { transfer: [detachedBuffer] });
  await rejectsWithFeaturePackageError(
    verifySignedFeaturePackage(detachedView, policy),
  );

  const throwingView = bytes.slice();
  Object.defineProperty(throwingView, "byteLength", {
    configurable: true,
    get() {
      throw new Error("unbounded view failure");
    },
  });
  await rejectsWith(
    verifySignedFeaturePackage(throwingView, policy),
    "invalid-envelope",
  );

  let arrayLikeLengthRead = false;
  const arrayLikeByteLength = {
    get length() {
      arrayLikeLengthRead = true;
      return FEATURE_PACKAGE_MAX_BYTES + 1;
    },
  };
  const arrayLikeView = bytes.slice();
  Object.defineProperty(arrayLikeView, "byteLength", {
    configurable: true,
    get: () => arrayLikeByteLength,
  });
  await rejectsWith(
    verifySignedFeaturePackage(arrayLikeView, policy),
    "invalid-envelope",
  );
  assertEquals(
    arrayLikeLengthRead,
    false,
    "non-numeric byteLength is rejected before using its array-like value",
  );
});

Deno.test("signed package rejects source, manifest, signature and key-id tampering", async () => {
  const bytes = await signed("globalThis.__packageSourceWasRun = true;");
  const envelope = JSON.parse(new TextDecoder().decode(bytes)) as Record<
    string,
    unknown
  >;
  envelope.source = "changed";
  await rejectsWith(
    verifySignedFeaturePackage(
      encoder.encode(JSON.stringify(envelope)),
      policy,
    ),
    "source-digest-mismatch",
  );

  const manifestTamper = JSON.parse(new TextDecoder().decode(bytes)) as Record<
    string,
    unknown
  >;
  (manifestTamper.manifest as Record<string, unknown>).version = "v2";
  await rejectsWith(
    verifySignedFeaturePackage(
      encoder.encode(JSON.stringify(manifestTamper)),
      policy,
    ),
    "invalid-signature",
  );

  const signatureTamper = JSON.parse(new TextDecoder().decode(bytes)) as Record<
    string,
    unknown
  >;
  const signature = String(signatureTamper.signature);
  signatureTamper.signature = (signature[0] === "A" ? "B" : "A") +
    signature.slice(1);
  await rejectsWith(
    verifySignedFeaturePackage(
      encoder.encode(JSON.stringify(signatureTamper)),
      policy,
    ),
    "invalid-signature",
  );

  const keyIdTamper = JSON.parse(new TextDecoder().decode(bytes)) as Record<
    string,
    unknown
  >;
  keyIdTamper.keyId = "other-key";
  const twoIds = policyWith({
    trustedKeys: [...policy.trustedKeys, { keyId: "other-key", publicKey }],
  });
  await rejectsWith(
    verifySignedFeaturePackage(
      encoder.encode(JSON.stringify(keyIdTamper)),
      twoIds,
    ),
    "invalid-signature",
  );
  await rejectsWith(
    verifySignedFeaturePackage(bytes, policyWith({ trustedKeys: [] })),
    "untrusted-key",
  );
  assertEquals(
    (globalThis as Record<string, unknown>).__packageSourceWasRun,
    undefined,
  );
});

Deno.test("signed package enforces signed feature, host, runtime, capability and sequence policy", async () => {
  const valid = await signed();
  await rejectsWith(
    verifySignedFeaturePackage(
      valid,
      policyWith({ featureId: "other-feature" }),
    ),
    "feature-mismatch",
  );
  await rejectsWith(
    verifySignedFeaturePackage(valid, policyWith({ hostSchema: 3 })),
    "host-incompatible",
  );
  await rejectsWith(
    verifySignedFeaturePackage(valid, policyWith({ hostBuildId: "different" })),
    "host-incompatible",
  );
  await rejectsWith(
    verifySignedFeaturePackage(
      valid,
      policyWith({ runtimeBuildId: "different" }),
    ),
    "runtime-incompatible",
  );
  await rejectsWith(
    verifySignedFeaturePackage(valid, policyWith({ allowedCapabilities: [] })),
    "capability-denied",
  );
  await rejectsWith(
    verifySignedFeaturePackage(
      valid,
      policyWith({ minimumReleaseSequence: 4 }),
    ),
    "sequence-too-low",
  );

  await rejectsWith(
    verifySignedFeaturePackage(
      await signed("source", metadata({ featureId: "different-feature" })),
      policy,
    ),
    "feature-mismatch",
  );
  await rejectsWith(
    verifySignedFeaturePackage(
      await signed(
        "source",
        metadata({
          compatibility: { ...metadata().compatibility, hostSchema: 3 },
        }),
      ),
      policy,
    ),
    "host-incompatible",
  );
  await rejectsWith(
    verifySignedFeaturePackage(
      await signed(
        "source",
        metadata({
          compatibility: {
            ...metadata().compatibility,
            hostBuildId: "another-host",
          },
        }),
      ),
      policy,
    ),
    "host-incompatible",
  );
  await rejectsWith(
    verifySignedFeaturePackage(
      await signed(
        "source",
        metadata({
          compatibility: {
            ...metadata().compatibility,
            runtimeBuildIds: ["other"],
          },
        }),
      ),
      policy,
    ),
    "runtime-incompatible",
  );
  await rejectsWith(
    verifySignedFeaturePackage(
      await signed(
        "source",
        metadata({
          compatibility: {
            ...metadata().compatibility,
            capabilities: ["network"],
          },
        }),
      ),
      policy,
    ),
    "capability-denied",
  );
  await rejectsWith(
    verifySignedFeaturePackage(
      await signed("source", metadata({ releaseSequence: 1 })),
      policyWith({ minimumReleaseSequence: 2 }),
    ),
    "sequence-too-low",
  );
});

Deno.test("signed package rejects noncanonical, malformed UTF-8 and unsupported envelopes", async () => {
  const bytes = await signed("x");
  const text = new TextDecoder().decode(bytes);
  const envelope = JSON.parse(text) as Record<string, unknown>;
  await rejectsWith(
    verifySignedFeaturePackage(encoder.encode(` ${text}`), policy),
    "invalid-envelope",
  );
  await rejectsWith(
    verifySignedFeaturePackage(
      encoder.encode(text.replace('"source":"x"', '"source":"\\u0078"')),
      policy,
    ),
    "invalid-envelope",
  );
  await rejectsWith(
    verifySignedFeaturePackage(
      encoder.encode(
        text.replace(
          '"formatVersion":1,',
          '"formatVersion":1,"formatVersion":1,',
        ),
      ),
      policy,
    ),
    "invalid-envelope",
  );
  await rejectsWith(
    verifySignedFeaturePackage(
      encoder.encode(JSON.stringify({ ...envelope, extra: true })),
      policy,
    ),
    "invalid-envelope",
  );
  const unknownManifest = JSON.parse(text) as Record<string, unknown>;
  (unknownManifest.manifest as Record<string, unknown>).unexpected = true;
  await rejectsWith(
    verifySignedFeaturePackage(
      encoder.encode(JSON.stringify(unknownManifest)),
      policy,
    ),
    "invalid-envelope",
  );
  const unknownCompatibility = JSON.parse(text) as Record<string, unknown>;
  const unknownCompatibilityManifest = unknownCompatibility.manifest as Record<
    string,
    unknown
  >;
  unknownCompatibilityManifest.compatibility = {
    ...(unknownCompatibilityManifest.compatibility as Record<string, unknown>),
    unexpected: true,
  };
  await rejectsWith(
    verifySignedFeaturePackage(
      encoder.encode(JSON.stringify(unknownCompatibility)),
      policy,
    ),
    "invalid-envelope",
  );
  await rejectsWith(
    verifySignedFeaturePackage(
      encoder.encode(
        JSON.stringify({
          source: envelope.source,
          formatVersion: 1,
          keyId: envelope.keyId,
          manifest: envelope.manifest,
          signature: envelope.signature,
        }),
      ),
      policy,
    ),
    "invalid-envelope",
  );
  await rejectsWith(
    verifySignedFeaturePackage(
      encoder.encode(text.replace('"formatVersion":1', '"formatVersion":2')),
      policy,
    ),
    "unsupported-version",
  );
  await rejectsWith(
    verifySignedFeaturePackage(
      new Uint8Array([0xef, 0xbb, 0xbf, ...bytes]),
      policy,
    ),
    "invalid-envelope",
  );
  await rejectsWith(
    verifySignedFeaturePackage(new Uint8Array([0xc3, 0x28]), policy),
    "invalid-envelope",
  );
  await rejectsWith(
    verifySignedFeaturePackage(
      new Uint8Array(FEATURE_PACKAGE_MAX_BYTES + 1),
      policy,
    ),
    "size-limit",
  );
  await rejectsWith(
    verifySignedFeaturePackage(
      encoder.encode(text.replace('"source":"x"', '"source":"\\ud800"')),
      policy,
    ),
    "invalid-source",
  );
});

Deno.test(
  "signed package rejects noncanonical nested manifest and compatibility properties",
  async () => {
    const bytes = await signed("source");
    const text = new TextDecoder().decode(bytes);
    const original = JSON.parse(text) as Record<string, unknown>;
    await verifySignedFeaturePackage(bytes, policy);

    const cases = [
      {
        name: "manifest property reordering",
        wire: text.replace(
          '"featureId":"hub-panel-menu","version":"v1.0"',
          '"version":"v1.0","featureId":"hub-panel-menu"',
        ),
      },
      {
        name: "duplicate manifest property",
        wire: text.replace(
          '"featureId":"hub-panel-menu",',
          '"featureId":"hub-panel-menu","featureId":"hub-panel-menu",',
        ),
      },
      {
        name: "compatibility property reordering",
        wire: text.replace(
          '"hostSchema":2,"hostBuildId":"floorp-host-1"',
          '"hostBuildId":"floorp-host-1","hostSchema":2',
        ),
      },
      {
        name: "duplicate compatibility property",
        wire: text.replace(
          '"hostSchema":2,',
          '"hostSchema":2,"hostSchema":2,',
        ),
      },
    ];

    for (const { name, wire } of cases) {
      assert(wire !== text, `${name} changes only the wire encoding`);
      const candidate = JSON.parse(wire) as Record<string, unknown>;
      assertEquals(candidate.keyId, original.keyId);
      assertEquals(candidate.source, original.source);
      assertEquals(candidate.signature, original.signature);
      assertEquals(candidate.manifest, original.manifest);
      await rejectsWith(
        verifySignedFeaturePackage(encoder.encode(wire), policy),
        "invalid-envelope",
      );
    }
  },
);

Deno.test("signed package enforces canonical base64url, digest and byte-length encodings", async () => {
  const bytes = await signed("source");
  const envelope = JSON.parse(new TextDecoder().decode(bytes)) as Record<
    string,
    unknown
  >;
  envelope.signature = `${String(envelope.signature)}=`;
  await rejectsWith(
    verifySignedFeaturePackage(
      encoder.encode(JSON.stringify(envelope)),
      policy,
    ),
    "invalid-envelope",
  );

  const canonical = JSON.parse(new TextDecoder().decode(bytes)) as Record<
    string,
    unknown
  >;
  const signature = String(canonical.signature);
  const last = alphabet.indexOf(signature.at(-1) ?? "");
  canonical.signature = signature.slice(0, -1) + alphabet[last ^ 1];
  await rejectsWith(
    verifySignedFeaturePackage(
      encoder.encode(JSON.stringify(canonical)),
      policy,
    ),
    "invalid-envelope",
  );
  const shortSignature = JSON.parse(new TextDecoder().decode(bytes)) as Record<
    string,
    unknown
  >;
  shortSignature.signature = "A";
  await rejectsWith(
    verifySignedFeaturePackage(
      encoder.encode(JSON.stringify(shortSignature)),
      policy,
    ),
    "invalid-envelope",
  );
  const oversizedSignature = JSON.parse(
    new TextDecoder().decode(bytes),
  ) as Record<string, unknown>;
  oversizedSignature.signature = String(oversizedSignature.signature).repeat(
    100,
  );
  await rejectsWith(
    verifySignedFeaturePackage(
      encoder.encode(JSON.stringify(oversizedSignature)),
      policy,
    ),
    "invalid-envelope",
  );

  const badKey = policyWith({
    trustedKeys: [{ keyId: "test-key", publicKey: `${publicKey}=` }],
  });
  await rejectsWith(
    verifySignedFeaturePackage(bytes, badKey),
    "invalid-policy",
  );
  await rejectsWith(
    verifySignedFeaturePackage(
      bytes,
      policyWith({ trustedKeys: [{ keyId: "test-key", publicKey: "A" }] }),
    ),
    "invalid-policy",
  );
  await rejectsWith(
    verifySignedFeaturePackage(
      bytes,
      policyWith({
        trustedKeys: [{
          keyId: "test-key",
          publicKey: publicKey.repeat(100),
        }],
      }),
    ),
    "invalid-policy",
  );
  await rejectsWith(
    verifySignedFeaturePackage(
      bytes,
      policyWith({
        trustedKeys: [{
          keyId: "test-key",
          publicKey: publicKey.slice(0, -1) +
            alphabet[alphabet.indexOf(publicKey.at(-1) ?? "") ^ 1],
        }],
      }),
    ),
    "invalid-policy",
  );
  const wrongPair = await crypto.subtle.generateKey(
    { name: "Ed25519" },
    true,
    ["sign", "verify"],
  ) as CryptoKeyPair;
  const wrongPublicKey = encodeBase64Url(
    new Uint8Array(await crypto.subtle.exportKey("raw", wrongPair.publicKey)),
  );
  await rejectsWith(
    verifySignedFeaturePackage(
      bytes,
      policyWith({
        trustedKeys: [{ keyId: "test-key", publicKey: wrongPublicKey }],
      }),
    ),
    "invalid-signature",
  );

  await rejectsWith(
    verifySignedFeaturePackage(
      await signedManifest("source", { sourceBytes: 99 }),
      policy,
    ),
    "source-digest-mismatch",
  );
  await rejectsWith(
    verifySignedFeaturePackage(
      await signedManifest("source", { sourceSha256: "0".repeat(64) }),
      policy,
    ),
    "source-digest-mismatch",
  );
});

Deno.test("signed package validates policy shape and trusted-key uniqueness", async () => {
  const bytes = await signed();
  await rejectsWith(
    verifySignedFeaturePackage(
      bytes,
      policyWith({ minimumReleaseSequence: -1 }),
    ),
    "invalid-policy",
  );
  await rejectsWith(
    verifySignedFeaturePackage(
      bytes,
      policyWith({ minimumReleaseSequence: Number.MAX_SAFE_INTEGER + 1 }),
    ),
    "invalid-policy",
  );
  for (
    const field of [
      "featureId",
      "hostSchema",
      "hostBuildId",
      "runtimeBuildId",
      "allowedCapabilities",
      "minimumReleaseSequence",
      "trustedKeys",
    ] as const
  ) {
    const missingField = { ...policy };
    Reflect.deleteProperty(missingField, field);
    await rejectsWith(
      verifySignedFeaturePackage(bytes, missingField),
      "invalid-policy",
    );
  }
  await rejectsWith(
    verifySignedFeaturePackage(
      bytes,
      policyWith({
        trustedKeys: [
          ...policy.trustedKeys,
          { keyId: "test-key", publicKey },
        ],
      }),
    ),
    "invalid-policy",
  );
  await rejectsWith(
    verifySignedFeaturePackage(
      bytes,
      { ...policy, unexpected: true } as FeaturePackagePolicy,
    ),
    "invalid-policy",
  );
  await rejectsWith(
    verifySignedFeaturePackage(
      bytes,
      policyWith({ trustedKeys: [{ keyId: "bad id", publicKey }] }),
    ),
    "invalid-policy",
  );
  await rejectsWith(
    verifySignedFeaturePackage(
      bytes,
      policyWith({ allowedCapabilities: ["hub-panel", "hub-panel"] }),
    ),
    "invalid-policy",
  );
  await rejectsWith(
    verifySignedFeaturePackage(
      bytes,
      policyWith({
        trustedKeys: Array.from({ length: 17 }, (_, index) => ({
          keyId: `test-key-${index}`,
          publicKey,
        })),
      }),
    ),
    "invalid-policy",
  );
  await rejectsWith(
    verifySignedFeaturePackage(
      bytes,
      {
        ...policy,
        trustedKeys: [{ keyId: "test-key", publicKey, extra: true }],
      } as unknown as FeaturePackagePolicy,
    ),
    "invalid-policy",
  );
  const hiddenFieldPolicy = { ...policy };
  Object.defineProperty(hiddenFieldPolicy, Symbol("unexpected"), {
    value: true,
  });
  await rejectsWith(
    verifySignedFeaturePackage(bytes, hiddenFieldPolicy),
    "invalid-policy",
  );
  const throwingPolicy = { ...policy };
  Object.defineProperty(throwingPolicy, "featureId", {
    get() {
      throw new Error("unbounded caller error");
    },
  });
  await rejectsWith(
    verifySignedFeaturePackage(bytes, throwingPolicy),
    "invalid-policy",
  );
});

Deno.test("signed package enforces identifier, array and source-size bounds", async () => {
  const maximumSource = "x".repeat(FEATURE_PACKAGE_SOURCE_MAX_BYTES);
  const maximumSourceResult = await verifySignedFeaturePackage(
    await signed(maximumSource),
    policy,
  );
  assertEquals(
    maximumSourceResult.manifest.sourceBytes,
    FEATURE_PACKAGE_SOURCE_MAX_BYTES,
  );
  await rejectsWith(
    verifySignedFeaturePackage(
      await signedManifest("x".repeat(FEATURE_PACKAGE_SOURCE_MAX_BYTES + 1)),
      policy,
    ),
    "size-limit",
  );
  const multibyteSource = "é".repeat(FEATURE_PACKAGE_SOURCE_MAX_BYTES / 2);
  const multibyteResult = await verifySignedFeaturePackage(
    await signed(multibyteSource),
    policy,
  );
  assertEquals(
    multibyteResult.manifest.sourceBytes,
    FEATURE_PACKAGE_SOURCE_MAX_BYTES,
  );
  await rejectsWith(
    signFeaturePackage(
      metadata(),
      `${multibyteSource}é`,
      "test-key",
      keyPair.privateKey,
    ),
    "size-limit",
  );

  const invalidManifests: Promise<Uint8Array>[] = [
    signedManifest("x", { featureId: "Hub-panel-menu" }),
    signedManifest("x", { featureId: "a".repeat(49) }),
    signedManifest("x", { version: "v".repeat(65) }),
    signedManifest("x", { releaseSequence: Number.MAX_SAFE_INTEGER + 1 }),
    signedManifest("x", {
      compatibility: { ...metadata().compatibility, hostBuildId: "bad id" },
    }),
    signedManifest("x", {
      compatibility: {
        ...metadata().compatibility,
        runtimeBuildIds: ["runtime-1", "runtime-1"],
      },
    }),
    signedManifest("x", {
      compatibility: {
        ...metadata().compatibility,
        capabilities: ["hub-panel", "analytics"],
      },
    }),
    signedManifest("x", {
      compatibility: {
        ...metadata().compatibility,
        runtimeBuildIds: Array.from(
          { length: 17 },
          (_, index) => `runtime-${index}`,
        ),
      },
    }),
    signedManifest("x", {
      compatibility: {
        ...metadata().compatibility,
        hostBuildId: "h".repeat(129),
      },
    }),
    signedManifest("x", {
      compatibility: {
        ...metadata().compatibility,
        runtimeBuildIds: ["r".repeat(129)],
      },
    }),
    signedManifest("x", {
      compatibility: {
        ...metadata().compatibility,
        capabilities: ["c".repeat(65)],
      },
    }),
    signedManifest("x", {
      compatibility: {
        ...metadata().compatibility,
        capabilities: Array.from({ length: 17 }, (_, index) => `cap-${index}`),
      },
    }),
  ];
  for (const invalid of invalidManifests) {
    await rejectsWith(
      verifySignedFeaturePackage(await invalid, policy),
      "invalid-envelope",
    );
  }
  await rejectsWith(
    verifySignedFeaturePackage(
      await signedManifest("x", {}, "bad/key"),
      policy,
    ),
    "invalid-envelope",
  );
  await rejectsWith(
    verifySignedFeaturePackage(
      await signedManifest("x", {}, "k".repeat(65)),
      policy,
    ),
    "invalid-envelope",
  );
  await rejectsWith(
    signFeaturePackage(
      metadata(),
      "x",
      "k".repeat(65),
      keyPair.privateKey,
    ),
    "invalid-metadata",
  );

  const maximumKeyId = `k${"a".repeat(63)}`;
  const maximumHostBuildId = "h".repeat(128);
  const maximumRuntimeBuildId = "r".repeat(128);
  const maximumCapability = "c".repeat(64);
  const maximumCapabilities = [
    maximumCapability,
    ...Array.from({ length: 15 }, (_, index) => `cap-${index}`),
  ].sort();
  const maximumRuntimeBuildIds = [
    maximumRuntimeBuildId,
    ...Array.from({ length: 15 }, (_, index) => `runtime-${index}`),
  ].sort();
  const maximumMetadata = metadata({
    compatibility: {
      hostSchema: 2,
      hostBuildId: maximumHostBuildId,
      runtimeBuildIds: maximumRuntimeBuildIds,
      capabilities: maximumCapabilities,
    },
  });
  const maximumTrustedKeys = [
    { keyId: maximumKeyId, publicKey },
    ...Array.from({ length: 15 }, (_, index) => ({
      keyId: `key-${index}`,
      publicKey,
    })),
  ];
  const maximumPackage = await signed("x", maximumMetadata, maximumKeyId);
  const maximumPolicy = policyWith({
    hostBuildId: maximumHostBuildId,
    runtimeBuildId: maximumRuntimeBuildId,
    allowedCapabilities: maximumCapabilities,
    trustedKeys: maximumTrustedKeys,
  });
  const maximumResult = await verifySignedFeaturePackage(
    maximumPackage,
    maximumPolicy,
  );
  assertEquals(maximumResult.keyId, maximumKeyId);
  await rejectsWith(
    verifySignedFeaturePackage(
      maximumPackage,
      policyWith({ hostBuildId: "h".repeat(129) }),
    ),
    "invalid-policy",
  );
  await rejectsWith(
    verifySignedFeaturePackage(
      maximumPackage,
      policyWith({ runtimeBuildId: "r".repeat(129) }),
    ),
    "invalid-policy",
  );
  await rejectsWith(
    verifySignedFeaturePackage(
      maximumPackage,
      policyWith({ allowedCapabilities: ["c".repeat(65)] }),
    ),
    "invalid-policy",
  );
  await rejectsWith(
    verifySignedFeaturePackage(
      maximumPackage,
      policyWith({
        allowedCapabilities: Array.from(
          { length: 17 },
          (_, index) => `cap-${index}`,
        ),
      }),
    ),
    "invalid-policy",
  );
  await rejectsWith(
    verifySignedFeaturePackage(
      maximumPackage,
      policyWith({
        trustedKeys: [{ keyId: "k".repeat(65), publicKey }],
      }),
    ),
    "invalid-policy",
  );

  const sorted = await signed(
    "x",
    metadata({
      compatibility: {
        hostSchema: 2,
        hostBuildId: "floorp-host-1",
        runtimeBuildIds: ["runtime-z", "runtime-a"],
        capabilities: ["hub-panel", "analytics"],
      },
    }),
  );
  const sortedResult = await verifySignedFeaturePackage(
    sorted,
    policyWith({
      runtimeBuildId: "runtime-a",
      allowedCapabilities: ["analytics", "hub-panel"],
    }),
  );
  assertEquals(
    sortedResult.manifest.compatibility.runtimeBuildIds,
    ["runtime-a", "runtime-z"],
  );
  assertEquals(
    sortedResult.manifest.compatibility.capabilities,
    ["analytics", "hub-panel"],
  );
});

Deno.test("signed package snapshots bytes and policy before crypto awaits", async () => {
  const bytes = await signed();
  const input = bytes.slice();
  const mutablePolicy = structuredClone(policy) as {
    featureId: string;
    hostSchema: number;
    hostBuildId: string;
    runtimeBuildId: string;
    allowedCapabilities: string[];
    minimumReleaseSequence: number;
    trustedKeys: { keyId: string; publicKey: string }[];
  };
  const pending = verifySignedFeaturePackage(input, mutablePolicy);
  input.fill(0);
  mutablePolicy.featureId = "changed";
  mutablePolicy.trustedKeys[0].publicKey = "A";
  mutablePolicy.allowedCapabilities.length = 0;
  const result = await pending;
  assertEquals(result.manifest.featureId, "hub-panel-menu");
});

Deno.test("signer snapshots metadata arrays before crypto awaits and bounds source", async () => {
  const mutable = metadata() as {
    featureId: string;
    version: string;
    releaseSequence: number;
    compatibility: {
      hostSchema: number;
      hostBuildId: string;
      runtimeBuildIds: string[];
      capabilities: string[];
    };
  };
  const pending = signFeaturePackage(
    mutable,
    "same source",
    "test-key",
    keyPair.privateKey,
  );
  mutable.featureId = "mutated";
  mutable.compatibility.runtimeBuildIds[0] = "mutated-runtime";
  mutable.compatibility.capabilities.push("mutated");
  const result = await pending;
  assertEquals(
    (await verifySignedFeaturePackage(result, policy)).manifest.featureId,
    "hub-panel-menu",
  );
  await rejectsWith(
    signFeaturePackage(metadata(), "", "test-key", keyPair.privateKey),
    "invalid-source",
  );
  await rejectsWith(
    signFeaturePackage(
      metadata(),
      "x".repeat(FEATURE_PACKAGE_SOURCE_MAX_BYTES + 1),
      "test-key",
      keyPair.privateKey,
    ),
    "size-limit",
  );
  await rejectsWith(
    signFeaturePackage(metadata(), "x", "bad id", keyPair.privateKey),
    "invalid-metadata",
  );
  await rejectsWith(
    signFeaturePackage(
      metadata({
        compatibility: {
          ...metadata().compatibility,
          capabilities: ["bad value"],
        },
      }),
      "x",
      "test-key",
      keyPair.privateKey,
    ),
    "invalid-metadata",
  );
  await rejectsWith(
    signFeaturePackage(
      { ...metadata(), extra: true } as FeaturePackageAuthoringMetadata,
      "x",
      "test-key",
      keyPair.privateKey,
    ),
    "invalid-metadata",
  );
  await rejectsWith(
    signFeaturePackage(
      metadata({
        compatibility: {
          ...metadata().compatibility,
          runtimeBuildIds: Array.from(
            { length: 17 },
            (_, index) => `runtime-${index}`,
          ),
        },
      }),
      "x",
      "test-key",
      keyPair.privateKey,
    ),
    "invalid-metadata",
  );
  const throwingMetadata = { ...metadata() };
  Object.defineProperty(throwingMetadata, "version", {
    get() {
      throw new Error("unbounded caller error");
    },
  });
  await rejectsWith(
    signFeaturePackage(
      throwingMetadata,
      "x",
      "test-key",
      keyPair.privateKey,
    ),
    "invalid-metadata",
  );
  const throwingKey = Object.create(null) as Record<string, unknown>;
  Object.defineProperty(throwingKey, "type", {
    get() {
      throw new Error("unbounded caller error");
    },
  });
  await rejectsWith(
    signFeaturePackage(
      metadata(),
      "x",
      "test-key",
      throwingKey as unknown as CryptoKey,
    ),
    "invalid-signing-key",
  );
});
