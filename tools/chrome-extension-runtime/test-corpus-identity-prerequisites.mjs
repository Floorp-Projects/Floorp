#!/usr/bin/env node
// SPDX-License-Identifier: MPL-2.0
// Genuine account/vault prerequisites only: never a primary-operation pass.
import { ChromePipe } from "./chrome-cdp-pipe.mjs";
import { verifyCorpusScripts } from "./corpus-script-integrity.mjs";
import { createHash } from "node:crypto";
import { execFileSync } from "node:child_process";
import { createServer } from "node:http";
import {
  cpSync,
  mkdtempSync,
  readFileSync,
  rmSync,
  writeFileSync,
} from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
const root = resolve(dirname(fileURLToPath(import.meta.url)), "../..");
const tools = join(root, "tools/chrome-extension-runtime");
const names = [
  "1Password",
  "LastPass",
  "Proton Pass",
  "Checker Plus for Gmail",
];
const selectedId = process.argv.includes("--id")
  ? process.argv[process.argv.indexOf("--id") + 1]
  : null;
/** @type {import("./corpus-types.ts").FrozenCorpus} */
const corpus = JSON.parse(
  readFileSync(join(tools, "corpus-lock.json")).toString("utf8"),
);
const records = corpus.extensions.filter((e) =>
  names.includes(e.name) && (!selectedId || e.id === selectedId)
);
if (!records.length) {
  throw Error("Select a frozen identity prerequisite package");
}
/** @type {import("./sidepanel-test-types.ts").ReferenceMetadata} */
const reference = JSON.parse(
  readFileSync(join(tools, "chrome-reference.json")).toString("utf8"),
);
const chrome = join(
  root,
  "_dist/chrome-reference/extracted/opt/google/chrome/chrome",
);
/** @param {import("node:crypto").BinaryLike} bytes */
const sha256 = (bytes) => createHash("sha256").update(bytes).digest("hex");
if (sha256(readFileSync(chrome)) !== reference.binary.sha256) {
  throw Error("Official reference binary digest mismatch");
}
const output = mkdtempSync(
  join(
    root,
    "_dist/chrome-runtime-tests/corpus-identity-prerequisites-reference-",
  ),
);
/** @type {{wrapper:string;publicRootsRetained:string[];authoredCaImported:boolean;hostTrustModified:false;certificateValidationDisabled:false}} */
const trust = JSON.parse(
  execFileSync("python3", [
    join(tools, "private-browser-trust.py"),
    "--output",
    output,
    "--browser",
    chrome,
  ], { encoding: "utf8" }),
);
/** @type {Record<string,string>} */
const proxies = {};
for (const scheme of ["http", "https"]) {
  const value = process.env[scheme + "_proxy"] ||
    process.env[scheme.toUpperCase() + "_PROXY"] || process.env.all_proxy ||
    process.env.ALL_PROXY;
  if (!value) continue;
  const parsed = new URL(value);
  if (parsed.username || parsed.password) {
    throw Error(
      "Environment proxy credentials require an explicit native authentication fixture",
    );
  }
  if (parsed.protocol !== "http:") {
    throw Error("Expected normal HTTP environment proxy transport");
  }
  proxies[scheme] = parsed.origin;
}
const bypass = (process.env.no_proxy || process.env.NO_PROXY || "").split(",")
  .map((e) => e.trim()).filter(Boolean).flatMap((e) =>
    e.startsWith(".") ? [e.slice(1), "*" + e] : [e]
  );
bypass.push("localhost", "127.0.0.1", "[::1]");
const proxyArguments = Object.keys(proxies).length
  ? [
    `--proxy-server=${
      Object.entries(proxies).map(([scheme, url]) => scheme + "=" + url).join(
        ";",
      )
    }`,
  ]
  : ["--no-proxy-server"];
