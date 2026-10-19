#!/usr/bin/env node
// SPDX-License-Identifier: MPL-2.0
import { ChromePipe } from "./chrome-cdp-pipe.mjs";
import { createHash } from "node:crypto";
import { createServer } from "node:http";
import { mkdirSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
const root = resolve(".");
const tools = join(root, "tools/chrome-extension-runtime");
const reference = JSON.parse(
  readFileSync(join(tools, "chrome-reference.json"))
);
const chrome = join(
  root,
  "_dist/chrome-reference/extracted/opt/google/chrome/chrome"
);
if (
  createHash("sha256").update(readFileSync(chrome)).digest("hex") !==
  reference.binary.sha256
)
  throw Error("Frozen Chrome reference differs");
const output = mkdtempSync(
  join(root, "_dist/chrome-runtime-tests/response-headers-reference-")
);
const traffic = [];
const server = createServer((request, response) => {
  const parsed = new URL(request.url, "http://localhost");
  traffic.push({ path: request.url, headers: request.headers });
  const headers = {
    "content-type": ["application/json"],
    ...JSON.parse(parsed.searchParams.get("h") || "{}"),
  };
  if (parsed.pathname === "/frame") {
    headers["content-type"] = ["text/html"];
    if (parsed.searchParams.get("allow") === "1") headers["x-allow"] = ["yes"];
  }
  response.writeHead(200, {
    ...headers,
    "access-control-allow-origin": "*",
    "cache-control": "no-store",
  });
  response.end(
    parsed.pathname === "/frame"
      ? `<script>fetch('/child').then(response=>parent.postMessage({key:${JSON.stringify(parsed.searchParams.get("key"))},allowed:response.ok},'*'),()=>parent.postMessage({key:${JSON.stringify(parsed.searchParams.get("key"))},allowed:false},'*'))</script>`
      : JSON.stringify({ path: request.url })
  );
});
await new Promise(done => server.listen(0, "127.0.0.1", done));
const origin = `http://127.0.0.1:${server.address().port}/`;
const fixture = join(output, "fixture");
mkdirSync(fixture);
writeFileSync(
  join(fixture, "manifest.json"),
  JSON.stringify({
    manifest_version: 3,
    name: "Authored response-header reference",
    version: "1",
    permissions: ["declarativeNetRequest", "declarativeNetRequestFeedback"],
    host_permissions: ["<all_urls>"],
  })
);
writeFileSync(
  join(fixture, "probe.html"),
  '<!doctype html><title>Header reference</title><script type="module" src="probe.mjs"></script>'
);
writeFileSync(
  join(fixture, "vectors.mjs"),
  readFileSync(join(tools, "ResponseHeadersVectors.mjs"))
);
writeFileSync(
  join(fixture, "probe.mjs"),
  `import {runResponseHeaderVectors} from './vectors.mjs'; try{window.result={passed:true,...await runResponseHeaderVectors(chrome,${JSON.stringify(origin)})};}catch(error){window.result={passed:false,error:String(error),checks:error.checks,observations:error.observations};}`
);
const browser = new ChromePipe(chrome, output);
const report = {
  passed: false,
  reference,
  browserMode: "headless",
  origin,
  traffic,
};
try {
  report.version = await browser.call("Browser.getVersion");
  if (report.version.product !== `Chrome/${reference.browserVersion}`)
    throw Error("Chrome version differs");
  const { id } = await browser.call("Extensions.loadUnpacked", {
    path: fixture,
  });
  const { targetId } = await browser.call("Target.createTarget", {
    url: `chrome-extension://${id}/probe.html`,
  });
  const { sessionId } = await browser.call("Target.attachToTarget", {
    targetId,
    flatten: true,
  });
  await browser.call("Runtime.enable", {}, sessionId);
  const deadline = Date.now() + 60000;
  while (Date.now() < deadline) {
    const result = await browser.call(
      "Runtime.evaluate",
      { expression: "window.result", returnByValue: true },
      sessionId
    );
    if (result.result.value) {
      Object.assign(report, result.result.value);
      break;
    }
    await new Promise(done => setTimeout(done, 100));
  }
  const missing = report.observations?.filter(
    info =>
      info.serverMustSee &&
      !traffic.some(request => request.path === info.serverMustSee)
  );
  if (missing?.length) {
    report.passed = false;
    throw Error(
      "Response-stage action ran before genuine server response: " +
        JSON.stringify(missing)
    );
  }
} catch (error) {
  report.error = String(error);
} finally {
  await browser.close();
  server.closeAllConnections();
  server.close();
  writeFileSync(join(output, "report.json"), JSON.stringify(report, null, 2));
  console.log(
    `${report.passed ? "PASS" : "FAIL"} ${join(output, "report.json")}`
  );
}
process.exitCode = report.passed ? 0 : 1;
