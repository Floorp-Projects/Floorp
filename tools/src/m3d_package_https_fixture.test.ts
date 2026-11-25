// SPDX-License-Identifier: MPL-2.0
/// <reference lib="deno.ns" />

import { assert, assertEquals, assertRejects } from "@std/assert";
import { startM3dPackageHttpsFixture } from "./m3d_package_https_fixture.ts";
import type { M3dPackageHttpsMode } from "./m3d_package_https_fixture.types.ts";

const encoder = new TextEncoder();
const FEATURE_ID = "fixture";
const COOKIE_SEED_PATH = "/__m3d/cookie/seed";
const COOKIE_CONTROL_PATH = "/__m3d/cookie/control";

function packagePath(hash: string, featureId = FEATURE_ID): string {
  return `/floorp-feature-packages/v1/${featureId}/${hash}.json`;
}

function packageBytes(version: string): Uint8Array {
  return encoder.encode(JSON.stringify({
    formatVersion: 1,
    manifest: { featureId: FEATURE_ID, version },
    source: version,
  }));
}

async function hash(bytes: Uint8Array): Promise<string> {
  const copy = new Uint8Array(bytes.length);
  copy.set(bytes);
  const digest = new Uint8Array(
    await crypto.subtle.digest("SHA-256", copy.buffer),
  );
  return Array.from(digest, (byte) => byte.toString(16).padStart(2, "0"))
    .join("");
}

function headersAreOnlyBooleans(value: unknown): boolean {
  if (typeof value !== "object" || value === null) return false;
  const record = value as Record<string, unknown>;
  return [
    "cookiePresent",
    "seedCookieMarkerMatch",
    "acquisitionCookieMarkerMatch",
    "authorizationPresent",
    "authorizationMarkerMatch",
    "referrerPresent",
    "referrerMarkerMatch",
  ].every((key) => typeof record[key] === "boolean");
}

async function withFixture(
  operation: (
    fixture: Awaited<ReturnType<typeof startM3dPackageHttpsFixture>>,
    packages: ReadonlyMap<string, Uint8Array>,
  ) => Promise<void>,
): Promise<void> {
  const one = packageBytes("signed-package-one");
  const two = packageBytes("signed-package-two");
  const packages = new Map([[await hash(one), one], [await hash(two), two]]);
  const fixture = await startM3dPackageHttpsFixture(packages);
  try {
    await operation(fixture, packages);
  } finally {
    const cleanup = await fixture.close();
    assertEquals(cleanup.closed, true);
    assertEquals(cleanup.listenersClosed, true);
    assertEquals(cleanup.activeConnectionsClosed, true);
    assertEquals(cleanup.stallsReleased, true);
    assertEquals(cleanup.acceptLoopsSettled, true);
    assertEquals(cleanup.cleanupErrorCount, 0);
  }
}

async function request(
  fixture: Awaited<ReturnType<typeof startM3dPackageHttpsFixture>>,
  client: Deno.HttpClient,
  mode: M3dPackageHttpsMode,
  path: string,
  origin = fixture.trustedOrigin,
  init: RequestInit = {},
): Promise<Response> {
  fixture.setMode(mode);
  return await fetch(`${origin}${path}`, {
    ...init,
    client,
    redirect: init.redirect ?? "follow",
    cache: "no-store",
  });
}

async function requestPackage(
  fixture: Awaited<ReturnType<typeof startM3dPackageHttpsFixture>>,
  client: Deno.HttpClient,
  mode: M3dPackageHttpsMode,
  hash: string,
  origin = fixture.trustedOrigin,
  init: RequestInit = {},
): Promise<Response> {
  return await request(
    fixture,
    client,
    mode,
    packagePath(hash),
    origin,
    init,
  );
}

async function waitFor(
  predicate: () => boolean,
  timeoutMs = 5_000,
): Promise<boolean> {
  const deadline = performance.now() + timeoutMs;
  while (performance.now() < deadline) {
    if (predicate()) return true;
    await new Promise<void>((resolve) => setTimeout(resolve, 10));
  }
  return predicate();
}

