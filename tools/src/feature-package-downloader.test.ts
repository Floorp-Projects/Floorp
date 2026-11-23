// SPDX-License-Identifier: MPL-2.0
/// <reference lib="deno.ns" />

import { assert, assertEquals, assertNotEquals } from "@std/assert";
import type { Deferred } from "./feature-package-downloader-test.types.ts";
import {
  FeaturePackageDownloader,
  FeaturePackageDownloadError,
} from "../../browser-features/modules/modules/feature-host/package/FeaturePackageDownloader.ts";
import type {
  FeaturePackageDownloaderPlatform,
  FeaturePackageDownloadRequest,
} from "../../browser-features/modules/modules/feature-host/package/FeaturePackageDownload.types.ts";
import {
  FeaturePackageError,
  signFeaturePackage,
  verifySignedFeaturePackage,
} from "../../browser-features/modules/modules/feature-host/package/FeaturePackageVerifier.ts";
import type {
  FeaturePackageAuthoringMetadata,
  FeaturePackagePolicy,
} from "../../browser-features/modules/modules/feature-host/package/types.ts";

const encoder = new TextEncoder();
const decoder = new TextDecoder();
const pair = await crypto.subtle.generateKey({ name: "Ed25519" }, true, [
  "sign",
  "verify",
]) as CryptoKeyPair;
const publicKey = encodeBase64Url(
  new Uint8Array(await crypto.subtle.exportKey("raw", pair.publicKey)),
);
const policy: FeaturePackagePolicy = {
  featureId: "hub-panel-menu",
  hostSchema: 2,
  hostBuildId: "floorp-host-1",
  runtimeBuildId: "runtime-1",
  allowedCapabilities: ["hub-panel"],
  minimumReleaseSequence: 1,
  trustedKeys: [{ keyId: "test-key", publicKey }],
};
const metadata: FeaturePackageAuthoringMetadata = {
  featureId: policy.featureId,
  version: "v1.0",
  releaseSequence: 3,
  compatibility: {
    hostSchema: policy.hostSchema,
    hostBuildId: policy.hostBuildId,
    runtimeBuildIds: [policy.runtimeBuildId],
    capabilities: ["hub-panel"],
  },
};
const validBytes = await signFeaturePackage(
  metadata,
  "const packageSource = 'exact';\n",
  "test-key",
  pair.privateKey,
);
const validHash = (await verifySignedFeaturePackage(validBytes, policy))
  .packageSha256;
const alternateBytes = await signFeaturePackage(
  metadata,
  "const packageSource = 'alternate';\n",
  "test-key",
  pair.privateKey,
);
const alternateHash = (await verifySignedFeaturePackage(alternateBytes, policy))
  .packageSha256;

function deferred<T>(): Deferred<T> {
  let resolve!: (value: T) => void;
  let reject!: (reason?: unknown) => void;
  const promise = new Promise<T>((resolvePromise, rejectPromise) => {
    resolve = resolvePromise;
    reject = rejectPromise;
  });
  return { promise, resolve, reject };
}

function encodeBase64Url(bytes: Uint8Array): string {
  let binary = "";
  for (const byte of bytes) binary += String.fromCharCode(byte);
  return btoa(binary).replaceAll("+", "-").replaceAll("/", "_").replace(
    /=+$/,
    "",
  );
}

function request(
  hash = validHash,
  requestPolicy: FeaturePackagePolicy = policy,
  signal?: AbortSignal,
): FeaturePackageDownloadRequest {
  return {
    origin: "https://packages.example",
    packageSha256: hash,
    policy: {
      ...requestPolicy,
      allowedCapabilities: [...requestPolicy.allowedCapabilities],
      trustedKeys: requestPolicy.trustedKeys.map((trustedKey) => ({
        ...trustedKey,
      })),
    },
    ...(signal ? { signal } : {}),
  };
}

function streamOf(bytes: Uint8Array): ReadableStream<Uint8Array> {
  return new ReadableStream<Uint8Array>({
    start(controller) {
      controller.enqueue(bytes.slice());
      controller.close();
    },
  });
}

function makeResponse(
  url: string,
  body: ReadableStream<Uint8Array> | null = streamOf(validBytes),
  options: {
    status?: number;
    responseUrl?: string;
    type?: string;
    redirected?: boolean;
    headers?: Record<string, string | null>;
  } = {},
): Response {
  const headers = new Headers({
    "content-type": "application/json; charset=utf-8",
    "content-length": String(validBytes.byteLength),
  });
  for (const [name, value] of Object.entries(options.headers ?? {})) {
    if (value === null) headers.delete(name);
    else headers.set(name, value);
  }
  const response = new Response(body, {
    status: options.status ?? 200,
    headers,
  });
  Object.defineProperties(response, {
    url: { configurable: true, value: options.responseUrl ?? url },
    type: { configurable: true, value: options.type ?? "basic" },
    redirected: { configurable: true, value: options.redirected ?? false },
  });
  return response;
}

