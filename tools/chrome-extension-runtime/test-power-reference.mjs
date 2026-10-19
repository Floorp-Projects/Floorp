#!/usr/bin/env node
// SPDX-License-Identifier: MPL-2.0
import { ChromePipe } from "./chrome-cdp-pipe.mjs";
import { createHash } from "node:crypto";
import { mkdirSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
const root = resolve(dirname(fileURLToPath(import.meta.url)), "../..");
/** @type {import("./offscreen-test-types.ts").CaptureReference} */
const reference = JSON.parse(
  readFileSync(
    join(root, "tools/chrome-extension-runtime/chrome-reference.json"),
    "utf8",
  ),
);
const browserPath = join(
  root,
  "_dist/chrome-reference/extracted",
  reference.binary.relativePath,
);
if (
  createHash("sha256").update(readFileSync(browserPath)).digest("hex") !==
    reference.binary.sha256
) throw Error("Chrome reference hash mismatch");
const parent = join(root, "_dist/chrome-runtime-tests");
mkdirSync(parent, { recursive: true });
const output = mkdtempSync(join(parent, "power-reference-"));
const browser = new ChromePipe(browserPath, output);
/** @type {import("./offscreen-test-types.ts").PowerReport} */
const report = { reference, output, passed: false };
try {
  report.version = await browser.call("Browser.getVersion");
  if (report.version.product !== `Chrome/${reference.browserVersion}`) {
    throw Error("Chrome version mismatch");
  }
  const directory = join(output, "extension");
  mkdirSync(directory);
  writeFileSync(
    join(directory, "manifest.json"),
    JSON.stringify({
      manifest_version: 3,
      name: "Power return-contract reference",
      version: "1.0",
      permissions: ["power"],
      background: { service_worker: "worker.js" },
    }),
  );
  writeFileSync(
    join(directory, "worker.js"),
    "chrome.runtime.onMessage.addListener(()=>{});",
  );
  writeFileSync(join(directory, "page.html"), "<title>Power reference</title>");
  const { id } = await browser.call("Extensions.loadUnpacked", {
    path: directory,
  });
  const { targetId } = await browser.call("Target.createTarget", {
    url: `chrome-extension://${id}/page.html`,
  });
  const { sessionId } = await browser.call("Target.attachToTarget", {
    targetId,
    flatten: true,
  });
  const deadline = Date.now() + 20000;
  while (true) {
    const state = await browser.call("Runtime.evaluate", {
      expression:
        "({url:location.href,ready:document.readyState,power:typeof chrome.power,runtime:chrome.runtime?.id})",
      returnByValue: true,
    }, sessionId);
    report.initial =
      /** @type {NonNullable<import("./offscreen-test-types.ts").PowerReport["initial"]>} */ (state
        .result.value);
    if (report.initial.runtime === id && report.initial.ready === "complete") {
      break;
    }
    if (Date.now() > deadline) {
      throw Error(
        "Actual extension power page did not load: " +
          JSON.stringify(report.initial),
      );
    }
    await new Promise((resolve) => setTimeout(resolve, 50));
  }
  const result = await browser.call("Runtime.evaluate", {
    expression:
      `(()=>{const api=chrome.power,result={keys:Object.keys(api).sort()};for(const level of ['system','display','display','system'])result[level]=typeof api.requestKeepAwake(level);result.release=typeof api.releaseKeepAwake();result.releaseAgain=typeof api.releaseKeepAwake();for(const [name,args]of [['requestKeepAwake',['invalid']],['requestKeepAwake',[]],['releaseKeepAwake',['extra']]]){try{api[name](...args);result[name+args.join()]=false;}catch(error){result[name+args.join()]={name:error.name,message:error.message};}}return result;})()`,
    returnByValue: true,
  }, sessionId);
  if (result.exceptionDetails) {
    throw Error(JSON.stringify(result.exceptionDetails));
  }
  report.contract =
    /** @type {import("./offscreen-test-types.ts").PowerContract} */ (result
      .result.value);
  if (
    report.contract.system !== "undefined" ||
    report.contract.display !== "undefined" ||
    report.contract.release !== "undefined" ||
    report.contract.releaseAgain !== "undefined" ||
    !report.contract.requestKeepAwakeinvalid ||
    !report.contract.requestKeepAwake || !report.contract.releaseKeepAwakeextra
  ) throw Error("Unexpected native Chrome power contract");
  /** @param {string} extensionId @returns {Promise<import("./offscreen-test-types.ts").PowerVisibility>} */
  const workerVisibility = async (extensionId) => {
    const end = Date.now() + 15000;
    let target;
    while (!target && Date.now() < end) {
      target = (await browser.call("Target.getTargets")).targetInfos.find(
        (info) =>
          info.type === "service_worker" &&
          info.url.startsWith(`chrome-extension://${extensionId}/`),
      );
      if (!target) await new Promise((resolve) => setTimeout(resolve, 50));
    }
    if (!target) throw Error("Actual Chrome extension worker was not started");
    const { sessionId: workerSession } = await browser.call(
      "Target.attachToTarget",
      { targetId: target.targetId, flatten: true },
    );
    while (Date.now() < end) {
      const visibility = await browser.call("Runtime.evaluate", {
        expression:
          "typeof chrome==='undefined'?null:({power:typeof chrome.power,tabCapture:typeof chrome.tabCapture,action:typeof chrome.action,commands:typeof chrome.commands,omnibox:typeof chrome.omnibox,actionIn:'action' in chrome,commandsIn:'commands' in chrome,omniboxIn:'omnibox' in chrome})",
        returnByValue: true,
      }, workerSession);
      if (visibility.exceptionDetails) {
        throw Error(JSON.stringify(visibility.exceptionDetails));
      }
      if (visibility.result.value) {
        return /** @type {import("./offscreen-test-types.ts").PowerVisibility} */ (visibility
          .result.value);
      }
      await new Promise((resolve) => setTimeout(resolve, 50));
    }
    throw Error("Native Chrome worker bindings did not initialize");
  };
  report.workerWithPower = await workerVisibility(id);
  if (
    report.workerWithPower.power !== "object" ||
    report.workerWithPower.tabCapture !== "undefined"
  ) throw Error("Unexpected declared-permission Worker namespaces");
  await browser.call("Extensions.uninstall", { id });
  report.workersWithoutPermission = [];
  for (const optional of [false, true]) {
    const fixture = join(
      output,
      optional ? "ungranted-optional" : "no-permission",
    );
    mkdirSync(fixture);
    writeFileSync(
      join(fixture, "manifest.json"),
      JSON.stringify({
        manifest_version: 3,
        name: "Permission namespace reference",
        version: "1.0",
        permissions: [],
        optional_permissions: optional ? ["power", "tabCapture"] : [],
        background: { service_worker: "worker.js" },
      }),
    );
    writeFileSync(
      join(fixture, "worker.js"),
      "chrome.runtime.onMessage.addListener(()=>{});",
    );
    const { id: fixtureId } = await browser.call("Extensions.loadUnpacked", {
      path: fixture,
    });
    const visibility = await workerVisibility(fixtureId);
    report.workersWithoutPermission.push({ optional, ...visibility });
    if (
      visibility.power !== "undefined" || visibility.tabCapture !== "undefined"
    ) throw Error("Unpermitted native Chrome Worker namespace was exposed");
    await browser.call("Extensions.uninstall", { id: fixtureId });
  }
  report.passed = true;
} finally {
  await browser.close();
  writeFileSync(join(output, "report.json"), JSON.stringify(report, null, 2));
  console.log(
    `${report.passed ? "PASS" : "FAIL"} ${join(output, "report.json")}`,
  );
}
