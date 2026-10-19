// SPDX-License-Identifier: MPL-2.0
// Actual frozen package primary operations on the pinned Chrome reference.
import { createHash } from "node:crypto";
import { createServer } from "node:http";
import { mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { assertFrozenChromeBinary, ChromePipe } from "./chrome-cdp-pipe.mjs";
import { verifyCorpusScripts } from "./corpus-script-integrity.mjs";
import {
  automaId,
  automaWorkflow,
  browserflowId,
  expectedAutomaCustomer,
  expectedCustomer,
  uiVisionId,
  uiVisionMacro,
} from "./corpus-automation-recipes.mjs";
const root = resolve(dirname(fileURLToPath(import.meta.url)), "../..");
assertFrozenChromeBinary(
  join(root, "_dist/chrome-reference/extracted/opt/google/chrome/chrome"),
);
const output = mkdtempSync(
  join(root, "_dist/chrome-runtime-tests/corpus-automation-reference-"),
);
const fixture = readFileSync(
  join(root, "tools/chrome-extension-runtime/fixtures/corpus-automation.html"),
);
/** @param {Uint8Array|string} bytes */
const sha256 = (bytes) => createHash("sha256").update(bytes).digest("hex");
const corpus =
  /** @type {import("./corpus-types.ts").FrozenCorpus} */ (JSON.parse(
    readFileSync(
      join(root, "tools/chrome-extension-runtime/corpus-lock.json"),
      "utf8",
    ),
  ));
const reference = JSON.parse(
  readFileSync(
    join(root, "tools/chrome-extension-runtime/chrome-reference.json"),
    "utf8",
  ),
);
const server = createServer((_request, response) => {
  response.writeHead(200, {
    "Content-Type": "text/html; charset=utf-8",
    "Cache-Control": "no-store",
  });
  response.end(fixture);
});
await new Promise((resolve) =>
  server.listen(0, "127.0.0.1", () => resolve(undefined))
);
const address = server.address();
if (!address || typeof address === "string") {
  throw Error("Automation fixture did not bind a TCP port");
}
const fixtureURL = `http://127.0.0.1:${address.port}/automation`;
const browser = new ChromePipe(
  join(root, "_dist/chrome-reference/extracted/opt/google/chrome/chrome"),
  output,
);
browser.events = [];
/** @param {number} ms */
const delay = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
/** @template [T=unknown] @param {string} session @param {string} expression @returns {Promise<T>} */
const evaluate = async (session, expression) => {
  const result = await browser.call(
    "Runtime.evaluate",
    { expression, returnByValue: true, awaitPromise: true, userGesture: true },
    session,
  );
  if (result.exceptionDetails) {
    throw Error(JSON.stringify(result.exceptionDetails));
  }
  return /** @type {T} */ (result.result.value);
};
/** @type {import("./automation-test-types.ts").AutomationReport} */
const report = {
  stage: "frozen-automation-primary-operation",
  reference,
  fixture: { url: fixtureURL, sha256: sha256(fixture) },
  extensions: [],
  output,
};
let failed = false;
const only = process.argv.find((argument) =>
  /^--(?:uivision|automa|browserflow)-only$/.test(argument)
);
/** @param {string} product */
const shouldRun = (product) => !only || only === `--${product}-only`;
try {
  if (shouldRun("uivision")) {
    const record = corpus.extensions.find((record) => record.id === uiVisionId);
    if (!record) throw Error("Frozen automation record missing");
    /** @type {import("./automation-test-types.ts").AutomationItem} */
    /** @type {import("./automation-test-types.ts").AutomationItem} */
    const item = {
      id: record.id,
      name: record.name,
      version: record.version,
      packageSha256: record.packageSha256,
      result: "failed",
    };
    report.extensions.push(item);
    try {
      Object.assign(item, verifyCorpusScripts(root, record));
      const { id } = await browser.call("Extensions.loadUnpacked", {
        path: join(
          root,
          "_dist/chrome-extension-corpus/chrome-unpacked",
          record.id,
        ),
      });
      if (id !== record.id) throw Error("Original Chrome identity mismatch");
      await delay(1000);
      const target = await browser.call("Target.createTarget", {
        url: fixtureURL,
      });
      const { sessionId } = await browser.call("Target.attachToTarget", {
        targetId: target.targetId,
        flatten: true,
      });
      await delay(1400);
      const options = await browser.call("Target.createTarget", {
        url: `chrome-extension://${id}/options.html`,
      });
      const { sessionId: optionsSession } = await browser.call(
        "Target.attachToTarget",
        { targetId: options.targetId, flatten: true },
      );
      await delay(1400);
      // Configure the real documented website-run feature in this private profile.
      // The origin allowlist is limited to our authored local fixture; no vendor code is changed.
      item.configuration = await evaluate(
        optionsSession,
        `(async()=>{const {config={}}=await chrome.storage.local.get('config');await chrome.storage.local.set({config:{...config,allowRunFromHttpSchema:true,websiteInvokePolicyVersion:1,websiteWhiteList:[${
          JSON.stringify(fixtureURL)
        }],showClassicMacros:true,showTestCaseTab:true,openAiChatTabOnce:false,macroFreshInstall:false}});return {websiteInvokePolicyVersion:1,websiteWhiteList:[${
          JSON.stringify(fixtureURL)
        }]}})()`,
      );
      await browser.call("Target.closeTarget", { targetId: options.targetId });
      await browser.call("Target.activateTarget", {
        targetId: target.targetId,
      });
      const tab = (
        await browser.call("Target.getTargets", { filter: [{ type: "tab" }] })
      ).targetInfos.find((tab) => tab.url === fixtureURL);
      if (!tab) throw Error("Actual authored browser tab missing");
      await browser.call("Extensions.triggerAction", {
        id,
        targetId: tab.targetId,
      });
      await delay(2200);
      item.macro = uiVisionMacro(fixtureURL);
      await evaluate(
        sessionId,
        `window.dispatchEvent(new CustomEvent('kantuSaveAndRunMacro',{detail:${
          JSON.stringify({
            json: item.macro,
            direct: true,
            storageMode: "browser",
          })
        }}))`,
      );
      let panelSession;
      for (let attempt = 0; attempt < 90; attempt++) {
        await delay(400);
        item.observed = await evaluate(
          sessionId,
          `({url:location.href,input:document.querySelector('#customer')?.value,result:document.querySelector('#operation-result')?.textContent,operation:document.documentElement.dataset.operation,clickCount:document.documentElement.dataset.clickCount})`,
        );
        if (!panelSession) {
          const panel = (
            await browser.call("Target.getTargets")
          ).targetInfos.find(
            (target) =>
              target.url === `chrome-extension://${id}/sidepanel.html`,
          );
          if (panel) {
            panelSession = (
              await browser.call("Target.attachToTarget", {
                targetId: panel.targetId,
                flatten: true,
              })
            ).sessionId;
          }
        }
        if (panelSession) {
          item.engineLog = await evaluate(
            panelSession,
            "document.body.innerText",
          );
        }
        if (
          item.observed?.operation === expectedCustomer &&
          item.engineLog?.includes("Macro completed") &&
          !item.engineLog.includes("[error]")
        ) {
          break;
        }
      }
      if (
        !item.observed || item.observed.input !== expectedCustomer ||
        item.observed.result !== `Submitted: ${expectedCustomer}` ||
        item.observed.clickCount !== "1"
      ) {
        throw Error(
          "Real macro did not produce the expected input and single click effect",
        );
      }
      if (
        !item.engineLog?.includes("Macro completed") ||
        item.engineLog.includes("[error]")
      ) {
        throw Error("Real extension macro assertion/completion missing");
      }
      item.checks = [
        "original signed CRX SHA-256, every loaded script unchanged and original Chrome identity",
        "documented SaveAndRunMacro import bridge",
        "real extension open/type/click/assertText engine",
        "actual target DOM value and one click",
        "real extension successful completion log",
      ];
      item.result = "pass";
      const screenshot = await browser.call(
        "Page.captureScreenshot",
        {},
        sessionId,
      );
      writeFileSync(
        join(output, `${id}-target.png`),
        Buffer.from(screenshot.data, "base64"),
      );
      console.log(`PASS ${record.name}: real macro open/type/click/assertText`);
    } catch (error) {
      item.error = String(error);
      failed = true;
      console.error(item.error);
    }
    await browser.call("Extensions.uninstall", { id: uiVisionId });
  }
  if (shouldRun("automa")) {
    const record = corpus.extensions.find((record) => record.id === automaId);
    if (!record) throw Error("Frozen automation record missing");
    /** @type {import("./automation-test-types.ts").AutomationItem} */
    /** @type {import("./automation-test-types.ts").AutomationItem} */
    const item = {
      id: record.id,
      name: record.name,
      version: record.version,
      packageSha256: record.packageSha256,
      result: "failed",
    };
    report.extensions.push(item);
    const automaURL = fixtureURL + "?automa";
    try {
      Object.assign(item, verifyCorpusScripts(root, record));
      const { id } = await browser.call("Extensions.loadUnpacked", {
        path: join(
          root,
          "_dist/chrome-extension-corpus/chrome-unpacked",
          record.id,
        ),
      });
      if (id !== record.id) throw Error("Original Chrome identity mismatch");
      await delay(900);
      const popup = await browser.call("Target.createTarget", {
        url: `chrome-extension://${id}/popup.html`,
      });
      const { sessionId } = await browser.call("Target.attachToTarget", {
        targetId: popup.targetId,
        flatten: true,
      });
      await delay(1600);
      /** @type {import("./automation-test-types.ts").AutomaStorage} */
      let stored = {};
      /** @type {import("./automation-test-types.ts").AutomaWorkflow|undefined} */
      let template;
      for (let attempt = 0; attempt < 100; attempt++) {
        stored = await evaluate(
          sessionId,
          'chrome.storage.local.get(["workflows","isFirstTime"])',
        );
        template = Object.values(stored.workflows || {}).find(
          (workflow) =>
            workflow.drawflow?.nodes?.some((node) => node.label === "forms") &&
            workflow.drawflow.nodes.some(
              (node) => node.label === "event-click",
            ),
        );
        if (template) break;
        await delay(200);
      }
      if (!template) {
        throw Error("Genuine packaged Automa example graph is missing");
      }
      item.workflow = automaWorkflow(template, automaURL);
      // A real user-authored workflow is input data. All engine/UI scripts stay immutable.
      await evaluate(
        sessionId,
        `chrome.storage.local.set(${
          JSON.stringify({
            workflows: {
              ...stored.workflows,
              [item.workflow.id]: item.workflow,
            },
            isFirstTime: false,
          })
        })`,
      );
      await browser.call("Page.reload", {}, sessionId);
      await delay(1000);
      item.initialUI = await evaluate(sessionId, "document.body.innerText");
      item.workerTarget = (
        await browser.call("Target.getTargets")
      ).targetInfos.find(
        (target) =>
          target.type === "service_worker" &&
          target.url.startsWith(`chrome-extension://${id}/`),
      );
      const button = /** @type {{x:number,y:number}|null} */ (await evaluate(
        sessionId,
        `(()=>{const card=[...document.querySelectorAll('.ui-card')].find(card=>card.querySelector('p')?.textContent===${
          JSON.stringify(item.workflow.name)
        });const button=card?.querySelector('button[title="Execute"]');if(!button)return null;button.scrollIntoView({block:'center'});const rect=button.getBoundingClientRect();return{x:rect.x+rect.width/2,y:rect.y+rect.height/2}})()`,
      ));
      if (!button) {
        throw Error("Real Automa workflow Execute UI control missing");
      }
      item.executeControl = button;
      await browser.call(
        "Input.dispatchMouseEvent",
        { type: "mousePressed", ...button, button: "left", clickCount: 1 },
        sessionId,
      );
      await browser.call(
        "Input.dispatchMouseEvent",
        { type: "mouseReleased", ...button, button: "left", clickCount: 1 },
        sessionId,
      );
      let targetSession;
      for (let attempt = 0; attempt < 100; attempt++) {
        await delay(350);
        const target = (
          await browser.call("Target.getTargets")
        ).targetInfos.find(
          (target) => target.url === automaURL && target.type === "page",
        );
        if (target) {
          targetSession = (
            await browser.call("Target.attachToTarget", {
              targetId: target.targetId,
              flatten: true,
            })
          ).sessionId;
          item.observed = await evaluate(
            targetSession,
            `({url:location.href,input:document.querySelector('#customer')?.value,result:document.querySelector('#operation-result')?.textContent,operation:document.documentElement.dataset.operation,clickCount:document.documentElement.dataset.clickCount})`,
          );
          if (item.observed?.operation === expectedAutomaCustomer) break;
        }
      }
      if (
        item.observed?.input !== expectedAutomaCustomer ||
        item.observed.result !== `Submitted: ${expectedAutomaCustomer}` ||
        item.observed.clickCount !== "1"
      ) {
        throw Error(
          "Actual Automa graph did not produce expected native input/click effects",
        );
      }
      const screenshot = await browser.call(
        "Page.captureScreenshot",
        {},
        targetSession,
      );
      writeFileSync(
        join(output, `${id}-target.png`),
        Buffer.from(screenshot.data, "base64"),
      );
      item.result = "pass";
      item.checks = [
        "original signed CRX SHA-256, every loaded script unchanged and original identity",
        "genuine packaged example graph with authored URL/selectors/input data",
        "actual Execute control native mouse click",
        "real Automa engine with the packaged default execution settings",
        "actual target DOM input and exactly one click",
      ];
      // Read persisted genuine engine log from a fresh UI context; execution may close its popup.
      const logTarget = await browser.call("Target.createTarget", {
        url: `chrome-extension://${id}/popup.html`,
      });
      const { sessionId: logSession } = await browser.call(
        "Target.attachToTarget",
        { targetId: logTarget.targetId, flatten: true },
      );
      await delay(700);
      item.engineState = await evaluate(
        logSession,
        'chrome.storage.local.get(["logs","workflowState"])',
      );
      console.log(
        `PASS ${record.name}: actual packaged graph new-tab/forms/event-click`,
      );
    } catch (error) {
      item.error = String(error);
      item.result = "failed";
      failed = true;
      console.error(item.error);
    }
  }
  if (shouldRun("browserflow")) {
    const record = corpus.extensions.find(
      (record) => record.id === browserflowId,
    );
    if (!record) throw Error("Frozen automation record missing");
    /** @type {import("./automation-test-types.ts").AutomationItem} */
    /** @type {import("./automation-test-types.ts").AutomationItem} */
    const item = {
      id: record.id,
      name: record.name,
      version: record.version,
      packageSha256: record.packageSha256,
      result: "failed",
    };
    report.extensions.push(item);
    try {
      Object.assign(item, verifyCorpusScripts(root, record));
      const { id } = await browser.call("Extensions.loadUnpacked", {
        path: join(
          root,
          "_dist/chrome-extension-corpus/chrome-unpacked",
          record.id,
        ),
      });
      if (id !== record.id) throw Error("Original Chrome identity mismatch");
      await delay(1200);
      const url = fixtureURL + "?browserflow";
      await browser.call("Target.createTarget", { url });
      await delay(1000);
      const tab = (
        await browser.call("Target.getTargets", { filter: [{ type: "tab" }] })
      ).targetInfos.find((target) => target.url === url);
      if (!tab) throw Error("Actual authored browser tab missing");
      await browser.call("Extensions.triggerAction", {
        id,
        targetId: tab.targetId,
      });
      /** @type {{url:string,text:string,buttons:string[]}|undefined} */
      let popup;
      for (let attempt = 0; attempt < 80; attempt++) {
        await delay(250);
        const target = (
          await browser.call("Target.getTargets")
        ).targetInfos.find(
          (target) => target.url === `chrome-extension://${id}/popup.html`,
        );
        if (!target) continue;
        const { sessionId } = await browser.call("Target.attachToTarget", {
          targetId: target.targetId,
          flatten: true,
        });
        popup = await evaluate(
          sessionId,
          `({url:location.href,text:document.body.innerText,buttons:[...document.querySelectorAll('button')].map(button=>button.innerText)})`,
        );
        if (popup?.buttons?.includes("Sign in")) break;
      }
      item.actualActionUI = popup;
      if (!popup?.buttons?.includes("Sign in")) {
        throw Error(
          "Actual Browserflow toolbar action did not produce its real sign-in UI",
        );
      }
      item.result = "blocked-account";
      item.blocker =
        "The actual frozen Chrome extension exposes only Sign in before recorder/macro controls; no Browserflow account was supplied.";
      item.checks = [
        "original signed CRX SHA-256, every loaded script unchanged and original Chrome identity",
        "actual trusted toolbar action on authored page",
        "native extension popup contains Sign in with no recorder/macro controls",
      ];
      console.log(
        "BLOCKED Browserflow: actual reference requires account before primary operation",
      );
    } catch (error) {
      item.error = String(error);
      failed = true;
      console.error(item.error);
    }
  }
} finally {
  writeFileSync(join(output, "report.json"), JSON.stringify(report, null, 2));
  writeFileSync(
    join(output, "events.json"),
    JSON.stringify(browser.events, null, 2),
  );
  await browser.close();
  server.close();
  console.log(`REPORT ${join(output, "report.json")}`);
}
if (failed) process.exitCode = 1;
