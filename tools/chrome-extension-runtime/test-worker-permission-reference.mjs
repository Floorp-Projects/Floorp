#!/usr/bin/env node
// SPDX-License-Identifier: MPL-2.0
// Genuine optional-permission prompt; never writes Chrome's permission state.
import { ChromePipe } from "./chrome-cdp-pipe.mjs";
import { createHash } from "node:crypto";
import { mkdirSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { createServer } from "node:http";
import { execFileSync } from "node:child_process";
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
) throw Error("Frozen Chrome binary mismatch");
const parent = join(root, "_dist/chrome-runtime-tests");
mkdirSync(parent, { recursive: true });
const output = mkdtempSync(join(parent, "worker-permission-reference-"));
const server = createServer((_request, response) =>
  response.end(
    "<!doctype html><title>Native worker permission reference target</title>Controlled test page",
  )
);
await new Promise((resolve) =>
  server.listen(0, "127.0.0.1", () => resolve(undefined))
);
const browser = new ChromePipe(chrome, output, [], { headed: true });
/** @type {import("./offscreen-test-types.ts").WorkerPermissionReport} */
const report = { reference, output, passed: false };
/** @param {number} milliseconds */
const pause = (milliseconds) =>
  new Promise((resolve) => setTimeout(resolve, milliseconds));
/** @template T @param {() => T | Promise<T>} read @param {string} label @param {number} [duration]
 * @returns {Promise<Exclude<T, false | 0 | "" | null | undefined>>} */