class TestPlatform implements FeaturePackageDownloaderPlatform {
  clock = 0;
  nextTimer = 0;
  readonly timers = new Map<number, { callback: () => void; delay: number }>();
  readonly calls: { url: string; options: RequestInit }[] = [];
  handler: (url: string, options: RequestInit) => Promise<Response> = (url) =>
    Promise.resolve(makeResponse(url));
  failTimer = false;
  failClock = false;

  fetch(url: string, options: RequestInit): Promise<Response> {
    this.calls.push({ url, options });
    return this.handler(url, options);
  }

  now(): number {
    if (this.failClock) return Number.NaN;
    return this.clock;
  }

  setTimeout(callback: () => void, delayMs: number): number {
    if (this.failTimer) throw new Error("timer failure");
    const id = ++this.nextTimer;
    this.timers.set(id, { callback, delay: delayMs });
    return id;
  }

  clearTimeout(handle: number): void {
    this.timers.delete(handle);
  }

  fireTimer(id = this.timers.keys().next().value): void {
    if (id === undefined) return;
    const timer = this.timers.get(id);
    if (!timer) return;
    this.timers.delete(id);
    timer.callback();
  }
}

function makeDownloader(platform = new TestPlatform()): {
  downloader: FeaturePackageDownloader;
  platform: TestPlatform;
} {
  return { downloader: new FeaturePackageDownloader(platform), platform };
}

async function rejectsWith(
  promise: Promise<unknown>,
  code: string,
): Promise<FeaturePackageDownloadError> {
  let caught: unknown;
  try {
    await promise;
  } catch (error: unknown) {
    caught = error;
  }
  assert(caught instanceof FeaturePackageDownloadError);
  assertEquals(caught.code, code);
  assert(caught.message.length < 128);
  assertEquals("cause" in caught, false);
  return caught;
}

async function eventLoopTurn(): Promise<void> {
  await new Promise<void>((resolve) => setTimeout(resolve, 0));
}

async function waitForCondition(condition: () => boolean): Promise<void> {
  const deadline = performance.now() + 5_000;
  while (!condition()) {
    if (performance.now() >= deadline) {
      throw new Error("Timed out waiting for downloader state");
    }
    await new Promise<void>((resolve) => setTimeout(resolve, 1));
  }
}

function expectedUrl(hash = validHash): string {
  return `https://packages.example/floorp-feature-packages/v1/${policy.featureId}/${hash}.json`;
}

Deno.test("downloader returns detached verified bytes with fixed private request options", async () => {
  const { downloader, platform } = makeDownloader();
  const result = await downloader.downloadFeaturePackageBytes(request());
  assertEquals(result, validBytes);
  assert(result.buffer !== validBytes.buffer);
  assertEquals(platform.calls.length, 1);
  assertEquals(platform.calls[0].url, expectedUrl());
  const options = platform.calls[0].options;
  assertEquals(options.method, "GET");
  assertEquals(options.credentials, "omit");
  assertEquals(options.redirect, "error");
  assertEquals(options.referrerPolicy, "no-referrer");
  assertEquals(options.cache, "no-store");
  assertEquals(options.headers, undefined);
  assert(options.signal instanceof AbortSignal);
  assertEquals(downloader.diagnosticsForTests.inFlight, 0);
  result.fill(0);
  assertEquals(validBytes[0], 123);
});

