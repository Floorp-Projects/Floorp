#!/usr/bin/env node
// SPDX-License-Identifier: MPL-2.0
// Real Chrome permissions and native SDK feature detection. No native host calls.
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
const output = mkdtempSync(join(parent, "worker-member-reference-"));
const rootAccessOnly = process.argv.includes("--pre-read-root");
const server = createServer((_request, response) =>
  response.end(
    "<!doctype html><title>Native worker member permission reference</title>Authored local permission target",
  )
);
await new Promise((resolve) =>
  server.listen(0, "127.0.0.1", () => resolve(undefined))
);
const address = server.address();
if (!address || typeof address === "string") {
  throw Error("Reference server has no TCP address");
}
const url = `http://127.0.0.1:${address.port}/`;
const browser = new ChromePipe(chrome, output, [], { headed: true });
/** @type {import("./worker-member-reference-types.ts").NativeMemberReport} */
const report = {
  reference,
  output,
  passed: false,
  scope:
    "Self-authored Chrome155 classic/module Worker member visibility. No native hosts installed or invoked, and no foreign addon mutation." +
    (rootAccessOnly
      ? " Pre-grant root enumeration/descriptor observation in fresh Workers; no native qualification claim."
      : ""),
  cases: [],
};
/** @param {number} milliseconds */
const pause = (milliseconds) =>
  new Promise((resolve) => setTimeout(resolve, milliseconds));
/** @template T @param {() => T | Promise<T>} read @param {string} label @param {number} [timeout]
 * @returns {Promise<Exclude<T, false | 0 | "" | null | undefined>>} */
async function until(read, label, timeout = 15000) {
  const end = Date.now() + timeout;
  while (Date.now() < end) {
    const value = await read();
    if (value) {
      return /** @type {Exclude<T, false | 0 | "" | null | undefined>} */ (value);
    }
    await pause(50);
  }
  throw Error("Timed out waiting for " + label);
}
/** @template T @param {string} session @param {string} expression @returns {Promise<T>} */
async function evaluate(session, expression) {
  const response = await browser.call("Runtime.evaluate", {
    expression,
    returnByValue: true,
    awaitPromise: true,
  }, session);
  if (response.exceptionDetails) {
    throw Error(JSON.stringify(response.exceptionDetails));
  }
  return /** @type {T} */ (response.result.value);
}
const workerSource = `
globalThis.generation=crypto.randomUUID();globalThis.originalChrome=chrome;
globalThis.originalManagement=chrome.management;globalThis.originalRuntime=chrome.runtime;
globalThis.phase='ready';globalThis.permissionEvents=[];
chrome.permissions.onAdded.addListener(permissions=>permissionEvents.push({event:'added',permissions}));
chrome.permissions.onRemoved.addListener(permissions=>permissionEvents.push({event:'removed',permissions}));
chrome.action.onClicked.addListener(async()=>{phase='prompting';try{const granted=await chrome.permissions.request({permissions:['management','nativeMessaging']});if(granted){globalThis.grantedMembers={getSelf:chrome.management.getSelf,getAll:chrome.management.getAll,setEnabled:chrome.management.setEnabled,connectNative:chrome.runtime.connectNative,sendNativeMessage:chrome.runtime.sendNativeMessage};}phase=granted?'granted':'denied';}catch(error){phase='error';globalThis.failure=String(error);}});
`;
const stateExpression =
  `(async()=>{if(originalManagement===null)originalManagement=chrome.management;if(originalRuntime===null)originalRuntime=chrome.runtime;const shape=(namespace,name)=>{const value=namespace[name],descriptor=Object.getOwnPropertyDescriptor(namespace,name);return{type:typeof value,inNamespace:name in namespace,descriptor:descriptor?{configurable:descriptor.configurable,enumerable:descriptor.enumerable,writable:descriptor.writable,valueType:typeof descriptor.value,get:typeof descriptor.get,set:typeof descriptor.set}:null};};return{generation,phase,sameChrome:chrome===originalChrome,sameManagement:chrome.management===originalManagement,sameRuntime:chrome.runtime===originalRuntime,permissions:{management:await chrome.permissions.contains({permissions:['management']}),nativeMessaging:await chrome.permissions.contains({permissions:['nativeMessaging']})},events:permissionEvents,managementKeys:Object.keys(chrome.management).sort(),runtimeKeys:Object.keys(chrome.runtime).sort(),members:{getSelf:shape(chrome.management,'getSelf'),getAll:shape(chrome.management,'getAll'),setEnabled:shape(chrome.management,'setEnabled'),connectNative:shape(chrome.runtime,'connectNative'),sendNativeMessage:shape(chrome.runtime,'sendNativeMessage')},sameGrantedMembers:typeof grantedMembers==='undefined'?null:Object.fromEntries(Object.entries(grantedMembers).map(([name,value])=>[name,(name==='connectNative'||name==='sendNativeMessage'?chrome.runtime:chrome.management)[name]===value]))};})()`;
