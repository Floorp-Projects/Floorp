#!/usr/bin/env node
// SPDX-License-Identifier: MPL-2.0
// Inspect actual frozen extensions. This stage does not count installation/UI as a primary-operation pass.
import { ChromePipe } from "./chrome-cdp-pipe.mjs";
import { createHash } from "node:crypto";
import { createServer } from "node:http";
import { mkdirSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
const tools = dirname(fileURLToPath(import.meta.url)),
  root = resolve(tools, "../..");
/** @param {string} name @param {string} fallback */
const option = (name, fallback) => {
  const i = process.argv.indexOf(name);
  return i < 0 ? fallback : process.argv[i + 1];
};
const chrome = resolve(
  option(
    "--chrome",
    join(root, "_dist/chrome-reference/extracted/opt/google/chrome/chrome"),
  ),
);
const reference = JSON.parse(
  readFileSync(join(tools, "chrome-reference.json"), "utf8"),
);
if (
  createHash("sha256").update(readFileSync(chrome)).digest("hex") !==
    reference.binary.sha256
) throw Error("Reference binary hash mismatch");
const corpus =
    /** @type {import("./corpus-types.ts").FrozenCorpus} */ (JSON.parse(
      readFileSync(join(tools, "corpus-lock.json"), "utf8"),
    )),
  selected = option("--ids", "").split(",").filter(Boolean);
const parent = join(root, "_dist/chrome-runtime-tests");
mkdirSync(parent, { recursive: true });
const output = mkdtempSync(join(parent, "corpus-reference-"));
/** @type {import("./crx-test-types.ts").InspectionReport} */
const report = {
  stage: "actual-package-ui-inspection",
  reference,
  primaryOperationPassRate: null,
  extensions: [],
  output,
};
const save = () =>
  writeFileSync(join(output, "report.json"), JSON.stringify(report, null, 2));
const server = createServer((_req, res) => {
  res.writeHead(200, {
    "Content-Type": "text/html; charset=utf-8",
    "Cache-Control": "no-store",
  });
  res.end(
    `<!doctype html><meta charset=utf-8><title>Floorp corpus authored page</title><h1>Controlled extension operation fixture</h1><p id=selection>Floorp compatibility verification text with an intentional speling mistake.</p><form><label>Username <input id=username name=username autocomplete=username></label><label>Password <input id=password type=password autocomplete=current-password></label><button type=button id=operation onclick="this.dataset.clicked='yes'">Controlled button</button></form><textarea id=editor>Authored draft text</textarea><a href=/second>Second authored page</a><div style="height:2400px;background:linear-gradient(white,#86c)">Long page for real screenshots</div>`,
  );
});
await new Promise((resolve) =>
  server.listen(0, "127.0.0.1", () => resolve(undefined))
);
const address = server.address();
if (!address || typeof address === "string") {
  throw Error("Inspection fixture did not bind a TCP port");
}
const fixture = `http://127.0.0.1:${address.port}/`;
const browser = new ChromePipe(chrome, output);
try {
  report.version = await browser.call("Browser.getVersion");
  if (report.version.product !== `Chrome/${reference.browserVersion}`) {
    throw Error("Reference version mismatch");
  }
  const fixtureTarget = await browser.call("Target.createTarget", {
    url: fixture,
  });
  for (const extension of corpus.extensions) {
    if (selected.length && !selected.includes(extension.id)) continue;
    /** @type {import("./crx-test-types.ts").InspectionItem} */
    const record = {
      id: extension.id,
      name: extension.name,
      version: extension.version,
      area: extension.area,
      primaryOperation: extension.primaryOperation,
      primaryOperationResult: "not-run",
    };
    report.extensions.push(record);
    if (extension.status !== "frozen") {
      record.status = "unavailable-package";
      save();
      continue;
    }
    let loaded = false, target;
    try {
      const directory = join(
        root,
        "_dist/chrome-extension-corpus/chrome-unpacked",
        extension.id,
      );
      const manifest = JSON.parse(
        readFileSync(join(directory, "manifest.json"), "utf8"),
      );
      const value = await browser.call("Extensions.loadUnpacked", {
        path: directory,
      });
      loaded = true;
      if (value.id !== extension.id) {
        throw Error("Original Chrome identity changed");
      }
      record.status = "loaded";
      record.background = manifest.background;
      record.entries = {
        popup: (manifest.action || manifest.browser_action)?.default_popup,
        sidePanel: manifest.side_panel?.default_path,
        options: manifest.options_ui?.page || manifest.options_page,
        devtools: manifest.devtools_page,
        newtab: manifest.chrome_url_overrides?.newtab,
      };
      await new Promise((resolve) => setTimeout(resolve, 300));
      record.targets = (await browser.call("Target.getTargets")).targetInfos
        .filter((info) =>
          info.url.startsWith(`chrome-extension://${extension.id}/`)
        ).map(({ type, url, title }) => ({ type, url, title }));
      const entry = record.entries.sidePanel || record.entries.popup ||
        record.entries.options || record.entries.devtools ||
        record.entries.newtab;
      if (entry) {
        ({ targetId: target } = await browser.call("Target.createTarget", {
          url: `chrome-extension://${extension.id}/${entry.replace(/^\//, "")}`,
        }));
        const { sessionId } = await browser.call("Target.attachToTarget", {
          targetId: target,
          flatten: true,
        });
        let snapshot;
        for (let i = 0; i < 25; i++) {
          const evaluation = await browser.call("Runtime.evaluate", {
            expression:
              `({url:location.href,title:document.title,readyState:document.readyState,text:document.body?.innerText?.slice(0,18000),controls:[...document.querySelectorAll('button,a,input,textarea,select,[role="button"]')].slice(0,100).map(e=>({tag:e.tagName,id:e.id,type:e.type,text:(e.innerText||e.getAttribute('aria-label')||e.getAttribute('placeholder')||'').slice(0,200)}))})`,
            returnByValue: true,
          }, sessionId);
          if (evaluation.exceptionDetails) {
            throw Error(JSON.stringify(evaluation.exceptionDetails));
          }
          snapshot =
            /** @type {import("./crx-test-types.ts").InspectionItem["ui"]} */ (evaluation
              .result.value);
          if (
            snapshot?.readyState === "complete" &&
            (snapshot?.text?.length ?? 0) > 20
          ) break;
          await new Promise((resolve) => setTimeout(resolve, 200));
        }
        record.ui = snapshot;
        const { data } = await browser.call("Page.captureScreenshot", {
          format: "png",
        }, sessionId);
        writeFileSync(
          join(output, `${extension.id}.png`),
          Buffer.from(data, "base64"),
        );
      }
      console.log(
        `INSPECTED ${record.name} ${record.version}: ${
          record.ui?.title || record.status
        }`,
      );
    } catch (error) {
      record.status = "inspection-error";
      record.error = String(error);
      console.log(`INSPECTION FAILED ${record.name}: ${error}`);
    } finally {
      if (target) {
        try {
          await browser.call("Target.closeTarget", { targetId: target });
        } catch {
          /* The target may already have closed during inspection cleanup. */
        }
      }
      if (loaded) {
        try {
          await browser.call("Extensions.uninstall", { id: extension.id });
        } catch (error) {
          record.uninstallError = String(error);
        }
      }
      const targets = (await browser.call("Target.getTargets")).targetInfos;
      for (const info of targets) {
        if (
          info.type === "page" && info.targetId !== fixtureTarget.targetId
        ) {
          try {
            await browser.call("Target.closeTarget", {
              targetId: info.targetId,
            });
          } catch {
            /* The target may already have closed during inspection cleanup. */
          }
        }
      }
      save();
    }
  }
} finally {
  await browser.close();
  await new Promise((resolve) => server.close(resolve));
  save();
}
console.log(`REPORT ${join(output, "report.json")}`);
