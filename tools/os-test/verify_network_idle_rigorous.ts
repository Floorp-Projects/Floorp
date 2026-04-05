#!/usr/bin/env -S deno run -A
// SPDX-License-Identifier: MPL-2.0

import { parseArgs } from "@std/cli/parse-args";

const DEFAULT_BASE_URL = "http://127.0.0.1:58261";

const TEST_PAGE_HTML = `<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8" />
  <title>Network Idle Test</title>
  <link rel="stylesheet" href="/slow-css" />
</head>
<body>
  <h1 id="title">Network Idle Test</h1>
  <img src="/slow-image" />
  <div id="logs"></div>
  <script>
    Promise.all([
      fetch('/slow-resource-1'),
      fetch('/slow-resource-2'),
    ]).then(() => {
      const done = document.createElement('p');
      done.id = 'done';
      done.textContent = 'done';
      document.body.appendChild(done);
    });
  </script>
</body>
</html>`;

type RequestResult = {
  status: number | null;
  body: unknown;
};

import { sleep } from "../src/async_utils.ts";

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null;
}

async function makeRequest(
  baseUrl: string,
  path: string,
  options: {
    method?: "GET" | "POST" | "DELETE";
    data?: unknown;
    timeoutMs?: number;
  } = {},
): Promise<RequestResult> {
  const url = `${baseUrl}${path}`;
  const timeoutMs = options.timeoutMs ?? 15_000;

  try {
    const response = await fetch(url, {
      method: options.method ?? "GET",
      headers:
        options.data !== undefined
          ? { "content-type": "application/json" }
          : undefined,
      body:
        options.data !== undefined ? JSON.stringify(options.data) : undefined,
      signal: AbortSignal.timeout(timeoutMs),
    });

    const raw = await response.text();
    let body: unknown = raw;
    if (raw.length > 0) {
      try {
        body = JSON.parse(raw);
      } catch {
        body = raw;
      }
    }

    return { status: response.status, body };
  } catch (error) {
    const detail = error instanceof Error ? error.message : String(error);
    return { status: null, body: detail };
  }
}

function startSlowServer(port: number): {
  url: string;
  close: () => Promise<void>;
} {
  const server = Deno.serve(
    { hostname: "127.0.0.1", port },
    async (request) => {
      const url = new URL(request.url);

      if (url.pathname === "/") {
        return new Response(TEST_PAGE_HTML, {
          headers: { "content-type": "text/html; charset=utf-8" },
        });
      }

      if (url.pathname === "/slow-css") {
        await sleep(600);
        return new Response("body { color: #222; }", {
          headers: { "content-type": "text/css" },
        });
      }

      if (url.pathname === "/slow-image") {
        await sleep(900);
        const png = Uint8Array.from([
          137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72, 68, 82, 0, 0, 0,
          1, 0, 0, 0, 1, 8, 6, 0, 0, 0, 31, 21, 196, 137, 0, 0, 0, 10, 73, 68,
          65, 84, 120, 156, 99, 0, 1, 0, 0, 5, 0, 1, 13, 10, 46, 174, 0, 0, 0,
          0, 73, 69, 78, 68, 174, 66, 96, 130,
        ]);
        return new Response(png, {
          headers: { "content-type": "image/png" },
        });
      }

      if (url.pathname === "/slow-resource-1") {
        await sleep(1200);
        return new Response("Slow resource 1", {
          headers: { "content-type": "text/plain" },
        });
      }

      if (url.pathname === "/slow-resource-2") {
        await sleep(700);
        return new Response("Slow resource 2", {
          headers: { "content-type": "text/plain" },
        });
      }

      return new Response("Not Found", { status: 404 });
    },
  );

  const addr = server.addr as Deno.NetAddr;
  return {
    url: `http://127.0.0.1:${addr.port}/`,
    close: async () => {
      await server.shutdown();
    },
  };
}