Deno.test({
  name:
    "M3d HTTPS fixture proves trusted and untrusted TLS behavior (Linux/OpenSSL 3)",
  ignore: Deno.build.os !== "linux",
  permissions: {
    read: true,
    write: true,
    run: ["openssl"],
    net: ["127.0.0.1"],
  },
  fn: async () =>
    await withFixture(async (fixture, packages) => {
      assertEquals("keyPem" in fixture, false);
      assert(fixture.caCertificatePem.includes("BEGIN CERTIFICATE"));
      const client = Deno.createHttpClient({
        caCerts: [fixture.caCertificatePem],
      });
      try {
        const [expectedHash, expectedBytes] = packages.entries().next().value!;
        const trusted = await requestPackage(
          fixture,
          client,
          "valid-streaming",
          expectedHash,
        );
        assertEquals(trusted.status, 200);
        assertEquals(
          trusted.headers.get("content-type"),
          "application/octet-stream",
        );
        assert(
          trusted.headers.get("set-cookie")?.startsWith(
            "m3d_acquisition_cookie=acquisition-marker;",
          ),
        );
        assertEquals(
          new Uint8Array(await trusted.arrayBuffer()),
          expectedBytes,
        );
        await assertRejects(() =>
          requestPackage(
            fixture,
            client,
            "valid-streaming",
            expectedHash,
            fixture.untrustedOrigin,
          )
        );
        const snapshot = fixture.snapshot();
        assertEquals(snapshot.requests, 1);
        assertEquals(snapshot.observations.length, 1);
        assert(snapshot.observations.every(headersAreOnlyBooleans));
        assertEquals(snapshot.observations[0].origin, "trusted");
        assertEquals(snapshot.observations[0].cookiePresent, false);
        assertEquals(snapshot.observations[0].authorizationPresent, false);
        assertEquals(snapshot.observations[0].referrerPresent, false);
        assertEquals(
          snapshot.ports.trusted === snapshot.ports.untrusted,
          false,
        );
        assertEquals(snapshot.ports.raw === snapshot.ports.trusted, false);
        assertEquals(new Set(Object.values(snapshot.ports)).size, 3);
      } finally {
        client.close();
      }
    }),
});

