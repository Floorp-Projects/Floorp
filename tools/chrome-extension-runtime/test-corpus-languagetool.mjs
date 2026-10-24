#!/usr/bin/env node
// SPDX-License-Identifier: MPL-2.0
import { ChromePipe } from "./chrome-cdp-pipe.mjs";
import { runLanguageToolOperation } from "./corpus-languagetool-operation.mjs";
import { createHash } from "node:crypto";
import { readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
const tools = dirname(fileURLToPath(import.meta.url));
const output = process.env.FLOORP_LANGUAGETOOL_OUTPUT;
const root = process.env.FLOORP_CORPUS_ROOT;
if (!output || !root) throw Error("Private fixture output/root required");
const id = "oldceeleldhonbafppcapldpdifcinji";
const record = /** @type {import("./network-test-types.ts").NetworkCorpus} */ (
  JSON.parse(readFileSync(join(tools, "corpus-lock.json"), "utf8"))
).extensions.find((item) => item.id === id);
if (!record) throw Error("Frozen LanguageTool record missing");
/** @type {import("./network-test-types.ts").LanguageFixture} */
const fixture = JSON.parse(readFileSync(join(output, "fixture.json"), "utf8"));
/** @type {import("./network-test-types.ts").ChromeReference} */
const reference = JSON.parse(
  readFileSync(join(tools, "chrome-reference.json"), "utf8"),
);
const chrome = join(
  root,
  "_dist/chrome-reference/extracted/opt/google/chrome/chrome",
);
if (
  createHash("sha256").update(readFileSync(chrome)).digest("hex") !==
  reference.binary.sha256
)
  throw Error("Frozen reference browser hash mismatch");
const headed = process.argv.includes("--headed");
/** @type {import("./network-test-types.ts").LanguageItem} */
const item = {
  id,
  name: record.name,
  version: record.version,
  packageSha256: record.packageSha256,
  sourceIntegrity: fixture.sourceIntegrity,
  primaryOperation: record.primaryOperation,
  primaryOperationResult: "not-run",
  checks: [],
};
/** @type {import("./network-test-types.ts").LanguageReport} */
const report = {
  stage: "actual-package-primary-operation",
  reference,
  fixture,
  displayMode: headed ? "headed" : "headless",
  extensions: [item],
  primaryOperationPassRate: null,
  output,
};
const save = () =>
  writeFileSync(join(output, "report.json"), JSON.stringify(report, null, 2));
const browser = new ChromePipe(
  chrome,
  output,
  ["--log-net-log=" + join(output, "native-net-log.json")],
  { headed },
);
browser.events = [];
/** @param {number} ms */
const delay = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
/** @template [T=unknown]
 * @param {string | undefined} sessionId
 * @param {string} expression
 * @returns {Promise<T>} */
async function evaluate(sessionId, expression) {
  if (!sessionId) throw Error("Native CDP session missing");
  const result = await browser.call(
    "Runtime.evaluate",
    { expression, awaitPromise: true, returnByValue: true },
    sessionId,
  );
  if (result.exceptionDetails)
    throw Error(JSON.stringify(result.exceptionDetails));
  return /** @type {T} */ (result.result.value);
}
/** @template T
 * @param {() => T | false | null | undefined | Promise<T | false | null | undefined>} read
 * @param {string} label
 * @param {number} [timeout]
 * @returns {Promise<T>} */
async function until(read, label, timeout = 30000) {
  const end = Date.now() + timeout;
  while (Date.now() < end) {
    const value = await read();
    if (value) return value;
    await delay(150);
  }
  throw Error("Timed out waiting for " + label);
}
const snapshotExpression = `(()=>{const all=[];const visit=root=>{for(const e of root.querySelectorAll('*')){const r=e.getBoundingClientRect();if(e.shadowRoot)visit(e.shadowRoot);if(r.width&&r.height&&(e.tagName.startsWith('LT-')||e.getAttribute('role')==='dialog'||e.className?.includes?.('lt-')))all.push({tag:e.tagName,cls:typeof e.className==='string'?e.className:'',text:e.innerText?.slice(0,1500),html:e.outerHTML.slice(0,2500)})}};visit(document);return{url:location.href,ready:document.readyState,installed:document.documentElement.getAttribute('data-lt-installed'),text:document.querySelector('#editor')?.value,body:document.body.innerText.slice(0,12000),product:all.slice(-60)}})()`;
/** @param {string | undefined} sessionId @returns {Promise<import("./network-test-types.ts").LanguageSnapshot>} */
function snapshot(sessionId) {
  return evaluate(sessionId, snapshotExpression);
}
let loaded = false;
/** @type {string | undefined} */
let sessionId;
try {
  report.version = await browser.call("Browser.getVersion");
  const { targetId } = await browser.call("Target.createTarget", {
    url: fixture.url,
  });
  ({ sessionId } = await browser.call("Target.attachToTarget", {
    targetId,
    flatten: true,
  }));
  await browser.call("Runtime.enable", {}, sessionId);
  await browser.call("Page.enable", {}, sessionId);
  await browser.call("Log.enable", {}, sessionId);
  await browser.call("Network.enable", {}, sessionId);
  await until(
    () =>
      evaluate(
        sessionId,
        "document.readyState==='complete'&&!!document.querySelector('#editor')",
      ),
    "authored editor",
  );
  item.baseline = await snapshot(sessionId);
  const loadedExtension = await browser.call("Extensions.loadUnpacked", {
    path: join(output, "chrome-package"),
  });
  loaded = true;
  if (loadedExtension.id !== id)
    throw Error("Original extension identity mismatch");
  await delay(1500);
  item.onboarding = [];
  for (const target of (await browser.call("Target.getTargets")).targetInfos) {
    if (
      target.type !== "page" ||
      !target.url.startsWith("https://languagetool.org/webextension/welcome")
    )
      continue;
    const attached = await browser.call("Target.attachToTarget", {
      targetId: target.targetId,
      flatten: true,
    });
    item.remoteSetupAttempt = await evaluate(
      attached.sessionId,
      "({url:location.href,ready:document.readyState,text:document.body.innerText.slice(0,4000)})",
    ).catch((error) => ({ url: target.url, error: String(error) }));
    await browser.call(
      "Page.navigate",
      { url: fixture.onboardingHost.url },
      attached.sessionId,
    );
    const notice = await until(async () => {
      /** @type {import("./network-test-types.ts").NoticeSnapshot} */
      const value = await evaluate(
        attached.sessionId,
        "({url:location.href,ready:document.readyState,text:document.body.innerText.slice(0,20000)})",
      );
      item.onboardingPending = value;
      return (
        value.url === fixture.onboardingHost.url &&
        value.ready === "complete" &&
        value.text.includes("Okay, got it") &&
        value
      );
    }, "actual LanguageTool data transmission notice");
    item.onboarding.push(notice);
    if (
      !notice.text.includes(
        "This extension will check your text by sending it to LanguageTool.org over an encrypted connection.",
      ) ||
      !notice.text.includes("Your text will not be stored") ||
      /agree to.*terms|purchase|subscribe|create.*account/i.test(notice.text)
    ) {
      item.primaryOperationResult = "blocked-unreviewed-consent";
      throw Error(
        "Actual notice differs from the authorized reversible synthetic-text transmission setup",
      );
    }
    await browser.call("Target.activateTarget", { targetId: target.targetId });
    await browser.call("Page.bringToFront", {}, attached.sessionId);
    /** @type {import("./network-test-types.ts").Point} */
    const noticePoint = await evaluate(
      attached.sessionId,
      `(()=>{const e=[...document.querySelectorAll('*')].find(e=>!e.children.length&&e.textContent.trim()==='Okay, got it');if(!e)throw Error('Missing actual notice control');e.scrollIntoView({block:'center'});const r=e.getBoundingClientRect();return{x:r.x+r.width/2,y:r.y+r.height/2}})()`,
    );
    for (const [type, buttons] of [
      ["mousePressed", 1],
      ["mouseReleased", 0],
    ])
      await browser.call(
        "Input.dispatchMouseEvent",
        { type, ...noticePoint, button: "left", buttons, clickCount: 1 },
        attached.sessionId,
      );
    item.consent = {
      notice: notice.text,
      nativeControl: "Okay, got it",
      scope:
        "Disposable browser profile; authored synthetic sentence only; no account",
    };
    await delay(500);
    await browser.call("Target.closeTarget", { targetId: target.targetId });
  }
  await browser.call("Page.reload", {}, sessionId);
  await until(
    () =>
      evaluate(
        sessionId,
        "document.readyState==='complete'&&!!document.querySelector('#editor')",
      ),
    "new native editor document",
  );
  await browser.call("Target.activateTarget", { targetId });
  await browser.call("Page.bringToFront", {}, sessionId);
  const verifyRemoval = await runLanguageToolOperation({
    item,
    url: fixture.url,
    text: fixture.text,
    snapshot: () => snapshot(sessionId),
    until,
    pause: delay,
    async click({ selector, text }) {
      item.lastInput = { operation: "click", selector, text };
      save();
      await browser.call("Target.activateTarget", { targetId });
      await browser.call("Page.bringToFront", {}, sessionId);
      /** @type {import("./network-test-types.ts").Point} */
      const point = await evaluate(
        sessionId,
        `(()=>{const e=[...document.querySelectorAll(${JSON.stringify(selector)})].find(e=>${text ? "e.textContent.trim()===" + JSON.stringify(text) : "true"});if(!e)throw Error('Missing real product control');const r=e.getBoundingClientRect();if(!r.width||!r.height)throw Error('Hidden real product control');return{x:r.x+r.width/2,y:r.y+r.height/2}})()`,
      );
      for (const [type, buttons] of [
        ["mousePressed", 1],
        ["mouseReleased", 0],
      ])
        await browser.call(
          "Input.dispatchMouseEvent",
          { type, ...point, button: "left", buttons, clickCount: 1 },
          sessionId,
        );
    },
    async type(text) {
      for (const type of ["keyDown", "keyUp"])
        await browser.call(
          "Input.dispatchKeyEvent",
          {
            type,
            key: "a",
            code: "KeyA",
            windowsVirtualKeyCode: 65,
            modifiers: 2,
          },
          sessionId,
        );
      await browser.call("Input.insertText", { text }, sessionId);
    },
    async navigate(url) {
      const old = await evaluate(sessionId, "performance.timeOrigin");
      await browser.call("Page.navigate", { url }, sessionId);
      await until(
        () =>
          evaluate(
            sessionId,
            `document.readyState==='complete'&&performance.timeOrigin!==${old}&&location.href===${JSON.stringify(url)}`,
          ).catch(() => false),
        "real new grammar document",
      );
    },
  });
  await browser.call("Extensions.uninstall", { id });
  loaded = false;
  await verifyRemoval();
  item.primaryOperationResult = "passed";
} catch (error) {
  if (item.primaryOperationResult === "not-run")
    item.primaryOperationResult = "failed";
  item.error = String(error);
  if (sessionId)
    item.lastSnapshot = await snapshot(sessionId).catch(() => null);
} finally {
  item.nativeEvents = browser.events.filter((event) =>
    [
      "Runtime.exceptionThrown",
      "Log.entryAdded",
      "Page.javascriptDialogOpening",
    ].includes(event.method),
  );
  item.serviceRequests = browser.events
    .filter(
      (event) =>
        event.method === "Network.requestWillBeSent" &&
        /languagetool/i.test(
          String(
            (event.params?.request &&
              /** @type {{url?:string}} */ (event.params.request).url) ||
              "",
          ),
        ),
    )
    .map((event) => ({
      url: /** @type {{url:string}} */ (event.params?.request).url,
      method: /** @type {{method:string}} */ (event.params?.request).method,
    }));
  item.serviceResponses = browser.events
    .filter(
      (event) =>
        event.method === "Network.responseReceived" &&
        /languagetool/i.test(
          String(
            (event.params?.response &&
              /** @type {{url?:string}} */ (event.params.response).url) ||
              "",
          ),
        ),
    )
    .map((event) => ({
      url: /** @type {{url:string}} */ (event.params?.response).url,
      status: /** @type {{status:number}} */ (event.params?.response).status,
    }));
  item.nativeTargets = await browser
    .call("Target.getTargets")
    .catch(() => null);
  if (loaded)
    await browser.call("Extensions.uninstall", { id }).catch(() => {});
  await browser.close();
  for (let attempt = 0; attempt < 10; attempt++) {
    try {
      /** @type {import("./network-test-types.ts").NativeNetLog} */
      const log = JSON.parse(
        readFileSync(join(output, "native-net-log.json"), "utf8"),
      );
      item.nativeServiceRequests = log.events
        .filter(
          (event) =>
            /languagetool/i.test(event.params?.url || "") &&
            /\/check(?:\?|$)/.test(event.params?.url ?? ""),
        )
        .map((event) => ({
          url: event.params?.url ?? "",
          method: event.params?.method,
          source: event.source?.id,
        }));
      break;
    } catch {
      await delay(100);
    }
  }
  save();
}
console.log(
  JSON.stringify({
    name: item.name,
    result: item.primaryOperationResult,
    error: item.error,
  }),
);
console.log("REPORT " + join(output, "report.json"));
if (item.primaryOperationResult !== "passed") process.exitCode = 1;
