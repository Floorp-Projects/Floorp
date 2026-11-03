#!/usr/bin/env node
// SPDX-License-Identifier: MPL-2.0
// Inspect genuine Chrome SDK descriptors without invoking privileged operations.
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
const chrome = join(
  root,
  "_dist/chrome-reference/extracted",
  reference.binary.relativePath,
);
if (
  createHash("sha256").update(readFileSync(chrome)).digest("hex") !==
    reference.binary.sha256
) throw Error("Frozen Chrome reference differs");
const parent = join(root, "_dist/chrome-runtime-tests");
mkdirSync(parent, { recursive: true });
const output = mkdtempSync(join(parent, "worker-feature-reference-"));
const browser = new ChromePipe(chrome, output);
/** @type {import("./worker-feature-reference-types.ts").WorkerFeatureReport} */
const report = {
  reference,
  output,
  passed: false,
  scope:
    "Self-authored Chrome155 classic/module method descriptors, own empty-rule dry-run results and nonexistent download ID errors. Detached native power/read-only capture calls; no capture streams, files opened, native hosts or external requests.",
  cases: [],
};
/** @param {number} ms */
const pause = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
/** @template T @param {()=>T|Promise<T>} read @param {string} label @returns {Promise<Exclude<T,false|null|undefined>>} */
async function until(read, label) {
  const end = Date.now() + 15000;
  while (Date.now() < end) {
    const value = await read();
    if (value) return /** @type {Exclude<T,false|null|undefined>} */ (value);
    await pause(50);
  }
  throw Error("Timeout: " + label);
}
/** @type {string|undefined} */
let id;
try {
  report.version = await browser.call("Browser.getVersion");
  if (report.version.product !== `Chrome/${reference.browserVersion}`) {
    throw Error("Frozen Chrome version differs");
  }
  /** @type {import("./worker-member-reference-types.ts").WorkerKind[]} */
  const kinds = ["classic", "module"];
  const variants = [
    { name: "minimal", extra: [], hosts: [] },
    { name: "activeTab", extra: ["activeTab"], hosts: [] },
    { name: "feedback", extra: ["declarativeNetRequestFeedback"], hosts: [] },
    { name: "allUrls", extra: [], hosts: ["<all_urls>"] },
    { name: "downloadsOpen", extra: ["downloads.open"], hosts: [] },
    { name: "detached", extra: ["power", "tabCapture"], hosts: [] },
  ];
  for (const kind of kinds) {
    for (const variant of variants) {
      /** @type {import("./worker-feature-reference-types.ts").WorkerFeatureCase} */
      const item = {
        kind,
        name: variant.name,
        permissions: ["declarativeNetRequest", "downloads", ...variant.extra],
        hostPermissions: variant.hosts,
      };
      report.cases.push(item);
      const fixture = join(output, kind + "-" + variant.name);
      mkdirSync(fixture);
      writeFileSync(
        join(fixture, "manifest.json"),
        JSON.stringify({
          manifest_version: 3,
          name: "Native " + kind + " " + variant.name + " feature reference",
          version: "1.0",
          permissions: item.permissions,
          host_permissions: item.hostPermissions,
          background: {
            service_worker: "worker.js",
            ...(kind === "module" ? { type: "module" } : {}),
          },
        }),
      );
      writeFileSync(
        join(fixture, "worker.js"),
        "globalThis.ready=true;chrome.runtime.onMessage.addListener(()=>{});",
      );
      ({ id } = await browser.call("Extensions.loadUnpacked", {
        path: fixture,
      }));
      item.extensionId = id;
      const extensionId = id;
      const worker = await until(
        async () =>
          (await browser.call("Target.getTargets")).targetInfos.find((target) =>
            target.type === "service_worker" &&
            target.url.startsWith(`chrome-extension://${extensionId}/`)
          ),
        "actual native Worker",
      );
      item.workerTargetId = worker.targetId;
      const { sessionId } = await browser.call("Target.attachToTarget", {
        targetId: worker.targetId,
        flatten: true,
      });
      await until(async () =>
        (await browser.call("Runtime.evaluate", {
          expression: "globalThis.ready===true",
          returnByValue: true,
        }, sessionId)).result.value, "native script initialization");
      const result = await browser.call("Runtime.evaluate", {
        expression:
          `(()=>{const targets=[['tabs.captureVisibleTab',chrome.tabs,'captureVisibleTab'],['DNR.testMatchOutcome',chrome.declarativeNetRequest,'testMatchOutcome'],['DNR.getMatchedRules',chrome.declarativeNetRequest,'getMatchedRules'],['DNR.onRuleMatchedDebug',chrome.declarativeNetRequest,'onRuleMatchedDebug'],['downloads.open',chrome.downloads,'open']];return Object.fromEntries(targets.map(([path,namespace,name])=>{const descriptor=Object.getOwnPropertyDescriptor(namespace,name);return[path,{type:typeof namespace[name],inNamespace:name in namespace,descriptor:descriptor?{configurable:descriptor.configurable,enumerable:descriptor.enumerable,writable:descriptor.writable,valueType:typeof descriptor.value,get:typeof descriptor.get,set:typeof descriptor.set}:null}]}));})()`,
        returnByValue: true,
      }, sessionId);
      if (result.exceptionDetails) {
        throw Error(JSON.stringify(result.exceptionDetails));
      }
      item.members =
        /** @type {Record<string,import("./worker-member-reference-types.ts").NativeMemberShape>} */ (result
          .result.value);
      const safeCalls = await browser.call("Runtime.evaluate", {
        expression:
          `(async()=>{const result={};for(const [name,call]of [['DNR.testMatchOutcome',()=>chrome.declarativeNetRequest.testMatchOutcome({url:'https://example.invalid/',type:'main_frame'})],['DNR.getMatchedRules',()=>chrome.declarativeNetRequest.getMatchedRules({})],['downloads.open',()=>chrome.downloads.open(-1)]]){try{result[name]={resolved:await call()};}catch(error){result[name]={name:error.name,message:error.message};}}return result;})()`,
        returnByValue: true,
        awaitPromise: true,
      }, sessionId);
      if (safeCalls.exceptionDetails) {
        throw Error(JSON.stringify(safeCalls.exceptionDetails));
      }
      item.safeCalls =
        /** @type {NonNullable<import("./worker-feature-reference-types.ts").WorkerFeatureCase["safeCalls"]>} */ (safeCalls
          .result.value);
      if (variant.name === "detached") {
        const detached = await browser.call("Runtime.evaluate", {
          expression:
            `(async()=>{const getSelf=chrome.management.getSelf,getURL=chrome.runtime.getURL,request=chrome.power.requestKeepAwake,release=chrome.power.releaseKeepAwake,captured=chrome.tabCapture.getCapturedTabs;return{selfId:(await getSelf()).id,url:getURL('authored-resource.txt'),power:typeof request('system'),release:typeof release(),capturedTabs:await captured(),foreignThis:{selfId:(await getSelf.call({})).id,url:getURL.call({},'authored-resource.txt'),power:typeof request.call({},'system'),release:typeof release.call({}),capturedTabs:await captured.call({})}};})()`,
          returnByValue: true,
          awaitPromise: true,
        }, sessionId);
        if (detached.exceptionDetails) {
          throw Error(JSON.stringify(detached.exceptionDetails));
        }
        item.detachedCalls =
          /** @type {NonNullable<import("./worker-feature-reference-types.ts").WorkerFeatureCase["detachedCalls"]>} */ (detached
            .result.value);
        const calls = item.detachedCalls;
        if (
          calls.selfId !== id || calls.foreignThis.selfId !== id ||
          calls.url !== `chrome-extension://${id}/authored-resource.txt` ||
          calls.foreignThis.url !== calls.url ||
          calls.power !== "undefined" || calls.release !== "undefined" ||
          calls.foreignThis.power !== "undefined" ||
          calls.foreignThis.release !== "undefined" ||
          !Array.isArray(calls.capturedTabs) ||
          !Array.isArray(calls.foreignThis.capturedTabs)
        ) {
          throw Error(
            "Actual detached/foreign-this Chrome native calls differ",
          );
        }
      }
      await browser.call("Extensions.uninstall", { id });
      id = undefined;
    }
  }
  report.passed = true;
} catch (error) {
  report.error = error instanceof Error
    ? error.stack || String(error)
    : String(error);
  throw error;
} finally {
  if (id) {
    try {
      await browser.call("Extensions.uninstall", { id });
    } catch { /* Private browser teardown removes this fixture. */ }
  }
  await browser.close();
  writeFileSync(join(output, "report.json"), JSON.stringify(report, null, 2));
  console.log(
    `${report.passed ? "PASS" : "FAIL"} ${join(output, "report.json")}`,
  );
}
