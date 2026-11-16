// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals, assertRejects, assertThrows } from "@std/assert";
import {
  createOsTestClient,
  reconcileOsTestClientArgs,
  resolveTokenFile,
  validateAuthenticatedBaseUrl,
} from "./os_test_client.ts";

const BASE = "http://127.0.0.1:5175";
const SECRET = "synthetic-test-secret-12345";
const bytes = (text: string) => new TextEncoder().encode(text);

Deno.test("token file CLI overrides env; absent means no discovery", async () => {
  assertEquals(resolveTokenFile("explicit", "environment"), "explicit");
  assertEquals(resolveTokenFile(undefined, "environment"), "environment");
  assertEquals(resolveTokenFile(), undefined);
  assertThrows(() => resolveTokenFile("", "fallback"));
  let reads = 0;
  const client = await createOsTestClient({ baseUrl: BASE }, {
    readTokenFile: () => {
      reads++;
      throw new Error("must not read");
    },
    fetch: (_input, init) => {
      assertEquals(new Headers(init?.headers).get("authorization"), null);
      return Promise.resolve(new Response('{"status":"ok"}'));
    },
  });
  assertEquals(
    await client.request("/health", {
      headers: { authorization: "forbidden" },
    }),
    { status: 200, body: { status: "ok" } },
  );
  assertEquals(reads, 0);
});

Deno.test("authenticated targets validated before any credential read or network", async () => {
  for (
    const baseUrl of [
      "https://127.0.0.1:5175",
      "http://localhost:5175",
      "http://example.com",
      "http://127.1:5175",
      "http://2130706433:5175",
      "http://127.0.0.1.evil:5175",
      "http://user@127.0.0.1:5175",
      "http://127.0.0.1:65536",
      "http://127.0.0.1:5175/path",
      "http://127.0.0.1:5175/?x=1",
      "http://127.0.0.1:5175/#x",
      "file:///tmp/token",
      "http://127.0.0.1:5175/../",
      " http://127.0.0.1:5175",
    ]
  ) {
    let reads = 0;
    let requests = 0;
    await assertRejects(() =>
      createOsTestClient({ baseUrl, tokenFile: "synthetic" }, {
        readTokenFile: () => {
          reads++;
          return Promise.resolve(bytes(SECRET));
        },
        fetch: () => {
          requests++;
          return Promise.resolve(new Response(""));
        },
      })
    );
    assertEquals(reads, 0);
    assertEquals(requests, 0);
  }
  assertEquals(validateAuthenticatedBaseUrl(BASE + "/").origin, BASE);
  assertEquals(
    validateAuthenticatedBaseUrl("http://[::1]:5175").origin,
    "http://[::1]:5175",
  );
});

Deno.test("bounded strict token input with optional single final newline", async () => {
  for (
    const text of [
      "",
      " ",
      SECRET + " ",
      " " + SECRET,
      SECRET + "\n\n",
      SECRET + "\t",
      SECRET + "\nother",
      "a=b",
      "é",
      "a".repeat(4097),
    ]
  ) {
    await assertRejects(
      () =>
        createOsTestClient({ baseUrl: BASE, tokenFile: "synthetic" }, {
          readTokenFile: (_path, limit) => {
            assertEquals(limit, 4096);
            return Promise.resolve(bytes(text));
          },
        }),
      Error,
      "Unable to load a valid OS API token file",
    );
  }
  await assertRejects(
    () =>
      createOsTestClient({ baseUrl: BASE, tokenFile: "synthetic" }, {
        readTokenFile: () => {
          throw new Error(SECRET);
        },
      }),
    Error,
    "Unable to load a valid OS API token file",
  );
  await assertRejects(() =>
    createOsTestClient({ baseUrl: BASE, tokenFile: "synthetic" }, {
      readTokenFile: () => Promise.resolve(new Uint8Array([255])),
    })
  );
  for (const ending of ["", "\n", "\r\n"]) {
    const client = await createOsTestClient({
      baseUrl: BASE,
      tokenFile: "synthetic",
    }, {
      readTokenFile: () => Promise.resolve(bytes(SECRET + ending)),
      fetch: (_input, init) => {
        assertEquals(
          new Headers(init?.headers).get("authorization"),
          "Bearer " + SECRET,
        );
        return Promise.resolve(new Response("ok"));
      },
    });
    await client.request("/health");
    assert(!JSON.stringify(client).includes(SECRET));
  }
});

