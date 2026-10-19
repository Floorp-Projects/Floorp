#!/usr/bin/env node
// SPDX-License-Identifier: MPL-2.0
import { ChromePipe } from "./chrome-cdp-pipe.mjs";
import { fileURLToPath } from "node:url";
import { dirname, resolve } from "node:path";
import { createHash } from "node:crypto";
const tools = dirname(fileURLToPath(import.meta.url)),
  root = resolve(tools, "../..");
const baseline = JSON.parse(
  readFileSync(join(tools, "chrome-reference.json"), "utf8"),
);
import { mkdirSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { createServer } from "node:http";
const server = createServer((q, r) =>
  r.end("<title>desktop capture target</title>")
);
await new Promise((resolve) => server.listen(0, "127.0.0.1", resolve));
const url = `http://127.0.0.1:${server.address().port}/`;
const output = mkdtempSync(
  join(root, "_dist/chrome-runtime-tests/desktop-chrome-contract-"),
);
const fixture = join(output, "fixture");
mkdirSync(fixture);
writeFileSync(
  join(fixture, "manifest.json"),
  JSON.stringify({
    manifest_version: 3,
    name: "Authored desktop API contract",
    version: "1.0",
    permissions: ["desktopCapture", "tabs"],
    background: { service_worker: "background.js" },
  }),
);
writeFileSync(
  join(fixture, "background.js"),
  `chrome.runtime.onMessage.addListener((message,sender,sendResponse)=>{const result={mode:message.mode,callbackCalled:false};try{const callback=(streamId,options)=>{result.callbackCalled=true;result.streamId=streamId;result.options=options;result.error=chrome.runtime.lastError?.message;sendResponse(result)};const id=message.target?chrome.desktopCapture.chooseDesktopMedia(['screen','window','tab'],message.target,callback):chrome.desktopCapture.chooseDesktopMedia(['screen','window','tab'],callback);result.id=id;result.idType=typeof id;result.idInteger=Number.isInteger(id);setTimeout(()=>{chrome.desktopCapture.cancelChooseDesktopMedia(id);setTimeout(()=>sendResponse(result),200)},100)}catch(error){result.thrown=String(error);sendResponse(result)}return true});`,
);
writeFileSync(
  join(fixture, "probe.html"),
  '<button id="probe">Actual gesture</button><script src="probe.js"></script>',
);
writeFileSync(
  join(fixture, "probe.js"),
  `globalThis.results=[];globalThis.request=async mode=>{const value=await chrome.runtime.sendMessage({mode,target:mode==='missing-target'?undefined:(await chrome.tabs.query({url:'${url}*'}))[0]});results.push(value);return value};document.querySelector('#probe').addEventListener('click',()=>request('actual-click'));`,
);
const chrome = join(
  root,
  "_dist/chrome-reference/extracted/opt/google/chrome/chrome",
);
const pipe = new ChromePipe(chrome, output);
const report = {
  passed: false,
  output,
  reference: baseline,
  scope:
    "Self-authored Chrome155 Worker chooser initiation/cancellation. No source selection or capture stream claim.",
};
try {
  report.version = await pipe.call("Browser.getVersion");
  report.binarySha256 = createHash("sha256").update(readFileSync(chrome))
    .digest("hex");
  if (
    report.version.product !== `Chrome/${baseline.browserVersion}` ||
    report.binarySha256 !== baseline.binary.sha256
  ) throw new Error("Chrome differs from frozen reference");
  await pipe.call("Target.createTarget", { url });
  const { id } = await pipe.call("Extensions.loadUnpacked", { path: fixture });
  const { targetId } = await pipe.call("Target.createTarget", {
    url: `chrome-extension://${id}/probe.html`,
  });
  const { sessionId } = await pipe.call("Target.attachToTarget", {
    targetId,
    flatten: true,
  });
  await new Promise((r) => setTimeout(r, 400));
  report.missingTarget = (await pipe.call("Runtime.evaluate", {
    expression: "request('missing-target')",
    awaitPromise: true,
    returnByValue: true,
  }, sessionId)).result;
  report.noGesture = (await pipe.call("Runtime.evaluate", {
    expression: "request('no-gesture')",
    awaitPromise: true,
    returnByValue: true,
  }, sessionId)).result;
  await pipe.call("Input.dispatchMouseEvent", {
    type: "mousePressed",
    x: 40,
    y: 20,
    button: "left",
    clickCount: 1,
  }, sessionId);
  await pipe.call("Input.dispatchMouseEvent", {
    type: "mouseReleased",
    x: 40,
    y: 20,
    button: "left",
    clickCount: 1,
  }, sessionId);
  await new Promise((r) => setTimeout(r, 1200));
  report.all = (await pipe.call("Runtime.evaluate", {
    expression: "results",
    returnByValue: true,
  }, sessionId)).result;
  const [missing, noGesture, click] = report.all.value;
  report.passed = missing.idInteger && missing.callbackCalled &&
    missing.error ===
      "A target tab is required when called from a service worker context." &&
    [noGesture, click].every((result) =>
      result.idInteger && !result.callbackCalled && !result.error &&
      !result.thrown
    );
} catch (error) {
  report.error = String(error);
} finally {
  await pipe.close();
  server.close();
  writeFileSync(join(output, "report.json"), JSON.stringify(report, null, 2));
  console.log(JSON.stringify(report, null, 2));
}

if (!report.passed) process.exitCode = 1;