proxyArguments.push("--proxy-bypass-list=" + [...new Set(bypass)].join(";"));
const fixture =
  "<!doctype html><meta charset=utf-8><title>Authored identity prerequisite</title><p>Empty authored page for the genuine toolbar action. No account data is provided.</p>";
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
  throw Error("Authored server has no TCP address");
}
const fixtureURL = `http://127.0.0.1:${address.port}/`;
/** @type {import("./sidepanel-test-types.ts").IdentityPrerequisiteReport} */
const report = {
  stage: "actual-package-prerequisite-only",
  reference: {
    ...reference,
    comparisonScope:
      "Genuine advertised native extension UI prerequisites only; no account creation, login or primary-operation success.",
  },
  output,
  displayMode: "headed",
  nativeQualified: false,
  primaryOperationPassRate: null,
  networkEnvironment: {
    normalEnvironmentProxy: true,
    noProxyHonored: true,
    privateTrust: trust,
  },
  authoredFixture: { url: fixtureURL, sha256: sha256(fixture) },
  extensions: [],
};
const save = () =>
  writeFileSync(join(output, "report.json"), JSON.stringify(report, null, 2));
const browser = new ChromePipe(trust.wrapper, output, proxyArguments, {
  headed: true,
});
browser.events = [];
/** @param {number} ms */
/** @param {number} ms */
const delay = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
/** @template T @param {string} session @param {string} expression @returns {Promise<T>} */
async function evaluate(session, expression) {
  const result = await browser.call("Runtime.evaluate", {
    expression,
    awaitPromise: true,
    returnByValue: true,
  }, session);
  if (result.exceptionDetails) {
    throw Error(JSON.stringify(result.exceptionDetails));
  }
  return /** @type {T} */ (result.result.value);
}
/** @param {string} targetId */
async function attach(targetId) {
  const { sessionId } = await browser.call("Target.attachToTarget", {
    targetId,
    flatten: true,
  });
  await browser.call("Runtime.enable", {}, sessionId);
  await browser.call("Page.enable", {}, sessionId);
  await browser.call("Network.enable", {}, sessionId);
  return sessionId;
}
/** @template T @param {()=>Promise<T>} read @param {string} label @returns {Promise<NonNullable<T>>} */
async function until(read, label) {
  const end = Date.now() + 18000;
  while (Date.now() < end) {
    const result = await read();
    if (result) return result;
    await delay(100);
  }
  throw Error("Timed out waiting for " + label);
}
/** @param {import("./sidepanel-test-types.ts").IdentityObservation} item */
function classifyPrerequisite(item) {
  const local = item.snapshots.filter((ui) =>
    ui.url.startsWith(`chrome-extension://${item.id}/`)
  );
  const text = local.map((ui) => ui.text).join("\n");
  if (
    item.name === "LastPass" && text.includes("LOG IN") &&
    text.includes("Master password")
  ) return "observed-account-gate";
  if (
    item.name === "1Password" && /sign in/i.test(text) && /account/i.test(text)
  ) return "observed-account-gate";
  if (
    item.name === "Proton Pass" &&
    item.snapshots.some((ui) =>
      ui.url.startsWith(
        "https://account.proton.me/pass/extension-onboarding",
      ) && /sign in/i.test(ui.text || "") && /account/i.test(ui.text || "")
    )
  ) return "observed-account-gate";
  if (
    item.name === "Checker Plus for Gmail" &&
    /Add (?:your )?(?:Gmail )?account|Sign in with Google/i.test(text)
  ) return "observed-account-gate";
  if (
    item.name === "Checker Plus for Gmail" &&
    text.includes("Problem detecting account")
  ) return "observed-account-unavailable";
  if (
    item.snapshots.some((ui) => ui.text?.includes("upstream connect error"))
  ) {
    return "blocked-external-service";
  }
  if (item.name === "1Password" && text.includes("Welcome to 1Password")) {
    return "observed-product-setup";
  }
  return "observed-ui-review-required";
}
/** @param {string} session @param {string} label */
async function clickLocalOnboarding(session, label) {
  /** @type {import("./sidepanel-test-types.ts").Point|null} */
  const point = await evaluate(
    session,
    `(()=>{const all=[];function walk(root){for(const e of root.querySelectorAll('*')){if(e.shadowRoot)walk(e.shadowRoot);if(e.matches('button,a,span,j-button,[role=button]')&&e.innerText?.trim()===${
      JSON.stringify(label)
    })all.push(e)}}walk(document);const e=all.find(e=>e.getBoundingClientRect().width&&e.getBoundingClientRect().height);if(!e)return null;const r=e.getBoundingClientRect(),x=r.x+r.width/2,y=r.y+r.height/2,hit=e.getRootNode().elementFromPoint(x,y);return hit&&(e===hit||e.contains(hit))?{x,y}:null})()`,
  );
  if (!point) return false;
  for (const type of ["mousePressed", "mouseReleased"]) {
    await browser.call("Input.dispatchMouseEvent", {
      type,
      ...point,
      button: "left",
      clickCount: 1,
    }, session);
  }
  return true;
}
const snapshot =
  `(()=>{const all=[];function walk(root){for(const e of root.querySelectorAll('*')){if(e.shadowRoot)walk(e.shadowRoot);if(e.getBoundingClientRect().width&&e.getBoundingClientRect().height)all.push(e)}}walk(document);return {url:location.href,title:document.title,ready:document.readyState,text:document.body?.innerText?.slice(0,22000),controls:all.filter(e=>e.matches('button,a,input,textarea,[role=button]')).slice(0,120).map(e=>({tag:e.tagName,type:e.type,id:e.id,name:e.name,label:(e.innerText||e.getAttribute('aria-label')||e.getAttribute('title')||e.getAttribute('placeholder')||'').slice(0,180)}))}})()`;