Deno.test({
  name:
    "M3d HTTPS fixture routes bounded response and raw HTTP modes (Linux/OpenSSL 3)",
  ignore: Deno.build.os !== "linux",
  permissions: {
    read: true,
    write: true,
    run: ["openssl"],
    net: ["127.0.0.1"],
  },
  fn: async () =>
    await withFixture(async (fixture, packages) => {
      const client = Deno.createHttpClient({
        caCerts: [fixture.caCertificatePem],
      });
      try {
        const [hashOne, bytesOne] = packages.entries().next().value!;
        const [, bytesTwo] = [...packages.entries()][1];
        const packageRequest = (
          mode: M3dPackageHttpsMode,
          origin = fixture.trustedOrigin,
          init: RequestInit = {},
        ) => requestPackage(fixture, client, mode, hashOne, origin, init);

        const redirect = await packageRequest("redirect");
        assertEquals(redirect.status, 204);
        assertEquals(fixture.snapshot().redirectTargets, 1);

        assertEquals(
          (await packageRequest("valid-streaming", fixture.trustedOrigin, {
            method: "POST",
          })).status,
          405,
        );
        assertEquals(
          (await request(fixture, client, "valid-streaming", "/package"))
            .status,
          404,
        );
        assertEquals(
          (await request(
            fixture,
            client,
            "valid-streaming",
            packagePath(hashOne, "Fixture"),
          )).status,
          404,
        );
        assertEquals(
          (await request(
            fixture,
            client,
            "valid-streaming",
            packagePath("0".repeat(64)),
          )).status,
          404,
        );

        assertEquals(
          (await packageRequest("status-rejected")).status,
          503,
        );
        assertEquals(
          (await packageRequest("content-type-rejected")).headers.get(
            "content-type",
          ),
          "text/plain",
        );
        const encoded = await packageRequest("content-encoding-rejected");
        assertEquals(encoded.headers.get("content-encoding"), "gzip");
        const declared = await packageRequest("oversized-declared");
        assertEquals(declared.status, 200);
        assertEquals(
          declared.headers.get("content-length"),
          String(1024 * 1024 + 1),
        );
        assertEquals(
          (await declared.arrayBuffer()).byteLength,
          1024 * 1024 + 1,
        );
        await assertRejects(() => declared.arrayBuffer());
        const oversized = await packageRequest("oversized-streamed");
        assertEquals(
          (await oversized.arrayBuffer()).byteLength,
          1024 * 1024 + 1,
        );
        await assertRejects(async () => {
          const response = await packageRequest("truncated-length");
          await response.arrayBuffer();
        });
        await assertRejects(async () => {
          const response = await packageRequest("read-failure");
          await response.arrayBuffer();
        });

        const wrong = await packageRequest("wrong-package");
        assertEquals(new Uint8Array(await wrong.arrayBuffer()), bytesTwo);
        assertEquals(bytesOne.length > 0, true);

        const stalledResponse = await packageRequest("stalled-body");
        const reader = stalledResponse.body!.getReader();
        const first = await reader.read();
        assertEquals(first.done, false);
        assertEquals(fixture.snapshot().activeStreams, 1);
        await fixture.releaseStalls();
        const rest: number[] = [...first.value!];
        for (;;) {
          const item = await reader.read();
          if (item.done) break;
          rest.push(...item.value);
        }
        assertEquals(new Uint8Array(rest), bytesOne);
        assertEquals(fixture.snapshot().activeStreams, 0);

        const abortController = new AbortController();
        const abortedResponse = await packageRequest(
          "stalled-body",
          fixture.trustedOrigin,
          { signal: abortController.signal },
        );
        const abortedReader = abortedResponse.body!.getReader();
        assertEquals((await abortedReader.read()).done, false);
        assertEquals(
          await waitFor(() => fixture.snapshot().activeStreams === 1),
          true,
        );
        abortController.abort();
        await assertRejects(() => abortedReader.read());
        assertEquals(
          await waitFor(() => {
            const state = fixture.snapshot();
            return state.activeStreams === 0 && state.activeConnections === 0;
          }),
          true,
        );
        assertEquals(fixture.snapshot().cancellations, 1);

        const equal = await packageRequest(
          "equal-content-length",
          fixture.rawOrigin,
        )
          .then(async (response) =>
            `response:${(await response.arrayBuffer()).byteLength}`
          )
          .catch(() => "rejected");
        const conflicting = await packageRequest(
          "conflicting-content-length",
          fixture.rawOrigin,
        )
          .then(async (response) =>
            `response:${(await response.arrayBuffer()).byteLength}`
          )
          .catch(() => "rejected");
        assert(equal === "rejected" || equal === `response:${bytesOne.length}`);
        assertEquals(conflicting, "rejected");

        const seed = await request(
          fixture,
          client,
          "cookie-seed",
          COOKIE_SEED_PATH,
        );
        assert(
          seed.headers.get("set-cookie")?.startsWith(
            "m3d_seed_cookie=seed-marker;",
          ),
        );
        await request(
          fixture,
          client,
          "cookie-control",
          COOKIE_CONTROL_PATH,
          fixture.trustedOrigin,
          {
            headers: {
              cookie: "m3d_seed_cookie=seed-marker",
              authorization: "Bearer m3d-fixture-marker",
              referer: "https://m3d-fixture.invalid/marker",
            },
          },
        );
        const snapshot = fixture.snapshot();
        assertEquals(
          snapshot.observations.some((item) => item.seedCookieMarkerMatch),
          true,
        );
        assertEquals(
          snapshot.observations.some((item) =>
            item.acquisitionCookieMarkerMatch
          ),
          false,
        );
        assertEquals(
          snapshot.observations.some((item) => item.authorizationMarkerMatch),
          true,
        );
        assertEquals(
          snapshot.observations.some((item) => item.referrerMarkerMatch),
          true,
        );
        const controlObservation = snapshot.observations.find((item) =>
          item.mode === "cookie-control"
        );
        assert(controlObservation);
        assertEquals(controlObservation.cookiePresent, true);
        assertEquals(controlObservation.authorizationPresent, true);
        assertEquals(controlObservation.referrerPresent, true);
        assertEquals(
          JSON.stringify(snapshot).includes("m3d_seed_cookie="),
          false,
        );
        assertEquals(
          JSON.stringify(snapshot).includes("m3d_acquisition_cookie="),
          false,
        );
        assertEquals(
          JSON.stringify(snapshot).includes("Bearer m3d-fixture-marker"),
          false,
        );
        assertEquals(
          JSON.stringify(snapshot).includes("m3d-fixture.invalid"),
          false,
        );
        assertEquals(snapshot.observationOverflow, false);
        const observedModes = new Set(
          snapshot.observations.map((item) => item.mode),
        );
        for (
          const mode of [
            "redirect",
            "status-rejected",
            "content-type-rejected",
            "content-encoding-rejected",
            "oversized-declared",
            "oversized-streamed",
            "truncated-length",
            "wrong-package",
            "stalled-body",
            "read-failure",
            "equal-content-length",
            "conflicting-content-length",
            "cookie-seed",
            "cookie-control",
          ] as const
        ) assert(observedModes.has(mode), mode);
        const detached = fixture.snapshot();
        Reflect.set(detached.observations[0], "cookiePresent", true);
        assertEquals(fixture.snapshot().observations[0].cookiePresent, false);
        const closingResponse = await packageRequest("stalled-body");
        const closingReader = closingResponse.body!.getReader();
        assertEquals((await closingReader.read()).done, false);
        assertEquals(
          await waitFor(() => fixture.snapshot().activeStreams === 1),
          true,
        );
        const cleanup = await fixture.close();
        assertEquals(cleanup.closed, true);
        assertEquals(cleanup.activeConnectionsClosed, true);
        assertEquals(cleanup.stallsReleased, true);
        assertEquals(fixture.snapshot().cancellations, 1);
      } finally {
        client.close();
      }
    }),
});
