#!/usr/bin/env node
// SPDX-License-Identifier: MPL-2.0
// Exercise unchanged frozen products through their own UI and real HTTP effects.
import { ChromePipe } from "./chrome-cdp-pipe.mjs";
import { verifyCorpusScripts } from "./corpus-script-integrity.mjs";
import { createHash } from "node:crypto";
import { createServer } from "node:http";
import { mkdirSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
/** @param {import("node:http").Server} server */
function portOf(server) {
  const address = server.address();
  if (!address || typeof address === "string")
    throw Error("Expected local TCP listener");
  return address.port;
}
const root = resolve(".");
const tools = join(root, "tools/chrome-extension-runtime");
/** @type {import("./network-test-types.ts").NetworkCorpus} */
const corpus = JSON.parse(
  readFileSync(join(tools, "corpus-lock.json"), "utf8"),
);
/** @type {import("./network-test-types.ts").ChromeReference} */
const reference = JSON.parse(
  readFileSync(join(tools, "chrome-reference.json"), "utf8"),
);
/** @param {string} name @param {string} fallback */
const option = (name, fallback) => {
  const index = process.argv.indexOf(name);
  return index < 0 ? fallback : (process.argv[index + 1] ?? fallback);
};
const ids = option(
  "--ids",
  [
    "pfnededegaaopdmhkdmcofjmoldfiped",
    "gcknhkkoolaabfmlnjonogaaifnjlfnp",
    "omghfjlpggmjjaagoclmmobgdodcjboh",
    "hnmpcagpplmpfojmgmnngilcnanddlhb",
    "mdnleldcmiljblolnjhpnblkcekpdkpa",
    "djflhoibgkdhkhhcedjiklpkjnoahfmg",
    "ddkjiahejlhfcafbddmgiahcphecmpfh",
  ].join(","),
).split(",");
const chrome = join(
  root,
  "_dist/chrome-reference/extracted/opt/google/chrome/chrome",
);
/** @param {Uint8Array} bytes */
const digest = (bytes) => createHash("sha256").update(bytes).digest("hex");
if (digest(readFileSync(chrome)) !== reference.binary.sha256)
  throw Error("Frozen reference binary differs");
const outputRoot = join(root, "_dist/chrome-runtime-tests");
mkdirSync(outputRoot, { recursive: true });
const output = mkdtempSync(join(outputRoot, "corpus-network-reference-"));
/** @type {import("./network-test-types.ts").TrafficRecord[]} */
const traffic = [];
const proxy = createServer((request, response) => {
  /** @type {import("./network-test-types.ts").TrafficRecord} */
  const record = {
    role: "proxy",
    url: request.url ?? "",
    headers: request.headers,
  };
  traffic.push(record);
  response.writeHead(200, {
    "Content-Type": "application/json",
    "Access-Control-Allow-Origin": "*",
    "Cache-Control": "no-store",
  });
  response.end(JSON.stringify(record));
});
const origin = createServer((request, response) => {
  /** @type {import("./network-test-types.ts").TrafficRecord} */
  const record = {
    role: "origin",
    url: request.url ?? "",
    headers: request.headers,
  };
  traffic.push(record);
  const script = (request.url ?? "").startsWith("/xpopup/xpopup.js");
  const page = request.url === "/" || (request.url ?? "").startsWith("/page?");
  response.writeHead(200, {
    "Content-Type": script
      ? "application/javascript"
      : page
        ? "text/html"
        : "application/json",
    "Access-Control-Allow-Origin": "*",
    "Cache-Control": "no-store",
  });
  response.end(
    script
      ? "window.__corpusAdLoaded=true;"
      : page
        ? `<!doctype html><title>Controlled network corpus</title><h1>Controlled authored network operation</h1><p id=ready>Local HTTP evidence</p><script src="http://127.0.0.1:${portOf(origin)}/xpopup/xpopup.js"></script>`
        : JSON.stringify(record),
  );
});
await Promise.all(
  [proxy, origin].map(
    (server) =>
      new Promise((done) =>
        server.listen(0, "127.0.0.1", () => done(undefined)),
      ),
  ),
);
const fixture = `http://127.0.0.1:${portOf(origin)}/`;
const headed = process.argv.includes("--headed");
const browser = new ChromePipe(
  chrome,
  output,
  [
    "--host-resolver-rules=MAP pagead2.googlesyndication.com 127.0.0.1, MAP bypass.floorp-proxy.test 127.0.0.1, MAP routed.floorp-proxy.test 127.0.0.1",
  ],
  { headed },
);
/** @type {import("./network-test-types.ts").NetworkReport} */
const report = {
  stage: "actual-package-primary-operation",
  browserMode: headed ? "headed" : "headless",
  reference,
  fixture,
  proxyPort: portOf(proxy),
  primaryOperationPassRate: null,
  extensions: [],
  traffic,
  output,
};
const save = () =>
  writeFileSync(join(output, "report.json"), JSON.stringify(report, null, 2));
/** @param {number} ms */
const delay = (ms) => new Promise((done) => setTimeout(done, ms));
/** @template [T=unknown]
 * @param {string} session
 * @param {string} expression
 * @returns {Promise<T>} */
async function evaluate(session, expression) {
  const result = await browser.call(
    "Runtime.evaluate",
    { expression, awaitPromise: true, returnByValue: true },
    session,
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
async function until(read, label, timeout = 15000) {
  const deadline = Date.now() + timeout;
  while (Date.now() < deadline) {
    const value = await read();
    if (value) return value;
    await delay(100);
  }
  throw Error("Timed out waiting for " + label);
}
const sessionTargets = new Map();
/** @param {string} targetId */
async function attach(targetId) {
  const { sessionId } = await browser.call("Target.attachToTarget", {
    targetId,
    flatten: true,
  });
  await browser.call("Runtime.enable", {}, sessionId);
  sessionTargets.set(sessionId, targetId);
  return sessionId;
}
/** @param {string} url */
async function open(url) {
  const { targetId } = await browser.call("Target.createTarget", { url });
  const session = await attach(targetId);
  await until(
    async () =>
      await evaluate(
        session,
        `location.href.startsWith(${JSON.stringify(url.split("#")[0])})&&document.readyState==='complete'`,
      ).catch(() => false),
    url,
  );
  return { targetId, session };
}
const deepDOM = `const roots=[document];for(let i=0;i<roots.length;++i){for(const e of roots[i].querySelectorAll('*'))if(e.shadowRoot)roots.push(e.shadowRoot);}const query=selector=>roots.flatMap(root=>[...root.querySelectorAll(selector)]);`;
/** @param {string} session @returns {Promise<import("./network-test-types.ts").UISnapshot>} */
function snapshot(session) {
  return evaluate(
    session,
    `(()=>{${deepDOM}return {url:location.href,ready:document.readyState,text:roots.flatMap(root=>[...root.children].filter(e=>e.tagName!=='STYLE').map(e=>e.innerText||'')).join('\\n').slice(0,16000),controls:query('button,a,input,select,textarea,[data-level],[role=button],[data-home-control]').slice(0,160).map(e=>({tag:e.tagName,id:e.id,name:e.name,type:e.type,value:e.value,text:e.innerText?.slice(0,180),model:e.getAttribute('ng-model'),data:{...e.dataset}})),components:query('*').filter(e=>e.shadowRoot).map(e=>({tag:e.tagName,attributes:[...e.attributes].map(a=>[a.name,a.value])}))};})()`,
  );
}
/** @param {string} session @param {string} selector @param {string} [text] */
async function click(session, selector, text) {
  await browser.call("Target.activateTarget", {
    targetId: sessionTargets.get(session),
  });
  await browser.call("Page.bringToFront", {}, session);
  /** @type {import("./network-test-types.ts").Point} */
  const point = await evaluate(
    session,
    `(()=>{${deepDOM}let e=query(${JSON.stringify(selector)}).find(e=>${text === undefined ? "true" : `e.textContent.trim()===${JSON.stringify(text)}`});if(!e)throw Error('Missing UI control '+${JSON.stringify(selector)});if(!e.getBoundingClientRect().width&&e.labels)e=[...e.labels].find(label=>label.getBoundingClientRect().width)||e;e.scrollIntoView({block:'nearest'});const r=e.getBoundingClientRect();if(!r.width||!r.height)throw Error('Hidden actual UI control');const x=r.x+r.width/2,y=r.y+r.height/2;const hit=document.elementFromPoint(x,y);if(hit!==e&&!e.contains(hit))throw Error('Actual UI control is occluded by '+hit?.outerHTML.slice(0,500));return{x,y}})()`,
  );
  for (const [type, buttons] of [
    ["mousePressed", 1],
    ["mouseReleased", 0],
  ]) {
    try {
      await browser.call(
        "Input.dispatchMouseEvent",
        { type, ...point, button: "left", buttons, clickCount: 1 },
        session,
      );
    } catch (error) {
      if (
        type !== "mouseReleased" ||
        !/target.*closed|session.*closed|no session/i.test(String(error))
      )
        throw error;
      // Closing the real popup is a product effect, subsequently verified by HTTP.
    }
  }
  await delay(100);
  return { clicked: true, backend: "native-cdp-mouse" };
}
/** @param {string} session @param {string} selector @param {string} value */
function fill(session, selector, value) {
  return evaluate(
    session,
    `(()=>{${deepDOM}const e=query(${JSON.stringify(selector)})[0];if(!e)throw Error('Missing UI field '+${JSON.stringify(selector)});e.focus();e.value=${JSON.stringify(value)};e.dispatchEvent(new Event('input',{bubbles:true}));e.dispatchEvent(new Event('change',{bubbles:true}));e.dispatchEvent(new Event('blur',{bubbles:true}));return e.value;})()`,
  );
}
/** @param {string} id @param {import("./network-test-types.ts").PageSession} page */
async function action(id, page) {
  await browser.call("Target.activateTarget", { targetId: page.targetId });
  const native = await until(
    async () =>
      (
        await browser.call("Target.getTargets", { filter: [{ type: "tab" }] })
      ).targetInfos.find((info) => info.url === fixture),
    "authored native tab",
  );
  await browser.call("Extensions.triggerAction", {
    id,
    targetId: native.targetId,
  });
  const target = await until(
    async () =>
      (await browser.call("Target.getTargets")).targetInfos.find(
        (info) =>
          info.type === "page" &&
          info.url.startsWith(`chrome-extension://${id}/`) &&
          /popup/.test(info.url),
      ),
    "real extension popup",
  );
  const session = await attach(target.targetId);
  await delay(350);
  return { targetId: target.targetId, session };
}
/** @param {import("./network-test-types.ts").PageSession} page @param {import("./network-test-types.ts").NetworkRecord} record */
async function routing(page, record) {
  record.routed = await until(async () => {
    try {
      /** @type {import("./network-test-types.ts").NetworkFetch} */
      const result = await evaluate(
        page.session,
        "fetch('http://corpus-proxy.invalid/proxied',{signal:AbortSignal.timeout(4000)}).then(r=>r.json())",
      );
      return result.role === "proxy" && result;
    } catch {
      return false;
    }
  }, "actual product proxy propagation and HTTP route");
  record.bypassed = await evaluate(
    page.session,
    `fetch('http://bypass.floorp-proxy.test:${portOf(origin)}/bypassed').then(r=>r.json())`,
  );
  if (record.bypassed?.role !== "origin")
    throw Error("Actual proxy product did not honor configured bypass");
  record.checks.push(
    "Product profile routes actual HTTP to controlled proxy and bypasses authored host",
  );
}
let keepAliveTargetId;
try {
  report.version = await browser.call("Browser.getVersion");
  keepAliveTargetId = (
    await browser.call("Target.createTarget", {
      url: "about:blank",
      background: true,
    })
  ).targetId;
  if (report.version.product !== `Chrome/${reference.browserVersion}`)
    throw Error("Frozen Chrome version differs");
  for (const id of ids) {
    const extension = corpus.extensions.find((item) => item.id === id);
    if (!extension) throw Error("Unselected corpus identity");
    /** @type {import("./network-test-types.ts").NetworkRecord} */
    const record = {
      id,
      name: extension.name,
      version: extension.version,
      packageSha256: extension.packageSha256,
      primaryOperation: extension.primaryOperation,
      primaryOperationResult: "not-run",
      checks: [],
      trace: [],
    };
    report.extensions.push(record);
    let loaded = false;
    /** @type {import("./network-test-types.ts").PageSession | undefined} */
    let baselinePage;
    try {
      if (extension.name === "uBlock Origin Lite") {
        baselinePage = await open(fixture);
        record.baselineAdLoaded = await until(() => {
          if (!baselinePage) throw Error("Baseline page missing");
          return evaluate(baselinePage.session, "!!window.__corpusAdLoaded");
        }, "authored script without product");
      }
      if (
        digest(
          readFileSync(
            join(root, "_dist/chrome-extension-corpus/packages", id + ".crx"),
          ),
        ) !== extension.packageSha256
      )
        throw Error("Immutable package differs");
      record.sourceIntegrity = verifyCorpusScripts(root, extension);
      const { id: installed } = await browser.call("Extensions.loadUnpacked", {
        path: join(root, "_dist/chrome-extension-corpus/chrome-unpacked", id),
      });
      loaded = true;
      if (installed !== id) throw Error("Original product identity differs");
      await delay(600);
      for (const target of (await browser.call("Target.getTargets"))
        .targetInfos) {
        if (
          target.type === "page" &&
          target.url.startsWith(`chrome-extension://${id}/`)
        )
          await browser.call("Target.closeTarget", {
            targetId: target.targetId,
          });
      }
      const page = baselinePage || (await open(fixture));
      if (extension.name === "ZeroOmega") {
        const options = await open(
          `chrome-extension://${id}/options.html#!/profile/proxy`,
        );
        await delay(700);
        record.trace.push(await snapshot(options.session));
        const skipPresent = /** @type {boolean} */ (
          await evaluate(
            options.session,
            `!!document.querySelector('.modal-footer button[ng-click*=skip]')?.getBoundingClientRect().width`,
          )
        );
        if (skipPresent) {
          await click(options.session, ".modal-footer button[ng-click*=skip]");
          await until(
            () =>
              evaluate(
                options.session,
                `!document.querySelector('.modal-backdrop')`,
              ),
            "actual product guide dismissal",
          );
        }
        await click(options.session, ".nav-profile a", "proxy");
        await until(
          () =>
            evaluate(
              options.session,
              `!!document.querySelector('input[ng-model="proxyEditors[scheme].host"]')`,
            ),
          "ZeroOmega proxy form",
        );
        await fill(
          options.session,
          'input[ng-model="proxyEditors[scheme].host"]',
          "127.0.0.1",
        );
        await fill(
          options.session,
          'input[ng-model="proxyEditors[scheme].port"]',
          String(portOf(proxy)),
        );
        await fill(
          options.session,
          'textarea[ng-model="bypassList"]',
          "bypass.floorp-proxy.test",
        );
        await click(options.session, 'a[ng-click="applyOptions()"]');
        await delay(500);
        record.trace.push(await snapshot(options.session));
        const popup = await action(id, page);
        record.trace.push(await snapshot(popup.session));
        await click(popup.session, "a", "proxy");
        await delay(500);
        record.proxyAfterSelection = await evaluate(
          options.session,
          "chrome.proxy.settings.get({})",
        );
        await routing(page, record);
        const restore = await action(id, page);
        await click(restore.session, "#js-direct");
        record.restored = await until(async () => {
          /** @type {import("./network-test-types.ts").NetworkFetch} */
          const value = await evaluate(
            page.session,
            `fetch('http://routed.floorp-proxy.test:${portOf(origin)}/restored',{signal:AbortSignal.timeout(3000)}).then(r=>r.json()).catch(error=>({failed:true,error:String(error)}))`,
          );
          return value?.role === "origin" && value;
        }, "actual product Direct routing restoration");
        record.checks.push("Product Direct UI stops controlled proxy routing");
      } else if (extension.name === "FoxyProxy") {
        const options = await open(
          `chrome-extension://${id}/content/options.html`,
        );
        await click(options.session, "#nav4");
        await click(options.session, '.proxy-head button[data-i18n="add"]');
        for (const [field, value] of Object.entries({
          title: "Corpus local proxy",
          hostname: "127.0.0.1",
          port: String(portOf(proxy)),
          type: "http",
        }))
          await fill(options.session, `.proxy-div [data-id="${field}"]`, value);
        // The product-wide passthrough UI generates its own actual bypass PAC.
        await fill(
          options.session,
          "#passthrough",
          "bypass.floorp-proxy.test",
        ).catch(() => {});
        await click(options.session, '.proxy-section button[type="submit"]');
        await delay(600);
        record.trace.push(await snapshot(options.session));
        const popup = await action(id, page);
        await click(popup.session, `input[value="127.0.0.1:${portOf(proxy)}"]`);
        await delay(600);
        await routing(page, record);
        const restore = await action(id, page);
        await click(restore.session, 'input[value="disable"]');
        record.restoredConfig = await until(async () => {
          /** @type {import("./network-test-types.ts").ProxyReadback} */
          const value = await evaluate(
            restore.session,
            "chrome.proxy.settings.get({})",
          );
          return value.value.mode === "system" && value;
        }, "actual product Disable system configuration");
        record.restored = await evaluate(
          page.session,
          `fetch('http://routed.floorp-proxy.test:${portOf(origin)}/restored',{signal:AbortSignal.timeout(3000)}).then(r=>r.json()).catch(error=>({failed:true,error:String(error)}))`,
        );
        if (record.restored?.role === "proxy")
          throw Error("Product Disable UI left controlled proxy route active");
        record.checks.push(
          "Product Disable UI selects system configuration and stops controlled proxy routing",
        );
      } else if (extension.name === "User Agent Switcher") {
        const popup = await action(id, page);
        record.trace.push(await snapshot(popup.session));
        await click(popup.session, "a", "Firefox");
        await click(popup.session, "a", "Windows Firefox 33");
        await delay(700);
        record.echo = await evaluate(
          page.session,
          `fetch(${JSON.stringify(fixture + "echo?ua=selected")}).then(r=>r.json())`,
        );
        if (!/Firefox\/33/.test(record.echo?.headers?.["user-agent"] ?? ""))
          throw Error(
            "Selected product UI did not change actual request User-Agent",
          );
        record.checks.push(
          "Selected product Firefox profile changes actual HTTP User-Agent",
        );
      } else if (extension.name === "uBlock Origin Lite") {
        const start = traffic.length;
        await browser.call("Page.reload", {}, page.session);
        await delay(1600);
        record.blocked = !(await evaluate(
          page.session,
          "!!window.__corpusAdLoaded",
        ));
        if (
          !record.blocked ||
          traffic
            .slice(start)
            .some((item) => item.url.startsWith("/xpopup/xpopup.js"))
        )
          throw Error(
            "Actual static product rules did not block authored ad request",
          );
        const popup = await action(id, page);
        record.trace.push(await snapshot(popup.session));
        await click(popup.session, '[data-level="0"]');
        await delay(1000);
        record.trace.push(await snapshot(popup.session));
        record.productDNR = await evaluate(
          popup.session,
          "chrome.declarativeNetRequest.getDynamicRules().then(rules=>({count:rules.length,allowRules:rules.filter(rule=>rule.action.type==='allowAllRequests')}))",
        );
        await until(
          () => evaluate(page.session, "!!window.__corpusAdLoaded"),
          "actual request allowed by product No filtering UI and its own reload",
        );
        record.checks.push(
          "Actual product static rules block controlled ad script; No filtering UI allows it after reload",
        );
      } else {
        const popup = await action(id, page);
        await delay(1500);
        record.trace.push(await snapshot(popup.session));
        if (extension.name === "Requestly") {
          await click(popup.session, "button", "Modify headers");
          const target = await until(
            async () =>
              (await browser.call("Target.getTargets")).targetInfos.find(
                (info) =>
                  info.type === "page" &&
                  info.url.startsWith("https://app.requestly.io/"),
              ),
            "actual Requestly rule editor tab",
          );
          const session = await attach(target.targetId);
          await delay(15000);
          const editor = await snapshot(session);
          record.trace.push(editor);
          if (
            editor.text.includes(
              "Running Requestly in automation without explicit permission is prohibited",
            )
          ) {
            record.primaryOperationResult =
              "blocked-product-automation-restriction";
            record.error =
              "Actual product explicitly requires permission for automation; no detection bypass or rule submission performed";
          } else {
            record.primaryOperationResult = "blocked-product-setup";
            record.error =
              "Actual remote Requestly rule editor has not demonstrated a controlled rule operation";
          }
        } else if (extension.name === "Windscribe") {
          await click(popup.session, "button", "Login");
          await delay(500);
          record.trace.push(await snapshot(popup.session));
          record.primaryOperationResult = "blocked-account-required";
          record.error =
            "Actual product requires VPN account login; no credentials supplied";
        } else if (extension.name === "Browsec") {
          record.primaryOperationResult = record.trace
            .at(-1)
            ?.text?.includes("Terms of Service")
            ? "blocked-terms-consent-required"
            : "failed";
          record.error =
            "Actual product requires accepting third party Terms of Service/Privacy Policy before VPN use; no agreement accepted on the user's behalf";
        } else {
          record.primaryOperationResult = "blocked-product-setup";
          record.error =
            "Actual product primary operation has not been demonstrated";
        }
      }
      if (record.primaryOperationResult === "not-run")
        record.primaryOperationResult = "passed";
    } catch (error) {
      record.primaryOperationResult = "failed";
      record.error = String(error);
      record.targets = (
        await browser
          .call("Target.getTargets")
          .catch(() => ({ targetInfos: [] }))
      ).targetInfos.filter((info) => info.url.includes(id));
    } finally {
      if (loaded)
        await browser
          .call("Extensions.uninstall", { id })
          .catch((error) => (record.uninstallError = String(error)));
      for (const target of (
        await browser
          .call("Target.getTargets")
          .catch(() => ({ targetInfos: [] }))
      ).targetInfos) {
        if (target.type === "page" && target.targetId !== keepAliveTargetId)
          await browser
            .call("Target.closeTarget", { targetId: target.targetId })
            .catch(() => {});
      }
      save();
      console.log(
        `${record.name}: ${record.primaryOperationResult} ${record.error || ""}`,
      );
    }
  }
} finally {
  await browser.close();
  for (const server of [proxy, origin]) {
    server.closeAllConnections();
    server.close();
  }
  save();
}
console.log(join(output, "report.json"));
if (
  !report.extensions.length ||
  report.extensions.some((record) => record.primaryOperationResult !== "passed")
)
  process.exitCode = 1;
