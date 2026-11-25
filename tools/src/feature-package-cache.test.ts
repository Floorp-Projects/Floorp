// SPDX-License-Identifier: MPL-2.0
/// <reference lib="deno.ns" />

import { assert, assertEquals } from "@std/assert";
import {
  FeaturePackageCacheError,
  openFeaturePackageCache,
} from "../../browser-features/modules/modules/feature-host/package/FeaturePackageCache.ts";
import {
  FeaturePackageError,
  signFeaturePackage,
  verifySignedFeaturePackage,
} from "../../browser-features/modules/modules/feature-host/package/FeaturePackageVerifier.ts";
import type {
  FeaturePackageCacheEntry,
  FeaturePackageCacheStorage,
  FeaturePackagePolicy,
} from "../../browser-features/modules/modules/feature-host/package/types.ts";
import type {
  CacheFaultPoint,
  MemoryCacheState,
} from "./feature-package-cache-test-types.ts";

const pair = await crypto.subtle.generateKey({ name: "Ed25519" }, true, [
  "sign",
  "verify",
]) as CryptoKeyPair;
const rawPublic = new Uint8Array(
  await crypto.subtle.exportKey("raw", pair.publicKey),
);
const publicKey = btoa(String.fromCharCode(...rawPublic))
  .replaceAll("+", "-")
  .replaceAll("/", "_")
  .replace(/=+$/, "");
const basePolicy: FeaturePackagePolicy = {
  featureId: "hub-panel-menu",
  hostSchema: 2,
  hostBuildId: "host-1",
  runtimeBuildId: "runtime-1",
  allowedCapabilities: ["hub-panel"],
  minimumReleaseSequence: 1,
  trustedKeys: [{ keyId: "cache-key", publicKey }],
};

function metadata(releaseSequence = 2) {
  return {
    featureId: "hub-panel-menu",
    version: "v1",
    releaseSequence,
    compatibility: {
      hostSchema: 2,
      hostBuildId: "host-1",
      runtimeBuildIds: ["runtime-1"],
      capabilities: ["hub-panel"],
    },
  };
}

async function packageBytes(source: string, sequence = 2): Promise<Uint8Array> {
  return await signFeaturePackage(
    metadata(sequence),
    source,
    "cache-key",
    pair.privateKey,
  );
}

function state(): MemoryCacheState {
  return {
    files: new Map(),
    specialEntries: new Map(),
    failurePoints: new Set(),
  };
}

function policyWithFeatureIdGetter(
  readFeatureId: () => string,
): FeaturePackagePolicy {
  return {
    get featureId() {
      return readFeatureId();
    },
    hostSchema: basePolicy.hostSchema,
    hostBuildId: basePolicy.hostBuildId,
    runtimeBuildId: basePolicy.runtimeBuildId,
    allowedCapabilities: basePolicy.allowedCapabilities,
    minimumReleaseSequence: basePolicy.minimumReleaseSequence,
    trustedKeys: basePolicy.trustedKeys,
  };
}

function deferred(): { readonly promise: Promise<void>; release(): void } {
  let release = (): void => {};
  const promise = new Promise<void>((resolve) => release = resolve);
  return { promise, release };
}

class MemoryStorage implements FeaturePackageCacheStorage {
  constructor(readonly value: MemoryCacheState) {}

