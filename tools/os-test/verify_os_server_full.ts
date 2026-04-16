#!/usr/bin/env -S deno run -A
// SPDX-License-Identifier: MPL-2.0

import { parseArgs } from "@std/cli/parse-args";

const DEFAULT_BASE_URL =
  Deno.env.get("FLOORP_OS_BASE_URL") ?? "http://127.0.0.1:58261";
const FINGERPRINT_COMMENT_RE = /<!--fp:([a-z0-9]{8}(?:[a-z0-9]{8})?)-->/g;

const TEST_PAGE_HTML = `<!doctype html>
<html>
<head>
  <meta charset="utf-8" />
  <title>Floorp OS Test</title>
  <style>
    body { font-family: sans-serif; margin: 20px; }
    #spacer { height: 1200px; }
    #footer { margin-top: 20px; }
    #dragSource, #dragTarget { padding: 8px; border: 1px dashed #888; margin: 4px 0; }
    #dragTarget { min-height: 40px; }
    .box { padding: 4px; margin: 4px 0; border: 1px solid #ccc; }
  </style>
  <script>
    function preventSubmit(event) {
      event.preventDefault();
      const log = document.getElementById("log");
      if (log) {
        log.textContent = "submitted";
      }
    }
  </script>
</head>
<body>
  <h1 id="title" data-test="title">Floorp OS Test</h1>
  <p class="desc">Example page for OS server API tests.</p>
  <a id="link" href="javascript:void(0)">Do nothing link</a>

  <form id="testForm" class="box" onsubmit="preventSubmit(event)">
    <input id="name" name="name" value="Alice" />
    <input id="email" name="email" type="email" value="alice@example.com" />
    <textarea id="message">Hello</textarea>
    <select id="color">
      <option value="red">Red</option>
      <option value="blue">Blue</option>
    </select>
    <input id="agree" type="checkbox" />
    <input id="fileInput" type="file" />
    <button id="submitBtn" type="submit">Submit</button>
    <button id="resetBtn" type="reset">Reset</button>
  </form>

  <div id="hoverTarget" class="box">Hover me</div>
  <div id="dblTarget" class="box">Double click me</div>
  <div id="contextTarget" class="box">Right click me</div>
  <div id="dragSource" draggable="true">Drag me</div>
  <div id="dragTarget">Drop here</div>
  <div id="editable" class="box" contenteditable="true">Editable</div>
  <div id="eventTarget" class="box">Event target</div>
  <div id="spacer"></div>
  <div id="footer" class="box">Footer</div>
  <div id="log" class="box"></div>
</body>
</html>`;

type TestStatus = "OK" | "FAILED" | "SKIPPED";

type TestResult = {
  name: string;
  status: TestStatus;
  detail?: string;
};

type RequestOptions = {
  method?: "GET" | "POST" | "DELETE";
  data?: unknown;
  stream?: boolean;
  timeoutMs?: number;
  retries?: number;
  retryIntervalMs?: number;
};

type RequestResult = {
  status: number | null;
  body: unknown;
};

type RunTestOptions = {
  method?: "GET" | "POST" | "DELETE";
  data?: unknown;
  expectedStatus?: number | number[];
  extractKey?: string;
  predicate?: (body: unknown) => boolean;
  expectDescription?: string;
  stream?: boolean;
  skipStatuses?: number[];
  timeoutMs?: number;
};

type RunValueTestOptions = {
  method?: "GET" | "POST" | "DELETE";
  data?: unknown;
  key?: string;
  expected?: unknown;
  predicate?: (value: unknown) => boolean;
  expectDescription?: string;
  expectedStatus?: number | number[];
  skipStatuses?: number[];
  timeoutMs?: number;
};

type RuntimeOptions = {
  baseUrl: string;
};

const TEST_RESULTS: TestResult[] = [];

function recordResult(name: string, status: TestStatus, detail?: string): void {
  TEST_RESULTS.push({ name, status, detail });
}

function clearResults(): void {
  TEST_RESULTS.length = 0;
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null;
}

function normalizeExpectedStatus(
  expectedStatus: number | number[] | undefined,
): number[] {
  if (expectedStatus === undefined) {
    return [200];
  }
  return Array.isArray(expectedStatus) ? expectedStatus : [expectedStatus];
}

function q(value: string): string {
  return encodeURIComponent(value);
}

function short(value: unknown): string {
  try {
    const text = JSON.stringify(value);
    if (!text) {
      return String(value);
    }
    return text.length > 180 ? `${text.slice(0, 177)}...` : text;
  } catch {
    const text = String(value);
    return text.length > 180 ? `${text.slice(0, 177)}...` : text;
  }
}

import { sleep } from "../src/async_utils.ts";

async function waitUntil(
  condition: () => boolean | Promise<boolean>,
  options: { timeoutMs?: number; intervalMs?: number } = {},
): Promise<boolean> {
  const timeoutMs = options.timeoutMs ?? 10_000;
  const intervalMs = options.intervalMs ?? 200;
  const deadline = Date.now() + timeoutMs;

  while (Date.now() < deadline) {
    try {
      if (await condition()) {
        return true;
      }
    } catch {
      // ignore polling errors
    }
    await sleep(intervalMs);
  }

  return false;
}

