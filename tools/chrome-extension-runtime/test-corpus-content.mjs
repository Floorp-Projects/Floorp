#!/usr/bin/env node
// SPDX-License-Identifier: MPL-2.0
// Actual immutable package operations on controlled content; no API substitutes.
import { ChromePipe } from "./chrome-cdp-pipe.mjs";
import { createHash } from "node:crypto";
import { createServer } from "node:http";
import { mkdirSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
const tools = dirname(fileURLToPath(import.meta.url)),
  root = resolve(tools, "../..");
const corpus = JSON.parse(readFileSync(join(tools, "corpus-lock.json")));
const reference = JSON.parse(
  readFileSync(join(tools, "chrome-reference.json")),
);
const chrome = join(
  root,
  "_dist/chrome-reference/extracted/opt/google/chrome/chrome",
);
const digest = (bytes) => createHash("sha256").update(bytes).digest("hex");
if (digest(readFileSync(chrome)) !== reference.binary.sha256) {
  throw Error("Reference binary hash mismatch");
}
const parent = join(root, "_dist/chrome-runtime-tests");
mkdirSync(parent, { recursive: true });
const output = mkdtempSync(join(parent, "corpus-content-reference-"));
const report = {
  stage: "actual-package-primary-operation",
  reference,
  fixture: {},
  extensions: [],
  primaryOperationPassRate: null,
  output,
};
const save = () =>
  writeFileSync(join(output, "report.json"), JSON.stringify(report, null, 2));
const authored =
  "<!doctype html><meta charset=utf-8><title>Authored content transformation</title><style>html,body{background:rgb(255,255,255);color:rgb(0,0,0)}body{margin:30px;font:20px sans-serif}#probe{padding:30px;background:rgb(210,220,230)}</style><h1>Authored extension content operation</h1><p id=probe>Verify the real installed extension changes this authored page.</p><a href=/second>Navigate and verify persistence</a>";
const server = createServer((req, res) => {
  res.writeHead(200, {
    "Content-Type": "text/html; charset=utf-8",
    "Cache-Control": "no-store",
  });
  res.end(authored);
});
await new Promise((resolve) => server.listen(0, "127.0.0.1", resolve));
const fixture = `http://127.0.0.1:${server.address().port}/`;
report.fixture = { url: fixture, sha256: digest(authored) };
const browser = new ChromePipe(chrome, output);
const delay = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
async function evaluate(sessionId, expression) {
  const r = await browser.call("Runtime.evaluate", {
    expression,
    returnByValue: true,
    awaitPromise: true,
  }, sessionId);
  if (r.exceptionDetails) throw Error(JSON.stringify(r.exceptionDetails));
  return r.result.value;
}
async function until(read, label) {
  const end = Date.now() + 25000;
  while (Date.now() < end) {
    const result = await read();
    if (result) return result;
    await delay(100);
  }
  throw Error("Timed out waiting for " + label);
}
const styles =
  `({url:location.href,ready:document.readyState,body:getComputedStyle(document.body).backgroundColor,color:getComputedStyle(document.body).color,probe:getComputedStyle(document.querySelector('#probe')).backgroundColor,markers:[...document.querySelectorAll('style.darkreader')].map(e=>e.className),text:document.querySelector('#probe')?.textContent})`;
try {
  report.version = await browser.call("Browser.getVersion");
  const { targetId } = await browser.call("Target.createTarget", {
    url: fixture,
  });
  const { sessionId } = await browser.call("Target.attachToTarget", {
    targetId,
    flatten: true,
  });
  await until(
    async () =>
      await evaluate(
        sessionId,
        "document.readyState==='complete'&&!!document.querySelector('#probe')",
      ),
    "authored content",
  );
  const baseline = await evaluate(sessionId, styles);
  for (const id of ["eimadpbcbfnmbkopoojfekhnkhdbieeh"]) {
    const extension = corpus.extensions.find((item) => item.id === id);
    const item = {
      id,
      name: extension.name,
      version: extension.version,
      packageSha256: extension.packageSha256,
      primaryOperation: extension.primaryOperation,
      primaryOperationResult: "not-run",
      checks: [],
      baseline,
    };
    report.extensions.push(item);
    let loaded = false;
    try {
      if (
        digest(
          readFileSync(
            join(root, "_dist/chrome-extension-corpus/packages", `${id}.crx`),
          ),
        ) !== extension.packageSha256
      ) throw Error("Frozen package hash mismatch");
      const result = await browser.call("Extensions.loadUnpacked", {
        path: join(root, "_dist/chrome-extension-corpus/chrome-unpacked", id),
      });
      loaded = true;
      if (result.id !== id) throw Error("Original Chrome identity mismatch");
      // Reload gives the unchanged extension its normal document-start injection.
      await browser.call("Page.reload", {}, sessionId);
      item.transformed = await until(async () => {
        const value = await evaluate(sessionId, styles).catch(() => null);
        return value?.markers?.length && value.body !== baseline.body
          ? value
          : null;
      }, "actual extension dynamic theme");
      item.checks.push(
        "Actual signed-package Dark Reader script transformed the authored page",
      );
      await browser.call(
        "Page.navigate",
        { url: fixture + "second" },
        sessionId,
      );
      item.afterNavigation = await until(async () => {
        const value = await evaluate(sessionId, styles).catch(() => null);
        return value?.url === fixture + "second" && value.markers?.length &&
            value.body !== baseline.body
          ? value
          : null;
      }, "theme after native navigation");
      item.checks.push("Transformation persists after real page navigation");
      const { data } = await browser.call("Page.captureScreenshot", {
        format: "png",
      }, sessionId);
      writeFileSync(join(output, `${id}.png`), Buffer.from(data, "base64"));
      await browser.call("Extensions.uninstall", { id });
      loaded = false;
      await browser.call("Page.reload", {}, sessionId);
      item.afterRemoval = await until(async () => {
        const value = await evaluate(sessionId, styles).catch(() => null);
        return value?.body === baseline.body && !value.markers.length
          ? value
          : null;
      }, "restored authored page after uninstall");
      item.checks.push("Uninstall removes the real transformation");
      item.primaryOperationResult = "passed";
    } catch (error) {
      item.primaryOperationResult = "failed";
      item.error = String(error);
    } finally {
      if (loaded) {
        try {
          await browser.call("Extensions.uninstall", { id });
        } catch {}
      }
      save();
    }
  }
} finally {
  await browser.close();
  await new Promise((resolve) => server.close(resolve));
  save();
}
console.log(
  JSON.stringify(
    report.extensions.map(({ name, primaryOperationResult, error }) => ({
      name,
      primaryOperationResult,
      error,
    })),
  ),
);
console.log(`REPORT ${join(output, "report.json")}`);