  #fail(point: CacheFaultPoint): void {
    if (this.value.failurePoints.has(point)) {
      throw new Error("private storage path and source");
    }
  }

  async initialize(): Promise<void> {
    this.#fail("initialize");
    await this.value.initializeGate?.promise;
  }

  inspect(name: string): Promise<FeaturePackageCacheEntry | null> {
    this.#fail("inspect");
    const kind = this.value.specialEntries.get(name);
    if (kind) return Promise.resolve({ name, kind });
    return Promise.resolve(
      this.value.files.has(name) ? { name, kind: "file" } : null,
    );
  }

  async read(name: string, maxBytes: number): Promise<Uint8Array> {
    this.#fail("read");
    const bytes = this.value.files.get(name);
    if (!bytes) throw new Error("private source path");
    this.value.readStarted?.(name);
    await this.value.readGate?.promise;
    const replacement = this.value.readReplacement;
    if (replacement) {
      this.value.readReplacement = undefined;
      return replacement.slice(0, maxBytes);
    }
    return bytes.slice(0, maxBytes);
  }

  enumerate(maxEntries: number): Promise<readonly FeaturePackageCacheEntry[]> {
    this.#fail("enumerate");
    const all: FeaturePackageCacheEntry[] = [
      ...Array.from(
        this.value.files.keys(),
        (name) => ({ name, kind: "file" as const }),
      ),
      ...Array.from(
        this.value.specialEntries,
        ([name, kind]) => ({ name, kind }),
      ),
    ];
    return Promise.resolve(all.slice(0, maxEntries));
  }

  async writeStaging(bytes: Uint8Array): Promise<void> {
    this.#fail("writeStaging");
    if (this.value.files.has("candidate.tmp")) {
      throw new Error("already exists");
    }
    this.value.files.set("candidate.tmp", bytes.slice());
    this.value.stagingStarted?.();
    await this.value.stagingGate?.promise;
  }

  moveStaging(name: string): Promise<void> {
    this.#fail("moveStaging");
    if (this.value.files.has(name) || this.value.specialEntries.has(name)) {
      throw new Error("no overwrite");
    }
    const staged = this.value.files.get("candidate.tmp");
    if (!staged) throw new Error("stage missing");
    this.value.files.set(name, staged);
    this.value.files.delete("candidate.tmp");
    return Promise.resolve();
  }

  removeStaging(): Promise<void> {
    this.#fail("removeStaging");
    this.value.files.delete("candidate.tmp");
    return Promise.resolve();
  }
}

async function expectCode(
  operation: Promise<unknown>,
  code: string,
): Promise<void> {
  let caught: unknown;
  try {
    await operation;
  } catch (error: unknown) {
    caught = error;
  }
  assert(
    caught instanceof FeaturePackageError ||
      caught instanceof FeaturePackageCacheError,
  );
  assertEquals(caught.code, code);
  assert(!caught.message.includes("private"), "cache errors are sanitized");
}

function expectSyncCode(operation: () => unknown, code: string): void {
  let caught: unknown;
  try {
    operation();
  } catch (error: unknown) {
    caught = error;
  }
  assert(caught instanceof FeaturePackageCacheError);
  assertEquals(caught.code, code);
}

Deno.test("verified cache preserves exact bytes through reopen and is idempotent", async () => {
  const bytes = await packageBytes(
    "globalThis.__m3bPackageSourceRan = true;\r\n",
  );
  const data = state();
  const storage = new MemoryStorage(data);
  let cache = await openFeaturePackageCache(
    basePolicy.featureId,
    "profile/features/hub-panel-menu",
    storage,
  );
  const first = await cache.put(bytes, basePolicy);
  const before = data.files.get(first.packageSha256)?.slice();
  const duplicate = await cache.put(bytes, basePolicy);
  assertEquals(
    duplicate.source,
    "globalThis.__m3bPackageSourceRan = true;\r\n",
  );
  assert(!Object.hasOwn(globalThis, "__m3bPackageSourceRan"));
  assertEquals(data.files.get(first.packageSha256), before);
  await cache.close();

  cache = await openFeaturePackageCache(
    basePolicy.featureId,
    "profile/features/hub-panel-menu",
    storage,
  );
  assertEquals(
    (await cache.read(first.packageSha256, basePolicy)).source,
    first.source,
  );
  await cache.close();
});

Deno.test("parallel puts of one signed envelope publish one exact file", async () => {
  const data = state();
  const gate = deferred();
  const started = deferred();
  const storage = new MemoryStorage(data);
  const operations: Promise<unknown>[] = [];
  let cache: Awaited<ReturnType<typeof openFeaturePackageCache>> | undefined;
  try {
    cache = await openFeaturePackageCache(
      basePolicy.featureId,
      "profile/parallel-put",
      storage,
    );
    data.stagingGate = gate;
    data.stagingStarted = started.release;
    const bytes = await packageBytes("parallel\r\n");
    const first = cache.put(bytes, basePolicy);
    operations.push(first);
    void first.then(() => started.release(), () => started.release());
    await started.promise;
    const second = cache.put(bytes, basePolicy);
    operations.push(second);
    gate.release();
    const [firstResult, secondResult] = await Promise.all([first, second]);
    assertEquals(firstResult.packageSha256, secondResult.packageSha256);
    assertEquals(data.files.size, 1);
    assert(!data.files.has("candidate.tmp"));
  } finally {
    gate.release();
    started.release();
    await Promise.allSettled(operations);
    await cache?.close();
  }
});

