// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertThrows } from "@std/assert";

import {
  devOsServerBaseUrl,
  OS_SERVER_PRODUCTION_DEFAULT_PORT,
} from "./port_offset.ts";
import {
  parseWrapperPortOffsetArg,
  resolveOsVerifyOptions,
} from "./os_test_options.ts";

const PRODUCTION_DEFAULT_BASE_URL =
  `http://127.0.0.1:${OS_SERVER_PRODUCTION_DEFAULT_PORT}`;

// ── explicit URL / env precedence ─────────────────────────────

Deno.test("--base-url wins over FLOORP_OS_BASE_URL and derived defaults", () => {
  const resolved = resolveOsVerifyOptions({
    baseUrlFlag: "http://127.0.0.1:9999",
    envBaseUrl: "http://127.0.0.1:8888",
    skipStart: false,
  });
  assertEquals(resolved.baseUrl, "http://127.0.0.1:9999");
});

Deno.test("FLOORP_OS_BASE_URL wins over derived defaults", () => {
  const resolved = resolveOsVerifyOptions({
    envBaseUrl: "http://127.0.0.1:8888",
    skipStart: false,
  });
  assertEquals(resolved.baseUrl, "http://127.0.0.1:8888");
});

Deno.test("blank flag/env values do not count as explicit URLs", () => {
  assertEquals(
    resolveOsVerifyOptions({
      baseUrlFlag: "   ",
      envBaseUrl: "",
      skipStart: false,
    }).baseUrl,
    devOsServerBaseUrl(0),
  );
});

// ── dev auto-start derives the dev-only OS port (KAN-135) ─────

Deno.test("auto-start without offset derives the dev OS port 5175", () => {
  const resolved = resolveOsVerifyOptions({ skipStart: false });
  assertEquals(resolved.baseUrl, devOsServerBaseUrl(0));
  assertEquals(resolved.startCommandArgs, []);
  assertEquals(resolved.portOffset, 0);
});

Deno.test("auto-start with offset derives 5175+offset and forwards it", () => {
  const resolved = resolveOsVerifyOptions({
    skipStart: false,
    portOffset: 100,
  });
  assertEquals(resolved.baseUrl, devOsServerBaseUrl(100));
  assertEquals(resolved.startCommandArgs, ["--port-offset", "100"]);
  assertEquals(resolved.portOffset, 100);
});

// ── skip-start keeps the packaged production default ──────────

Deno.test("skip-start without offset keeps the production default 58261", () => {
  const resolved = resolveOsVerifyOptions({ skipStart: true });
  assertEquals(resolved.baseUrl, PRODUCTION_DEFAULT_BASE_URL);
  assertEquals(resolved.startCommandArgs, []);
});

Deno.test("skip-start WITH offset targets the running dev launch", () => {
  const resolved = resolveOsVerifyOptions({
    skipStart: true,
    portOffset: 20,
  });
  assertEquals(resolved.baseUrl, devOsServerBaseUrl(20));
  // no start command arguments: nothing is being started
  assertEquals(resolved.startCommandArgs, []);
});

Deno.test("explicit URL wins over the production default at skip-start", () => {
  const resolved = resolveOsVerifyOptions({
    skipStart: true,
    baseUrlFlag: "http://127.0.0.1:58261",
  });
  assertEquals(resolved.baseUrl, "http://127.0.0.1:58261");
});

// ── shared offset validation ──────────────────────────────────

Deno.test("invalid offsets fail with the shared dev-port-offset error", () => {
  for (const bad of [Number.NaN, -1, 60344, 1.5]) {
    assertThrows(
      () => resolveOsVerifyOptions({ skipStart: false, portOffset: bad }),
      Error,
    );
  }
  // the message is the shared one, not a wrapper-specific variant
  assertThrows(
    () => resolveOsVerifyOptions({ skipStart: false, portOffset: 1 }),
    Error,
    "non-overlap span",
  );
});

// ── wrapper CLI edges (pure, no probing/browser) ─────────────

Deno.test("absent --port-offset resolves to no offset", () => {
  assertEquals(parseWrapperPortOffsetArg(undefined, false), undefined);
  assertEquals(parseWrapperPortOffsetArg(undefined, true), undefined);
});

Deno.test("concrete offsets pass through shared validation", () => {
  assertEquals(parseWrapperPortOffsetArg("0", false), 0);
  assertEquals(parseWrapperPortOffsetArg("100", false), 100);
  assertThrows(
    () => parseWrapperPortOffsetArg("1", false),
    Error,
    "non-overlap span",
  );
});

Deno.test("bare --port-offset with a missing value is rejected", () => {
  // an empty flag value must not silently default to offset 0
  for (const empty of ["", "   "]) {
    assertThrows(
      () => parseWrapperPortOffsetArg(empty, false),
      Error,
      "--port-offset requires a value",
    );
    assertThrows(
      () => parseWrapperPortOffsetArg(empty, true),
      Error,
      "--port-offset requires a value",
    );
  }
});

Deno.test("--port-offset auto is rejected together with --skip-start", () => {
  // rejection happens before any probing: a free rung has no running
  // OS-Server to target and the existing offset cannot be inferred
  assertThrows(
    () => parseWrapperPortOffsetArg("auto", true),
    Error,
    "--port-offset auto cannot be combined with --skip-start",
  );
  // without skip-start, auto stays the caller-resolvable spec
  assertEquals(parseWrapperPortOffsetArg("auto", false), "auto");
});

Deno.test("explicit base URL does not bypass offset validation", () => {
  for (const bad of [Number.NaN, -1, 60344, 1.5]) {
    assertThrows(
      () =>
        resolveOsVerifyOptions({
          skipStart: false,
          portOffset: bad,
          baseUrlFlag: "http://127.0.0.1:9999",
        }),
      Error,
    );
    assertThrows(
      () =>
        resolveOsVerifyOptions({
          skipStart: true,
          portOffset: bad,
          envBaseUrl: "http://127.0.0.1:8888",
        }),
      Error,
    );
  }
  assertThrows(
    () =>
      resolveOsVerifyOptions({
        skipStart: true,
        portOffset: 1,
        baseUrlFlag: "http://127.0.0.1:9999",
      }),
    Error,
    "non-overlap span",
  );
});