function extractFingerprintsFromText(markdownText: string): string[] {
  const seen = new Set<string>();
  const fingerprints: string[] = [];

  for (const match of markdownText.matchAll(FINGERPRINT_COMMENT_RE)) {
    const fp = match[1];
    if (!seen.has(fp)) {
      seen.add(fp);
      fingerprints.push(fp);
    }
  }

  return fingerprints;
}

async function makeRequest(
  runtime: RuntimeOptions,
  path: string,
  options: RequestOptions = {},
): Promise<RequestResult> {
  const method = options.method ?? "GET";
  const stream = options.stream ?? false;
  const timeoutMs = options.timeoutMs ?? 60_000;
  const retries = options.retries ?? 1;
  const retryIntervalMs = options.retryIntervalMs ?? 200;

  const attempts = Math.max(1, retries + 1);
  const url = `${runtime.baseUrl}${path}`;

  for (let attempt = 0; attempt < attempts; attempt++) {
    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), timeoutMs);

    try {
      const init: RequestInit = {
        method,
        signal: controller.signal,
        headers: {},
      };

      if (options.data !== undefined) {
        (init.headers as Record<string, string>)["content-type"] =
          "application/json";
        init.body = JSON.stringify(options.data);
      }

      const response = await fetch(url, init);
      if (stream) {
        response.body?.cancel();
        clearTimeout(timeoutId);
        return { status: response.status, body: "<stream>" };
      }

      const raw = await response.text();
      let body: unknown = raw;
      if (raw.length > 0) {
        try {
          body = JSON.parse(raw);
        } catch {
          body = raw;
        }
      }

      clearTimeout(timeoutId);
      return { status: response.status, body };
    } catch (error) {
      clearTimeout(timeoutId);

      if (attempt < attempts - 1) {
        await sleep(retryIntervalMs);
        continue;
      }

      const detail = error instanceof Error ? error.message : String(error);
      console.error(`Failed to connect ${url}: ${detail}`);
      return { status: null, body: null };
    }
  }

  return { status: null, body: null };
}

async function runTest(
  runtime: RuntimeOptions,
  name: string,
  path: string,
  options: RunTestOptions = {},
): Promise<unknown | null> {
  const allowed = normalizeExpectedStatus(options.expectedStatus);
  const skipStatuses = options.skipStatuses ?? [];

  console.log(`Testing ${name} (${options.method ?? "GET"} ${path})... `);

  const { status, body } = await makeRequest(runtime, path, {
    method: options.method,
    data: options.data,
    stream: options.stream,
    timeoutMs: options.timeoutMs ?? 8_000,
  });

  if (status === null) {
    console.log("FAILED (connection failed)");
    recordResult(name, "FAILED", "connection failed");
    return null;
  }

  if (skipStatuses.includes(status)) {
    console.log(`SKIPPED (status=${status})`);
    recordResult(name, "SKIPPED", `status ${status}`);
    return null;
  }

  if (!allowed.includes(status)) {
    console.log(`FAILED (status=${status}, expected=${allowed.join(",")})`);
    console.log(`  Response: ${short(body)}`);
    recordResult(
      name,
      "FAILED",
      `status ${status}, expected ${allowed.join(",")}`,
    );
    return null;
  }

  if (options.predicate) {
    let ok = false;
    try {
      ok = options.predicate(body);
    } catch (error) {
      const detail = error instanceof Error ? error.message : String(error);
      console.log(`FAILED (predicate error: ${detail})`);
      recordResult(name, "FAILED", `predicate error: ${detail}`);
      return null;
    }

    if (!ok) {
      const detail = options.expectDescription ?? "predicate did not match";
      console.log(`FAILED (${detail})`);
      recordResult(name, "FAILED", detail);
      return null;
    }

    const detail = options.expectDescription ?? "predicate matched";
    console.log(`OK (${detail})`);
    recordResult(name, "OK", detail);
  } else {
    const detail = allowed.length > 1 ? `status ${status}` : undefined;
    console.log(`OK${detail ? ` (${detail})` : ""}`);
    recordResult(name, "OK", detail);
  }

  if (options.extractKey && isRecord(body)) {
    return body[options.extractKey];
  }
  return body;
}

