// SPDX-License-Identifier: MPL-2.0
// Actual frozen Chrome PAC URL routing reference, using local proxy endpoints.
import { spawn } from "node:child_process";
import { createHash } from "node:crypto";
import { mkdirSync, mkdtempSync, writeFileSync, readFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { createServer } from "node:http";
const root = resolve(".");
const chrome = join(
  root,
  "_dist/chrome-reference/extracted/opt/google/chrome/chrome"
);
const output = mkdtempSync(
  join(root, "_dist/chrome-runtime-tests/proxy-reference-")
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
  })
);
writeFileSync(
  join(fixture, "probe.html"),
  "<!doctype html><title>PAC URL reference</title>"
);
const traffic = [];
function endpoint(role) {
  const server = createServer((req, res) => {
    traffic.push({ role, method: req.method, url: req.url });
    res.end(role);
  });
  server.on("connect", (req, socket) => {
    traffic.push({ role, method: "CONNECT", url: req.url });
    socket.end("HTTP/1.1 502 Bad Gateway\r\nContent-Length: 0\r\n\r\n");
  });
  return server;
}
const full = endpoint("full"),
  stripped = endpoint("stripped");
await Promise.all(
  [full, stripped].map(
    server =>
      new Promise(resolveListen => server.listen(0, "127.0.0.1", resolveListen))
  )
);
class ChromePipe {
  constructor() {
    this.next = 0;
    this.pending = new Map();
    this.buffer = "";
    this.stderr = "";
    this.process = spawn(
      chrome,
      [
        "--headless",
        "--disable-gpu",
        "--disable-background-networking",
        "--disable-component-update",
        "--disable-sync",
        "--disable-default-apps",
        "--no-first-run",
        "--no-default-browser-check",
        "--remote-debugging-pipe",
        "--enable-unsafe-extension-debugging",
        `--user-data-dir=${join(output, "chrome-profile")}`,
        "about:blank",
      ],
      { stdio: ["ignore", "ignore", "pipe", "pipe", "pipe"] }
    );
    this.process.stderr.on("data", chunk => {
      this.stderr += chunk;
    });
    this.process.stdio[4].on("data", chunk => {
      this.buffer += chunk;
      let index;
      while ((index = this.buffer.indexOf("\0")) >= 0) {
        const text = this.buffer.slice(0, index);
        this.buffer = this.buffer.slice(index + 1);
        if (!text) continue;
        const message = JSON.parse(text);
        const pending = this.pending.get(message.id);
        if (pending) {
          this.pending.delete(message.id);
          if (message.error) {
            pending.reject(new Error(JSON.stringify(message.error)));
          } else pending.resolve(message.result);
        }
      }
    });
    this.process.on("error", error => this.fail(error));
    this.process.on("exit", code =>
      this.fail(new Error(`Chrome exited: ${code}`))
    );
  }
  fail(error) {
    for (const pending of this.pending.values()) pending.reject(error);
    this.pending.clear();
  }
  call(method, params = {}, sessionId) {
    const id = ++this.next;
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pending.delete(id);
        reject(new Error(`CDP ${method} timed out`));
      }, 8000);
      this.pending.set(id, {
        resolve(value) {
          clearTimeout(timer);
          resolve(value);
        },
        reject(error) {
          clearTimeout(timer);
          reject(error);
        },
      });
      this.process.stdio[3].write(
        JSON.stringify({
          id,
          method,
          params,
          ...(sessionId ? { sessionId } : {}),
        }) + "\0"
      );
    });
  }
  async close() {
    try {
      await this.call("Browser.close");
    } catch {
      /* Process may already have exited. */
    }
    this.process.kill();
    writeFileSync(join(output, "chrome.log"), this.stderr);
  }
}

const browser = new ChromePipe();
let report;
try {
  const version = await browser.call("Browser.getVersion");
  const baseline = JSON.parse(
    readFileSync(
      join(root, "tools/chrome-extension-runtime/chrome-reference.json"),
      "utf8"
    )
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
  const data="function FindProxyForURL(url,host){return url.indexOf('/secret')>=0?'PROXY 127.0.0.1:${full.address().port}':'PROXY 127.0.0.1:${stripped.address().port}';}";
  await chrome.proxy.settings.set({value:{mode:'pac_script',pacScript:{data}},scope:'regular'});
  const http=await fetch('http://floorp-proxy-reference.invalid/secret',{signal:AbortSignal.timeout(4000)}).then(r=>r.text());
  let https;try{await fetch('https://floorp-proxy-reference.invalid/secret',{signal:AbortSignal.timeout(3000)});https='unexpected-success';}catch(error){https=String(error);}
  return {http,https,value:await chrome.proxy.settings.get({})};
 })()`,
    },
    sessionId
  );
  report = {
    schemaVersion: 1,
    version,
    traffic,
    result: result.result.value,
    exception: result.exceptionDetails,
  };
  report.passed =
    report.result?.http === "full" &&
    traffic.some(r => r.method === "CONNECT" && r.role === "stripped") &&
    !traffic.some(r => r.method === "CONNECT" && r.role === "full");
  writeFileSync(
    join(output, "report.json"),
    JSON.stringify(report, null, 2) + "\n"
  );
  console.log(
    JSON.stringify({
      passed: report.passed,
      report: join(output, "report.json"),
      traffic,
      result: report.result,
    })
  );
  if (!report.passed) process.exitCode = 1;
} finally {
  await browser.close();
  for (const server of [full, stripped]) {
    server.closeAllConnections();
    server.close();
  }
}