Deno.test("cache reads reject tampering, truncation, replacement, missing files and changed policy", async () => {
  const firstBytes = await packageBytes("first source\r\n");
  const otherBytes = await packageBytes("other source\r\n");
  const data = state();
  const storage = new MemoryStorage(data);
  const cache = await openFeaturePackageCache(
    basePolicy.featureId,
    "profile/read-errors",
    storage,
  );
  const first = await cache.put(firstBytes, basePolicy);
  const other = await cache.put(otherBytes, basePolicy);

  const saved = data.files.get(first.packageSha256);
  assert(saved);
  data.files.set(first.packageSha256, saved.slice(0, saved.length - 1));
  await expectCode(
    cache.read(first.packageSha256, basePolicy),
    "invalid-envelope",
  );
  data.files.set(first.packageSha256, otherBytes);
  await expectCode(
    cache.read(first.packageSha256, basePolicy),
    "cache-corrupt",
  );
  data.files.set(first.packageSha256, firstBytes);
  await expectCode(
    cache.read(first.packageSha256, {
      ...basePolicy,
      minimumReleaseSequence: 9,
    }),
    "sequence-too-low",
  );
  await expectCode(
    cache.read(first.packageSha256, { ...basePolicy, trustedKeys: [] }),
    "untrusted-key",
  );
  await expectCode(
    cache.read(first.packageSha256, {
      ...basePolicy,
      featureId: "another-feature",
    }),
    "cache-policy-mismatch",
  );
  await expectCode(
    cache.read(first.packageSha256, {
      ...basePolicy,
      hostBuildId: "other-host",
    }),
    "host-incompatible",
  );
  await expectCode(
    cache.read(first.packageSha256, {
      ...basePolicy,
      runtimeBuildId: "other-runtime",
    }),
    "runtime-incompatible",
  );
  await expectCode(
    cache.read(first.packageSha256, {
      ...basePolicy,
      allowedCapabilities: [],
    }),
    "capability-denied",
  );

  const envelopeText = new TextDecoder().decode(firstBytes);
  const signature = /"signature":"([A-Za-z0-9_-]+)"/.exec(envelopeText)?.[1];
  assert(signature);
  const tamperedSignature = (signature[0] === "A" ? "B" : "A") +
    signature.slice(1);
  const tamperedEnvelope = new TextEncoder().encode(
    envelopeText.replace(
      '"signature":"' + signature + '"',
      '"signature":"' + tamperedSignature + '"',
    ),
  );
  data.files.set(first.packageSha256, tamperedEnvelope);
  await expectCode(
    cache.read(first.packageSha256, basePolicy),
    "invalid-signature",
  );
  data.files.set(first.packageSha256, firstBytes);
  data.files.delete(first.packageSha256);
  await expectCode(
    cache.read(first.packageSha256, basePolicy),
    "cache-missing",
  );
  assert(other.packageSha256 !== first.packageSha256);
  await cache.close();
});

Deno.test("read snapshots nested policy through a held storage read", async () => {
  const data = state();
  const cache = await openFeaturePackageCache(
    basePolicy.featureId,
    "profile/read-policy-snapshot",
    new MemoryStorage(data),
  );
  const bytes = await packageBytes("read-policy\r\n");
  const stored = await cache.put(bytes, basePolicy);
  const gate = deferred();
  const started = deferred();
  data.readGate = gate;
  data.readStarted = started.release;
  let read: Promise<unknown> | undefined;
  try {
    const mutablePolicy = {
      ...basePolicy,
      allowedCapabilities: [...basePolicy.allowedCapabilities],
      trustedKeys: basePolicy.trustedKeys.map((key) => ({ ...key })),
    };
    read = cache.read(stored.packageSha256, mutablePolicy);
    void read.then(() => started.release(), () => started.release());
    await started.promise;
    mutablePolicy.allowedCapabilities.splice(0);
    mutablePolicy.trustedKeys.splice(0);
    gate.release();
    const result = await read as { readonly source: string };
    assertEquals(result.source, "read-policy\r\n");
  } finally {
    gate.release();
    started.release();
    await read?.then(() => undefined, () => undefined);
    await cache.close();
  }
});

Deno.test("repeated synchronous input failures release operation slots", async () => {
  const cache = await openFeaturePackageCache(
    basePolicy.featureId,
    "profile/synchronous-failures",
    new MemoryStorage(state()),
  );
  const oversized = new Uint8Array(1_048_577);
  for (let index = 0; index < 20; index++) {
    await expectCode(
      cache.put(oversized, basePolicy),
      "size-limit",
    );
    await expectCode(
      cache.read("not-a-hash", basePolicy),
      "cache-invalid-package-hash",
    );
    await new Promise<void>((resolve) => setTimeout(resolve, 0));
  }
  assertEquals(
    (await cache.put(await packageBytes("slots"), basePolicy)).source,
    "slots",
  );
  await cache.close();
});