Deno.test("downloader rejects a different valid package and malformed signed envelopes", async () => {
  const { downloader, platform } = makeDownloader();
  platform.handler = (url) =>
    Promise.resolve(makeResponse(url, streamOf(alternateBytes), {
      headers: { "content-length": String(alternateBytes.length) },
    }));
  await rejectsWith(
    downloader.downloadFeaturePackageBytes(request()),
    "expected-package-mismatch",
  );
  platform.handler = (url) =>
    Promise.resolve(makeResponse(url, streamOf(alternateBytes), {
      headers: { "content-length": String(alternateBytes.length) },
    }));
  assertEquals(
    await downloader.downloadFeaturePackageBytes(request(alternateHash)),
    alternateBytes,
  );

  const invalidSignature = JSON.parse(decoder.decode(validBytes)) as Record<
    string,
    unknown
  >;
  invalidSignature.signature = "A".repeat(86);
  const malformed = [
    encoder.encode(JSON.stringify(invalidSignature)),
    new Uint8Array([0xc3, 0x28]),
    encoder.encode(` ${decoder.decode(validBytes)}`),
  ];
  for (const bytes of malformed) {
    platform.handler = (url) =>
      Promise.resolve(makeResponse(url, streamOf(bytes), {
        headers: { "content-length": String(bytes.length) },
      }));
    const error = await rejectsWith(
      downloader.downloadFeaturePackageBytes(request()),
      "package-rejected",
    );
    assert(
      error.verifierCode === "invalid-signature" ||
        error.verifierCode === "invalid-envelope",
    );
  }
  const incompatible = { ...policy, featureId: "another-feature" };
  platform.handler = (url) => Promise.resolve(makeResponse(url));
  const policyError = await rejectsWith(
    downloader.downloadFeaturePackageBytes(request(validHash, incompatible)),
    "package-rejected",
  );
  assertEquals(policyError.verifierCode, "feature-mismatch");
});

Deno.test("downloader snapshots request and policy and rejects hostile preflight input", async () => {
  const { downloader, platform } = makeDownloader();
  const mutable = request() as {
    origin: string;
    packageSha256: string;
    policy: FeaturePackagePolicy;
  };
  mutable.policy = {
    ...policy,
    allowedCapabilities: [...policy.allowedCapabilities],
    trustedKeys: policy.trustedKeys.map((trustedKey) => ({ ...trustedKey })),
  };
  const response = deferred<Response>();
  platform.handler = () => response.promise;
  const pending = downloader.downloadFeaturePackageBytes(mutable);
  mutable.origin = "http://attacker.invalid";
  mutable.packageSha256 = "0".repeat(64);
  (mutable.policy as { featureId: string }).featureId = "changed";
  assertEquals(platform.calls[0].url, expectedUrl());
  response.resolve(makeResponse(expectedUrl()));
  assertEquals(await pending, validBytes);

  let getterReads = 0;
  const throwing = {
    packageSha256: validHash,
    policy: request().policy,
    get origin() {
      getterReads++;
      throw new Error("hostile getter");
    },
  };
  const getterError = await rejectsWith(
    downloader.downloadFeaturePackageBytes(
      throwing as unknown as FeaturePackageDownloadRequest,
    ),
    "invalid-request",
  );
  assertEquals(getterReads, 1);
  assertEquals(getterError.message, "Package download request is invalid.");

  const brandedError = new FeaturePackageDownloadError("invalid-request");
  Object.defineProperties(brandedError, {
    code: { configurable: true, value: "package-rejected" },
    message: { configurable: true, value: "attacker-controlled message" },
    cause: { configurable: true, value: "attacker-controlled cause" },
  });
  const proxyTarget = request();
  let brandedDescriptorReads = 0;
  const brandedProxy = new Proxy(proxyTarget, {
    getOwnPropertyDescriptor(target, key) {
      if (key === "origin") {
        brandedDescriptorReads++;
        throw brandedError;
      }
      return Reflect.getOwnPropertyDescriptor(target, key);
    },
  });
  const normalizedProxyError = await rejectsWith(
    downloader.downloadFeaturePackageBytes(brandedProxy),
    "invalid-request",
  );
  assertEquals(brandedDescriptorReads, 1);
  assertEquals(
    normalizedProxyError.message,
    "Package download request is invalid.",
  );
  assertEquals(Object.hasOwn(normalizedProxyError, "cause"), false);

  const revoked = Proxy.revocable({}, {});
  revoked.revoke();
  const revokedError = await rejectsWith(
    downloader.downloadFeaturePackageBytes(
      revoked.proxy as FeaturePackageDownloadRequest,
    ),
    "invalid-request",
  );
  assertEquals(revokedError.message, "Package download request is invalid.");

  const nestedDownloadError = new FeaturePackageDownloadError(
    "invalid-request",
  );
  Object.defineProperties(nestedDownloadError, {
    code: { configurable: true, value: "network-failure" },
    message: { configurable: true, value: "attacker-controlled message" },
    cause: { configurable: true, value: "attacker-controlled cause" },
  });
  let codeGetterReads = 0;
  const nestedVerifierError = new Proxy(
    new FeaturePackageError("invalid-policy"),
    {
      get(target, key, receiver) {
        if (key === "code") {
          codeGetterReads++;
          throw nestedDownloadError;
        }
        return Reflect.get(target, key, receiver);
      },
    },
  );
  const hostilePolicy = new Proxy(policy, {
    get(target, key, receiver) {
      if (key === "featureId") throw nestedVerifierError;
      return Reflect.get(target, key, receiver);
    },
  });
  const policyError = await rejectsWith(
    downloader.downloadFeaturePackageBytes({
      ...request(),
      policy: hostilePolicy,
    }),
    "invalid-policy",
  );
  assertEquals(policyError.message, "Package download policy is invalid.");
  assertEquals(codeGetterReads, 0);
  assertEquals(downloader.diagnosticsForTests.inFlight, 0);
  for (
    const origin of [
      "http://packages.example",
      "https://packages.example/",
      "https://user@packages.example",
      "https://packages.example/path",
      "https://packages.example?x=1",
      "https://packages.example#fragment",
      "https://packages.example ",
    ]
  ) {
    await rejectsWith(
      downloader.downloadFeaturePackageBytes({ ...request(), origin }),
      "invalid-endpoint",
    );
  }
  await rejectsWith(
    downloader.downloadFeaturePackageBytes({
      ...request(),
      packageSha256: validHash.toUpperCase(),
    }),
    "invalid-request",
  );
  await rejectsWith(
    downloader.downloadFeaturePackageBytes(
      { ...request(), extra: true } as FeaturePackageDownloadRequest,
    ),
    "invalid-request",
  );
  assertEquals(downloader.diagnosticsForTests.inFlight, 0);
});

