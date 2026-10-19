// SPDX-License-Identifier: MPL-2.0
import { createHash } from "node:crypto";
import { createServer } from "node:http";
import { mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { assertFrozenChromeBinary, ChromePipe } from "./chrome-cdp-pipe.mjs";
import {
  attachNativeDevtools,
  waitForNativeUI,
} from "./chrome-devtools-panel.mjs";
import { verifyCorpusScripts } from "./corpus-script-integrity.mjs";
import {
  expectedProducts,
  webscraperId,
  webscraperSitemap,
} from "./corpus-automation-recipes.mjs";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "../..");
assertFrozenChromeBinary(
  join(root, "_dist/chrome-reference/extracted/opt/google/chrome/chrome"),
);
const output = mkdtempSync(
  join(root, "_dist/chrome-runtime-tests/corpus-webscraper-reference-"),
);
const fixture = readFileSync(
  join(root, "tools/chrome-extension-runtime/fixtures/corpus-automation.html"),
);
const corpus =
  /** @type {import("./corpus-types.ts").FrozenCorpus} */ (JSON.parse(
    readFileSync(
      join(root, "tools/chrome-extension-runtime/corpus-lock.json"),
      "utf8",
    ),
  ));
const record = corpus.extensions.find((record) => record.id === webscraperId);
if (!record) throw Error("Frozen automation record missing");
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
  throw Error("Scraper fixture did not bind a TCP port");
}
const url = `http://127.0.0.1:${address.port}/products`;
const browser = new ChromePipe(
  join(root, "_dist/chrome-reference/extracted/opt/google/chrome/chrome"),
  output,
  ["--auto-open-devtools-for-tabs", "--window-size=1600,1200"],
  { headed: true },
);
browser.events = [];
/** @param {number} ms */
const pause = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
/** @type {import("./automation-test-types.ts").AutomationItem} */
const item = {
  id: record.id,
  name: record.name,
  version: record.version,
  packageSha256: record.packageSha256,
  result: "failed",
};
/** @type {import("./automation-test-types.ts").AutomationReport} */
const report = {
  stage: "frozen-webscraper-primary-operation",
  reference: JSON.parse(
    readFileSync(
      join(root, "tools/chrome-extension-runtime/chrome-reference.json"),
      "utf8",
    ),
  ),
  fixture: { url, sha256: createHash("sha256").update(fixture).digest("hex") },
  extensions: [item],
  output,
};
let failed = false;
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
  await pause(1200);
  await browser.call("Target.createTarget", { url });
  const devtools = await attachNativeDevtools(browser, url);
  item.nativeDevtools = devtools.target;
  item.selectedTabs = await devtools.selectPanel("Web Scraper");
  const panel = await devtools.attachPanel(
    id,
    "devtools_web_scraper_panel.html",
  );
  item.nativePanel = panel.target;
  await waitForNativeUI(
    () =>
      panel.evaluate(
        "Boolean(document.querySelector('#create-sitemap-nav-button'))",
      ),
    "actual extension panel initialization",
  );
  await panel.clickExpression(
    "document.querySelector('#create-sitemap-nav-button')",
  );
  await panel.clickExpression("document.querySelector('a.import-sitemap')");
  await waitForNativeUI(
    () => panel.evaluate("Boolean(document.querySelector('#sitemapJSON'))"),
    "real sitemap import input",
  );
  const sitemap = webscraperSitemap(url);
  item.sitemap = sitemap;
  await panel.clickExpression("document.querySelector('#sitemapJSON')");
  await browser.call(
    "Input.insertText",
    { text: JSON.stringify(item.sitemap) },
    devtools.sessionId,
  );
  await panel.clickExpression(
    "document.querySelector('#submit-import-sitemap')",
  );
  await waitForNativeUI(async () => {
    item.imported = await panel.snapshot();
    return item.imported.url.includes(
      "/sitemap/" + sitemap._id + "/selectors/",
    );
  }, "genuine imported sitemap");
  await panel.clickExpression("document.querySelector('#sitemap-nav-button')");
  await panel.clickExpression("document.querySelector('a.scrape')");
  await waitForNativeUI(
    () =>
      panel.evaluate(
        "Boolean(document.querySelector('#submit-scrape-sitemap'))",
      ),
    "real scraping form",
  );
  await panel.clickExpression(
    "document.querySelector('#submit-scrape-sitemap')",
  );
  await pause(5000);
  await waitForNativeUI(
    async () => {
      item.observed = await panel.snapshot();
      if (
        item.observed.text?.includes("Apple") &&
        item.observed.text?.includes("Pear")
      ) {
        return true;
      }
      if (
        await panel.evaluate(
          "Array.from(document.querySelectorAll('button')).some(e=>e.textContent.includes('Refresh'))",
        )
      ) {
        await panel.clickExpression(
          "Array.from(document.querySelectorAll('button')).find(e=>e.textContent.includes('Refresh'))",
        );
      }
      await pause(1000);
      return false;
    },
    "actual extracted data rows",
    60000,
  );
  /** @type {string[][]} */
  const rows = await panel.evaluate(
    "Array.from(document.querySelectorAll('table tr')).map(row=>Array.from(row.querySelectorAll('th,td')).map(cell=>cell.innerText))",
  );
  item.rows = rows;
  const products = rows.slice(1).map((row) => row.slice(-2));
  if (JSON.stringify(products) !== JSON.stringify(expectedProducts)) {
    throw Error("Genuine extracted product values mismatch");
  }
  if (rows.slice(1).some((row) => row[1] !== url)) {
    throw Error("Extracted rows refer to another source");
  }
  item.checks = [
    "frozen signed CRX SHA-256 and every loaded script unchanged",
    "actual native DevTools frontend and registered extension panel iframe",
    "native pointer controls and native text input for authored sitemap JSON",
    "real extension Start scraping command and Refresh Data",
    "actual extracted Apple/Pear values and source URL",
  ];
  item.result = "pass";
  const screenshot = await browser.call(
    "Page.captureScreenshot",
    {},
    devtools.sessionId,
  );
  writeFileSync(
    join(output, "native-devtools-result.png"),
    Buffer.from(screenshot.data, "base64"),
  );
  console.log(
    "PASS Web Scraper: genuine sitemap import and local data extraction",
  );
} catch (error) {
  item.error = String(error);
  failed = true;
  console.error(error);
} finally {
  writeFileSync(join(output, "report.json"), JSON.stringify(report, null, 2));
  writeFileSync(
    join(output, "events.json"),
    JSON.stringify(browser.events, null, 2),
  );
  await browser.close();
  server.close();
  console.log("REPORT " + join(output, "report.json"));
}
if (failed) process.exitCode = 1;