Deno.test("cache snapshots queued inputs and rejects the seventeenth operation before reading it", async () => {
  const data = state();
  const gate = deferred();
  const started = deferred();
  const storage = new MemoryStorage(data);
  const operations: Promise<unknown>[] = [];
  let cache: Awaited<ReturnType<typeof openFeaturePackageCache>> | undefined;
  try {
    cache = await openFeaturePackageCache(
      basePolicy.featureId,
      "profile/queue",
      storage,
    );
    data.stagingGate = gate;
    data.stagingStarted = started.release;
    const input = await packageBytes("first\r\n");
    const first = cache.put(input, basePolicy);
    operations.push(first);
    void first.then(() => started.release(), () => started.release());
    await started.promise;
    const queuedInput = await packageBytes("queued\r\n");
    const mutable = queuedInput.slice();
    const policy = { ...basePolicy, minimumReleaseSequence: 1 };
    const queued = cache.put(mutable, policy);
    mutable.fill(0);
    policy.minimumReleaseSequence = 100;
    const pending = Array.from(
      { length: 14 },
      () => cache!.put(input, basePolicy),
    );
    operations.push(queued, ...pending);
    let touched = false;
    const hostile = new Proxy(new Uint8Array(2), {
      get(target, property, receiver) {
        touched = true;
        return Reflect.get(target, property, receiver);
      },
    });
    const seventeenth = cache.put(
      hostile as unknown as Uint8Array,
      basePolicy,
    );
    operations.push(seventeenth);
    assert(!touched, "seventeenth request is rejected before byte snapshot");
    gate.release();
    await expectCode(seventeenth, "cache-capacity");
    const [firstResult, queuedResult] = await Promise.all([first, queued]);
    await Promise.all(pending);
    assertEquals(firstResult.source, "first\r\n");
    assertEquals(queuedResult.source, "queued\r\n");
  } finally {
    gate.release();
    started.release();
    await Promise.allSettled(operations);
    await cache?.close();
  }
});

Deno.test("preparation failures retain operation slots until their queue nodes drain", async () => {
  const data = state();
  const gate = deferred();
  const started = deferred();
  const storage = new MemoryStorage(data);
  const operations: Promise<unknown>[] = [];
  let cache: Awaited<ReturnType<typeof openFeaturePackageCache>> | undefined;
  try {
    cache = await openFeaturePackageCache(
      basePolicy.featureId,
      "profile/preparation-capacity",
      storage,
    );
    data.stagingGate = gate;
    data.stagingStarted = started.release;
    const input = await packageBytes("held-slot-predecessor\r\n");
    const predecessor = cache.put(input, basePolicy);
    operations.push(predecessor);
    await started.promise;

    const failingPolicy = policyWithFeatureIdGetter(() => {
      throw new Error("invalid policy snapshot");
    });
    for (let index = 0; index < 15; index++) {
      await expectCode(
        cache.put(input, failingPolicy),
        "invalid-policy",
      );
    }

    let bytesTouched = false;
    let policyTouched = false;
    const hostileBytes = new Uint8Array(1);
    Object.defineProperty(hostileBytes, "byteLength", {
      get() {
        bytesTouched = true;
        return 1;
      },
    });
    const hostilePolicy = policyWithFeatureIdGetter(() => {
      policyTouched = true;
      return basePolicy.featureId;
    });
    let seventeenthError: unknown;
    const seventeenth = cache.put(hostileBytes, hostilePolicy).then(
      () => seventeenthError = new Error("expected cache capacity rejection"),
      (error: unknown) => seventeenthError = error,
    );
    operations.push(seventeenth);
    await Promise.resolve();
    assert(
      seventeenthError instanceof FeaturePackageCacheError,
      "capacity rejection is prompt",
    );
    assertEquals(seventeenthError.code, "cache-capacity");
    assert(!bytesTouched, "capacity rejection precedes byte snapshot");
    assert(!policyTouched, "capacity rejection precedes policy snapshot");

    gate.release();
    await predecessor;
    await new Promise<void>((resolve) => setTimeout(resolve, 0));
    const recovered = Array.from(
      { length: 16 },
      () => cache!.put(input, basePolicy),
    );
    operations.push(...recovered);
    const results = await Promise.all(recovered);
    assertEquals(results.length, 16);
    assert(
      results.every((result) => result.source === "held-slot-predecessor\r\n"),
      "all operation slots are available after failed queue nodes drain",
    );
  } finally {
    gate.release();
    started.release();
    await Promise.allSettled(operations);
    await cache?.close();
  }
});