async function runValueTest(
  runtime: RuntimeOptions,
  name: string,
  path: string,
  options: RunValueTestOptions = {},
): Promise<unknown | null> {
  const allowed = normalizeExpectedStatus(options.expectedStatus);
  const skipStatuses = options.skipStatuses ?? [];

  console.log(`Testing ${name} (${options.method ?? "GET"} ${path})... `);

  const { status, body } = await makeRequest(runtime, path, {
    method: options.method,
    data: options.data,
    timeoutMs: options.timeoutMs ?? 8_000,
  });

  if (status === null) {
    console.log("FAILED (connection failed)");
    recordResult(name, "FAILED", "connection failed");
    return null;
  }

  if (skipStatuses.includes(status)) {
    console.log(`SKIPPED (status=${status})`);
    recordResult(name, "SKIPPED", `status ${status}`);
    return null;
  }

  if (!allowed.includes(status)) {
    console.log(`FAILED (status=${status}, expected=${allowed.join(",")})`);
    console.log(`  Response: ${short(body)}`);
    recordResult(
      name,
      "FAILED",
      `status ${status}, expected ${allowed.join(",")}`,
    );
    return null;
  }

  let actual: unknown = body;
  if (options.key) {
    actual = isRecord(body) ? body[options.key] : undefined;
  }

  let ok = true;
  if (options.predicate) {
    try {
      ok = options.predicate(actual);
    } catch (error) {
      ok = false;
      const detail = error instanceof Error ? error.message : String(error);
      console.log(`FAILED (predicate error: ${detail})`);
      recordResult(name, "FAILED", `predicate error: ${detail}`);
      return actual;
    }
  } else if (options.expected !== undefined) {
    ok = Object.is(actual, options.expected);
  }

  if (ok) {
    const detail =
      options.expectDescription ??
      (options.expected !== undefined
        ? `== ${short(options.expected)}`
        : "matches");
    console.log(`OK (value=${short(actual)}; ${detail})`);
    recordResult(name, "OK", detail);
  } else {
    const detail =
      options.expectDescription ?? `expected ${short(options.expected)}`;
    console.log(`FAILED (value=${short(actual)}; ${detail})`);
    recordResult(name, "FAILED", detail);
  }

  return actual;
}

function printSummary(): {
  total: number;
  passed: number;
  failed: number;
  skipped: number;
} {
  if (TEST_RESULTS.length === 0) {
    console.log("Test summary: no tests were recorded.");
    return { total: 0, passed: 0, failed: 0, skipped: 0 };
  }

  const total = TEST_RESULTS.length;
  const passed = TEST_RESULTS.filter((r) => r.status === "OK").length;
  const failed = TEST_RESULTS.filter((r) => r.status === "FAILED").length;
  const skipped = TEST_RESULTS.filter((r) => r.status === "SKIPPED").length;

  console.log("Test summary:");
  console.log(`  Total: ${total}`);
  console.log(`  Passed: ${passed}`);
  console.log(`  Failed: ${failed}`);
  console.log(`  Skipped: ${skipped}`);

  if (failed > 0) {
    console.log("  Failed tests:");
    for (const result of TEST_RESULTS.filter((r) => r.status === "FAILED")) {
      console.log(
        `    - ${result.name}${result.detail ? ` (${result.detail})` : ""}`,
      );
    }
  }

  return { total, passed, failed, skipped };
}

async function createTempUploadFile(): Promise<string> {
  const path = await Deno.makeTempFile({
    prefix: "floorp-os-test-",
    suffix: ".txt",
  });
  await Deno.writeTextFile(path, "Floorp OS server upload test\n");
  return path;
}

type TestPageServer = {
  url: string;
  close: () => Promise<void>;
};

function startTestHttpServer(): TestPageServer {
  const server = Deno.serve({ hostname: "127.0.0.1", port: 0 }, (request) => {
    const url = new URL(request.url);
    if (url.pathname === "/test-page.html") {
      return new Response(TEST_PAGE_HTML, {
        headers: { "content-type": "text/html; charset=utf-8" },
      });
    }
    return new Response("Not Found", { status: 404 });
  });

  const addr = server.addr as Deno.NetAddr;
  return {
    url: `http://127.0.0.1:${addr.port}/test-page.html`,
    close: async () => {
      await server.shutdown();
    },
  };
}

async function resolveFingerprint(
  runtime: RuntimeOptions,
  prefix: string,
  instanceId: string,
  fingerprint: string,
): Promise<RequestResult> {
  return await makeRequest(
    runtime,
    `${prefix}/instances/${instanceId}/resolveFingerprint?fingerprint=${q(fingerprint)}`,
    { timeoutMs: 8_000 },
  );
}

async function findResolvableFingerprint(
  runtime: RuntimeOptions,
  prefix: string,
  instanceId: string,
  markdownText: string,
  selectorHint?: string,
): Promise<{ fingerprint: string; selector: string } | null> {
  const fingerprints = extractFingerprintsFromText(markdownText);
  let firstMatch: { fingerprint: string; selector: string } | null = null;

  for (const fp of fingerprints.slice(0, 120)) {
    const { status, body } = await resolveFingerprint(
      runtime,
      prefix,
      instanceId,
      fp,
    );
    if (status !== 200 || !isRecord(body)) {
      continue;
    }
    const selector = body.selector;
    if (typeof selector !== "string" || selector.length === 0) {
      continue;
    }

    if (!firstMatch) {
      firstMatch = { fingerprint: fp, selector };
    }
    if (selectorHint && selector.includes(selectorHint)) {
      return { fingerprint: fp, selector };
    }
  }

  return firstMatch;
}

async function findNonexistentFingerprint(
  runtime: RuntimeOptions,
  prefix: string,
  instanceId: string,
): Promise<string | null> {
  const fixedCandidates = [
    "deadbeef",
    "deadbeefdeadbeef",
    "cafebabe",
    "cafebabecafebabe",
    "aaaaaaaa",
    "aaaaaaaaaaaaaaaa",
  ];

  for (const candidate of fixedCandidates) {
    const { status } = await resolveFingerprint(
      runtime,
      prefix,
      instanceId,
      candidate,
    );
    if (status === 404) {
      return candidate;
    }
  }

  for (let i = 1; i <= 64; i++) {
    const candidate = i.toString(16).padStart(8, "0");
    const { status } = await resolveFingerprint(
      runtime,
      prefix,
      instanceId,
      candidate,
    );
    if (status === 404) {
      return candidate;
    }
  }

  return null;
}