Deno.test("request getters cannot bypass the reservation during reentry", async () => {
  const { downloader, platform } = makeDownloader();
  const fetches = [deferred<Response>(), deferred<Response>()];
  platform.handler = () => fetches[platform.calls.length - 1].promise;
  let nested: Promise<Uint8Array> | undefined;
  const reentrant = {
    get origin() {
      nested = downloader.downloadFeaturePackageBytes(request());
      return "https://packages.example";
    },
    packageSha256: validHash,
    policy: request().policy,
  };
  const outer = downloader.downloadFeaturePackageBytes(reentrant);
  assertEquals(platform.calls.length, 2);
  assertEquals(downloader.diagnosticsForTests.inFlight, 2);
  fetches[0].resolve(makeResponse(expectedUrl()));
  fetches[1].resolve(makeResponse(expectedUrl()));
  assertEquals(await outer, validBytes);
  assertEquals(await nested, validBytes);
  assertEquals(downloader.diagnosticsForTests.inFlight, 0);
});

Deno.test("downloader validates response status, URL, redirects, type, and visible header grammar", async () => {
  const cases: Array<Parameters<typeof makeResponse>[2]> = [
    { status: 206 },
    { responseUrl: "https://packages.example/other" },
    { redirected: true },
    { type: "opaque" },
    { type: "opaqueredirect" },
    { type: "error" },
    { type: "default" },
    { type: "unknown" },
    {
      headers: {
        "content-type": "application/json; charset=utf-8; charset=utf-8",
      },
    },
    { headers: { "content-type": "application/json; charset=latin1" } },
    { headers: { "content-type": "application/octet-stream; charset=utf-8" } },
    { headers: { "content-type": "x".repeat(129) } },
    { headers: { "content-encoding": "gzip" } },
    { headers: { "content-encoding": "identity, identity" } },
    { headers: { "content-encoding": "x".repeat(33) } },
    { headers: { "content-length": "01" } },
    { headers: { "content-length": "1, 1" } },
    { headers: { "content-length": "1048577" } },
  ];
  for (const options of cases) {
    const { downloader, platform } = makeDownloader();
    platform.handler = (url) =>
      Promise.resolve(makeResponse(url, undefined, options));
    await rejectsWith(
      downloader.downloadFeaturePackageBytes(request()),
      "unacceptable-response",
    );
    await waitForCondition(() => downloader.diagnosticsForTests.inFlight === 0);
  }

  const { downloader: corsDownloader, platform: corsPlatform } =
    makeDownloader();
  corsPlatform.handler = (url) =>
    Promise.resolve(makeResponse(url, undefined, { type: "cors" }));
  assertEquals(
    await corsDownloader.downloadFeaturePackageBytes(request()),
    validBytes,
  );

  const { downloader, platform } = makeDownloader();
  platform.handler = (url) =>
    Promise.resolve(makeResponse(url, undefined, {
      headers: {
        "content-length": null,
        "content-type": 'Application/JSON ; Charset = "UTF-8"',
        "content-encoding": "\tIDENTITY \t",
      },
    }));
  assertEquals(
    await downloader.downloadFeaturePackageBytes(request()),
    validBytes,
  );
});