try {
  const version = await browser.call("Browser.getVersion");
  report.version = version;
  const nativeBrowser = (await browser.call("SystemInfo.getProcessInfo"))
    .processInfo.find((process) => process.type === "browser");
  if (!nativeBrowser) throw Error("Actual native browser process missing");
  if (!version.product.endsWith("/" + reference.browserVersion)) {
    throw Error("Reference version mismatch");
  }
  for (const record of records) {
    const packageDirectory = join(output, "packages", record.id);
    cpSync(
      join(root, "_dist/chrome-extension-corpus/chrome-unpacked", record.id),
      packageDirectory,
      { recursive: true },
    );
    // Chrome reserves store verification metadata for packed installs. Loaded
    // vendor scripts and the original manifest remain byte-for-byte unchanged.
    rmSync(join(packageDirectory, "_metadata"), {
      recursive: true,
      force: true,
    });
    /** @type {import("./sidepanel-test-types.ts").IdentityObservation} */
    const item = {
      id: record.id,
      name: record.name,
      version: record.version,
      packageSha256: record.packageSha256,
      primaryOperation: record.primaryOperation,
      ...verifyCorpusScripts(root, record, packageDirectory),
      primaryOperationResult: "not-attempted",
      prerequisiteStatus: "observed-ui-review-required",
      snapshots: [],
      nativeQualified: false,
    };
    report.extensions.push(item);
    save();
    let loaded = false;
    try {
      const initial = new Set(
        (await browser.call("Target.getTargets")).targetInfos.map((e) =>
          e.targetId
        ),
      );
      const result = await browser.call("Extensions.loadUnpacked", {
        path: packageDirectory,
      });
      if (result.id !== record.id) {
        throw Error("Frozen Chrome identity mismatch");
      }
      loaded = true;
      const { targetId } = await browser.call("Target.createTarget", {
        url: fixtureURL + record.id,
      });
      const page = await attach(targetId);
      await until(
        () =>
          evaluate(
            page,
            "document.readyState==='complete'&&document.title==='Authored identity prerequisite'",
          ),
        "controlled authored page",
      );
      await browser.call("Target.activateTarget", { targetId });
      const windows = execFileSync("xdotool", [
        "search",
        "--onlyvisible",
        "--pid",
        String(nativeBrowser.id),
      ], { encoding: "utf8" }).trim().split(/\s+/);
      const nativeWindow = windows.find((id) =>
        execFileSync("xdotool", ["getwindowname", id], { encoding: "utf8" })
          .includes("Authored identity prerequisite")
      );
      if (!nativeWindow) throw Error("Owned native reference window missing");
      execFileSync("xdotool", ["windowfocus", nativeWindow]);
      await delay(150);
      const tab = await until(
        async () =>
          (await browser.call("Target.getTargets", {
            filter: [{ type: "tab" }],
          })).targetInfos.find((e) => e.url === fixtureURL + record.id),
        "actual native tab target",
      );
      await browser.call("Extensions.triggerAction", {
        id: record.id,
        targetId: tab.targetId,
      });
      item.actualNativeAction = true;
      const sessions = new Map();
      for (const wait of [1500, 3500, 6000, 2500, 2500]) {
        await delay(wait);
        const targets = (await browser.call("Target.getTargets")).targetInfos
          .filter((e) =>
            e.type === "page" && e.url !== fixtureURL + record.id &&
            !initial.has(e.targetId)
          );
        for (const target of targets) {
          try {
            let session = sessions.get(target.targetId);
            if (!session) {
              session = await attach(target.targetId);
              sessions.set(target.targetId, session);
            }
            /** @type {import("./sidepanel-test-types.ts").SidePanelSnapshot} */
            const ui = await evaluate(session, snapshot);
            item.snapshots.push({
              observedAfterMs: wait,
              targetId: target.targetId,
              ...ui,
            });
            const screenshot = await browser.call("Page.captureScreenshot", {
              format: "png",
            }, session);
            const screenshotPath = join(
              output,
              `${record.id}-${item.snapshots.length}.png`,
            );
            writeFileSync(
              screenshotPath,
              Buffer.from(screenshot.data, "base64"),
            );
            const latest = item.snapshots.at(-1);
            if (!latest) throw Error("Native UI snapshot was not recorded");
            latest.screenshotPath = screenshotPath;
            const label =
              record.name === "1Password" || record.name === "Proton Pass"
                ? "Continue"
                : null;
            const onboardingSource =
              ui.url.startsWith(`chrome-extension://${record.id}/`) ||
              record.name === "Proton Pass" &&
                ui.url.startsWith(
                  "https://account.proton.me/pass/extension-onboarding",
                );
            if (
              label && !item.localOnboardingClick &&
              onboardingSource &&
              ui.controls.some((control) => control.label === label)
            ) {
              if (
                record.name === "1Password" || record.name === "Proton Pass"
              ) {
                await browser.call("Target.activateTarget", {
                  targetId: target.targetId,
                });
              }
              if (await clickLocalOnboarding(session, label)) {
                item.localOnboardingClick = {
                  label,
                  targetId: target.targetId,
                  authenticationAttempted: false,
                };
              }
            }
          } catch (error) {
            item.observationErrors ??= [];
            item.observationErrors.push(String(error));
          }
        }
        save();
        if (classifyPrerequisite(item) === "observed-account-gate") break;
      }
      item.networkFailures = (browser.events || []).filter((e) =>
        ["Network.responseReceived", "Network.loadingFailed"].includes(e.method)
      ).flatMap(
        /** @param {import("./cdp-types.ts").CDPEvent} e @returns {{error?:string;status?:number;url?:string}[]} */ (
          e,
        ) => {
          if (e.method === "Network.loadingFailed") {
            return [{ error: String(e.params?.errorText || "unknown") }];
          }
          const response =
            /** @type {{status:number;url:string}} */ (e.params?.response);
          if (response.status < 400) return [];
          const url = new URL(response.url);
          return [{ status: response.status, url: url.origin + url.pathname }];
        },
      ).slice(-30);
      if (!item.snapshots.length) {
        throw Error("No actual advertised native UI could be inspected");
      }
      item.prerequisiteStatus = classifyPrerequisite(item);
    } catch (error) {
      item.prerequisiteStatus = "failed-observation";
      item.error = String(error);
    } finally {
      if (loaded) {
        try {
          await browser.call("Extensions.uninstall", { id: record.id });
        } catch (error) {
          item.uninstallError = String(error);
        }
      }
      for (
        const target of (await browser.call("Target.getTargets")).targetInfos
          .filter((e) => e.type === "page" && e.url !== "about:blank")
      ) {
        try {
          await browser.call("Target.closeTarget", {
            targetId: target.targetId,
          });
        } catch { /* Vendor view already closed on uninstall. */ }
      }
      browser.events = [];
      save();
      console.log(record.name + ": " + item.prerequisiteStatus);
    }
  }
} finally {
  report.observationCompleted = report.extensions.length === records.length &&
    report.extensions.every((item) =>
      item.prerequisiteStatus !== "failed-observation"
    );
  if (!report.observationCompleted) process.exitCode = 1;
  save();
  await browser.close();
  await new Promise((resolve) => server.close(resolve));
}
console.log(join(output, "report.json"));