Deno.test("failed storage work does not block later operations or expose raw errors", async () => {
  const data = state();
  const cache = await openFeaturePackageCache(
    basePolicy.featureId,
    "profile/recovery",
    new MemoryStorage(data),
  );
  const bytes = await packageBytes("recover\r\n");
  data.failurePoints.add("writeStaging");
  await expectCode(cache.put(bytes, basePolicy), "cache-storage");
  data.failurePoints.delete("writeStaging");
  const result = await cache.put(bytes, basePolicy);
  assertEquals(result.source, "recover\r\n");
  await cache.close();
});

Deno.test("move and stale-stage cleanup failures preserve data and remain retryable", async () => {
  const data = state();
  const cache = await openFeaturePackageCache(
    basePolicy.featureId,
    "profile/stage-failures",
    new MemoryStorage(data),
  );
  const bytes = await packageBytes("staging\r\n");
  data.failurePoints.add("moveStaging");
  await expectCode(cache.put(bytes, basePolicy), "cache-storage");
  assert(data.files.has("candidate.tmp"));
  data.failurePoints.delete("moveStaging");
  data.failurePoints.add("removeStaging");
  await expectCode(cache.put(bytes, basePolicy), "cache-storage");
  assert(data.files.has("candidate.tmp"), "failed cleanup preserves staging");
  assertEquals(
    data.files.size,
    1,
    "failed cleanup creates no alternate staging file",
  );
  data.failurePoints.delete("removeStaging");
  const result = await cache.put(bytes, basePolicy);
  assertEquals(result.source, "staging\r\n");
  assert(!data.files.has("candidate.tmp"));
  await cache.close();
});

Deno.test("corrupt existing package is never overwritten or repaired", async () => {
  const data = state();
  const cache = await openFeaturePackageCache(
    basePolicy.featureId,
    "profile/corrupt-existing",
    new MemoryStorage(data),
  );
  const bytes = await packageBytes("preserve\r\n");
  const verified = await cache.put(bytes, basePolicy);
  const corrupt = new Uint8Array([1, 2, 3]);
  data.files.set(verified.packageSha256, corrupt);
  await expectCode(cache.put(bytes, basePolicy), "invalid-envelope");
  assertEquals(data.files.get(verified.packageSha256), corrupt);
  await cache.close();
});

Deno.test("post-publication readback failures preserve files and allow recovery", async () => {
  const data = state();
  const storage = new MemoryStorage(data);
  let cache = await openFeaturePackageCache(
    basePolicy.featureId,
    "profile/readback-recovery",
    storage,
  );
  const bytes = await packageBytes("published\r\n");
  const verified = await verifySignedFeaturePackage(bytes, basePolicy);
  data.failurePoints.add("read");
  await expectCode(cache.put(bytes, basePolicy), "cache-storage");
  data.failurePoints.delete("read");
  const storedAfterReadFailure = data.files.get(verified.packageSha256);
  assertEquals(storedAfterReadFailure, bytes);
  assert(!data.files.has("candidate.tmp"));
  const beforeRetry = storedAfterReadFailure?.slice();
  await cache.close();

  cache = await openFeaturePackageCache(
    basePolicy.featureId,
    "profile/readback-recovery",
    storage,
  );
  const recovered = await cache.put(bytes, basePolicy);
  assertEquals(recovered.packageSha256, verified.packageSha256);
  assertEquals(data.files.get(verified.packageSha256), beforeRetry);

  const primary = await packageBytes("primary\r\n");
  const replacement = await packageBytes("other valid signed envelope\r\n");
  const primaryHash = (await verifySignedFeaturePackage(primary, basePolicy))
    .packageSha256;
  const replacementHash = (
    await verifySignedFeaturePackage(replacement, basePolicy)
  ).packageSha256;
  assert(primaryHash !== replacementHash);
  data.readReplacement = replacement;
  await expectCode(cache.put(primary, basePolicy), "cache-corrupt");
  assertEquals(data.files.get(primaryHash), primary);
  assert(!data.files.has(replacementHash));
  const retry = await cache.put(primary, basePolicy);
  assertEquals(retry.packageSha256, primaryHash);
  assertEquals(data.files.get(primaryHash), primary);
  await cache.close();
});

