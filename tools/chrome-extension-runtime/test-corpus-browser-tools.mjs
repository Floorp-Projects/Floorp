#!/usr/bin/env node
// SPDX-License-Identifier: MPL-2.0
// Real frozen tab/session/cookie/dashboard operations, driven through native input.
import { ChromePipe } from "./chrome-cdp-pipe.mjs";
import { verifyCorpusScripts } from "./corpus-script-integrity.mjs";
import { createHash } from "node:crypto";
import { execFileSync } from "node:child_process";
import { createServer } from "node:http";
import { mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
const root = resolve(dirname(fileURLToPath(import.meta.url)), "../..");
const tools = join(root, "tools/chrome-extension-runtime");
/** @param {string} key @param {string} fallback */
const option = (key, fallback) => {
  const index = process.argv.indexOf(key);
  return index < 0 ? fallback : process.argv[index + 1] ?? fallback;
};
const id = option("--id", "chphlpgkkbolifaimnlloiipkdnihall");
const corpus =
  /** @type {import("./corpus-types.ts").FrozenCorpus} */ (JSON.parse(
    readFileSync(join(tools, "corpus-lock.json"), "utf8"),
  ));
const record = corpus.extensions.find((e) => e.id === id);
if (!record) throw Error("Frozen extension missing");
const reference = JSON.parse(
  readFileSync(join(tools, "chrome-reference.json"), "utf8"),
);
/** @param {string | Uint8Array} bytes */
const hash = (bytes) => createHash("sha256").update(bytes).digest("hex");
const chrome = join(
  root,
  "_dist/chrome-reference/extracted/opt/google/chrome/chrome",
);
if (hash(readFileSync(chrome)) !== reference.binary.sha256) {
  throw Error("Reference binary hash mismatch");
}
if (
  hash(
    readFileSync(
      join(root, "_dist/chrome-extension-corpus/packages", id + ".crx"),
    ),
  ) !== record.packageSha256
) throw Error("Frozen signed CRX digest mismatch");
const output = mkdtempSync(
  join(root, "_dist/chrome-runtime-tests/corpus-browser-tools-reference-"),
);
/** @type {import("./browser-tools-test-types.ts").ReferenceBrowserToolsReport} */
const report = {
  stage: "actual-package-primary-operation",
  reference: {
    ...reference,
    comparisonScope:
      "Frozen real extension primary operations on authored pages using unchanged signed vendor scripts and native UI input.",
  },
  output,
  fixture: { url: "" },
  displayMode: process.argv.includes("--headed") ? "headed" : "headless",
  extension: {
    id,
    name: record.name,
    version: record.version,
    packageSha256: record.packageSha256,
    primaryOperation: record.primaryOperation,
    ...verifyCorpusScripts(root, record),
    primaryOperationResult: "not-run",
    checks: [],
    snapshots: [],
  },
};
const save = () =>
  writeFileSync(join(output, "report.json"), JSON.stringify(report, null, 2));
const authored =
  "<!doctype html><meta charset=utf-8><title>Authored saved session</title><h1>Authored browser operation fixture</h1><p id=probe>Controlled cookie and saved-tab state.</p>";
const server = createServer((req, res) => {
  res.writeHead(200, {
    "Content-Type": "text/html",
    "Cache-Control": "no-store",
  });
  res.end(
    authored.replace(
      "Authored saved session",
      "Authored saved session " + req.url,
    ),
  );
});
await new Promise((resolve) =>
  server.listen(0, "127.0.0.1", () => resolve(undefined))
);
const fixture = `http://127.0.0.1:${
  /** @type {import("node:net").AddressInfo} */ (server.address()).port
}/`;
report.fixture = { url: fixture, templateSha256: hash(authored) };
const browser = new ChromePipe(chrome, output, [], {
  headed: process.argv.includes("--headed"),
});
browser.events = [];
/** @param {number} ms */
const delay = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
const targetsBySession = new Map();
/** @template [T=unknown] @param {string} sessionId @param {string} expression @returns {Promise<T>} */
async function evaluate(sessionId, expression) {
  const response = await browser.call("Runtime.evaluate", {
    expression,
    returnByValue: true,
    awaitPromise: true,
  }, sessionId);
  if (response.exceptionDetails) {
    throw Error(JSON.stringify(response.exceptionDetails));
  }
  return /** @type {T} */ (response.result.value);
}
/** @template T @param {() => T | Promise<T>} read @param {string} label @param {number} [timeout] */
async function until(read, label, timeout = 20000) {
  const deadline = Date.now() + timeout;
  while (Date.now() < deadline) {
    const value = await read();
    if (value) return value;
    await delay(150);
  }
  throw Error("Timed out waiting for " + label);
}
/** @param {string} targetId */
async function attach(targetId) {
  const sessionId = (await browser.call("Target.attachToTarget", {
    targetId,
    flatten: true,
  })).sessionId;
  targetsBySession.set(sessionId, targetId);
  await browser.call("Runtime.enable", {}, sessionId);
  await browser.call("Log.enable", {}, sessionId);
  await browser.call("Page.enable", {}, sessionId);
  return sessionId;
}
/** @param {string} url */
async function open(url) {
  const { targetId } = await browser.call("Target.createTarget", { url });
  const sessionId = await attach(targetId);
  await until(
    () => evaluate(sessionId, "document.readyState==='complete'"),
    "document readiness",
  );
  return { targetId, sessionId };
}
const snapshotExpression =
  `({url:location.href,title:document.title,text:document.body?.innerText?.slice(0,18000),controls:[...document.querySelectorAll('button,a,input,textarea,select,[role=button],[class*=button]')].filter(e=>e.getBoundingClientRect().width&&e.getBoundingClientRect().height).slice(0,100).map(e=>({tag:e.tagName,id:e.id,class:e.className,type:e.type,name:e.name,value:e.value,text:(e.innerText||e.getAttribute('aria-label')||e.getAttribute('title')||'').slice(0,180)}))})`;
/** @param {string} sessionId @param {string} label */
async function snapshot(sessionId, label) {
  const value =
    /** @type {import("./browser-tools-test-types.ts").OperationSnapshot} */ (await evaluate(
      sessionId,
      snapshotExpression,
    ));
  report.extension.snapshots.push({ label, ...value });
  save();
  return value;
}
/** @param {string} sessionId @param {string} selector @param {string} [text] */
async function click(sessionId, selector, text) {
  await browser.call("Target.activateTarget", {
    targetId: targetsBySession.get(sessionId),
  });
  const point = await until(() =>
    evaluate(
      sessionId,
      `(()=>{const e=[...document.querySelectorAll(${
        JSON.stringify(selector)
      })].reverse().find(e=>(!${
        JSON.stringify(text || "")
      }||e.textContent.trim()===${
        JSON.stringify(text || "")
      })&&e.getBoundingClientRect().width&&e.getBoundingClientRect().height);if(!e)return null;e.scrollIntoView({block:'center'});const r=e.getBoundingClientRect();const x=r.x+r.width/2,y=r.y+r.height/2;const hit=document.elementFromPoint(x,y);return hit&&(hit===e||e.contains(hit))?{x,y}:null})()`,
    ), "clickable native UI control " + selector + " " + (text || ""));
  if (!point) {
    throw Error(
      "Visible native UI control missing: " + selector + " " + (text || ""),
    );
  }
  for (const type of ["mousePressed", "mouseReleased"]) {
    await browser.call("Input.dispatchMouseEvent", {
      type,
      ...point,
      button: "left",
      clickCount: 1,
    }, sessionId);
  }
}
/** @param {string} sessionId @param {string} selector @param {string} text */
async function type(sessionId, selector, text) {
  await click(sessionId, selector);
  await browser.call("Input.insertText", { text }, sessionId);
}
/** @param {string} sessionId @param {string} selector @param {string} text */
async function replace(sessionId, selector, text) {
  await click(sessionId, selector);
  for (const type of ["keyDown", "keyUp"]) {
    await browser.call("Input.dispatchKeyEvent", {
      type,
      key: "a",
      code: "KeyA",
      modifiers: 2,
      windowsVirtualKeyCode: 65,
    }, sessionId);
  }
  await browser.call("Input.insertText", { text }, sessionId);
}
/** @param {{targetId:string, sessionId:string}} page */
async function nativeAction(page) {
  await browser.call("Target.activateTarget", { targetId: page.targetId });
  if (process.argv.includes("--headed")) {
    const windows = execFileSync("xdotool", [
      "search",
      "--onlyvisible",
      "--pid",
      String(browser.process.pid),
    ], { encoding: "utf8" }).trim().split(/\s+/);
    const window = windows.find((window) =>
      execFileSync("xdotool", ["getwindowname", window], { encoding: "utf8" })
        .includes("Authored saved session")
    );
    if (!window) throw Error("Owned native Chrome window missing");
    execFileSync("xdotool", ["windowfocus", window]);
    await delay(150);
  }
  const url = await evaluate(page.sessionId, "location.href");
  const tab = await until(
    async () =>
      (await browser.call("Target.getTargets", { filter: [{ type: "tab" }] }))
        .targetInfos.find((e) => e.url === url),
    "native tab target",
  );
  await browser.call("Extensions.triggerAction", {
    id,
    targetId: tab.targetId,
  });
}
/** @param {RegExp} pattern */
async function extensionPage(pattern) {
  const target = await until(
    async () =>
      (await browser.call("Target.getTargets")).targetInfos.find((e) =>
        e.type === "page" && e.url.startsWith(`chrome-extension://${id}/`) &&
        pattern.test(e.url)
      ),
    "actual extension page",
  );
  const sessionId = await attach(target.targetId);
  await until(
    () => evaluate(sessionId, "document.readyState==='complete'"),
    "extension document",
  );
  await delay(700);
  return { targetId: target.targetId, sessionId };
}
try {
  report.version = await browser.call("Browser.getVersion");
  const loaded = await browser.call("Extensions.loadUnpacked", {
    path: join(root, "_dist/chrome-extension-corpus/chrome-unpacked", id),
  });
  if (loaded.id !== id) throw Error("Original Chrome identity mismatch");
  if (record.name === "OneTab") {
    await until(
      async () =>
        (await browser.call("Target.getTargets")).targetInfos.some((e) =>
          e.url.startsWith("https://www.one-tab.com/help")
        ),
      "OneTab native first-install welcome action",
    );
    await delay(700);
  }
  const first = await open(fixture + "first");
  const second = await open(fixture + "second");
  if (record.name === "OneTab") {
    const ui = await open(`chrome-extension://${id}/onetab.html`);
    await delay(1000);
    const initial = await snapshot(ui.sessionId, "actual-OneTab-view");
    if (initial.text.includes("Continue")) {
      await click(ui.sessionId, "button,a,div,span", "Continue");
      await delay(500);
      const granted = () =>
        evaluate(
          ui.sessionId,
          "chrome.permissions.contains({permissions:['tabGroups']})",
        );
      if (!await granted()) {
        if (!process.argv.includes("--headed")) {
          throw Error(
            "OneTab requires its real native tabGroups permission prompt; use --headed",
          );
        }
        const focus = execFileSync("xdotool", ["getwindowfocus"], {
          encoding: "utf8",
        }).trim();
        const pid = Number(
          execFileSync("xdotool", ["getwindowpid", focus], { encoding: "utf8" })
            .trim(),
        );
        if (pid !== browser.process.pid) {
          throw Error(
            "Native permission dialog lost this owned Chrome process focus",
          );
        }
        execFileSync("xdotool", ["key", "Tab", "Return"]);
        await until(granted, "actual tabGroups consent prompt approval");
        report.extension.checks.push(
          "Accepted the real OneTab-requested native tabGroups permission dialog in the owned test browser",
        );
      }
      await delay(500);
      await snapshot(ui.sessionId, "after-native-consent");
    }
    await click(
      ui.sessionId,
      "button,a,div,span",
      "Store all open windows here",
    );
    await delay(500);
    const tabsGranted = () =>
      evaluate(
        ui.sessionId,
        "chrome.permissions.contains({permissions:['tabGroups']})",
      );
    if (!await tabsGranted()) {
      if (!process.argv.includes("--headed")) {
        throw Error("Native optional tabGroups consent requires headed Chrome");
      }
      const focus = execFileSync("xdotool", ["getwindowfocus"], {
        encoding: "utf8",
      }).trim();
      if (
        Number(
          execFileSync("xdotool", ["getwindowpid", focus], { encoding: "utf8" })
            .trim(),
        ) !== browser.process.pid
      ) throw Error("Optional permission dialog is outside owned Chrome");
      execFileSync("import", [
        "-window",
        "root",
        join(output, "native-tabgroups-consent.png"),
      ]);
      execFileSync("xdotool", ["key", "Tab", "Return"]);
      await until(
        tabsGranted,
        "native tabGroups approval from real Store all UI",
      );
      report.extension.checks.push(
        "Approved the real optional tabGroups request from the product Store all button",
      );
    }
    await until(
      () =>
        evaluate(
          ui.sessionId,
          `document.body.innerText.includes('Authored saved session /first')&&document.body.innerText.includes('Authored saved session /second')`,
        ),
      "saved authored session",
    );
    await snapshot(ui.sessionId, "saved-session");
    const current = (await browser.call("Target.getTargets")).targetInfos;
    if (
      current.some((e) =>
        [fixture + "first", fixture + "second"].includes(e.url)
      )
    ) throw Error("Authored tabs were not actually closed by OneTab");
    report.extension.checks.push(
      "Real OneTab Store all open windows UI saved and closed both authored tabs",
    );
    report.extension.restoreCandidates = await evaluate(
      ui.sessionId,
      `([...document.querySelectorAll('a,button,div,span')].filter(e=>e.textContent.trim()==='Restore all').map(e=>({html:e.outerHTML.slice(0,700),rect:JSON.stringify(e.getBoundingClientRect().toJSON())})))`,
    );
    await click(ui.sessionId, "a,button,div,span", "Restore all");
    await delay(700);
    const restore = await snapshot(ui.sessionId, "after-restore-click");
    if (restore.text.includes("After restoring tabs:")) {
      await click(
        ui.sessionId,
        "button,a,div,span",
        "Remove them from your list",
      );
      await click(ui.sessionId, "button,a,div,span", "Save choice");
      report.extension.checks.push(
        "Selected the product's first-use restore behavior through its actual UI",
      );
    }
    await until(async () => {
      const list = (await browser.call("Target.getTargets")).targetInfos;
      return ["first", "second"].every((path) =>
        list.some((e) => e.url === fixture + path)
      );
    }, "actual restored authored tabs");
    report.extension.checks.push(
      "Actual Restore all UI recreated both authored tabs",
    );
    report.extension.primaryOperationResult = "passed";
  } else if (record.name === "Momentum") {
    const ui = await open(`chrome-extension://${id}/index.html`);
    await until(
      () =>
        evaluate(
          ui.sessionId,
          "Boolean(document.querySelector('#name')?.getBoundingClientRect().width)",
        ),
      "Momentum's actual name onboarding",
      45000,
    ).catch(async (error) => {
      await snapshot(ui.sessionId, "onboarding-not-ready");
      throw error;
    });
    await snapshot(ui.sessionId, "advertised-view");
    await type(ui.sessionId, "#name", "Authored User");
    await click(ui.sessionId, "button", "Continue");
    await until(
      () => evaluate(ui.sessionId, "!document.querySelector('#name')"),
      "Momentum name submission",
    );
    await snapshot(ui.sessionId, "after-name-setting");
    await click(ui.sessionId, "button,a,div,span", "Stay logged out");
    await until(
      () =>
        evaluate(
          ui.sessionId,
          "document.body.innerText.includes('Authored User')&&!document.body.innerText.includes('How would you like to log in')",
        ),
      "Momentum's advertised guest dashboard",
    );
    await snapshot(ui.sessionId, "guest-dashboard");
    const previousLoader =
      (await browser.call("Page.getFrameTree", {}, ui.sessionId)).frameTree
        .frame.loaderId;
    await browser.call("Page.reload", {}, ui.sessionId);
    await until(
      async () =>
        (await browser.call("Page.getFrameTree", {}, ui.sessionId)).frameTree
          .frame.loaderId !== previousLoader,
      "Momentum's distinct reloaded document",
    );
    await until(
      () =>
        evaluate(
          ui.sessionId,
          "document.readyState==='complete'&&document.body?.innerText?.includes('Authored User')",
        ),
      "Momentum reloaded onboarding",
      45000,
    );
    const after = await snapshot(ui.sessionId, "after-native-reload");
    if (!after.text.includes("Authored User")) {
      throw Error(
        "Stored authored name not yet visible after reload; inspect onboarding evidence",
      );
    }
    report.extension.checks.push(
      "Actual dashboard UI persisted authored setting across native reload",
    );
    report.extension.primaryOperationResult = "passed";
  } else if (record.name === "Cookie Editor") {
    await nativeAction(first);
    const ui = await extensionPage(/cookie-list\.html/);
    const initial = await snapshot(ui.sessionId, "native-cookie-action");
    if (initial.text.includes("Request permission")) {
      await click(ui.sessionId, "#request-permission");
      await delay(500);
      const allowed = () =>
        evaluate(
          ui.sessionId,
          `chrome.permissions.contains({origins:[${
            JSON.stringify(fixture + "*")
          }]})`,
        );
      if (!await allowed()) {
        if (!process.argv.includes("--headed")) {
          throw Error(
            "Cookie-Editor's real host permission dialog requires headed Chrome",
          );
        }
        const focus = execFileSync("xdotool", ["getwindowfocus"], {
          encoding: "utf8",
        }).trim();
        if (
          Number(
            execFileSync("xdotool", ["getwindowpid", focus], {
              encoding: "utf8",
            }).trim(),
          ) !== browser.process.pid
        ) throw Error("Native host permission dialog outside owned Chrome");
        execFileSync("import", [
          "-window",
          "root",
          join(output, "native-host-consent.png"),
        ]);
        execFileSync("xdotool", ["key", "Tab", "Return"]);
        await until(allowed, "Cookie-Editor's actual host consent");
      }
      report.extension.checks.push(
        "Approved the product-requested actual native host permission dialog",
      );
    }
    await until(
      () =>
        evaluate(
          ui.sessionId,
          "Boolean(document.querySelector('#no-cookies')?.getBoundingClientRect().width)",
        ),
      "Cookie-Editor's permission refresh and empty-cookie UI",
    );
    await delay(450);
    await snapshot(ui.sessionId, "native-cookie-list-ready");
    await click(ui.sessionId, "#create-cookie");
    await type(ui.sessionId, "#name-create", "floorp_authored_cookie");
    await type(ui.sessionId, "#value-create", "created-through-real-ui");
    await click(ui.sessionId, "#save-create-cookie");
    await until(
      () =>
        evaluate(
          first.sessionId,
          "document.cookie.includes('floorp_authored_cookie=created-through-real-ui')",
        ),
      "actual created cookie on authored origin",
    );
    await snapshot(ui.sessionId, "created-cookie");
    await click(ui.sessionId, ".header-name", "floorp_authored_cookie");
    await click(ui.sessionId, ".input-value");
    await browser.call("Input.dispatchKeyEvent", {
      type: "keyDown",
      key: "a",
      code: "KeyA",
      modifiers: 2,
      windowsVirtualKeyCode: 65,
    }, ui.sessionId);
    await browser.call("Input.dispatchKeyEvent", {
      type: "keyUp",
      key: "a",
      code: "KeyA",
      modifiers: 2,
      windowsVirtualKeyCode: 65,
    }, ui.sessionId);
    await browser.call(
      "Input.insertText",
      { text: "edited-through-real-ui" },
      ui.sessionId,
    );
    await click(ui.sessionId, "button.save");
    await until(
      () =>
        evaluate(
          first.sessionId,
          "document.cookie.includes('floorp_authored_cookie=edited-through-real-ui')",
        ),
      "actual edited cookie on authored origin",
    );
    await snapshot(ui.sessionId, "edited-cookie");
    await click(ui.sessionId, "button.delete");
    await until(
      () =>
        evaluate(
          first.sessionId,
          "!document.cookie.includes('floorp_authored_cookie=')",
        ),
      "actual deleted cookie on authored origin",
    );
    report.extension.checks.push(
      "Actual native product UI created, edited, and deleted a cookie; independent authored page observed all three effects",
    );
    report.extension.primaryOperationResult = "passed";
  } else if (record.name === "Session Buddy") {
    await nativeAction(first);
    const ui = await extensionPage(/session-buddy\.html/);
    const initial = await snapshot(ui.sessionId, "native-session-view");
    if (initial.text.includes("Thanks for installing Session Buddy")) {
      await click(ui.sessionId, "button", "Close");
    }
    await click(ui.sessionId, "button", "Save");
    await until(
      () =>
        evaluate(
          ui.sessionId,
          "location.search.includes('s=')&&document.body.innerText.includes('Authored saved session /first')&&document.body.innerText.includes('Authored saved session /second')",
        ),
      "actual saved collection",
    );
    await until(
      () =>
        evaluate(
          ui.sessionId,
          "Boolean(document.querySelector('#set-editor-title')?.getBoundingClientRect().width)",
        ),
      "actual collection name dialog",
    );
    await replace(ui.sessionId, "#set-editor-title", "Authored Session");
    await click(ui.sessionId, "button", "OK");
    await snapshot(ui.sessionId, "actual-saved-collection");
    await browser.call("Target.closeTarget", { targetId: first.targetId });
    await browser.call("Target.closeTarget", { targetId: second.targetId });
    const priorLoader =
      (await browser.call("Page.getFrameTree", {}, ui.sessionId)).frameTree
        .frame.loaderId;
    await browser.call("Page.reload", {}, ui.sessionId);
    await until(
      async () =>
        (await browser.call("Page.getFrameTree", {}, ui.sessionId)).frameTree
          .frame.loaderId !== priorLoader,
      "distinct reloaded collection document",
    );
    await until(
      () =>
        evaluate(
          ui.sessionId,
          "document.body?.innerText.includes('Authored Session')&&document.body?.innerText.includes('Authored saved session /first')&&document.body?.innerText.includes('Authored saved session /second')",
        ),
      "saved collection after closing source tabs and reloading",
    );
    await click(ui.sessionId, "button", "Open");
    await delay(600);
    await snapshot(ui.sessionId, "actual-open-controls");
    await until(async () => {
      const restored = (await browser.call("Target.getTargets")).targetInfos;
      return ["first", "second"].every((path) =>
        restored.some((e) => e.url === fixture + path)
      );
    }, "Session Buddy restored both exact authored URLs");
    report.extension.checks.push(
      "Saved both authored tabs through actual Save UI, closed source tabs, reloaded the stored collection, then Open restored both exact URLs",
    );
    report.extension.primaryOperationResult = "passed";
  } else if (record.name === "Toby") {
    await nativeAction(first);
    const ui = await extensionPage(/sidepanel\.html/);
    const initial = await snapshot(ui.sessionId, "actual-Toby-view");
    if (initial.text.includes("Start")) {
      await click(ui.sessionId, "button,a,div,span", "Start");
    }
    await delay(700);
    const onboarding = await snapshot(ui.sessionId, "actual-Toby-onboarding");
    if (onboarding.text.includes("Sign in to save tabs to Toby")) {
      report.extension.primaryOperationResult = "blocked-account";
      report.extension.blocker =
        "The actual advertised panel requires a Toby account before saving tabs; no test account was supplied.";
      report.extension.checks.push(
        "Observed the genuine product sign-in gate; no collection operation counted as passed",
      );
    } else {throw Error(
        "Unexpected Toby onboarding; inspect actual UI before collection operations",
      );}
  } else {
    await nativeAction(first);
    const ui = await extensionPage(/./);
    await snapshot(ui.sessionId, "native-action");
    throw Error(
      "Primary operation recipe needs the recorded actual UI controls",
    );
  }
} catch (error) {
  report.extension.primaryOperationResult = "failed";
  report.extension.error = String(error);
  report.extension.errorStack = error instanceof Error
    ? error.stack
    : undefined;
  report.extension.remainingTargets = await browser.call("Target.getTargets")
    .catch(() => null);
  for (const target of report.extension.remainingTargets?.targetInfos || []) {
    if (
      target.type !== "page" ||
      !target.url.startsWith(`chrome-extension://${id}/`)
    ) continue;
    const sessionId = [...targetsBySession].find(([, value]) =>
      value === target.targetId
    )?.[0] || await attach(target.targetId).catch(() => null);
    if (sessionId) {
      await snapshot(sessionId, "actual-extension-UI-after-failure").catch(() =>
        null
      );
    }
  }
  report.extension.consoleEvidence = browser.events.filter((e) =>
    [
      "Runtime.exceptionThrown",
      "Log.entryAdded",
      "Page.javascriptDialogOpening",
    ].includes(e.method)
  );
  if (process.argv.includes("--headed")) {
    try {
      execFileSync("import", [
        "-window",
        "root",
        join(output, "native-failure.png"),
      ]);
    } catch { /* A closed native browser cannot supply a screenshot. */ }
  }
} finally {
  await browser.close();
  await new Promise((resolve) => server.close(resolve));
  save();
}
console.log(
  JSON.stringify({
    output,
    name: record.name,
    result: report.extension.primaryOperationResult,
    error: report.extension.error,
  }),
);
if (report.extension.primaryOperationResult !== "passed") process.exitCode = 1;
