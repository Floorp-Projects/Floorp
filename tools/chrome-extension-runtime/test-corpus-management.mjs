#!/usr/bin/env node
// SPDX-License-Identifier: MPL-2.0
// Operate the unchanged Extensity package through its own visible UI.
import { ChromePipe } from "./chrome-cdp-pipe.mjs";
import { createHash } from "node:crypto";
import { createServer } from "node:http";
import { mkdirSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "../..");
const tools = join(root, "tools/chrome-extension-runtime");
const corpus =
  /** @type {import("./corpus-types.ts").FrozenCorpus} */ (JSON.parse(
    readFileSync(join(tools, "corpus-lock.json"), "utf8"),
  ));
const reference = JSON.parse(
  readFileSync(join(tools, "chrome-reference.json"), "utf8"),
);
const id = "jjmflmamggggndanpgfnpelongoepncg";
const frozen = corpus.extensions.find((item) => item.id === id);
if (!frozen) throw Error("Frozen extension missing");
/** @param {string | Uint8Array} bytes */
const digest = (bytes) => createHash("sha256").update(bytes).digest("hex");
const chrome = join(
  root,
  "_dist/chrome-reference/extracted/opt/google/chrome/chrome",
);
if (digest(readFileSync(chrome)) !== reference.binary.sha256) {
  throw Error("Reference binary hash mismatch");
}
if (
  digest(
    readFileSync(
      join(root, "_dist/chrome-extension-corpus/packages", id + ".crx"),
    ),
  ) !== frozen.packageSha256
) throw Error("Frozen signed package hash mismatch");
const output = mkdtempSync(
  join(root, "_dist/chrome-runtime-tests/corpus-management-reference-"),
);
const fixtureDirectory = join(output, "authored-extension");
mkdirSync(fixtureDirectory);
const fixtureFiles = {
  "manifest.json": JSON.stringify({
    manifest_version: 3,
    name: "Authored lifecycle probe",
    version: "1.0",
    content_scripts: [{ matches: ["http://127.0.0.1/*"], js: ["content.js"] }],
  }),
  "content.js":
    "document.documentElement.dataset.authoredLifecycleProbe = 'active';",
};
for (const [name, bytes] of Object.entries(fixtureFiles)) {
  writeFileSync(join(fixtureDirectory, name), bytes);
}
/** @type {import("./browser-tools-test-types.ts").ReferenceBrowserToolsReport} */
const report = {
  stage: "actual-package-primary-operation",
  reference,
  output,
  extension: {
    id,
    name: frozen.name,
    version: frozen.version,
    packageSha256: frozen.packageSha256,
    primaryOperation: frozen.primaryOperation,
    primaryOperationResult: "not-run",
    checks: [],
    snapshots: [],
  },
  fixture: {
    url: "",
    files: Object.fromEntries(
      Object.entries(fixtureFiles).map((
        [name, bytes],
      ) => [name, digest(bytes)]),
    ),
  },
};
const save = () =>
  writeFileSync(join(output, "report.json"), JSON.stringify(report, null, 2));
const html =
  "<!doctype html><title>Authored lifecycle fixture</title><p>Observe real extension content-script lifecycle.</p>";