Deno.test("cache refuses unsafe layouts and full capacity without deleting entries", async () => {
  const data = state();
  data.specialEntries.set("unexpected", "file");
  const cache = await openFeaturePackageCache(
    basePolicy.featureId,
    "profile/unsafe",
    new MemoryStorage(data),
  );
  await expectCode(
    cache.put(await packageBytes("unsafe"), basePolicy),
    "cache-unsafe-layout",
  );
  assert(data.specialEntries.has("unexpected"));
  await cache.close();

  const full = state();
  for (let index = 0; index < 32; index++) {
    full.files.set(index.toString(16).padStart(64, "0"), new Uint8Array([1]));
  }
  const fullCache = await openFeaturePackageCache(
    basePolicy.featureId,
    "profile/full",
    new MemoryStorage(full),
  );
  await expectCode(
    fullCache.put(await packageBytes("full"), basePolicy),
    "cache-capacity",
  );
  assertEquals(full.files.size, 32);
  await fullCache.close();
});

Deno.test("close stops admission synchronously, drains accepted work and reuses its promise", async () => {
  const data = state();
  const gate = deferred();
  const started = deferred();
  const storage = new MemoryStorage(data);
  const operations: Promise<unknown>[] = [];
  let cache: Awaited<ReturnType<typeof openFeaturePackageCache>> | undefined;
  let reopened: Awaited<ReturnType<typeof openFeaturePackageCache>> | undefined;
  let close: Promise<void> | undefined;
  try {
    cache = await openFeaturePackageCache(
      basePolicy.featureId,
      "profile/close",
      storage,
    );
    data.stagingGate = gate;
    data.stagingStarted = started.release;
    const accepted = cache.put(
      await packageBytes("closing\r\n"),
      basePolicy,
    );
    operations.push(accepted);
    void accepted.then(() => started.release(), () => started.release());
    await started.promise;
    close = cache.close();
    let closeSettled = false;
    void close.then(() => closeSettled = true);
    assert(close === cache.close(), "repeated close returns the same promise");
    await Promise.resolve();
    assert(!closeSettled, "close remains pending while accepted I/O is held");
    expectSyncCode(
      () =>
        openFeaturePackageCache(
          basePolicy.featureId,
          "profile/close",
          new MemoryStorage(state()),
        ),
      "cache-owner-active",
    );
    const late = cache.put(await packageBytes("late"), basePolicy);
    operations.push(late);
    gate.release();
    await expectCode(late, "cache-closed");
    await accepted;
    await close;
    reopened = await openFeaturePackageCache(
      basePolicy.featureId,
      "profile/close",
      storage,
    );
    await cache.close();
    expectSyncCode(
      () =>
        openFeaturePackageCache(
          basePolicy.featureId,
          "profile/close",
          new MemoryStorage(state()),
        ),
      "cache-owner-active",
    );
  } finally {
    gate.release();
    started.release();
    await Promise.allSettled(operations);
    await close;
    await cache?.close();
    await reopened?.close();
  }
});

Deno.test("snapshot-time close drains an accepted put before releasing its owner", async () => {
  const data = state();
  const gate = deferred();
  const started = deferred();
  const storage = new MemoryStorage(data);
  const operations: Promise<unknown>[] = [];
  let cache: Awaited<ReturnType<typeof openFeaturePackageCache>> | undefined;
  let duplicate:
    | Awaited<ReturnType<typeof openFeaturePackageCache>>
    | undefined;
  let reopened: Awaited<ReturnType<typeof openFeaturePackageCache>> | undefined;
  let close: Promise<void> | undefined;
  try {
    cache = await openFeaturePackageCache(
      basePolicy.featureId,
      "profile/snapshot-close",
      storage,
    );
    data.stagingGate = gate;
    data.stagingStarted = started.release;
    const bytes = await packageBytes("snapshot-close\r\n");
    const policy = policyWithFeatureIdGetter(() => {
      close = cache!.close();
      return basePolicy.featureId;
    });
    const accepted = cache.put(bytes, policy);
    operations.push(accepted);
    assert(close, "policy getter closes the cache during snapshot");
    await started.promise;

    let closeSettled = false;
    void close.then(() => closeSettled = true);
    await Promise.resolve();
    assert(!closeSettled, "close waits for accepted staging I/O");
    assert(close === cache.close(), "repeated close returns the same promise");

    let duplicateError: unknown;
    try {
      duplicate = await openFeaturePackageCache(
        basePolicy.featureId,
        "profile/snapshot-close",
        new MemoryStorage(state()),
      );
    } catch (error: unknown) {
      duplicateError = error;
    }
    assert(
      duplicateError instanceof FeaturePackageCacheError,
      "the owner remains reserved while accepted staging is held",
    );
    assertEquals(duplicateError.code, "cache-owner-active");

    gate.release();
    await accepted;
    await close;
    reopened = await openFeaturePackageCache(
      basePolicy.featureId,
      "profile/snapshot-close",
      storage,
    );
  } finally {
    gate.release();
    started.release();
    await Promise.allSettled(operations);
    await close;
    await cache?.close();
    await duplicate?.close();
    await reopened?.close();
  }
});