export async function main(argv: string[] = Deno.args): Promise<number> {
  const parsed = parseArgs(argv, {
    string: ["base-url", "port", "timeout"],
    default: {
      "base-url": DEFAULT_BASE_URL,
      port: "0",
      timeout: "10000",
    },
  });

  const baseUrl = String(parsed["base-url"]);
  const serverPort = Number(parsed.port);
  const networkIdleTimeout = Number(parsed.timeout);

  let failures = 0;
  const assertCase = (name: string, condition: boolean, detail = "") => {
    if (condition) {
      console.log(`[OK] ${name}`);
      return;
    }
    failures += 1;
    console.log(`[FAIL] ${name}${detail ? ` - ${detail}` : ""}`);
  };

  const localServer = await startSlowServer(serverPort);
  const testUrl = localServer.url;
  console.log(`Testing network idle with ${testUrl}`);

  let instanceId: string | null = null;

  try {
    const created = await makeRequest(baseUrl, "/scraper/instances", {
      method: "POST",
    });
    assertCase(
      "Create scraper instance",
      created.status === 200 &&
        isRecord(created.body) &&
        typeof created.body.instanceId === "string",
      String(created.body),
    );

    if (
      !(
        created.status === 200 &&
        isRecord(created.body) &&
        typeof created.body.instanceId === "string"
      )
    ) {
      return 1;
    }

    instanceId = created.body.instanceId;

    const navigated = await makeRequest(
      baseUrl,
      `/scraper/instances/${instanceId}/navigate`,
      {
        method: "POST",
        data: { url: testUrl },
      },
    );
    assertCase(
      "Navigate scraper",
      navigated.status === 200,
      String(navigated.body),
    );
    if (navigated.status !== 200) {
      return 1;
    }

    const waitDone = await makeRequest(
      baseUrl,
      `/scraper/instances/${instanceId}/waitForElement`,
      {
        method: "POST",
        data: { selector: "#done", timeout: 10000 },
        timeoutMs: 20_000,
      },
    );
    assertCase(
      "waitForElement(#done) contract",
      waitDone.status === 200 &&
        isRecord(waitDone.body) &&
        waitDone.body.ok === true &&
        waitDone.body.found === true,
      String(waitDone.body),
    );

    const firstStart = performance.now();
    const firstIdle = await makeRequest(
      baseUrl,
      `/scraper/instances/${instanceId}/waitForNetworkIdle`,
      {
        method: "POST",
        data: { timeout: networkIdleTimeout },
        timeoutMs: 20_000,
      },
    );
    const firstDuration = (performance.now() - firstStart) / 1000;
    console.log(
      `First waitForNetworkIdle duration: ${firstDuration.toFixed(2)}s`,
    );

    assertCase(
      "waitForNetworkIdle contract (first)",
      firstIdle.status === 200 &&
        isRecord(firstIdle.body) &&
        typeof firstIdle.body.ok === "boolean",
      String(firstIdle.body),
    );

    assertCase(
      "waitForNetworkIdle should succeed after done",
      isRecord(firstIdle.body) && firstIdle.body.ok === true,
      String(firstIdle.body),
    );

    const secondStart = performance.now();
    const secondIdle = await makeRequest(
      baseUrl,
      `/scraper/instances/${instanceId}/waitForNetworkIdle`,
      {
        method: "POST",
        data: { timeout: networkIdleTimeout },
        timeoutMs: 20_000,
      },
    );
    const secondDuration = (performance.now() - secondStart) / 1000;
    console.log(
      `Second waitForNetworkIdle duration: ${secondDuration.toFixed(2)}s`,
    );

    assertCase(
      "waitForNetworkIdle contract (second)",
      secondIdle.status === 200 &&
        isRecord(secondIdle.body) &&
        typeof secondIdle.body.ok === "boolean",
      String(secondIdle.body),
    );

    const secondCeiling = Math.max(
      firstDuration + 6.0,
      networkIdleTimeout / 1000 + 2.0,
      12.0,
    );
    assertCase(
      "Second wait should not be dramatically slower",
      secondDuration <= secondCeiling,
      `first=${firstDuration.toFixed(2)}s second=${secondDuration.toFixed(2)}s`,
    );

    const shortIdle = await makeRequest(
      baseUrl,
      `/scraper/instances/${instanceId}/waitForNetworkIdle`,
      {
        method: "POST",
        data: { timeout: 10 },
        timeoutMs: 20_000,
      },
    );

    assertCase(
      "waitForNetworkIdle short-timeout contract",
      shortIdle.status === 200 &&
        isRecord(shortIdle.body) &&
        typeof shortIdle.body.ok === "boolean",
      String(shortIdle.body),
    );
  } finally {
    if (instanceId) {
      await makeRequest(baseUrl, `/scraper/instances/${instanceId}`, {
        method: "DELETE",
      });
    }
    await localServer.close();
  }

  console.log(`Completed with ${failures} failure(s)`);
  return failures > 0 ? 1 : 0;
}

if (import.meta.main) {
  const code = await main(Deno.args);
  Deno.exit(code);
}
