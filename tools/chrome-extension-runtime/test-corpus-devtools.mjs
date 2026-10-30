#!/usr/bin/env node
// SPDX-License-Identifier: MPL-2.0
/** @import {BinaryInput, CorpusLock, ChromeReference, CorpusReport, CorpusOperation, DevtoolsSnapshot, Point, ChromeFrontend, ChromePanel, NativeTargetInfo} from "./assessment-test-types.ts" */
// Real framework stores, native DevTools UI, and immutable signed product scripts.
import { ChromePipe } from "./chrome-cdp-pipe.mjs";
import {
  attachNativeDevtools,
  evaluateNativeUI,
  waitForNativeUI,
} from "./chrome-devtools-panel.mjs";
import { buildDevtoolsFixture } from "./build-devtools-fixture.mjs";
import { buildVueFixture } from "./build-vue-fixture.mjs";
import { verifyCorpusScripts } from "./corpus-script-integrity.mjs";
import { createHash } from "node:crypto";
import { createServer } from "node:http";
import { cpSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
const root = resolve(dirname(fileURLToPath(import.meta.url)), "../..");
const tools = join(root, "tools/chrome-extension-runtime");
const output = mkdtempSync(
  join(root, "_dist/chrome-runtime-tests/corpus-devtools-reference-"),
);
/** @param {BinaryInput} bytes */
const digest = (bytes) => createHash("sha256").update(bytes).digest("hex");
/** @type {CorpusLock} */
const corpus = JSON.parse(
  readFileSync(join(tools, "corpus-lock.json")).toString("utf8"),
);
/** @type {ChromeReference} */
const reference = JSON.parse(
  readFileSync(join(tools, "chrome-reference.json")).toString("utf8"),
);
/** @param {string} name @param {string} fallback */
const option = (name, fallback) =>
  process.argv.includes(name)
    ? process.argv[process.argv.indexOf(name) + 1] ?? fallback
    : fallback;
const ids = option(
  "--ids",
  "fmkadmapgofadopljbjfkapdkoienihi,lmhkpmbekcpmknklioeibfkpmmfibljd",
).split(",");
const chrome = join(
  root,
  "_dist/chrome-reference/extracted/opt/google/chrome/chrome",
);
if (digest(readFileSync(chrome)) !== reference.binary.sha256) {
  throw Error("Reference binary digest mismatch");
}
const isVueFixture = ids.includes("nhdogjmejiglipccpnnnanhbledajbpd");
if (isVueFixture && ids.length !== 1) {
  throw Error("Run Vue with its separate genuine authored framework fixture");
}
const frameworkMetadata = isVueFixture
  ? buildVueFixture(output)
  : buildDevtoolsFixture(output);
const app = readFileSync(join(output, "app.js"));
const html = readFileSync(join(output, "index.html"));
const server = createServer((request, response) => {
  const source = request.url === "/app.js"
    ? app
    : request.url?.startsWith("/app")
    ? html
    : null;
  response.writeHead(source ? 200 : 404, {
    "Content-Type": request.url === "/app.js"
      ? "application/javascript"
      : "text/html",
    "Cache-Control": "no-store",
  });
  response.end(source || "Not found");
});
await new Promise((resolve) =>
  server.listen(0, "127.0.0.1", () => resolve(undefined))
);
const address = server.address();
if (!address || typeof address === "string") {
  throw Error("Native fixture server has no TCP address");
}
const url = `http://127.0.0.1:${address.port}/app`;
/** @type {CorpusReport<DevtoolsSnapshot>} */
const report = {
  stage: "actual-package-devtools-primary-operation",
  reference,
  displayMode: "headed",
  fixture: {
    url,
    htmlSha256: digest(html),
    appSha256: digest(app),
    frameworks: frameworkMetadata,
  },
  extensions: [],
  primaryOperationPassRate: null,
  output,
};
const browser = new ChromePipe(chrome, output, [
  "--auto-open-devtools-for-tabs",
  "--window-size=1600,1200",
], { headed: true });
browser.events = [];
/** @param {number} ms @returns {Promise<void>} */
const pause = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
let failed = false;
try {
  report.version = await browser.call("Browser.getVersion");
  for (const id of ids) {
    const record = corpus.extensions.find((record) => record.id === id);
    if (!record) throw Error("Unfrozen extension " + id);
    /** @type {CorpusOperation<DevtoolsSnapshot>} */
    const item = {
      id,
      name: record.name,
      version: record.version,
      packageSha256: record.packageSha256,
      result: "failed",
      primaryOperationResult: "not-run",
      checks: [],
    };
    report.extensions.push(item);
    /** @type {ChromeFrontend|undefined} */ let native;
    /** @type {ChromePanel|undefined} */ let panel;
    /** @type {string|undefined} */ let appTarget;
    try {
      const directory = join(output, id);
      cpSync(
        join(root, "_dist/chrome-extension-corpus/chrome-unpacked", id),
        directory,
        {
          recursive: true,
          filter: (path) => !path.split("/").includes("_metadata"),
        },
      );
      Object.assign(item, verifyCorpusScripts(root, record, directory));
      const loaded = await browser.call("Extensions.loadUnpacked", {
        path: directory,
      });
      if (loaded.id !== id) throw Error("Original extension identity mismatch");
      await pause(1200);
      const fixtureURL = url + "/" + id;
      item.fixtureURL = fixtureURL;
      const { targetId } = await browser.call("Target.createTarget", {
        url: fixtureURL,
      });
      appTarget = targetId;
      const { sessionId } = await browser.call("Target.attachToTarget", {
        targetId,
        flatten: true,
      });
      await browser.call("Runtime.enable", {}, sessionId);
      /** @template T @param {string} expression @returns {Promise<T>} */
      const evaluate = (expression) =>
        evaluateNativeUI(browser, sessionId, expression);
      const isVue = id === "nhdogjmejiglipccpnnnanhbledajbpd";
      await waitForNativeUI(
        () =>
          evaluate(
            isVue
              ? "document.querySelector('#vue-count')?.textContent==='Vue count: 0'"
              : "document.querySelector('#react-count')?.textContent==='React count: 0'",
          ),
        "actual framework initial rendering",
      );
      native = await attachNativeDevtools(browser, item.fixtureURL);
      const activeNative = native;
      item.nativeDevtools = activeNative.target;
      const title = id === "fmkadmapgofadopljbjfkapdkoienihi"
        ? "Components ⚛"
        : isVue
        ? "Vue"
        : "Redux";
      item.selectedTabs = await activeNative.selectPanel(title);
      panel = await activeNative.attachPanel(
        id,
        id === "fmkadmapgofadopljbjfkapdkoienihi"
          ? "panel.html"
          : isVue
          ? "pages/devtools-panel.html"
          : "devpanel.html",
      );
      const activePanel = panel;
      item.nativePanel = activePanel.target;
      await waitForNativeUI(async () => {
        item.initialPanel = await activePanel.snapshot();
        return item.initialPanel.text.includes(
          id === "fmkadmapgofadopljbjfkapdkoienihi"
            ? "FloorpCounter"
            : isVue
            ? "FloorpVueCounterLabel"
            : "Floorp authored Redux store",
        );
      }, "actual framework component or store in native panel");
      /** @param {string} selector */
      const nativeAppClick = async (selector) => {
        /** @type {Point} */
        const point = await evaluate(
          `(()=>{const r=document.querySelector(${
            JSON.stringify(selector)
          }).getBoundingClientRect();return{x:r.x+r.width/2,y:r.y+r.height/2}})()`,
        );
        await browser.call("Target.activateTarget", { targetId });
        await browser.call("Page.bringToFront", {}, sessionId);
        for (
          const [type, buttons] of [["mouseMoved", 0], ["mousePressed", 1], [
            "mouseReleased",
            0,
          ]]
        ) {
          await browser.call("Input.dispatchMouseEvent", {
            type,
            x: point.x,
            y: point.y,
            button: type === "mouseMoved" ? "none" : "left",
            buttons,
            clickCount: 1,
          }, sessionId);
        }
      };
      if (id === "fmkadmapgofadopljbjfkapdkoienihi") {
        await activePanel.clickExpression(
          `[...document.querySelectorAll('[data-testname="ComponentTreeListItem"]')].find(e=>e.textContent.trim()==='FloorpCounterLabel')`,
        );
        await waitForNativeUI(async () => {
          /** @type {string[]} */
          const props = await activePanel.evaluate(
            `(()=>{const root=document.querySelector('[data-testname="InspectedElementPropsTree"]');return root?[...root.querySelectorAll('input')].map(e=>e.value):[]})()`,
          );
          return props[0] === "count" && props[1] === "0";
        }, "actual native component props count0");
        item.selectedComponent = await activePanel.snapshot();
        item.propsBefore = await activePanel.evaluate(
          `(()=>{const root=document.querySelector('[data-testname="InspectedElementPropsTree"]');return root?{text:root.innerText,inputs:[...root.querySelectorAll('input')].map(e=>({value:e.value,placeholder:e.placeholder,test:e.getAttribute('data-testname'),type:e.type}))}:null})()`,
        );
        await nativeAppClick("#react-increment");
        await waitForNativeUI(
          () =>
            evaluate(
              `document.getElementById('react-count').textContent==='React count: 1'`,
            ),
          "real framework native-button state update",
        );
        await waitForNativeUI(async () => {
          /** @type {string[]} */
          const props = await activePanel.evaluate(
            `(()=>{const root=document.querySelector('[data-testname="InspectedElementPropsTree"]');return root?[...root.querySelectorAll('input')].map(e=>e.value):[]})()`,
          );
          return props[0] === "count" && props[1] === "1";
        }, "actual native component props count1");
        item.updatedComponent = await activePanel.snapshot();
        item.propsAfter = await activePanel.evaluate(
          `(()=>{const root=document.querySelector('[data-testname="InspectedElementPropsTree"]');return root?{text:root.innerText,inputs:[...root.querySelectorAll('input')].map(e=>({value:e.value,placeholder:e.placeholder,test:e.getAttribute('data-testname'),type:e.type}))}:null})()`,
        );
      }
      if (id === "lmhkpmbekcpmknklioeibfkpmmfibljd") {
        await activePanel.clickExpression(
          `[...document.querySelectorAll('*')].find(e=>!e.children.length&&e.textContent.trim()==='State')`,
        );
        await waitForNativeUI(async () => {
          item.stateBefore = await activePanel.snapshot();
          return /^count\(pin\):0$/m.test(item.stateBefore.text);
        }, "actual native Redux state count0");
        await nativeAppClick("#redux-increment");
        await waitForNativeUI(
          () =>
            evaluate(
              `document.getElementById('redux-count').textContent==='Redux count: 1'`,
            ),
          "actual native Redux application state1",
        );
        await waitForNativeUI(async () => {
          item.stateAfter = await activePanel.snapshot();
          return /^count\(pin\):1$/m.test(item.stateAfter.text) &&
            item.stateAfter.text.includes("AUTHORED_INCREMENT");
        }, "actual native Redux action and state count1");
      }
      if (isVue) {
        await activePanel.clickExpression(
          `[...document.querySelectorAll('*')].find(e=>!e.children.length&&e.textContent.includes('FloorpVueCounterLabel'))`,
        );
        item.selectedComponent = await waitForNativeUI(async () => {
          const value = await activePanel.snapshot();
          return value.text.includes("FloorpVueCounterLabel") &&
            /\bcount\b\s*:?\s*0\b/.test(value.text) && value;
        }, "actual Vue native component prop count0");
        await nativeAppClick("#vue-increment");
        await waitForNativeUI(
          () =>
            evaluate(
              "document.querySelector('#vue-count')?.textContent==='Vue count: 1'",
            ),
          "actual Vue rendered native-button update",
        );
        item.updatedComponent = await waitForNativeUI(async () => {
          const value = await activePanel.snapshot();
          return value.text.includes("FloorpVueCounterLabel") &&
            /\bcount\b\s*:?\s*1\b/.test(value.text) && value;
        }, "actual Vue native component prop count1");
      }
      await browser.call("Extensions.uninstall", { id });
      await waitForNativeUI(
        async () =>
          !(await browser.call("Target.getTargets", { filter: [{}] }))
            .targetInfos.some((frame) =>
              /** @type {NativeTargetInfo} */ (frame).parentId ===
                  activeNative.target.targetId &&
                frame.url === activePanel.target.url
            ),
        "real extension panel removed on uninstall",
      );
      item.checks = isVue
        ? [
          "Signed Vue DevTools scripts unchanged through frozen archive verification",
          "Official frozen Vue development build exposes its genuine component tree",
          "Native Vue component selection exposes authored prop count0",
          "Actual native app button changes real Vue state and native component prop count1",
          "Actual uninstall removes the native extension panel",
        ]
        : id === "lmhkpmbekcpmknklioeibfkpmmfibljd"
        ? [
          "Actual frozen Redux native panel exposes authored Redux store",
          "Native State view shows count0",
          "Native authored app button dispatches AUTHORED_INCREMENT and actual State view shows count1",
          "Uninstall removes the actual native extension panel",
        ]
        : [
          "Actual frozen extension Components panel exposes genuine React component tree",
          "Native panel inspection shows authored component props count0",
          "Native authored app button changes real React state and native extension panel props count1",
          "Uninstall removes the actual native extension panel",
        ];
      item.result = item.primaryOperationResult = "passed";
    } catch (error) {
      failed = true;
      item.primaryOperationResult = "failed";
      item.error = String(
        error instanceof Error ? error.stack || error.message : error,
      );
      if (native) {
        try {
          item.availableNativeTabs = await native.tabs();
        } catch (tabsError) {
          item.nativeTabsDiagnosticError = String(tabsError);
        }
      }
      if (panel) {
        try {
          item.panelFailure = await panel.snapshot();
        } catch (panelError) {
          item.nativePanelDiagnosticError = String(panelError);
        }
      }
    }
    if (appTarget) {
      await browser.call("Target.closeTarget", { targetId: appTarget });
    }
    writeFileSync(join(output, "report.json"), JSON.stringify(report, null, 2));
  }
} finally {
  report.events = browser.events.filter((event) =>
    event.method === "Runtime.exceptionThrown" ||
    event.method === "Log.entryAdded"
  );
  await browser.close();
  await new Promise((resolve) => server.close(() => resolve(undefined)));
  writeFileSync(join(output, "report.json"), JSON.stringify(report, null, 2));
  console.log("REPORT " + join(output, "report.json"));
}
if (failed) process.exitCode = 1;