Deno.test("downloader bounds chunks, empty reads, and declared length while allowing absent length", async () => {
  const { downloader, platform } = makeDownloader();
  platform.handler = (url) =>
    Promise.resolve(makeResponse(url, undefined, {
      headers: { "content-length": String(validBytes.length + 1) },
    }));
  await rejectsWith(
    downloader.downloadFeaturePackageBytes(request()),
    "unacceptable-response",
  );

  const tooLarge = new Uint8Array(1_048_577);
  platform.handler = (url) =>
    Promise.resolve(makeResponse(url, streamOf(tooLarge), {
      headers: { "content-length": null },
    }));
  await rejectsWith(
    downloader.downloadFeaturePackageBytes(request()),
    "size-limit",
  );

  const emptyStream = new ReadableStream<Uint8Array>({
    start(controller) {
      for (let index = 0; index < 17; index++) {
        controller.enqueue(new Uint8Array());
      }
      controller.close();
    },
  });
  platform.handler = (url) =>
    Promise.resolve(makeResponse(url, emptyStream, {
      headers: { "content-length": "0" },
    }));
  await rejectsWith(
    downloader.downloadFeaturePackageBytes(request()),
    "unacceptable-response",
  );

  platform.handler = (url) =>
    Promise.resolve(makeResponse(url, streamOf(validBytes), {
      headers: { "content-length": null },
    }));
  assertEquals(
    await downloader.downloadFeaturePackageBytes(request()),
    validBytes,
  );
});

Deno.test("caller cancellation survives stopImmediatePropagation and isolates its reason", async () => {
  const { downloader, platform } = makeDownloader();
  const caller = new AbortController();
  const sentinel = { privateCallerReason: true };
  caller.signal.addEventListener(
    "abort",
    (event) => event.stopImmediatePropagation(),
  );
  const pendingFetch = deferred<Response>();
  platform.handler = () => pendingFetch.promise;
  const pending = downloader.downloadFeaturePackageBytes(
    request(validHash, policy, caller.signal),
  );
  const fetchSignal = platform.calls[0].options.signal as AbortSignal;
  caller.abort(sentinel);
  await rejectsWith(pending, "aborted");
  assertEquals(fetchSignal.aborted, true);
  assertNotEquals(fetchSignal.reason, sentinel);
  assertEquals(typeof fetchSignal.reason, "string");
  pendingFetch.resolve(makeResponse(expectedUrl()));
  await waitForCondition(() => downloader.diagnosticsForTests.inFlight === 0);
});

Deno.test("late read results after caller abort cannot publish package bytes", async () => {
  const { downloader, platform } = makeDownloader();
  const caller = new AbortController();
  const read = deferred<ReadableStreamReadResult<Uint8Array>>();
  const cancel = deferred<void>();
  const fakeReader = {
    closed: Promise.resolve(),
    read: () => read.promise,
    cancel: () => cancel.promise,
    releaseLock() {},
  };
  const response = makeResponse(expectedUrl());
  Object.defineProperty(response, "body", {
    configurable: true,
    value: { getReader: () => fakeReader },
  });
  platform.handler = () => Promise.resolve(response);
  const pending = downloader.downloadFeaturePackageBytes(
    request(validHash, policy, caller.signal),
  );
  await waitForCondition(() => downloader.diagnosticsForTests.reading === 1);
  caller.abort("stop");
  await rejectsWith(pending, "aborted");
  read.resolve({ done: false, value: validBytes });
  await waitForCondition(() => downloader.diagnosticsForTests.reading === 0);
  assertEquals(downloader.diagnosticsForTests.verifying, 0);
  assertEquals(downloader.diagnosticsForTests.inFlight, 1);
  cancel.resolve();
  await waitForCondition(() => downloader.diagnosticsForTests.inFlight === 0);
});

Deno.test("four noncooperative fetches retain capacity and the fifth reads no getters", async () => {
  const { downloader, platform } = makeDownloader();
  const pendingFetches = Array.from({ length: 4 }, () => deferred<Response>());
  const controllers = Array.from({ length: 4 }, () => new AbortController());
  platform.handler = () => pendingFetches[platform.calls.length - 1].promise;
  const pending = controllers.map((controller) =>
    downloader.downloadFeaturePackageBytes(
      request(validHash, policy, controller.signal),
    )
  );
  assertEquals(downloader.diagnosticsForTests.inFlight, 4);
  let getterReads = 0;
  const fifth = new Proxy({}, {
    ownKeys() {
      getterReads++;
      return ["origin", "packageSha256", "policy"];
    },
    get() {
      getterReads++;
      return "unexpected";
    },
  });
  await rejectsWith(
    downloader.downloadFeaturePackageBytes(
      fifth as FeaturePackageDownloadRequest,
    ),
    "busy",
  );
  assertEquals(getterReads, 0);
  for (const controller of controllers) controller.abort("stop");
  await Promise.all(pending.map((item) => rejectsWith(item, "aborted")));
  for (const item of pendingFetches) item.resolve(makeResponse(expectedUrl()));
  await waitForCondition(() => downloader.diagnosticsForTests.inFlight === 0);
});