const server = createServer((_req, res) => {
  res.writeHead(200, {
    "Content-Type": "text/html",
    "Cache-Control": "no-store",
  });
  res.end(html);
});
await new Promise((resolve) =>
  server.listen(0, "127.0.0.1", () => resolve(undefined))
);
report.fixture.url = `http://127.0.0.1:${
  /** @type {import("node:net").AddressInfo} */ (server.address()).port
}/`;
report.fixture.pageSha256 = digest(html);
const browser = new ChromePipe(chrome, output);
/** @param {number} ms */
const delay = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
/** @template [T=unknown] @param {string} sessionId @param {string} expression @returns {Promise<T>} */
async function evaluate(sessionId, expression) {
  const response = await browser.call("Runtime.evaluate", {
    expression,
    returnByValue: true,
    awaitPromise: true,
  }, sessionId);
  if (response.exceptionDetails) {
    throw Error(JSON.stringify(response.exceptionDetails));
  }
  return /** @type {T} */ (response.result.value);
}
/** @template T @param {() => T | Promise<T>} read @param {string} label */
async function until(read, label) {
  const deadline = Date.now() + 20000;
  while (Date.now() < deadline) {
    const value = await read();
    if (value) return value;
    await delay(100);
  }
  throw Error("Timed out waiting for " + label);
}
/** @param {string} url */
async function open(url) {
  const { targetId } = await browser.call("Target.createTarget", { url });
  const { sessionId } = await browser.call("Target.attachToTarget", {
    targetId,
    flatten: true,
  });
  await until(
    () => evaluate(sessionId, "document.readyState==='complete'"),
    "document",
  );
  return { targetId, sessionId };
}
/** @param {string} sessionId */
async function clickProbe(sessionId) {
  const point = await evaluate(
    sessionId,
    `(() => { const e=[...document.querySelectorAll('li.extension')].find(e=>e.innerText.includes('Authored lifecycle probe')); if(!e) return null; e.scrollIntoView(); const r=e.getBoundingClientRect(); return {x:r.x+r.width/2,y:r.y+r.height/2}; })()`,
  );
  if (!point) throw Error("Real Extensity extension row missing");
  await browser.call("Input.dispatchMouseEvent", {
    type: "mousePressed",
    button: "left",
    clickCount: 1,
    ...point,
  }, sessionId);
  await browser.call("Input.dispatchMouseEvent", {
    type: "mouseReleased",
    button: "left",
    clickCount: 1,
    ...point,
  }, sessionId);
}
try {
  report.version = await browser.call("Browser.getVersion");
  report.fixture.extensionId =
    (await browser.call("Extensions.loadUnpacked", { path: fixtureDirectory }))
      .id;
  const loaded = await browser.call("Extensions.loadUnpacked", {
    path: join(root, "_dist/chrome-extension-corpus/chrome-unpacked", id),
  });
  if (loaded.id !== id) throw Error("Original Chrome identity mismatch");
  const page = await open(report.fixture.url);
  await until(
    () =>
      evaluate(
        page.sessionId,
        "document.documentElement.dataset.authoredLifecycleProbe==='active'",
      ),
    "initial authored content script",
  );
  const ui = await open(`chrome-extension://${id}/index.html`);
  await until(
    () =>
      evaluate(
        ui.sessionId,
        "[...document.querySelectorAll('li.extension')].some(e=>e.innerText.includes('Authored lifecycle probe'))",
      ),
    "Extensity actual extension list",
  );
  report.extension.before = await evaluate(
    ui.sessionId,
    "document.body.innerText",
  );
  const enabled = () =>
    evaluate(
      ui.sessionId,
      `new Promise((resolve,reject)=>chrome.management.get(${
        JSON.stringify(report.fixture.extensionId)
      },r=>chrome.runtime.lastError?reject(Error(chrome.runtime.lastError.message)):resolve(r.enabled)))`,
    );
  await clickProbe(ui.sessionId);
  await until(
    async () => (await enabled()) === false,
    "Extensity disable operation",
  );
  await browser.call("Page.reload", {}, page.sessionId);
  await until(
    () =>
      evaluate(
        page.sessionId,
        "document.readyState==='complete'&&!document.documentElement.dataset.authoredLifecycleProbe",
      ),
    "content script absent after disable and navigation",
  );
  report.extension.checks.push(
    "Trusted click on unchanged Extensity UI disabled the authored extension and stopped its actual content script",
  );
  await clickProbe(ui.sessionId);
  await until(enabled, "Extensity enable operation");
  await browser.call("Page.reload", {}, page.sessionId);
  await until(
    () =>
      evaluate(
        page.sessionId,
        "document.documentElement.dataset.authoredLifecycleProbe==='active'",
      ),
    "content script returns after enable and navigation",
  );
  report.extension.checks.push(
    "Second trusted UI click re-enabled the extension and restored its actual content script",
  );
  const { data } = await browser.call("Page.captureScreenshot", {
    format: "png",
  }, ui.sessionId);
  writeFileSync(
    join(output, "extensity-enabled.png"),
    Buffer.from(data, "base64"),
  );
  report.extension.primaryOperationResult = "passed";
} catch (error) {
  report.extension.primaryOperationResult = "failed";
  report.extension.error = String(error);
} finally {
  await browser.close();
  await new Promise((resolve) => server.close(resolve));
  save();
}
console.log(JSON.stringify({ output, ...report.extension }));
if (report.extension.primaryOperationResult !== "passed") process.exitCode = 1;