Deno.test("policy getter nested operations follow the outer FIFO reservation", async () => {
  const data = state();
  const gate = deferred();
  const started = deferred();
  const storage = new MemoryStorage(data);
  const operations: Promise<unknown>[] = [];
  const events: string[] = [];
  let cache: Awaited<ReturnType<typeof openFeaturePackageCache>> | undefined;
  let nested: Promise<unknown> | undefined;
  try {
    cache = await openFeaturePackageCache(
      basePolicy.featureId,
      "profile/snapshot-nested",
      storage,
    );
    const seedSource = "fifo-seed\r\n";
    const seed = await cache.put(await packageBytes(seedSource), basePolicy);
    data.stagingGate = gate;
    data.stagingStarted = () => {
      events.push("outer-staging");
      started.release();
    };
    data.readStarted = (name) => {
      if (name === seed.packageSha256) events.push("nested-read");
    };

    const policy = policyWithFeatureIdGetter(() => {
      nested = cache!.read(seed.packageSha256, basePolicy);
      operations.push(nested);
      return basePolicy.featureId;
    });
    const outer = cache.put(
      await packageBytes("fifo-outer\r\n"),
      policy,
    );
    operations.push(outer);
    await started.promise;
    assertEquals(events, ["outer-staging"]);
    assert(nested, "policy getter admitted the nested read");

    let nestedSettled = false;
    void nested.then(() => nestedSettled = true, () => nestedSettled = true);
    await Promise.resolve();
    assert(!nestedSettled, "nested read waits behind the outer operation");

    gate.release();
    await outer;
    const result = await nested;
    assertEquals((result as { readonly source: string }).source, seedSource);
    assertEquals(events, ["outer-staging", "nested-read"]);
  } finally {
    gate.release();
    started.release();
    await Promise.allSettled(operations);
    await cache?.close();
  }
});

Deno.test("throwing snapshot after close drains safely without unhandled rejection", async () => {
  const data = state();
  const gate = deferred();
  const started = deferred();
  const storage = new MemoryStorage(data);
  const operations: Promise<unknown>[] = [];
  const unhandled: unknown[] = [];
  const onUnhandled = (event: PromiseRejectionEvent): void => {
    unhandled.push(event.reason);
    event.preventDefault();
  };
  let cache: Awaited<ReturnType<typeof openFeaturePackageCache>> | undefined;
  let reopened: Awaited<ReturnType<typeof openFeaturePackageCache>> | undefined;
  let duplicate:
    | Awaited<ReturnType<typeof openFeaturePackageCache>>
    | undefined;
  let close: Promise<void> | undefined;
  globalThis.addEventListener("unhandledrejection", onUnhandled);
  try {
    cache = await openFeaturePackageCache(
      basePolicy.featureId,
      "profile/snapshot-throws",
      storage,
    );
    data.stagingGate = gate;
    data.stagingStarted = started.release;
    const accepted = cache.put(
      await packageBytes("snapshot-throws-first\r\n"),
      basePolicy,
    );
    operations.push(accepted);
    await started.promise;

    const policy = policyWithFeatureIdGetter(() => {
      close = cache!.close();
      throw new Error("policy getter failure");
    });
    const failed = cache.put(
      await packageBytes("snapshot-throws-second\r\n"),
      policy,
    );
    operations.push(failed);
    const failure = failed.then(
      () => new Error("expected snapshot preparation to fail"),
      (error: unknown) => error,
    );
    assert(close, "policy getter closes the cache before throwing");

    let closeSettled = false;
    void close.then(() => closeSettled = true);
    assert(close === cache.close(), "repeated close returns the same promise");
    await Promise.resolve();
    assert(!closeSettled, "close waits for the held predecessor");

    let duplicateError: unknown;
    try {
      duplicate = await openFeaturePackageCache(
        basePolicy.featureId,
        "profile/snapshot-throws",
        new MemoryStorage(state()),
      );
    } catch (error: unknown) {
      duplicateError = error;
    }
    assert(duplicateError instanceof FeaturePackageCacheError);
    assertEquals(duplicateError.code, "cache-owner-active");

    await new Promise<void>((resolve) => setTimeout(resolve, 0));
    assertEquals(
      unhandled,
      [],
      "preparation does not reject a waiting deferred",
    );
    gate.release();
    await accepted;
    const error = await failure;
    assert(error instanceof FeaturePackageError);
    assertEquals(error.code, "invalid-policy");
    await close;
    await new Promise<void>((resolve) => setTimeout(resolve, 0));
    assertEquals(unhandled, [], "no unhandled rejection after queue drain");
    reopened = await openFeaturePackageCache(
      basePolicy.featureId,
      "profile/snapshot-throws",
      storage,
    );
  } finally {
    gate.release();
    started.release();
    await Promise.allSettled(operations);
    await close;
    await cache?.close();
    await duplicate?.close();
    await reopened?.close();
    await new Promise<void>((resolve) => setTimeout(resolve, 0));
    const unhandledAfterCleanup = [...unhandled];
    globalThis.removeEventListener("unhandledrejection", onUnhandled);
    assertEquals(
      unhandledAfterCleanup,
      [],
      "cleanup does not hide unhandled rejections",
    );
  }
});