Deno.test("delayed timer dispatch cannot allow late fetch success and early timer reschedules", async () => {
  const { downloader, platform } = makeDownloader();
  const pendingFetch = deferred<Response>();
  platform.handler = () => pendingFetch.promise;
  const pending = downloader.downloadFeaturePackageBytes(request());
  const firstTimer = platform.timers.keys().next().value as number;
  platform.clock = 50;
  platform.fireTimer(firstTimer);
  assertEquals([...platform.timers.values()][0]?.delay, 19_950);
  platform.clock = 20_001;
  pendingFetch.resolve(makeResponse(expectedUrl()));
  await rejectsWith(pending, "timeout");
  await waitForCondition(() => downloader.diagnosticsForTests.inFlight === 0);
  assertEquals(downloader.diagnosticsForTests.inFlight, 0);
  assertEquals(downloader.diagnosticsForTests.verifying, 0);
});

Deno.test("stream read failure remains ordinary while pending cancellation and lock failure stay blocked", async () => {
  const { downloader, platform } = makeDownloader();
  const terminalReason = { streamFailure: true };
  const errored = new ReadableStream<Uint8Array>({
    start(controller) {
      controller.error(terminalReason);
    },
  });
  platform.handler = (url) => Promise.resolve(makeResponse(url, errored));
  await rejectsWith(
    downloader.downloadFeaturePackageBytes(request()),
    "network-failure",
  );
  await waitForCondition(() => downloader.diagnosticsForTests.inFlight === 0);
  assertEquals(downloader.diagnosticsForTests.blocked, 0);
  assertEquals(downloader.diagnosticsForTests.inFlight, 0);

  const sameReason: unknown = undefined;
  const independentFailureReader = {
    closed: Promise.resolve(),
    read: () => Promise.reject(sameReason),
    cancel: () => Promise.reject(sameReason),
    releaseLock() {},
  };
  const independentFailureResponse = makeResponse(expectedUrl());
  Object.defineProperty(independentFailureResponse, "body", {
    configurable: true,
    value: { getReader: () => independentFailureReader },
  });
  platform.handler = () => Promise.resolve(independentFailureResponse);
  await rejectsWith(
    downloader.downloadFeaturePackageBytes(request()),
    "network-failure",
  );
  await waitForCondition(() => downloader.diagnosticsForTests.blocked === 1);
  assertEquals(downloader.diagnosticsForTests.blocked, 1);
  assertEquals(downloader.diagnosticsForTests.inFlight, 1);

  const cancel = deferred<void>();
  const caller = new AbortController();
  const stalled = new ReadableStream<Uint8Array>({
    pull() {
      return new Promise<void>(() => {});
    },
    cancel() {
      return cancel.promise;
    },
  });
  platform.handler = (url) => Promise.resolve(makeResponse(url, stalled));
  const pending = downloader.downloadFeaturePackageBytes(
    request(validHash, policy, caller.signal),
  );
  await waitForCondition(() => downloader.diagnosticsForTests.reading === 1);
  caller.abort("cancel");
  await rejectsWith(pending, "aborted");
  assertEquals(downloader.diagnosticsForTests.inFlight, 2);
  cancel.resolve();
  await waitForCondition(() => downloader.diagnosticsForTests.inFlight === 1);

  const { downloader: lockDownloader, platform: lockPlatform } =
    makeDownloader();
  let reads = 0;
  let cancelCalls = 0;
  const lockCaller = new AbortController();
  const fakeReader = {
    closed: Promise.resolve(),
    read() {
      reads++;
      return Promise.resolve(
        reads === 1
          ? { done: false, value: validBytes }
          : { done: true, value: undefined },
      );
    },
    cancel() {
      cancelCalls++;
      return Promise.resolve();
    },
    releaseLock() {
      throw new Error("lock failure");
    },
  };
  const lockedResponse = makeResponse(expectedUrl());
  Object.defineProperty(lockedResponse, "body", {
    configurable: true,
    value: { getReader: () => fakeReader },
  });
  lockPlatform.handler = () => Promise.resolve(lockedResponse);
  await rejectsWith(
    lockDownloader.downloadFeaturePackageBytes(
      request(validHash, policy, lockCaller.signal),
    ),
    "cleanup-unproven",
  );
  assertEquals(lockDownloader.diagnosticsForTests.blocked, 1);
  assertEquals(lockDownloader.diagnosticsForTests.inFlight, 1);
  const privateFetchSignal = lockPlatform.calls[0].options
    .signal as AbortSignal;
  assertEquals(lockPlatform.timers.size, 0);
  assertEquals(privateFetchSignal.aborted, false);
  lockCaller.abort("later");
  assertEquals(privateFetchSignal.aborted, false);
  assertEquals(cancelCalls, 0);
});