async function until(read, label, duration = 15000) {
  const end = Date.now() + duration;
  while (Date.now() < end) {
    const result = await read();
    if (result) {
      return /** @type {Exclude<T, false | 0 | "" | null | undefined>} */ (result);
    }
    await pause(50);
  }
  throw Error("Timed out waiting for " + label);
}
/** @template T @param {string} session @param {string} expression @returns {Promise<T>} */
const evaluate = async (session, expression) => {
  const result = await browser.call("Runtime.evaluate", {
    expression,
    returnByValue: true,
    awaitPromise: true,
  }, session);
  if (result.exceptionDetails) {
    throw Error(JSON.stringify(result.exceptionDetails));
  }
  return /** @type {T} */ (result.result.value);
};
/** @type {string | undefined} */
let extensionId;
try {
  report.version = await browser.call("Browser.getVersion");
  if (report.version.product !== `Chrome/${reference.browserVersion}`) {
    throw Error("Frozen Chrome version mismatch");
  }
  const directory = join(output, "extension");
  mkdirSync(directory);
  writeFileSync(
    join(directory, "manifest.json"),
    JSON.stringify({
      manifest_version: 3,
      name: "Native dynamic Worker permission reference",
      version: "1.0",
      optional_permissions: ["power", "tabCapture"],
      background: { service_worker: "worker.js" },
      action: {},
    }),
  );
  writeFileSync(
    join(directory, "worker.js"),
    `globalThis.generation=crypto.randomUUID();globalThis.initialChrome=chrome;globalThis.phase='ready';globalThis.permissionEvents=[];chrome.permissions.onAdded.addListener(permissions=>permissionEvents.push({event:'added',permissions}));chrome.permissions.onRemoved.addListener(permissions=>permissionEvents.push({event:'removed',permissions}));chrome.action.onClicked.addListener(async()=>{globalThis.phase='prompting';try{globalThis.granted=await chrome.permissions.request({permissions:['power','tabCapture']});if(granted){globalThis.grantedPower=chrome.power;globalThis.grantedTabCapture=chrome.tabCapture;}globalThis.phase=granted?'granted':'denied';}catch(error){globalThis.phase='error';globalThis.failure=String(error);}});`,
  );
  ({ id: extensionId } = await browser.call("Extensions.loadUnpacked", {
    path: directory,
  }));
  const worker = await until(
    async () =>
      (await browser.call("Target.getTargets")).targetInfos.find((target) =>
        target.type === "service_worker" &&
        target.url.startsWith(`chrome-extension://${extensionId}/`)
      ),
    "actual extension worker",
  );
  const { sessionId } = await browser.call("Target.attachToTarget", {
    targetId: worker.targetId,
    flatten: true,
  });
  await until(
    () =>
      evaluate(sessionId, "typeof generation!=='undefined'&&phase==='ready'"),
    "native Worker bindings and script initialization",
  );
  /** @returns {Promise<import("./offscreen-test-types.ts").WorkerPermissionState>} */
  const state = () =>
    evaluate(
      sessionId,
      `(async()=>{const shape=name=>{const descriptor=Object.getOwnPropertyDescriptor(chrome,name),value=chrome[name];return {inChrome:name in chrome,keys:value?Object.keys(value).sort():[],descriptor:descriptor?{configurable:descriptor.configurable,enumerable:descriptor.enumerable,writable:descriptor.writable,valueType:typeof descriptor.value,get:typeof descriptor.get,set:typeof descriptor.set}:null};};return {generation,sameChrome:chrome===initialChrome,phase,power:typeof chrome.power,tabCapture:typeof chrome.tabCapture,level:chrome.power?.Level,contains:await chrome.permissions.contains({permissions:['power','tabCapture']}),events:permissionEvents,chromeKeys:Object.keys(chrome).sort(),powerShape:shape('power'),tabCaptureShape:shape('tabCapture'),samePower:typeof grantedPower==='undefined'?null:chrome.power===grantedPower,sameTabCapture:typeof grantedTabCapture==='undefined'?null:chrome.tabCapture===grantedTabCapture};})()`,
    );
  report.workerTargetId = worker.targetId;
  report.initial = await state();
  if (
    report.initial.power !== "undefined" ||
    report.initial.tabCapture !== "undefined" || report.initial.contains
  ) throw Error("Optional permissions were not initially absent");
  /** @type {import("./offscreen-test-types.ts").NativeRootContract} */
  const rootContract = await evaluate(
    sessionId,
    `(()=>{
    const originalPrototype=Object.getPrototypeOf(chrome),result={
      ordinaryPrototype:originalPrototype===Object.prototype,
      tag:Object.prototype.toString.call(chrome),
      extensible:Object.isExtensible(chrome),
      unknown:typeof chrome.floorpAuthoredUnknown,
      unknownIn:'floorpAuthoredUnknown' in chrome
    };
    Object.defineProperty(chrome,'floorpAuthoredAccessor',{get(){return 17},enumerable:false,configurable:true});
    result.accessor={value:chrome.floorpAuthoredAccessor,enumerable:Object.getOwnPropertyDescriptor(chrome,'floorpAuthoredAccessor').enumerable};
    result.accessorDeleted=delete chrome.floorpAuthoredAccessor;
    Object.defineProperty(chrome,'floorpAuthoredReadOnly',{value:23,writable:false,enumerable:false,configurable:true});
    result.readOnly={set:Reflect.set(chrome,'floorpAuthoredReadOnly',29),value:chrome.floorpAuthoredReadOnly,writable:Object.getOwnPropertyDescriptor(chrome,'floorpAuthoredReadOnly').writable};
    delete chrome.floorpAuthoredReadOnly;
    const symbol=Symbol('authored');chrome[symbol]='native symbol';result.symbol=chrome[symbol];delete chrome[symbol];
    const testPrototype=Object.create(originalPrototype,{floorpAuthoredInherited:{value:31}});
    Object.setPrototypeOf(chrome,testPrototype);result.inherited=chrome.floorpAuthoredInherited;
    Object.setPrototypeOf(chrome,originalPrototype);result.prototypeRestored=Object.getPrototypeOf(chrome)===originalPrototype;
    result.deletedMissingPower=delete chrome.power;result.deletedMissingTabCapture=delete chrome.tabCapture;
    return result;
  })()`,
  );
  report.rootContract = rootContract;
  if (
    !rootContract.ordinaryPrototype || rootContract.tag !== "[object Object]" ||
    !rootContract.extensible || rootContract.unknown !== "undefined" ||
    rootContract.unknownIn ||
    rootContract.accessor.value !== 17 || rootContract.accessor.enumerable ||
    !rootContract.accessorDeleted ||
    rootContract.readOnly.set || rootContract.readOnly.value !== 23 ||
    rootContract.readOnly.writable ||
    rootContract.symbol !== "native symbol" || rootContract.inherited !== 31 ||
    !rootContract.prototypeRestored ||
    !rootContract.deletedMissingPower || !rootContract.deletedMissingTabCapture
  ) {
    throw Error(
      "Native Chrome root object property semantics differ from the frozen reference",
    );
  }
  const address = server.address();
  if (!address || typeof address === "string") {
    throw Error("Loopback server has no TCP address");
  }
  const url = `http://127.0.0.1:${address.port}/`;
  const { targetId } = await browser.call("Target.createTarget", { url });
  await browser.call("Target.activateTarget", { targetId });
  const tab = await until(
    async () =>
      (await browser.call("Target.getTargets", { filter: [{ type: "tab" }] }))
        .targetInfos.find((target) => target.url === url),
    "native browser tab",
  );
  await browser.call("Extensions.triggerAction", {
    id: extensionId,
    targetId: tab.targetId,
  });
  await until(
    async () =>
      ["prompting", "granted", "denied", "error"].includes(
        await evaluate(sessionId, "phase"),
      ),
    "actual action user gesture reaches permissions.request",
  );
  await pause(600);
  report.phaseBeforeConsent = await evaluate(sessionId, "phase");
  execFileSync("import", [
    "-display",
    process.env.DISPLAY || ":101",
    "-window",
    "root",
    join(output, "permission-prompt.png"),
  ]);
  console.log(
    "Native permission prompt: " + join(output, "permission-prompt.png"),
  );
  if (report.phaseBeforeConsent !== "prompting") {
    throw Error("No genuine pending permission prompt was observed");
  }
  // This test owns the only browser on its isolated display. Send a real native
  // dialog keyboard selection, after verifying the focused X11 window's PID.
  const focused = execFileSync("xdotool", ["getwindowfocus"], {
    encoding: "utf8",
  }).trim();
  const pid = Number(
    execFileSync("xdotool", ["getwindowpid", focused], { encoding: "utf8" })
      .trim(),
  );
  report.nativeConsent = {
    focusedWindow: focused,
    pid,
    title: execFileSync("xdotool", ["getwindowname", focused], {
      encoding: "utf8",
    }).trim(),
  };
  if (pid !== browser.process.pid) {
    throw Error(
      "The permission dialog does not belong to the isolated Chrome process",
    );
  }
  execFileSync("xdotool", ["key", "--window", focused, "Tab", "Return"]);
  await until(
    async () =>
      ["granted", "denied", "error"].includes(
        await evaluate(sessionId, "phase"),
      ),
    "real native permission decision",
    60000,
  );
  report.afterDecision = await state();
  if (report.afterDecision.phase !== "granted") {
    throw Error("Actual optional permission request was not granted");
  }
  report.granted = await until(async () => {
    const current = await state();
    return current.power === "object" && current.tabCapture === "object" &&
      current.contains && current;
  }, "both native namespaces appear in the same Worker");
  /** @type {NonNullable<import("./offscreen-test-types.ts").WorkerPermissionReport["realAPIs"]>} */
  const realAPIs = await evaluate(
    sessionId,
    `(async()=>{const system=typeof chrome.power.requestKeepAwake('system');const release=typeof chrome.power.releaseKeepAwake();return {system,release,capturedTabs:await chrome.tabCapture.getCapturedTabs(),streamId:typeof chrome.tabCapture.getMediaStreamId,level:chrome.power.Level};})()`,
  );
  report.realAPIs = realAPIs;
  if (
    realAPIs.system !== "undefined" ||
    realAPIs.release !== "undefined" ||
    !Array.isArray(realAPIs.capturedTabs) ||
    realAPIs.streamId !== "function"
  ) throw Error("Granted namespaces did not execute genuine native APIs");
  report.removed = await evaluate(
    sessionId,
    "chrome.permissions.remove({permissions:['power','tabCapture']})",
  );
  report.revoked = await until(async () => {
    const current = await state();
    report.afterRemovalObserved = current;
    return !current.contains && current.events.some((event) =>
      event.event === "removed"
    ) && current;
  }, "actual permission removal and native onRemoved event");
  report.revokedCalls = await evaluate(
    sessionId,
    `(async()=>{const result={};for(const name of ['requestKeepAwake','releaseKeepAwake']){try{const value=name==='requestKeepAwake'?chrome.power[name]('system'):chrome.power[name]();result[name]={returns:typeof value};}catch(error){result[name]={name:error.name,message:error.message};}}try{result.getCapturedTabs={resolved:await chrome.tabCapture.getCapturedTabs()};}catch(error){result.getCapturedTabs={name:error.name,message:error.message};}return result;})()`,
  );
  report.finalWorkerTargetId = (await browser.call("Target.getTargets"))
    .targetInfos.find((target) =>
      target.type === "service_worker" && target.url === worker.url
    )?.targetId;
  report.sameWorker = report.initial.generation === report.granted.generation &&
    report.initial.generation === report.revoked.generation &&
    report.workerTargetId === report.finalWorkerTargetId;
  if (
    !report.sameWorker || !report.granted.sameChrome ||
    !report.revoked.sameChrome
  ) {
    throw Error(
      "The native Worker or chrome object was replaced during permission updates",
    );
  }
  if (
    !report.revoked.samePower || !report.revoked.sameTabCapture ||
    report.revoked.power !== "object" ||
    report.revoked.tabCapture !== "object" ||
    !report.revoked.powerShape.inChrome ||
    !report.revoked.tabCaptureShape.inChrome
  ) {
    throw Error(
      "Revoked namespaces did not retain the Chrome reference's existing objects",
    );
  }
  if (
    !report.revokedCalls ||
    Object.values(report.revokedCalls).some((call) =>
      call.name !== "Error" ||
      !call.message?.includes("is not available in this context")
    )
  ) {
    throw Error("Revoked native API calls were not synchronously denied");
  }
  report.passed = true;
} catch (error) {
  report.error = String(error);
  throw error;
} finally {
  if (extensionId) {
    try {
      await browser.call("Extensions.uninstall", { id: extensionId });
    } catch { /* Browser shutdown also removes the private test fixture. */ }
  }
  await browser.close();
  await new Promise((resolve, reject) =>
    server.close((error) => error ? reject(error) : resolve(undefined))
  );
  writeFileSync(join(output, "report.json"), JSON.stringify(report, null, 2));
  console.log(
    `${report.passed ? "PASS" : "FAIL"} ${join(output, "report.json")}`,
  );
}