async function testWaitContract(
  runtime: RuntimeOptions,
  prefix: string,
  instanceId: string,
): Promise<void> {
  const base = `${prefix}/instances/${instanceId}`;

  await runTest(
    runtime,
    "WaitForElement existing returns ok=true/found=true",
    `${base}/waitForElement`,
    {
      method: "POST",
      data: { selector: "#title", timeout: 2000 },
      predicate: (body) =>
        isRecord(body) && body.ok === true && body.found === true,
      expectDescription: "ok=true and found=true",
    },
  );

  await runTest(
    runtime,
    "WaitForElement timeout returns ok=false/found=false",
    `${base}/waitForElement`,
    {
      method: "POST",
      data: { selector: "#does-not-exist", timeout: 100 },
      predicate: (body) =>
        isRecord(body) && body.ok === false && body.found === false,
      expectDescription: "ok=false and found=false",
    },
  );
}

async function testNegativeValidationMatrix(
  runtime: RuntimeOptions,
  prefix: string,
  instanceId: string,
): Promise<void> {
  const base = `${prefix}/instances/${instanceId}`;

  await runTest(
    runtime,
    "Click missing selector/fingerprint -> 400",
    `${base}/click`,
    {
      method: "POST",
      data: {},
      expectedStatus: 400,
    },
  );

  await runTest(
    runtime,
    "Click invalid fingerprint format -> 400",
    `${base}/click`,
    {
      method: "POST",
      data: { fingerprint: "bad!" },
      expectedStatus: 400,
    },
  );

  await runTest(
    runtime,
    "WaitForElement missing selector/fingerprint -> 400",
    `${base}/waitForElement`,
    {
      method: "POST",
      data: { timeout: 100 },
      expectedStatus: 400,
    },
  );

  await runTest(runtime, "Input missing value -> 400", `${base}/input`, {
    method: "POST",
    data: { selector: "#name" },
    expectedStatus: 400,
  });

  await runTest(
    runtime,
    "UploadFile missing filePath -> 400",
    `${base}/uploadFile`,
    {
      method: "POST",
      data: { selector: "#fileInput" },
      expectedStatus: 400,
    },
  );

  await runTest(runtime, "PressKey missing key -> 400", `${base}/pressKey`, {
    method: "POST",
    data: {},
    expectedStatus: 400,
  });

  await runTest(runtime, "SetCookie missing value -> 400", `${base}/cookie`, {
    method: "POST",
    data: { name: "missing-value" },
    expectedStatus: 400,
  });
}

async function testFingerprintSelectorCompatibility(
  runtime: RuntimeOptions,
  prefix: string,
  instanceId: string,
  includeGetElement: boolean,
): Promise<void> {
  const base = `${prefix}/instances/${instanceId}`;

  const textBody = await runTest(
    runtime,
    "Extract markdown for fingerprints",
    `${base}/text`,
  );
  if (
    !isRecord(textBody) ||
    typeof textBody.text !== "string" ||
    textBody.text.length === 0
  ) {
    recordResult(
      "Fingerprint extraction",
      "SKIPPED",
      "text response not available",
    );
    return;
  }

  const resolved = await findResolvableFingerprint(
    runtime,
    prefix,
    instanceId,
    textBody.text,
    "#title",
  );
  if (!resolved) {
    recordResult(
      "Fingerprint extraction",
      "SKIPPED",
      "no resolvable fingerprint found",
    );
    return;
  }

  await runValueTest(
    runtime,
    "Resolve fingerprint returns selector",
    `${base}/resolveFingerprint?fingerprint=${q(resolved.fingerprint)}`,
    {
      key: "selector",
      predicate: (value) => typeof value === "string" && value.length > 0,
      expectDescription: "non-empty selector",
    },
  );

  if (includeGetElement) {
    await runValueTest(
      runtime,
      "Get element via fingerprint",
      `${base}/element?fingerprint=${q(resolved.fingerprint)}`,
      {
        key: "element",
        predicate: (value) => typeof value === "string" && value.includes("<"),
        expectDescription: "returns element HTML",
      },
    );
  }

  await runTest(runtime, "Click via fingerprint", `${base}/click`, {
    method: "POST",
    data: { fingerprint: resolved.fingerprint },
    predicate: (body) => isRecord(body) && typeof body.ok === "boolean",
    expectDescription: "response has boolean ok",
  });

  await runTest(
    runtime,
    "Click with selector priority over invalid fingerprint",
    `${base}/click`,
    {
      method: "POST",
      data: { selector: "#link", fingerprint: "bad!" },
      predicate: (body) => isRecord(body) && typeof body.ok === "boolean",
      expectDescription: "selector path works with bad fingerprint",
    },
  );

  const missingFp = await findNonexistentFingerprint(
    runtime,
    prefix,
    instanceId,
  );
  if (missingFp) {
    await runTest(
      runtime,
      "Click with non-existent fingerprint -> 404",
      `${base}/click`,
      {
        method: "POST",
        data: { fingerprint: missingFp },
        expectedStatus: 404,
      },
    );
  }
}