Deno.test("downloader retains capacity while early crypto rejection drains siblings", async () => {
  const { downloader, platform } = makeDownloader();
  const subtle = crypto.subtle;
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
    platform.handler = (url) => Promise.resolve(makeResponse(url));
    const pending = downloader.downloadFeaturePackageBytes(request());
    let settled = false;
    void pending.then(
      () => settled = true,
      () => settled = true,
    );
    await waitForCondition(() =>
      downloader.diagnosticsForTests.verifying === 1
    );
    await eventLoopTurn();
    assertEquals(digestCalls, 2);
    assertEquals(settled, false);
    assertEquals(downloader.diagnosticsForTests.inFlight, 1);
    pendingDigest.resolve(new ArrayBuffer(32));
    await eventLoopTurn();
    assertEquals(settled, false);
    assertEquals(downloader.diagnosticsForTests.inFlight, 1);
    pendingImport.resolve(pair.publicKey);
    await rejectsWith(pending, "unsupported-platform");
    await waitForCondition(() =>
      downloader.diagnosticsForTests.verifying === 0 &&
      downloader.diagnosticsForTests.inFlight === 0
    );
    assertEquals(settled, true);
  } finally {
    if (digestDescriptor) {
      Object.defineProperty(subtle, "digest", digestDescriptor);
    } else Reflect.deleteProperty(subtle, "digest");
    if (importKeyDescriptor) {
      Object.defineProperty(subtle, "importKey", importKeyDescriptor);
    } else {
      Reflect.deleteProperty(subtle, "importKey");
    }
  }
});

Deno.test("deadline covers a pending verification and late verification cannot publish", async () => {
  const { downloader, platform } = makeDownloader();
  const subtle = crypto.subtle;
  const digestDescriptor = Object.getOwnPropertyDescriptor(subtle, "digest");
  const verifyDescriptor = Object.getOwnPropertyDescriptor(subtle, "verify");
  const originalDigest = subtle.digest.bind(subtle);
  const originalVerify = subtle.verify.bind(subtle);
  const pendingDigest = deferred<ArrayBuffer>();
  let delayed = false;
  let delayedDigestResult: Promise<ArrayBuffer> | null = null;
  let signatureVerified = false;
  const patchedDigest: SubtleCrypto["digest"] = (...args) => {
    const result = originalDigest(...args);
    if (!delayed) {
      delayed = true;
      delayedDigestResult = result;
      return pendingDigest.promise;
    }
    return result;
  };
  const patchedVerify: SubtleCrypto["verify"] = async (...args) => {
    signatureVerified = await originalVerify(...args);
    return signatureVerified;
  };
  try {
    Object.defineProperty(subtle, "digest", {
      configurable: true,
      value: patchedDigest,
    });
    Object.defineProperty(subtle, "verify", {
      configurable: true,
      value: patchedVerify,
    });
    platform.handler = (url) => Promise.resolve(makeResponse(url));
    const pending = downloader.downloadFeaturePackageBytes(request());
    await waitForCondition(() =>
      downloader.diagnosticsForTests.verifying === 1
    );
    platform.clock = 20_000;
    const timerId = platform.timers.keys().next().value as number;
    platform.fireTimer(timerId);
    await rejectsWith(pending, "timeout");
    assertEquals(downloader.diagnosticsForTests.inFlight, 1);
    assertEquals(downloader.diagnosticsForTests.verifying, 1);
    assert(delayedDigestResult);
    pendingDigest.resolve(await delayedDigestResult);
    await waitForCondition(() =>
      downloader.diagnosticsForTests.verifying === 0 &&
      downloader.diagnosticsForTests.inFlight === 0
    );
    assertEquals(signatureVerified, true);
  } finally {
    if (digestDescriptor) {
      Object.defineProperty(subtle, "digest", digestDescriptor);
    } else Reflect.deleteProperty(subtle, "digest");
    if (verifyDescriptor) {
      Object.defineProperty(subtle, "verify", verifyDescriptor);
    } else Reflect.deleteProperty(subtle, "verify");
  }
});