Deno.test("scope origin checked again for every request, including malicious path", async () => {
  let calls = 0;
  const client = await createOsTestClient({
    baseUrl: BASE,
    tokenFile: "synthetic",
  }, {
    readTokenFile: () => Promise.resolve(bytes(SECRET)),
    fetch: (input, init) => {
      calls++;
      assertEquals(new URL(String(input)).origin, BASE);
      assertEquals(init?.redirect, "error");
      assertEquals(
        new Headers(init?.headers).get("authorization"),
        "Bearer " + SECRET,
      );
      return Promise.resolve(new Response("ok"));
    },
  });
  for (
    const path of ["@example.com/health", "0/health", "@127.0.0.1:58261/health"]
  ) {
    await assertRejects(() => client.request(path));
  }
  assertEquals(calls, 0);
  await client.request("/health");
  assertEquals(calls, 1);
});

Deno.test("all redirects rejected, including streaming and same-origin locations", async () => {
  for (const status of [301, 302, 303, 307, 308]) {
    for (
      const location of [
        BASE + "/health",
        "http://127.0.0.1:58261/health",
        "https://example.com/",
      ]
    ) {
      let calls = 0;
      const client = await createOsTestClient({
        baseUrl: BASE,
        tokenFile: "synthetic",
      }, {
        readTokenFile: () => Promise.resolve(bytes(SECRET)),
        fetch: (_input, init) => {
          calls++;
          assertEquals(init?.redirect, "error");
          return Promise.resolve(
            new Response(SECRET, { status, headers: { location } }),
          );
        },
      });
      await assertRejects(() => client.request("/health", {}, true));
      assertEquals(calls, 1);
    }
  }
});

Deno.test("response token redacted from text, JSON keys/values and JSON escapes", async () => {
  for (
    const body of [
      SECRET,
      JSON.stringify({ [SECRET]: [SECRET] }),
      '{"echo":"' + SECRET.replaceAll("s", "\\u0073") + '"}',
    ]
  ) {
    const client = await createOsTestClient({
      baseUrl: BASE,
      tokenFile: "synthetic",
    }, {
      readTokenFile: () => Promise.resolve(bytes(SECRET)),
      fetch: () => Promise.resolve(new Response(body, { status: 401 })),
    });
    const result = await client.request("/health");
    assertEquals(result.status, 401);
    assert(!JSON.stringify(result).includes(SECRET));
    assert(JSON.stringify(result).includes("[REDACTED]"));
  }
});

Deno.test("fetch errors cannot expose token; client does not log", async () => {
  const messages: string[] = [];
  const saved = { log: console.log, error: console.error };
  console.log = (...args: unknown[]) => {
    messages.push(String(args));
  };
  console.error = (...args: unknown[]) => {
    messages.push(String(args));
  };
  try {
    const client = await createOsTestClient({
      baseUrl: BASE,
      tokenFile: "synthetic",
    }, {
      readTokenFile: () => Promise.resolve(bytes(SECRET)),
      fetch: () => {
        throw new Error("Bearer " + SECRET);
      },
    });
    const error = await assertRejects(() => client.request("/health"));
    assert(!String(error).includes(SECRET));
    assertEquals(messages, []);
  } finally {
    Object.assign(console, saved);
  }
});

Deno.test("parallel instances keep separate closure tokens and cancel streams", async () => {
  const clients = await Promise.all(
    [SECRET, "other-synthetic-secret"].map(async (secret, index) => {
      const baseUrl = "http://127.0.0.1:" + (5175 + index);
      return await createOsTestClient({ baseUrl, tokenFile: "synthetic" }, {
        readTokenFile: () => Promise.resolve(bytes(secret)),
        fetch: (input, init) => {
          assertEquals(new URL(String(input)).origin, baseUrl);
          assertEquals(
            new Headers(init?.headers).get("authorization"),
            "Bearer " + secret,
          );
          return Promise.resolve(new Response(secret));
        },
      });
    }),
  );
  await Promise.all(
    clients.flatMap((
      client,
    ) => [client.request("/health"), client.request("/stream", {}, true)]),
  );
});

Deno.test("wrapper reconciles targets on both sides, rejecting conflicts and raw tokens", () => {
  const resolved = reconcileOsTestClientArgs([
    "--skip-start",
    "--base-url",
    BASE,
  ], ["--quick", "--token-file", "synthetic"]);
  assertEquals(resolved, {
    wrapperArgs: ["--skip-start"],
    verifierArgs: ["--quick"],
    baseUrlFlag: BASE,
    tokenFileFlag: "synthetic",
  });
  for (const key of ["--base-url", "--token-file"]) {
    assertThrows(() => reconcileOsTestClientArgs([key, "one"], [key + "=two"]));
    assertThrows(() => reconcileOsTestClientArgs([key, "one"], [key + "=one"]));
    assertThrows(() => reconcileOsTestClientArgs([], [key]));
    assertThrows(() => reconcileOsTestClientArgs([key + "="], []));
    assertThrows(() => reconcileOsTestClientArgs([key, "--quick"], []));
  }
  for (const arg of ["--token=" + SECRET, "--auth-token", "--authorization"]) {
    assertThrows(() => reconcileOsTestClientArgs([], [arg]));
  }
});