async function testSharedAutomation(
  runtime: RuntimeOptions,
  basePath: "/tabs" | "/scraper",
  instanceId: string,
  options: {
    includeGetElement: boolean;
    uploadFilePath: string;
    testPageUrl: string;
  },
): Promise<void> {
  const prefix = `${basePath}/instances/${instanceId}`;
  const testPageHost = new URL(options.testPageUrl).hostname;

  const waitForSelector = async (
    selector: string,
    timeoutMs = 8_000,
  ): Promise<boolean> => {
    return await waitUntil(
      async () => {
        const { status, body } = await makeRequest(
          runtime,
          `${prefix}/waitForElement`,
          {
            method: "POST",
            data: { selector, timeout: 400 },
            timeoutMs: 4_000,
            retries: 1,
          },
        );
        return (
          status === 200 &&
          isRecord(body) &&
          body.ok === true &&
          body.found === true
        );
      },
      { timeoutMs, intervalMs: 300 },
    );
  };

  await runTest(runtime, "Navigate to test page", `${prefix}/navigate`, {
    method: "POST",
    data: { url: options.testPageUrl },
  });

  if (!(await waitForSelector("#title", 10_000))) {
    recordResult(
      "Wait after navigate (#title)",
      "FAILED",
      "page did not become ready in time",
    );
  }

  await runValueTest(runtime, "Get URI", `${prefix}/uri`, {
    key: "uri",
    expected: options.testPageUrl,
  });

  await runValueTest(runtime, "Get HTML", `${prefix}/html`, {
    key: "html",
    predicate: (value) =>
      typeof value === "string" && value.includes("Floorp OS Test"),
    expectDescription: "contains Floorp OS Test",
  });

  await runValueTest(runtime, "Get Text (markdown)", `${prefix}/text`, {
    key: "text",
    predicate: (value) => typeof value === "string" && value.length > 0,
    expectDescription: "non-empty markdown",
  });

  await testWaitContract(runtime, basePath, instanceId);
  await testFingerprintSelectorCompatibility(
    runtime,
    basePath,
    instanceId,
    options.includeGetElement,
  );
  await testNegativeValidationMatrix(runtime, basePath, instanceId);

  await runValueTest(
    runtime,
    "Get element text (#title)",
    `${prefix}/elementText?selector=${q("#title")}`,
    {
      key: "text",
      expected: "Floorp OS Test",
    },
  );

  await runValueTest(
    runtime,
    "Get element textContent (#message)",
    `${prefix}/elementTextContent?selector=${q("#message")}`,
    {
      key: "text",
      expected: "Hello",
    },
  );

  await runTest(runtime, "Viewport screenshot", `${prefix}/screenshot`);
  await runTest(
    runtime,
    "Element screenshot (#title)",
    `${prefix}/elementScreenshot?selector=${q("#title")}`,
  );
  await runTest(
    runtime,
    "Full page screenshot",
    `${prefix}/fullPageScreenshot`,
  );
  await runTest(runtime, "Region screenshot", `${prefix}/regionScreenshot`, {
    method: "POST",
    data: { rect: { x: 0, y: 0, width: 200, height: 200 } },
  });

  await runTest(runtime, "Fill form", `${prefix}/fillForm`, {
    method: "POST",
    data: {
      formData: {
        "#name": "Taro Yamada",
        "#email": "yamada@example.com",
        "#message": "Floorp OS Server API test",
      },
      typingMode: false,
    },
    timeoutMs: 30_000,
  });

  await runValueTest(
    runtime,
    "Get value (#name)",
    `${prefix}/value?selector=${q("#name")}`,
    {
      key: "value",
      expected: "Taro Yamada",
    },
  );

  await runTest(runtime, "Submit form", `${prefix}/submit`, {
    method: "POST",
    data: { selector: "#testForm" },
  });

  await runTest(runtime, "Clear input (#name)", `${prefix}/clearInput`, {
    method: "POST",
    data: { selector: "#name" },
  });

  await runValueTest(
    runtime,
    "Get value (#name after clear)",
    `${prefix}/value?selector=${q("#name")}`,
    {
      key: "value",
      expected: "",
    },
  );

  await runValueTest(
    runtime,
    "Get attribute (#title data-test)",
    `${prefix}/attribute?selector=${q("#title")}&name=data-test`,
    {
      key: "value",
      expected: "title",
    },
  );

  await runValueTest(
    runtime,
    "Is visible (#title)",
    `${prefix}/isVisible?selector=${q("#title")}`,
    {
      key: "visible",
      expected: true,
    },
  );

  await runValueTest(
    runtime,
    "Is enabled (#name)",
    `${prefix}/isEnabled?selector=${q("#name")}`,
    {
      key: "enabled",
      expected: true,
    },
  );

  await runTest(
    runtime,
    "Select option (#color=red)",
    `${prefix}/selectOption`,
    {
      method: "POST",
      data: { selector: "#color", value: "red" },
    },
  );

  await runValueTest(
    runtime,
    "Get value (#color)",
    `${prefix}/value?selector=${q("#color")}`,
    {
      key: "value",
      expected: "red",
    },
  );

  await runTest(runtime, "Set checked (#agree)", `${prefix}/setChecked`, {
    method: "POST",
    data: { selector: "#agree", checked: true },
  });

  await runTest(runtime, "Hover (#hoverTarget)", `${prefix}/hover`, {
    method: "POST",
    data: { selector: "#hoverTarget" },
  });

  await runTest(runtime, "ScrollTo (#footer)", `${prefix}/scrollTo`, {
    method: "POST",
    data: { selector: "#footer" },
  });

  await runValueTest(runtime, "Page title", `${prefix}/title`, {
    key: "title",
    expected: "Floorp OS Test",
  });

  await runTest(runtime, "Double click (#dblTarget)", `${prefix}/doubleClick`, {
    method: "POST",
    data: { selector: "#dblTarget" },
  });

  await runTest(
    runtime,
    "Right click (#contextTarget)",
    `${prefix}/rightClick`,
    {
      method: "POST",
      data: { selector: "#contextTarget" },
    },
  );

  await runTest(runtime, "Focus (#name)", `${prefix}/focus`, {
    method: "POST",
    data: { selector: "#name" },
  });

  await runTest(runtime, "Drag and drop", `${prefix}/dragAndDrop`, {
    method: "POST",
    data: { sourceSelector: "#dragSource", targetSelector: "#dragTarget" },
  });

  await runTest(
    runtime,
    "Set innerHTML (#editable)",
    `${prefix}/setInnerHTML`,
    {
      method: "POST",
      data: { selector: "#editable", html: "<b>Bold</b>" },
    },
  );

  await runValueTest(
    runtime,
    "Text content after innerHTML",
    `${prefix}/elementTextContent?selector=${q("#editable")}`,
    {
      key: "text",
      expected: "Bold",
    },
  );

  await runTest(
    runtime,
    "Set textContent (#editable)",
    `${prefix}/setTextContent`,
    {
      method: "POST",
      data: { selector: "#editable", text: "Plain text" },
    },
  );

  await runValueTest(
    runtime,
    "Text content after textContent",
    `${prefix}/elementTextContent?selector=${q("#editable")}`,
    {
      key: "text",
      expected: "Plain text",
    },
  );

  await runTest(runtime, "Dispatch event", `${prefix}/dispatchEvent`, {
    method: "POST",
    data: {
      selector: "#eventTarget",
      eventType: "custom-event",
      options: { bubbles: true, cancelable: true },
    },
  });

  await runTest(runtime, "Input typing mode", `${prefix}/input`, {
    method: "POST",
    data: {
      selector: "#name",
      value: "Typed Text",
      typingMode: true,
      typingDelayMs: 10,
    },
  });

  await runValueTest(
    runtime,
    "Get value (#name after typing)",
    `${prefix}/value?selector=${q("#name")}`,
    {
      key: "value",
      expected: "Typed Text",
    },
  );

  await runTest(runtime, "Upload file", `${prefix}/uploadFile`, {
    method: "POST",
    data: { selector: "#fileInput", filePath: options.uploadFilePath },
    expectedStatus: 200,
    skipStatuses: [501],
  });

  await runValueTest(
    runtime,
    "Get value (#fileInput)",
    `${prefix}/value?selector=${q("#fileInput")}`,
    {
      key: "value",
      predicate: (value) => {
        if (typeof value !== "string") {
          return false;
        }
        return (
          value.length === 0 ||
          value.includes(options.uploadFilePath.split(/[\\/]/).pop() ?? "")
        );
      },
      expectDescription: "empty or contains uploaded filename",
    },
  );

  await runTest(
    runtime,
    "Wait for network idle",
    `${prefix}/waitForNetworkIdle`,
    {
      method: "POST",
      data: { timeout: 2000 },
      skipStatuses: [501],
    },
  );

  await runTest(runtime, "Accept alert", `${prefix}/acceptAlert`, {
    method: "POST",
    skipStatuses: [501],
  });

  await runTest(runtime, "Dismiss alert", `${prefix}/dismissAlert`, {
    method: "POST",
    skipStatuses: [501],
  });

  await runTest(
    runtime,
    "Navigate to test page (cookie host)",
    `${prefix}/navigate`,
    {
      method: "POST",
      data: { url: options.testPageUrl },
    },
  );

  if (!(await waitForSelector("#title", 10_000))) {
    recordResult(
      "Wait before cookie tests (#title)",
      "FAILED",
      "page did not become ready in time",
    );
  }

  await runTest(runtime, "Get cookies", `${prefix}/cookies`);

  await runTest(runtime, "Set cookie", `${prefix}/cookie`, {
    method: "POST",
    data: {
      name: "floorp-test",
      value: "123",
      domain: testPageHost,
      path: "/",
      sameSite: "Lax",
    },
    expectedStatus: 200,
  });

  await runValueTest(runtime, "Get cookies (after set)", `${prefix}/cookies`, {
    key: "cookies",
    predicate: (value) =>
      Array.isArray(value) &&
      value.some(
        (cookie) =>
          isRecord(cookie) &&
          cookie.name === "floorp-test" &&
          cookie.value === "123",
      ),
    expectDescription: "contains floorp-test=123",
  });
}