Deno.test("ordinary preflight, setup, network, read, signature, and abort failures release all slots", async () => {
  let repeatedGetterReads = 0;
  let repeatedDescriptorTrapCalls = 0;
  const cases: Array<{
    fail(
      downloader: FeaturePackageDownloader,
      platform: TestPlatform,
    ): Promise<unknown>;
    code: string;
  }> = [
    {
      code: "invalid-endpoint",
      fail: (downloader) => {
        return downloader.downloadFeaturePackageBytes({
          ...request(),
          origin: "http://bad.invalid",
        });
      },
    },
    {
      code: "invalid-request",
      fail: (downloader) => {
        const input = {
          packageSha256: validHash,
          policy: request().policy,
          get origin() {
            repeatedGetterReads++;
            throw new Error("hostile getter");
          },
        };
        return downloader.downloadFeaturePackageBytes(
          input as unknown as FeaturePackageDownloadRequest,
        );
      },
    },
    {
      code: "invalid-request",
      fail: (downloader) => {
        const thrown = new FeaturePackageDownloadError("invalid-request");
        Object.defineProperties(thrown, {
          code: { configurable: true, value: "package-rejected" },
          message: { configurable: true, value: "hostile" },
          cause: { configurable: true, value: "hostile" },
        });
        return downloader.downloadFeaturePackageBytes(
          new Proxy(request(), {
            getOwnPropertyDescriptor(target, key) {
              if (key === "origin") {
                repeatedDescriptorTrapCalls++;
                throw thrown;
              }
              return Reflect.getOwnPropertyDescriptor(target, key);
            },
          }),
        );
      },
    },
    {
      code: "unsupported-platform",
      fail: (downloader, platform) => {
        platform.failTimer = true;
        return downloader.downloadFeaturePackageBytes(request());
      },
    },
    {
      code: "network-failure",
      fail: (downloader, platform) => {
        platform.handler = () =>
          Promise.reject(new Error("secret network detail"));
        return downloader.downloadFeaturePackageBytes(request());
      },
    },
    {
      code: "network-failure",
      fail: (downloader, platform) => {
        const stream = new ReadableStream<Uint8Array>({
          start(controller) {
            controller.error(new Error("secret stream detail"));
          },
        });
        platform.handler = (url) => Promise.resolve(makeResponse(url, stream));
        return downloader.downloadFeaturePackageBytes(request());
      },
    },
    {
      code: "package-rejected",
      fail: (downloader, platform) => {
        const bad = JSON.parse(decoder.decode(validBytes)) as Record<
          string,
          unknown
        >;
        bad.signature = "A".repeat(86);
        const bytes = encoder.encode(JSON.stringify(bad));
        platform.handler = (url) =>
          Promise.resolve(makeResponse(url, streamOf(bytes), {
            headers: { "content-length": String(bytes.length) },
          }));
        return downloader.downloadFeaturePackageBytes(request());
      },
    },
    {
      code: "aborted",
      fail: (downloader, platform) => {
        const pendingFetch = deferred<Response>();
        const controller = new AbortController();
        platform.handler = () => pendingFetch.promise;
        const pending = downloader.downloadFeaturePackageBytes(
          request(validHash, policy, controller.signal),
        );
        controller.abort("stop");
        pendingFetch.resolve(makeResponse(expectedUrl()));
        return pending;
      },
    },
  ];
  for (const testCase of cases) {
    const platform = new TestPlatform();
    const downloader = new FeaturePackageDownloader(platform);
    for (let attempt = 0; attempt < 4; attempt++) {
      await rejectsWith(testCase.fail(downloader, platform), testCase.code);
      await waitForCondition(() =>
        downloader.diagnosticsForTests.inFlight === 0
      );
    }
    platform.failTimer = false;
    platform.handler = (url) => Promise.resolve(makeResponse(url));
    const results = await Promise.all(
      Array.from(
        { length: 4 },
        () => downloader.downloadFeaturePackageBytes(request()),
      ),
    );
    for (const result of results) assertEquals(result, validBytes);
    assertEquals(downloader.diagnosticsForTests.inFlight, 0);
  }
  assertEquals(repeatedGetterReads, 4);
  assertEquals(repeatedDescriptorTrapCalls, 4);
});