Deno.test("nested proxy prototype traps become bounded cache errors", async () => {
  const storage = new MemoryStorage(state());
  let cache: Awaited<ReturnType<typeof openFeaturePackageCache>> | undefined;
  let reopened: Awaited<ReturnType<typeof openFeaturePackageCache>> | undefined;
  try {
    cache = await openFeaturePackageCache(
      basePolicy.featureId,
      "profile/proxy-error-normalization",
      storage,
    );
    const rawMessage = "nested prototype trap detail";
    const proxyB = new Proxy({}, {
      getPrototypeOf() {
        throw new Error(rawMessage);
      },
    });
    const proxyA = new Proxy({}, {
      getPrototypeOf() {
        throw proxyB;
      },
    });
    const policy = policyWithFeatureIdGetter(() => {
      throw proxyA;
    });

    let rejected: unknown;
    try {
      await cache.put(
        await packageBytes("proxy-error-normalization\r\n"),
        policy,
      );
    } catch (error: unknown) {
      rejected = error;
    }
    assert(rejected instanceof FeaturePackageCacheError);
    assertEquals(rejected.code, "cache-storage");
    assertEquals(rejected.message, "Package cache storage operation failed.");
    assert(!rejected.message.includes(rawMessage));

    const closing = cache.close();
    let timer: ReturnType<typeof setTimeout> | undefined;
    const closeSettled = await Promise.race([
      closing.then(() => true, () => false),
      new Promise<boolean>((resolve) => {
        timer = setTimeout(() => resolve(false), 100);
      }),
    ]);
    if (timer !== undefined) clearTimeout(timer);
    assert(closeSettled, "close drains the failed preparation node");
    reopened = await openFeaturePackageCache(
      basePolicy.featureId,
      "profile/proxy-error-normalization",
      storage,
    );
  } finally {
    await reopened?.close();
    if (cache) void cache.close().catch(() => undefined);
  }
});

Deno.test("opening reserves ownership before initialization and releases failed reservations", async () => {
  const data = state();
  const gate = deferred();
  data.initializeGate = gate;
  const storage = new MemoryStorage(data);
  const opening = openFeaturePackageCache(
    basePolicy.featureId,
    "profile/opening",
    storage,
  );
  try {
    let duplicateCode = "";
    try {
      openFeaturePackageCache(
        basePolicy.featureId,
        "profile/opening",
        new MemoryStorage(state()),
      );
    } catch (error: unknown) {
      if (error instanceof FeaturePackageCacheError) {
        duplicateCode = error.code;
      }
    }
    assertEquals(duplicateCode, "cache-owner-active");
    gate.release();
    const cache = await opening;
    await cache.close();

    data.failurePoints.add("initialize");
    await expectCode(
      openFeaturePackageCache(
        basePolicy.featureId,
        "profile/failed-open",
        storage,
      ),
      "cache-storage",
    );
    data.failurePoints.delete("initialize");
    const reopened = await openFeaturePackageCache(
      basePolicy.featureId,
      "profile/failed-open",
      storage,
    );
    await reopened.close();
  } finally {
    gate.release();
    await opening.then((cache) => cache.close(), () => undefined);
  }
});
