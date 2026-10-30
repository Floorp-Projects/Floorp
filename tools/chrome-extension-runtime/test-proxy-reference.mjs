// SPDX-License-Identifier: MPL-2.0
// Actual frozen Chrome PAC URL routing reference, using local proxy endpoints.
import { ChromePipe } from "./chrome-cdp-pipe.mjs";
import { createHash } from "node:crypto";
import { mkdirSync, mkdtempSync, writeFileSync, readFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { createServer } from "node:http";
/** @param {import("node:http").Server} server */
function portOf(server) {
  const address = server.address();
  if (!address || typeof address === "string")
    throw Error("Expected local TCP listener");
  return address.port;
}
const root = resolve(".");
const chrome = join(
  root,
  "_dist/chrome-reference/extracted/opt/google/chrome/chrome",
);
const output = mkdtempSync(
  join(root, "_dist/chrome-runtime-tests/proxy-reference-"),
);
const fixture = join(output, "fixture");
mkdirSync(fixture);
writeFileSync(
  join(fixture, "manifest.json"),
  JSON.stringify({
    manifest_version: 3,
    name: "PAC URL reference",
    version: "1",
    permissions: ["proxy"],
    host_permissions: ["<all_urls>"],
  }),
);
writeFileSync(
  join(fixture, "probe.html"),
  "<!doctype html><title>PAC URL reference</title>",
);
/** @type {import("./network-test-types.ts").ProxyTraffic[]} */
const traffic = [];
/** @param {string} role */
function endpoint(role) {
  const server = createServer((req, res) => {
    traffic.push({ role, method: req.method, url: req.url ?? "" });
    res.end(role);
  });
  server.on("connect", (req, socket) => {
    traffic.push({ role, method: "CONNECT", url: req.url ?? "" });
    socket.end("HTTP/1.1 502 Bad Gateway\r\nContent-Length: 0\r\n\r\n");
  });
  return server;
}
const full = endpoint("full"),
  stripped = endpoint("stripped");
await Promise.all(
  [full, stripped].map(
    (server) =>
      new Promise((resolveListen) =>
        server.listen(0, "127.0.0.1", () => resolveListen(undefined)),
      ),
  ),
);
const browser = new ChromePipe(chrome, output);

try {
  const version = await browser.call("Browser.getVersion");
  const baseline = JSON.parse(
    readFileSync(
      join(root, "tools/chrome-extension-runtime/chrome-reference.json"),
      "utf8",
    ),
  );
  if (
    version.product !== `Chrome/${baseline.browserVersion}` ||
    createHash("sha256").update(readFileSync(chrome)).digest("hex") !==
      baseline.binary.sha256
  )
    throw Error("Reference browser differs from frozen verified Chrome");
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
  const result = await browser.call(
    "Runtime.evaluate",
    {
      awaitPromise: true,
      returnByValue: true,
      expression: `(async()=>{
  const data="function FindProxyForURL(url,host){return url.indexOf('/secret')>=0||url.indexOf('authored-user')>=0?'PROXY 127.0.0.1:${portOf(full)}':'PROXY 127.0.0.1:${portOf(stripped)}';}";
  await chrome.proxy.settings.set({value:{mode:'pac_script',pacScript:{data}},scope:'regular'});
  const http=await fetch('http://floorp-proxy-reference.invalid/secret',{signal:AbortSignal.timeout(4000)}).then(r=>r.text());
  let https;try{await fetch('https://floorp-proxy-reference.invalid/secret',{signal:AbortSignal.timeout(3000)});https='unexpected-success';}catch(error){https=String(error);}
  return {http,https,value:await chrome.proxy.settings.get({})};
 })()`,
    },
    sessionId,
  );
  /** @type {import("./network-test-types.ts").ProxyReferenceReport} */
  const report = {
    schemaVersion: 1,
    version,
    traffic,
    result:
      /** @type {import("./network-test-types.ts").ProxyReferenceReport["result"]} */ (
        result.result.value
      ),
    exception: result.exceptionDetails,
  };
  /** @type {["credentials" | "fragment", string, string][]} */
  const paths = [
    [
      "credentials",
      "http://authored-user:authored-password@floorp-proxy-reference.invalid/clean-credentials",
      "/clean-credentials",
    ],
    [
      "fragment",
      "http://floorp-proxy-reference.invalid/clean-fragment#/secret",
      "/clean-fragment",
    ],
  ];
  for (const [kind, url, path] of paths) {
    await browser.call("Target.createTarget", { url });
    const deadline = Date.now() + 5000;
    while (
      Date.now() < deadline &&
      !traffic.some(
        (request) => request.method === "GET" && request.url.includes(path),
      )
    )
      await new Promise((resolveTick) => setTimeout(resolveTick, 50));
    report[kind] = traffic.find(
      (request) => request.method === "GET" && request.url.includes(path),
    );
  }
  report.passed =
    report.result?.http === "full" &&
    traffic.some((r) => r.method === "CONNECT" && r.role === "stripped") &&
    !traffic.some((r) => r.method === "CONNECT" && r.role === "full") &&
    report.credentials?.role === "stripped" &&
    report.fragment?.role === "stripped";
  writeFileSync(
    join(output, "report.json"),
    JSON.stringify(report, null, 2) + "\n",
  );
  console.log(
    JSON.stringify({
      passed: report.passed,
      report: join(output, "report.json"),
      traffic,
      result: report.result,
    }),
  );
  if (!report.passed) process.exitCode = 1;
} finally {
  await browser.close();
  for (const server of [full, stripped]) {
    server.closeAllConnections();
    server.close();
  }
}
