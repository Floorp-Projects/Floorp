#!/usr/bin/env node
// SPDX-License-Identifier: MPL-2.0
/** @import {Truthy, BinaryInput, CorpusLock, ChromeReference, CorpusReport, CorpusOperation, ContentSnapshot, EditorSnapshot, Point, ContextCreatedEvent} from "./assessment-test-types.ts" */
// Actual immutable package operations on controlled content; no API substitutes.
import { ChromePipe } from "./chrome-cdp-pipe.mjs";
import { createHash } from "node:crypto";
import { createServer } from "node:http";
import { mkdirSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
const tools = dirname(fileURLToPath(import.meta.url)),
  root = resolve(tools, "../..");
/** @type {CorpusLock} */
const corpus = JSON.parse(
  readFileSync(join(tools, "corpus-lock.json")).toString("utf8"),
);
/** @type {ChromeReference} */
const reference = JSON.parse(
  readFileSync(join(tools, "chrome-reference.json")).toString("utf8"),
);
const chrome = join(
  root,
  "_dist/chrome-reference/extracted/opt/google/chrome/chrome",
);
/** @param {BinaryInput} bytes */
const digest = (bytes) => createHash("sha256").update(bytes).digest("hex");
if (digest(readFileSync(chrome)) !== reference.binary.sha256) {
  throw Error("Reference binary hash mismatch");
}
/** @param {string} name @param {string} fallback */
const option = (name, fallback) => {
  const index = process.argv.indexOf(name);
  return index < 0 ? fallback : process.argv[index + 1];
};
const ids = option("--ids", "eimadpbcbfnmbkopoojfekhnkhdbieeh").split(",")
  .filter(Boolean);
const parent = join(root, "_dist/chrome-runtime-tests");
mkdirSync(parent, { recursive: true });
const output = mkdtempSync(join(parent, "corpus-content-reference-"));
/** @type {CorpusReport<ContentSnapshot>} */
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
/** @param {string} name */
const isUserscript = (name) =>
  name === "Violentmonkey" || name === "Tampermonkey";
const userscriptSelected = corpus.extensions.some((item) =>
  ids.includes(item.id) && isUserscript(item.name)
);
const authoredBase =
  "<!doctype html><meta charset=utf-8><title>Authored content transformation</title><style>html,body{background:rgb(255,255,255);color:rgb(0,0,0)}body{margin:30px;font:20px sans-serif}#probe{padding:30px;background:rgb(210,220,230)}</style><h1>Authored extension content operation</h1><p id=probe>Verify the real installed extension changes this authored page.</p><a href=/second>Navigate and verify persistence</a>";
const authored = authoredBase +
  (userscriptSelected
    ? '<iframe src="/frame" title="Authored matching frame"></iframe>'
    : "");
const server = createServer((req, res) => {
  res.writeHead(200, {
    "Content-Type": "text/html; charset=utf-8",
    "Cache-Control": "no-store",
  });
  res.end(req.url === "/frame" ? authoredBase : authored);
});
await new Promise((resolve) =>
  server.listen(0, "127.0.0.1", () => resolve(undefined))
);
const address = server.address();
if (!address || typeof address === "string") {
  throw Error("Native fixture server has no TCP address");
}
const fixture = `http://127.0.0.1:${address.port}/`;
report.fixture = { url: fixture, sha256: digest(authored) };
const headed = process.argv.includes("--headed");
report.displayMode = headed ? "headed" : "headless";
const browser = new ChromePipe(chrome, output, [], { headed });
browser.events = [];
/** @param {number} ms @returns {Promise<void>} */
const delay = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
/** @template T @param {string} sessionId @param {string} expression @returns {Promise<T>} */
async function evaluate(sessionId, expression) {
  const r = await browser.call("Runtime.evaluate", {
    expression,
    returnByValue: true,
    awaitPromise: true,
  }, sessionId);
  if (r.exceptionDetails) throw Error(JSON.stringify(r.exceptionDetails));
  return /** @type {T} */ (r.result.value);
}
/** @template T @param {()=>T|Promise<T>} read @param {string} label @returns {Promise<Truthy<T>>} */
async function until(read, label) {
  const end = Date.now() + 25000;
  while (Date.now() < end) {
    const result = await read();
    if (result) return /** @type {Truthy<T>} */ (result);
    await delay(100);
  }
  throw Error("Timed out waiting for " + label);
}
const styles =
  `({url:location.href,ready:document.readyState,body:getComputedStyle(document.body).backgroundColor,color:getComputedStyle(document.body).color,probe:getComputedStyle(document.querySelector('#probe')).backgroundColor,markers:[...document.querySelectorAll('style.darkreader')].map(e=>e.className),text:document.querySelector('#probe')?.textContent,userScript:document.documentElement.dataset.floorpUserScript,frameUserScript:document.querySelector('iframe')?.contentDocument?.documentElement.dataset.floorpUserScript})`;
/** @param {string} sessionId @returns {Promise<ContentSnapshot>} */
const readContentSnapshot = (sessionId) => evaluate(sessionId, styles);

try {
  report.version = await browser.call("Browser.getVersion");
  const { targetId } = await browser.call("Target.createTarget", {
    url: fixture,
  });
  const { sessionId } = await browser.call("Target.attachToTarget", {
    targetId,
    flatten: true,
  });
  await browser.call("Runtime.enable", {}, sessionId);
  await browser.call("Log.enable", {}, sessionId);
  await until(
    async () =>
      await evaluate(
        sessionId,
        "document.readyState==='complete'&&!!document.querySelector('#probe')",
      ),
    "authored content",
  );
  const baseline = await readContentSnapshot(sessionId);
  for (const id of ids) {
    const extension = corpus.extensions.find((item) => item.id === id);
    if (!extension) throw Error("Unfrozen extension " + id);
    /** @type {CorpusOperation<ContentSnapshot>} */
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
      for (
        const target of (await browser.call("Target.getTargets")).targetInfos
          .filter((target) =>
            target.type === "service_worker" &&
            target.url.startsWith(`chrome-extension://${id}/`)
          )
      ) {
        const worker = await browser.call("Target.attachToTarget", {
          targetId: target.targetId,
          flatten: true,
        });
        await browser.call("Runtime.enable", {}, worker.sessionId);
      }
      if (extension.name === "Stylus") {
        const { targetId: editorTarget } = await browser.call(
          "Target.createTarget",
          { url: `chrome-extension://${id}/edit.html?domain=127.0.0.1` },
        );
        const { sessionId: editorSession } = await browser.call(
          "Target.attachToTarget",
          { targetId: editorTarget, flatten: true },
        );
        item.editorBefore = await until(async () => {
          /** @type {EditorSnapshot|null} */
          const value = await evaluate(
            editorSession,
            `({ready:document.readyState,text:document.body?.innerText?.slice(0,10000),hasEditor:!!document.querySelector('.CodeMirror')?.CodeMirror,controls:[...document.querySelectorAll('button,input,textarea,select')].map(e=>({id:e.id,type:e.type,text:e.textContent,disabled:e.disabled,checked:e.checked,value:e.value}))})`,
          ).catch(() => null);
          return value?.ready === "complete" && value.hasEditor ? value : null;
        }, "actual Stylus editor");
        /** @param {string} text */
        const replaceFocused = async (text) => {
          for (
            const [type, key, code, windowsVirtualKeyCode, modifiers] of [
              ["rawKeyDown", "Control", "ControlLeft", 17, 2],
              ["rawKeyDown", "a", "KeyA", 65, 2],
              ["keyUp", "a", "KeyA", 65, 2],
              ["keyUp", "Control", "ControlLeft", 17, 0],
            ]
          ) {
            await browser.call("Input.dispatchKeyEvent", {
              type,
              key,
              code,
              windowsVirtualKeyCode,
              modifiers,
            }, editorSession);
          }
          await browser.call("Input.insertText", { text }, editorSession);
        };
        await evaluate(
          editorSession,
          `document.querySelector('#name').focus()`,
        );
        await replaceFocused("Floorp authored primary style");
        await evaluate(
          editorSession,
          `document.querySelector('.CodeMirror').CodeMirror.focus()`,
        );
        await replaceFocused(
          "#probe { background-color: rgb(12, 34, 56) !important; }",
        );
        item.editorAfterTyping = await evaluate(
          editorSession,
          `({name:document.querySelector('#name').value,code:document.querySelector('.CodeMirror').CodeMirror.getValue(),saveDisabled:document.querySelector('#save-button').disabled,enabled:document.querySelector('#enabled').checked,inputs:[...document.querySelectorAll('input')].map(e=>({id:e.id,value:e.value,checked:e.checked}))})`,
        );
        /** @type {Point} */
        const point = await evaluate(
          editorSession,
          `(()=>{const e=document.querySelector('#save-button');e.scrollIntoView({block:'center'});const r=e.getBoundingClientRect();return{x:r.x+r.width/2,y:r.y+r.height/2}})()`,
        );
        await browser.call("Input.dispatchMouseEvent", {
          type: "mousePressed",
          ...point,
          button: "left",
          buttons: 1,
          clickCount: 1,
        }, editorSession);
        await browser.call("Input.dispatchMouseEvent", {
          type: "mouseReleased",
          ...point,
          button: "left",
          buttons: 0,
          clickCount: 1,
        }, editorSession);
        item.editorSaved = await until(async () => {
          /** @type {EditorSnapshot|null} */
          const value = await evaluate(
            editorSession,
            `({url:location.href,text:document.body.innerText.slice(0,10000),saveDisabled:document.querySelector('#save-button').disabled,enabled:document.querySelector('#enabled').checked,inputs:[...document.querySelectorAll('input')].map(e=>({id:e.id,value:e.value,checked:e.checked}))})`,
          ).catch(() => null);
          return value?.url?.includes("id=") && value.saveDisabled
            ? value
            : null;
        }, "style saved through native editor controls");
        await browser.call("Target.closeTarget", { targetId: editorTarget });
        await browser.call("Target.activateTarget", { targetId });
        item.checks.push(
          "Authored CSS saved through the unchanged Stylus editor using native text and pointer input",
        );
      } else if (isUserscript(extension.name)) {
        const { targetId: permissionTarget } = await browser.call(
          "Target.createTarget",
          {
            url: `chrome://extensions/?id=${id}`,
          },
        );
        const { sessionId: permissionSession } = await browser.call(
          "Target.attachToTarget",
          {
            targetId: permissionTarget,
            flatten: true,
          },
        );
        const permissionControl =
          `(()=>{function find(root){for(const e of root.querySelectorAll('*')){if(e.id==='allow-user-scripts')return e;const found=e.shadowRoot&&find(e.shadowRoot);if(found)return found}}const row=find(document);if(!row)return null;row.scrollIntoView({block:'center'});const toggle=row.shadowRoot.querySelector('cr-toggle');const r=toggle.getBoundingClientRect();return {checked:row.checked,x:r.x+r.width/2,y:r.y+r.height/2}})()`;
        item.userScriptsPermissionBefore = await until(
          () => evaluate(permissionSession, permissionControl),
          "native Allow User Scripts control",
        );
        if (!item.userScriptsPermissionBefore.checked) {
          for (
            const [type, buttons] of [["mousePressed", 1], ["mouseReleased", 0]]
          ) {
            await browser.call("Input.dispatchMouseEvent", {
              type,
              x: item.userScriptsPermissionBefore.x,
              y: item.userScriptsPermissionBefore.y,
              button: "left",
              buttons,
              clickCount: 1,
            }, permissionSession);
          }
        }
        item.userScriptsPermissionAfter = await until(async () => {
          /** @type {import("./assessment-test-types.ts").PermissionControl|null} */
          const value = await evaluate(permissionSession, permissionControl);
          return value?.checked && value;
        }, "native user-granted userscript permission");
        item.checks.push(
          "Allow User Scripts enabled through the real test-profile extension details control using native pointer input",
        );
        await browser.call("Target.closeTarget", {
          targetId: permissionTarget,
        });
        const editorPath = extension.name === "Violentmonkey"
          ? "options/index.html#scripts/_new"
          : "options.html#nav=new-user-script";
        const { targetId: editorTarget } = await browser.call(
          "Target.createTarget",
          {
            url: `chrome-extension://${id}/${editorPath}`,
          },
        );
        const { sessionId: editorSession } = await browser.call(
          "Target.attachToTarget",
          {
            targetId: editorTarget,
            flatten: true,
          },
        );
        item.editorBefore = await until(async () => {
          /** @type {EditorSnapshot|null} */
          const value = await evaluate(
            editorSession,
            `({url:location.href,text:document.body?.innerText?.slice(0,10000),hasEditor:!!document.querySelector('.CodeMirror')?.CodeMirror})`,
          ).catch(() => null);
          return value?.hasEditor && value;
        }, "actual userscript editor");
        const code = [
          "// ==UserScript==",
          "// @name Floorp authored primary userscript",
          "// @namespace floorp-local-authored",
          "// @version 1.0",
          `// @include ${fixture}*`,
          "// @grant none",
          "// @inject-into content",
          "// @run-at document-end",
          "// ==/UserScript==",
          'document.documentElement.dataset.floorpUserScript = "authored-primary-operation";',
          'document.querySelector("#probe")?.style.setProperty("background-color", "rgb(12, 34, 56)", "important");',
        ].join("\n");
        item.authoredUserscriptSha256 = digest(code);
        await evaluate(
          editorSession,
          `document.querySelector('.CodeMirror').CodeMirror.focus()`,
        );
        for (
          const [type, key, code, windowsVirtualKeyCode, modifiers] of [
            ["rawKeyDown", "Control", "ControlLeft", 17, 2],
            ["rawKeyDown", "a", "KeyA", 65, 2],
            ["keyUp", "a", "KeyA", 65, 2],
            ["keyUp", "Control", "ControlLeft", 17, 0],
          ]
        ) {
          await browser.call("Input.dispatchKeyEvent", {
            type,
            key,
            code,
            windowsVirtualKeyCode,
            modifiers,
          }, editorSession);
        }
        await browser.call("Input.insertText", { text: code }, editorSession);
        item.editorAfterTyping = await evaluate(
          editorSession,
          `({code:document.querySelector('.CodeMirror').CodeMirror.getValue(),text:document.body.innerText.slice(0,10000)})`,
        );
        if (item.editorAfterTyping?.code !== code) {
          throw Error("Native editor input differs from authored userscript");
        }
        if (extension.name === "Tampermonkey") {
          const filePoint = await evaluate(
            editorSession,
            `(()=>{const e=[...document.querySelectorAll('*')].find(e=>!e.children.length&&e.textContent.trim()==='File'&&e.getBoundingClientRect().width);if(!e)throw Error('Missing visible native File menu');e.scrollIntoView({block:'center'});const r=e.getBoundingClientRect();return{x:r.x+r.width/2,y:r.y+r.height/2}})()`,
          );
          for (
            const [type, buttons] of [["mouseMoved", 0], ["mousePressed", 1], [
              "mouseReleased",
              0,
            ]]
          ) {
            await browser.call("Input.dispatchMouseEvent", {
              type,
              ...filePoint,
              button: type === "mouseMoved" ? "none" : "left",
              buttons,
              clickCount: 1,
            }, editorSession);
          }
          await until(
            () =>
              evaluate(
                editorSession,
                `[...document.querySelectorAll('*')].some(e=>!e.children.length&&e.textContent.trim()==='Save'&&e.getBoundingClientRect().width)`,
              ),
            "visible native Tampermonkey Save menu item",
          );
        }
        const editorImage = await browser.call("Page.captureScreenshot", {
          format: "png",
        }, editorSession);
        writeFileSync(
          join(output, `${id}-editor.png`),
          Buffer.from(editorImage.data, "base64"),
        );
        /** @type {Point} */
        const point = await evaluate(
          editorSession,
          `(()=>{const e=${
            extension.name === "Violentmonkey"
              ? "[...document.querySelectorAll('button')].find(e=>e.textContent.trim()==='Save & Close')"
              : "[...document.querySelectorAll('*')].find(e=>!e.children.length&&e.textContent.trim()==='Save'&&e.getBoundingClientRect().width)"
          };if(!e)throw Error('Missing native userscript Save control');e.scrollIntoView({block:'center'});const r=e.getBoundingClientRect();return{x:r.x+r.width/2,y:r.y+r.height/2,disabled:e.disabled,rect:r.toJSON()}})()`,
        );
        item.saveControl = point;
        for (
          const [type, buttons] of [["mousePressed", 1], ["mouseReleased", 0]]
        ) {
          await browser.call("Input.dispatchMouseEvent", {
            type,
            ...point,
            button: "left",
            buttons,
            clickCount: 1,
          }, editorSession);
        }
        if (extension.name === "Tampermonkey") {
          await delay(500);
          item.afterNativeSave = await evaluate(
            editorSession,
            `({url:location.href,text:document.body.innerText.slice(0,10000)})`,
          );
          await browser.call("Page.navigate", {
            url: `chrome-extension://${id}/options.html#nav=dashboard`,
          }, editorSession);
        }
        item.editorSaved = await until(async () => {
          /** @type {EditorSnapshot|null} */
          const value = await evaluate(
            editorSession,
            `({url:location.href,text:document.body.innerText.slice(0,10000),hasEditor:!!document.querySelector('.CodeMirror')})`,
          ).catch(() => null);
          return value && !value.hasEditor &&
            value.text?.includes("Floorp authored primary userscript") && value;
        }, "actual saved userscript in native dashboard");
        item.checks.push(
          "Authored matching userscript saved through unchanged extension editor using native text and pointer input",
        );
        if (extension.name === "Violentmonkey") {
          item.nativeUserscriptRegistration = await until(async () => {
            const target = (await browser.call("Target.getTargets")).targetInfos
              .find((target) =>
                target.type === "service_worker" &&
                target.url.startsWith(`chrome-extension://${id}/`)
              );
            if (!target) {
              return null;
            }
            const { sessionId: workerSession } = await browser.call(
              "Target.attachToTarget",
              { targetId: target.targetId, flatten: true },
            );
            try {
              /** @type {import("./assessment-test-types.ts").UserScriptRegistrationObservation} */
              const value = await evaluate(
                workerSession,
                `(async()=>({scripts:await chrome.userScripts.getScripts(),permissions:await chrome.permissions.getAll()}))()`,
              );
              return value.scripts.length && value;
            } finally {
              await browser.call("Target.detachFromTarget", {
                sessionId: workerSession,
              });
            }
          }, "real native userscript bootstrap registration");
        }

        await browser.call("Target.closeTarget", { targetId: editorTarget });
        await browser.call("Target.activateTarget", { targetId });
      } else if (extension.name !== "Dark Reader") {
        throw Error(
          "No primary-operation recipe implemented for " + extension.name,
        );
      }
      // Reload gives the unchanged extension its normal document-start injection.
      await browser.call("Page.reload", {}, sessionId);
      item.transformed = await until(async () => {
        const value = await readContentSnapshot(sessionId).catch(() => null);
        item.lastSnapshot = value ?? undefined;
        return isUserscript(extension.name)
          ? value?.userScript === "authored-primary-operation" &&
            value.frameUserScript === "authored-primary-operation" &&
            value.probe === "rgb(12, 34, 56)" && value
          : extension.name === "Stylus"
          ? value?.probe === "rgb(12, 34, 56)" && value
          : value?.markers?.length && value.body !== baseline.body
          ? value
          : null;
      }, "actual extension dynamic theme");
      item.checks.push(
        `Actual signed-package ${extension.name} script transformed the authored page`,
      );
      await browser.call(
        "Page.navigate",
        { url: fixture + "second" },
        sessionId,
      );
      item.afterNavigation = await until(async () => {
        const value = await readContentSnapshot(sessionId).catch(() => null);
        return value?.url === fixture + "second" &&
            (isUserscript(extension.name)
              ? value.userScript === "authored-primary-operation" &&
                value.frameUserScript === "authored-primary-operation" &&
                value.probe === "rgb(12, 34, 56)"
              : extension.name === "Stylus"
              ? value.probe === "rgb(12, 34, 56)"
              : value.markers?.length && value.body !== baseline.body)
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
        const value = await readContentSnapshot(sessionId).catch(() => null);
        return value?.body === baseline.body &&
            value.probe === baseline.probe && !value.markers.length
          ? value
          : null;
      }, "restored authored page after uninstall");
      item.checks.push("Uninstall removes the real transformation");
      item.primaryOperationResult = "passed";
    } catch (error) {
      item.contextStates = [];
      for (
        const rawEvent of browser.events.filter((event) =>
          event.method === "Runtime.executionContextCreated" &&
          event.sessionId === sessionId
        )
      ) {
        const event = /** @type {ContextCreatedEvent} */ (rawEvent);
        try {
          const value = await browser.call("Runtime.evaluate", {
            contextId: event.params.context.id,
            expression:
              `({url:location.href,ready:document.readyState,init:typeof window["**VMInitInjection**"],vm:typeof window.Violentmonkey,runtimeId:globalThis.chrome?.runtime?.id,scripts:document.scripts.length})`,
            returnByValue: true,
          }, sessionId);
          if (value.result?.value) {
            item.contextStates.push({
              context: event.params.context,
              .../** @type {import("./assessment-test-types.ts").ContextDiagnosticSnapshot} */ (value
                .result.value),
            });
          }
        } catch (contextError) {
          (item.contextDiagnosticErrors ||= []).push(String(contextError));
        }
      }
      item.primaryOperationResult = "failed";
      item.error = String(error);
    } finally {
      if (loaded) {
        try {
          await browser.call("Extensions.uninstall", { id });
        } catch (uninstallError) {
          item.cleanupError = String(uninstallError);
        }
      }
      item.nativeExecutionContexts = browser.events.filter((event) =>
        event.method === "Runtime.executionContextCreated"
      ).map((rawEvent) => {
        const event = /** @type {ContextCreatedEvent} */ (rawEvent);
        return { sessionId: event.sessionId, ...event.params.context };
      });
      item.nativeEvents = browser.events.filter((event) =>
        event.method === "Runtime.exceptionThrown" ||
        event.method === "Log.entryAdded" ||
        event.method === "Runtime.consoleAPICalled"
      );
      save();
    }
  }
} finally {
  await browser.close();
  await new Promise((resolve) => server.close(() => resolve(undefined)));
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
if (
  !report.extensions.length ||
  report.extensions.some((item) => item.primaryOperationResult !== "passed")
) process.exitCode = 1;