async function testBrowserInfo(runtime: RuntimeOptions): Promise<void> {
  console.log("\n[Browser Info]");
  await runTest(runtime, "Get tabs", "/browser/tabs");
  await runTest(runtime, "Get history", "/browser/history");
  await runTest(runtime, "Get downloads", "/browser/downloads");
  await runTest(runtime, "Get context", "/browser/context");
  await runTest(runtime, "Browser events stream", "/browser/events", {
    stream: true,
  });
}

async function testWorkspaces(runtime: RuntimeOptions): Promise<void> {
  console.log("\n[Workspaces]");
  const workspaces = await runTest(runtime, "List workspaces", "/workspaces", {
    extractKey: "workspaces",
  });

  await runTest(runtime, "Current workspace", "/workspaces/current");
  await runTest(runtime, "Next workspace", "/workspaces/next", {
    method: "POST",
  });
  await runTest(runtime, "Previous workspace", "/workspaces/previous", {
    method: "POST",
  });

  if (
    Array.isArray(workspaces) &&
    workspaces.length > 0 &&
    isRecord(workspaces[0])
  ) {
    const firstId = workspaces[0].id;
    if (typeof firstId === "string" && firstId.length > 0) {
      await runTest(
        runtime,
        `Switch workspace ${firstId}`,
        `/workspaces/${firstId}/switch`,
        {
          method: "POST",
        },
      );
    }
  }
}

