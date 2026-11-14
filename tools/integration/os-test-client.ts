// SPDX-License-Identifier: MPL-2.0
// Run: deno run --allow-net=127.0.0.1 tools/integration/os-test-client.ts
// Synthetic token bytes are injected; no credential files or browser are used.
import { assertEquals, assertRejects } from "@std/assert";
import { createOsTestClient } from "../src/os_test_client.ts";

const syntheticToken = "FDH-SYNTHETIC-NOT-A-CREDENTIAL";
let redirectedRequests = 0;
let sameOriginRedirects = 0;
let authenticatedRequests = 0;
const redirectTarget = Deno.serve(
  { hostname: "127.0.0.1", port: 0, onListen: () => {} },
  () => {
    redirectedRequests++;
    return new Response("redirect must never be followed");
  },
);
const api = Deno.serve(
  { hostname: "127.0.0.1", port: 0, onListen: () => {} },
  (request) => {
    const pathname = new URL(request.url).pathname;
    if (pathname === "/followed") sameOriginRedirects++;
    if (request.headers.get("authorization") !== "Bearer " + syntheticToken) {
      return new Response("authentication required", { status: 401 });
    }
    authenticatedRequests++;
    if (pathname === "/redirect-cross-origin") {
      return new Response(null, {
        status: 302,
        headers: {
          location: "http://127.0.0.1:" + redirectTarget.addr.port + "/collect",
        },
      });
    }
    if (pathname === "/redirect-same-origin") {
      return new Response(null, {
        status: 307,
        headers: { location: "/followed" },
      });
    }
    if (pathname === "/echo") {
      return Response.json({ reflected: syntheticToken });
    }
    return Response.json({ status: "ok" });
  },
);
try {
  const baseUrl = "http://127.0.0.1:" + api.addr.port;
  const client = await createOsTestClient(
    { baseUrl, tokenFile: "synthetic-input-only" },
    {
      readTokenFile: () =>
        Promise.resolve(new TextEncoder().encode(syntheticToken)),
    },
  );
  assertEquals((await client.request("/health")).status, 200);
  const unauthenticated = await createOsTestClient({ baseUrl });
  assertEquals((await unauthenticated.request("/health")).status, 401);
  assertEquals(
    JSON.stringify((await client.request("/echo")).body).includes(
      syntheticToken,
    ),
    false,
  );
  await assertRejects(
    () => client.request("/redirect-cross-origin"),
    Error,
    "OS API request failed",
  );
  await assertRejects(
    () => client.request("/redirect-same-origin"),
    Error,
    "OS API request failed",
  );
  assertEquals(redirectedRequests, 0);
  assertEquals(sameOriginRedirects, 0);
  assertEquals(authenticatedRequests, 4);
  console.log(
    "PASS: bearer authentication, 401, redaction and both redirect refusal cases (real loopback HTTP; synthetic token only)",
  );
} finally {
  await Promise.all([api.shutdown(), redirectTarget.shutdown()]);
}