/** @type {string | undefined} */
let installedId;
try {
  report.version = await browser.call("Browser.getVersion");
  if (report.version.product !== `Chrome/${reference.browserVersion}`) {
    throw Error("Frozen Chrome version mismatch");
  }
  /** @type {import("./worker-member-reference-types.ts").WorkerKind[]} */
  const kinds = ["classic", "module"];
  for (const kind of kinds) {
    /** @type {import("./worker-member-reference-types.ts").NativeMemberCase["mode"][]} */
    const modes = rootAccessOnly
      ? [
        "optional-unread",
        "optional-unread-keys",
        "optional-unread-ownKeys",
        "optional-unread-descriptor",
      ]
      : ["undeclared", "optional", "optional-unread"];
    for (const mode of modes) {
      const optional = mode !== "undeclared";
      const unread = mode.startsWith("optional-unread");
      const directory = join(
        output,
        `${kind}-${mode}`,
      );
      mkdirSync(directory);
      writeFileSync(
        join(directory, "manifest.json"),
        JSON.stringify({
          manifest_version: 3,
          name: `Native ${kind} member permission reference`,
          version: "1.0",
          ...(optional
            ? { optional_permissions: ["management", "nativeMessaging"] }
            : {}),
          background: {
            service_worker: "worker.js",
            ...(kind === "module" ? { type: "module" } : {}),
          },
          action: {},
        }),
      );
      writeFileSync(
        join(directory, "worker.js"),
        unread
          ? workerSource.replace(
            "globalThis.originalManagement=chrome.management;globalThis.originalRuntime=chrome.runtime;",
            "globalThis.originalManagement=null;globalThis.originalRuntime=null;",
          )
          : workerSource,
      );
      writeFileSync(
        join(directory, "page.html"),
        "<!doctype html><title>Fresh member visibility after actual grant</title>",
      );
      ({ id: installedId } = await browser.call("Extensions.loadUnpacked", {
        path: directory,
      }));
      const id = installedId;
      const worker = await until(
        async () =>
          (await browser.call("Target.getTargets")).targetInfos.find((target) =>
            target.type === "service_worker" &&
            target.url.startsWith(`chrome-extension://${id}/`)
          ),
        "actual " + kind + " worker",
      );
      const { sessionId } = await browser.call("Target.attachToTarget", {
        targetId: worker.targetId,
        flatten: true,
      });
      await until(
        () =>
          evaluate(
            sessionId,
            "typeof generation!=='undefined'&&phase==='ready'",
          ),
        "Worker script initialization",
      );
      /** @type {import("./worker-member-reference-types.ts").NativeMemberCase} */
      const item = {
        kind,
        mode,
        extensionId: id,
        workerTargetId: worker.targetId,
      };
      report.cases.push(item);
      /** @returns {Promise<import("./worker-member-reference-types.ts").MemberPermissionState>} */
      const state = () => evaluate(sessionId, stateExpression);
      /** @type {{generation: string; sameChrome: boolean}} */
      const initialIdentity = await evaluate(
        sessionId,
        "({generation,sameChrome:chrome===originalChrome})",
      );
      item.preRequestIdentity = initialIdentity;
      if (rootAccessOnly) {
        const access = mode === "optional-unread"
          ? "none"
          : mode.slice("optional-unread-".length);
        item.preGrantRootAccess = await evaluate(
          sessionId,
          `(()=>{const kind=${
            JSON.stringify(access)
          };if(kind==='none')return{kind};if(kind==='keys')return{kind,names:Object.keys(chrome)};if(kind==='ownKeys')return{kind,names:Reflect.ownKeys(chrome).map(String)};const d=Object.getOwnPropertyDescriptor(chrome,'management');return{kind,descriptor:d?{configurable:d.configurable,enumerable:d.enumerable,writable:d.writable,valueType:typeof d.value,get:typeof d.get,set:typeof d.set}:null};})()`,
        );
      }
      if (!unread) {
        item.initial = await state();
        item.ownInfo = await evaluate(
          sessionId,
          "chrome.management.getSelf().then(({id,name})=>({id,name}))",
        );
        if (item.ownInfo?.id !== id) {
          throw Error("Public management.getSelf did not return the own addon");
        }
      }
      if (item.initial) {
        if (
          item.initial.members.getSelf.type !== "function" ||
          ["getAll", "setEnabled", "connectNative", "sendNativeMessage"].some(
            (name) =>
              item.initial?.members[name].type !== "undefined" ||
              item.initial.members[name].inNamespace ||
              item.initial.members[name].descriptor !== null,
          )
        ) {
          throw Error(
            "Undeclared/ungranted method visibility differs from native Chrome",
          );
        }
      }
      if (!optional) {
        await browser.call("Extensions.uninstall", { id });
        installedId = undefined;
        continue;
      }
      const { targetId } = await browser.call("Target.createTarget", { url });
      await browser.call("Target.activateTarget", { targetId });
      const tab = await until(
        async () =>
          (await browser.call("Target.getTargets", {
            filter: [{ type: "tab" }],
          })).targetInfos.find((target) => target.url === url),
        "native authored tab",
      );
      await browser.call("Extensions.triggerAction", {
        id,
        targetId: tab.targetId,
      });
      await until(
        async () =>
          ["prompting", "granted", "denied", "error"].includes(
            await evaluate(sessionId, "phase"),
          ),
        "trusted action permission request",
      );
      await pause(500);
      const phase = await evaluate(sessionId, "phase");
      if (phase === "prompting") {
        const screenshot = join(
          output,
          kind + "-" + mode + "-permission-prompt.png",
        );
        execFileSync("import", [
          "-display",
          process.env.DISPLAY || ":101",
          "-window",
          "root",
          screenshot,
        ]);
        const focusedWindow = execFileSync("xdotool", ["getwindowfocus"], {
          encoding: "utf8",
        }).trim();
        const pid = Number(
          execFileSync("xdotool", ["getwindowpid", focusedWindow], {
            encoding: "utf8",
          }).trim(),
        );
        const title = execFileSync(
          "xdotool",
          ["getwindowname", focusedWindow],
          { encoding: "utf8" },
        ).trim();
        if (pid !== browser.process.pid) {
          throw Error(
            "Native permission prompt is not owned by the private browser",
          );
        }
        item.decision = {
          kind: "native-prompt",
          focusedWindow,
          pid,
          title,
          screenshot,
        };
        execFileSync("xdotool", [
          "key",
          "--window",
          focusedWindow,
          "Tab",
          "Return",
        ]);
      } else if (phase === "granted") item.decision = { kind: "quiet-grant" };
      else throw Error("Actual optional request was " + phase);
      if (rootAccessOnly && item.decision.kind !== "native-prompt") {
        throw Error(
          "Pre-read root oracle requires an actual native Allow prompt",
        );
      }
      await until(
        async () =>
          ["granted", "denied", "error"].includes(
            await evaluate(sessionId, "phase"),
          ),
        "native permission decision",
        30000,
      );
      item.granted = await state();
      if (
        item.granted.phase !== "granted" ||
        !item.granted.permissions.management ||
        !item.granted.permissions.nativeMessaging
      ) throw Error("Actual requested permissions were not granted");
      await until(
        async () =>
          (await state()).events.some((event) => event.event === "added"),
        "native permissions.onAdded delivery",
      );
      await pause(1500);
      item.grantedLater = await state();
      if (rootAccessOnly) {
        const descriptorRead = mode === "optional-unread-descriptor";
        for (const [name, shape] of Object.entries(item.grantedLater.members)) {
          const managementPrivileged = name === "getAll" ||
            name === "setEnabled";
          const expected = descriptorRead && managementPrivileged
            ? "undefined"
            : "function";
          if (shape.type !== expected) {
            throw Error(
              "Native Chrome root pre-read method snapshot changed: " + mode +
                " " + name + " expected " + expected + " got " + shape.type,
            );
          }
        }
        const before = item.preGrantRootAccess;
        if (descriptorRead) {
          const descriptor = before?.descriptor;
          if (
            descriptor?.valueType !== "object" || !descriptor.writable ||
            !descriptor.enumerable || !descriptor.configurable ||
            descriptor.get !== "undefined" || descriptor.set !== "undefined"
          ) {
            throw Error(
              "Native Chrome root descriptor no longer materializes an own data namespace",
            );
          }
        } else if (
          before?.kind !== "none" &&
          (!before?.names?.includes("management") ||
            !before.names.includes("runtime"))
        ) {
          throw Error(
            "Native Chrome root enumeration omitted public namespaces",
          );
        }
      }
      if (
        (!rootAccessOnly || mode === "optional-unread") &&
        ["getAll", "setEnabled", "connectNative", "sendNativeMessage"].some(
          (name) =>
            item.grantedLater?.members[name].type !==
              (unread ? "function" : "undefined"),
        )
      ) {
        throw Error(
          "Native Chrome namespace initialization no longer determines method visibility",
        );
      }
      await evaluate(
        sessionId,
        "globalThis.grantedMembers={getSelf:chrome.management.getSelf,getAll:chrome.management.getAll,setEnabled:chrome.management.setEnabled,connectNative:chrome.runtime.connectNative,sendNativeMessage:chrome.runtime.sendNativeMessage};true",
      );
      const freshPage = await browser.call("Target.createTarget", {
        url: `chrome-extension://${id}/page.html`,
      });
      const freshSession = await browser.call("Target.attachToTarget", {
        targetId: freshPage.targetId,
        flatten: true,
      });
      await until(
        () =>
          evaluate(
            freshSession.sessionId,
            "typeof chrome!=='undefined'&&chrome.runtime?.id===" +
              JSON.stringify(id),
          ),
        "fresh extension page bindings",
      );
      item.freshGrantedPageMembers = await evaluate(
        freshSession.sessionId,
        "({getSelf:typeof chrome.management.getSelf,getAll:typeof chrome.management.getAll,setEnabled:typeof chrome.management.setEnabled,connectNative:typeof chrome.runtime.connectNative,sendNativeMessage:typeof chrome.runtime.sendNativeMessage})",
      );
      await browser.call("Target.closeTarget", {
        targetId: freshPage.targetId,
      });
      item.removed = await evaluate(
        sessionId,
        "chrome.permissions.remove({permissions:['management','nativeMessaging']})",
      );
      item.revoked = await until(async () => {
        const current = await state();
        return !current.permissions.management &&
          !current.permissions.nativeMessaging && current.events.some((event) =>
            event.event === "removed"
          ) && current;
      }, "actual permission revocation");
      item.revokedGetAll = await evaluate(
        sessionId,
        "(async()=>{try{return{resolved:await grantedMembers.getAll()};}catch(error){return{name:error.name,message:error.message};}})()",
      );
      item.finalWorkerTargetId = (await browser.call("Target.getTargets"))
        .targetInfos.find((target) =>
          target.type === "service_worker" && target.url === worker.url
        )?.targetId;
      item.sameWorker = item.workerTargetId === item.finalWorkerTargetId &&
        initialIdentity.generation === item.granted.generation &&
        initialIdentity.generation === item.revoked.generation;
      if (
        !item.sameWorker || !item.granted.sameChrome ||
        !item.revoked.sameChrome || !item.revoked.sameRuntime ||
        !item.revoked.sameManagement
      ) throw Error("Worker or captured root/namespace was replaced");
      if (
        !item.removed || !item.freshGrantedPageMembers ||
        Object.values(item.freshGrantedPageMembers).some((type) =>
          type !== "function"
        ) ||
        Object.values(item.revoked.sameGrantedMembers || {}).some((same) =>
          !same
        )
      ) {
        throw Error("Actual granted/revoked method identity contract differs");
      }
      if (
        item.grantedLater.members.getAll.type === "function" &&
        (item.revokedGetAll?.name !== "Error" ||
          !item.revokedGetAll.message?.includes(
            "is not available in this context",
          ))
      ) {
        throw Error(
          "Captured privileged management.getAll was not denied after native revocation",
        );
      }
      await browser.call("Extensions.uninstall", { id });
      installedId = undefined;
      await browser.call("Target.closeTarget", { targetId });
    }
  }
  report.passed = true;
} catch (error) {
  report.error = error instanceof Error
    ? error.stack || String(error)
    : String(error);
  throw error;
} finally {
  if (installedId) {
    try {
      await browser.call("Extensions.uninstall", { id: installedId });
    } catch { /* Browser teardown also removes this private fixture. */ }
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