async function testTabManager(
  runtime: RuntimeOptions,
  uploadFilePath: string,
  testPageUrl: string,
): Promise<void> {
  console.log("\n[Tab Manager]");

  await runTest(runtime, "List managed tabs", "/tabs/list");

  const browserTabs = await runTest(
    runtime,
    "Get browser tabs for attach",
    "/browser/tabs",
  );
  if (
    Array.isArray(browserTabs) &&
    browserTabs.length > 0 &&
    isRecord(browserTabs[0])
  ) {
    const browserId = browserTabs[0].id;
    if (typeof browserId === "number") {
      const attachedId = await runTest(
        runtime,
        "Attach to tab",
        "/tabs/attach",
        {
          method: "POST",
          data: { browserId },
          extractKey: "instanceId",
        },
      );
      if (typeof attachedId === "string" && attachedId.length > 0) {
        await runTest(
          runtime,
          "Destroy attached instance",
          `/tabs/instances/${attachedId}`,
          {
            method: "DELETE",
          },
        );
      }
    }
  }

  const tabId = await runTest(
    runtime,
    "Create tab instance",
    "/tabs/instances",
    {
      method: "POST",
      data: { url: testPageUrl, inBackground: true },
      extractKey: "instanceId",
    },
  );

  if (typeof tabId !== "string" || tabId.length === 0) {
    console.log(
      "Skipping remaining tab manager tests due to creation failure.",
    );
    return;
  }

  try {
    await runTest(
      runtime,
      "Check tab instance exists",
      `/tabs/instances/${tabId}/exists`,
    );
    await runTest(runtime, "Get tab instance info", `/tabs/instances/${tabId}`);

    await runTest(
      runtime,
      "Navigate tab instance",
      `/tabs/instances/${tabId}/navigate`,
      {
        method: "POST",
        data: { url: testPageUrl },
      },
    );

    await sleep(2000);
    await runTest(
      runtime,
      "Get tab instance URI",
      `/tabs/instances/${tabId}/uri`,
    );

    await testSharedAutomation(runtime, "/tabs", tabId, {
      includeGetElement: true,
      uploadFilePath,
      testPageUrl,
    });
  } finally {
    await runTest(runtime, "Destroy tab instance", `/tabs/instances/${tabId}`, {
      method: "DELETE",
    });
  }
}

async function testScraper(
  runtime: RuntimeOptions,
  uploadFilePath: string,
  testPageUrl: string,
): Promise<void> {
  console.log("\n[Scraper]");

  const scraperId = await runTest(
    runtime,
    "Create scraper instance",
    "/scraper/instances",
    {
      method: "POST",
      extractKey: "instanceId",
    },
  );

  if (typeof scraperId !== "string" || scraperId.length === 0) {
    console.log("Skipping remaining scraper tests due to creation failure.");
    return;
  }

  try {
    await runTest(
      runtime,
      "Check scraper instance exists",
      `/scraper/instances/${scraperId}/exists`,
    );

    await testSharedAutomation(runtime, "/scraper", scraperId, {
      includeGetElement: false,
      uploadFilePath,
      testPageUrl,
    });
  } finally {
    await runTest(
      runtime,
      "Destroy scraper instance",
      `/scraper/instances/${scraperId}`,
      {
        method: "DELETE",
      },
    );
  }
}

async function testParallelTabStability(
  runtime: RuntimeOptions,
  testPageUrl: string,
  workers: number,
  loops: number,
): Promise<void> {
  console.log("\n[Parallel Stability]");

  if (workers <= 0 || loops <= 0) {
    recordResult("Parallel tab stability", "SKIPPED", "workers/loops set to 0");
    return;
  }

  const workerTask = async (workerIndex: number): Promise<string[]> => {
    const errors: string[] = [];

    for (let loopIndex = 0; loopIndex < loops; loopIndex++) {
      const created = await makeRequest(runtime, "/tabs/instances", {
        method: "POST",
        data: { url: testPageUrl, inBackground: true },
        timeoutMs: 20_000,
        retries: 2,
      });

      if (
        created.status !== 200 ||
        !isRecord(created.body) ||
        typeof created.body.instanceId !== "string"
      ) {
        errors.push(
          `worker=${workerIndex} loop=${loopIndex} create failed status=${created.status}`,
        );
        continue;
      }

      const tabId = created.body.instanceId;

      try {
        const waited = await makeRequest(
          runtime,
          `/tabs/instances/${tabId}/waitForElement`,
          {
            method: "POST",
            data: { selector: "#title", timeout: 3000 },
            timeoutMs: 20_000,
            retries: 2,
          },
        );

        if (
          waited.status !== 200 ||
          !isRecord(waited.body) ||
          waited.body.ok !== true
        ) {
          errors.push(
            `worker=${workerIndex} loop=${loopIndex} wait failed status=${waited.status} body=${short(waited.body)}`,
          );
        }

        const valued = await makeRequest(
          runtime,
          `/tabs/instances/${tabId}/value?selector=${q("#name")}`,
          {
            timeoutMs: 20_000,
            retries: 2,
          },
        );

        if (
          valued.status !== 200 ||
          !isRecord(valued.body) ||
          !("value" in valued.body)
        ) {
          errors.push(
            `worker=${workerIndex} loop=${loopIndex} value failed status=${valued.status} body=${short(valued.body)}`,
          );
        }
      } finally {
        await makeRequest(runtime, `/tabs/instances/${tabId}`, {
          method: "DELETE",
          timeoutMs: 20_000,
          retries: 1,
        });
      }
    }

    return errors;
  };

  const allErrors = (
    await Promise.all(
      Array.from({ length: workers }, (_, index) => workerTask(index)),
    )
  ).flat();

  if (allErrors.length > 0) {
    recordResult(
      "Parallel tab stability",
      "FAILED",
      `${allErrors.length} issues; first: ${allErrors[0]}`,
    );
    console.log(`FAILED (${allErrors.length} issues)`);
  } else {
    recordResult(
      "Parallel tab stability",
      "OK",
      `workers=${workers}, loops=${loops}`,
    );
    console.log(`OK (workers=${workers}, loops=${loops})`);
  }
}

export async function main(argv: string[] = Deno.args): Promise<number> {
  const parsed = parseArgs(argv, {
    string: ["base-url", "concurrency-workers", "concurrency-loops"],
    boolean: [
      "quick",
      "skip-browser-info",
      "skip-workspaces",
      "skip-tabs",
      "skip-scraper",
      "skip-concurrency",
    ],
    default: {
      "base-url": DEFAULT_BASE_URL,
      "concurrency-workers": "3",
      "concurrency-loops": "2",
      quick: false,
      "skip-browser-info": false,
      "skip-workspaces": false,
      "skip-tabs": false,
      "skip-scraper": false,
      "skip-concurrency": false,
    },
  });

  const runtime: RuntimeOptions = {
    baseUrl: String(parsed["base-url"]),
  };
  const quick = parsed.quick === true;
  const skipBrowserInfo = parsed["skip-browser-info"] === true;
  const skipWorkspaces = parsed["skip-workspaces"] === true;
  const skipTabs = parsed["skip-tabs"] === true;
  const skipScraper = parsed["skip-scraper"] === true;
  const skipConcurrency = parsed["skip-concurrency"] === true;
  const workerCount = Number(parsed["concurrency-workers"]);
  const loopCount = Number(parsed["concurrency-loops"]);

  clearResults();

  console.log(`Verifying Floorp OS Server API at ${runtime.baseUrl}`);
  console.log("Ensure floorp.os.enabled is true in about:config");
  console.log("-".repeat(50));

  const uploadFilePath = await createTempUploadFile();
  let server: TestPageServer | null = null;

  try {
    try {
      server = await startTestHttpServer();
    } catch (error) {
      const detail = error instanceof Error ? error.message : String(error);
      console.error(`Failed to start local test HTTP server: ${detail}`);
      recordResult("Start local test HTTP server", "FAILED", detail);
      const summary = printSummary();
      return summary.failed > 0 ? 1 : 0;
    }

    const health = await runTest(runtime, "Health check", "/health");
    if (health === null) {
      console.error("\nServer seems down or unreachable.");
      recordResult("Health check reachability", "FAILED", "server unreachable");
      const summary = printSummary();
      return summary.failed > 0 ? 1 : 0;
    }

    if (!skipBrowserInfo) {
      await testBrowserInfo(runtime);
    }
    if (!skipWorkspaces) {
      await testWorkspaces(runtime);
    }
    if (!skipTabs) {
      await testTabManager(runtime, uploadFilePath, server.url);
    }
    if (!skipScraper) {
      await testScraper(runtime, uploadFilePath, server.url);
    }

    if (!skipConcurrency && !skipTabs) {
      const workers = quick ? 2 : workerCount;
      const loops = quick ? 1 : loopCount;
      await testParallelTabStability(runtime, server.url, workers, loops);
    }
  } finally {
    await server?.close();
    await Deno.remove(uploadFilePath).catch(() => undefined);

    console.log("-".repeat(50));
    console.log("Verification complete.");
  }

  const summary = printSummary();
  return summary.failed > 0 ? 1 : 0;
}

if (import.meta.main) {
  const code = await main(Deno.args);
  Deno.exit(code);
}
