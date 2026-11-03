/// <reference path="../../libs/@types/gecko/index.d.ts" />
// SPDX-License-Identifier: MPL-2.0
import { boundedAutomationQuery } from "./automation-query.mjs";
import { installFrozenCorpus } from "./corpus-install-fixture.mjs";
import { attachNativeFloorpDevtools } from "./floorp-devtools-panel.mjs";
import {
  expectedProducts,
  webscraperId,
  webscraperSitemap,
} from "./corpus-automation-recipes.mjs";
/** @type {{ExtensionParent:import("./assessment-test-types.ts").AssessmentExtensionParent}} */
const { ExtensionParent } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionParent.sys.mjs",
);
const { FileUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/FileUtils.sys.mjs",
);
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
/** @param {number} ms */
const pause = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
/** @template T @param {()=>T|false|null|undefined|Promise<T|false|null|undefined>} read @param {string} label @param {number} [timeout] @returns {Promise<T>} */
async function until(read, label, timeout = 30000) {
  const end = Date.now() + timeout;
  while (Date.now() < end) {
    const result = await read();
    if (result) return result;
    await pause(100);
  }
  throw Error("Timed out waiting for " + label);
}
function resourceProtocol() {
  const handler = Services.io.getProtocolHandler("resource");
  if (!handler.QueryInterface) {
    throw Error("Native resource protocol interface unavailable");
  }
  return handler.QueryInterface(Ci.nsIResProtocolHandler);
}
export async function runWebScraper() {
  const root = Services.env.get("FLOORP_CORPUS_ROOT"),
    url = Services.env.get("FLOORP_CORPUS_FIXTURE");
  const output = PathUtils.join(
    Services.env.get("FLOORP_CORPUS_OUTPUT"),
    "web-scraper",
  );
  await IOUtils.makeDirectory(output);
  const corpus =
    /** @type {import("./corpus-types.ts").FrozenCorpus} */ (await IOUtils
      .readJSON(
        PathUtils.join(
          root,
          "tools",
          "chrome-extension-runtime",
          "corpus-lock.json",
        ),
      ));
  const record = corpus.extensions.find((record) => record.id === webscraperId);
  if (!record) throw Error("Frozen automation record missing");
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
    runtime: {
      version: Services.appinfo.version,
      buildId: Services.appinfo.appBuildID,
    },
    fixture: JSON.parse(
      Services.env.get("FLOORP_CORPUS_FIXTURE_METADATA") || "{}",
    ),
    extensions: [item],
    output,
  };
  resourceProtocol()
    .setSubstitution(
      "floorp-automation-corpus",
      Services.io.newFileURI(
        new FileUtils.File(
          Services.env.get("FLOORP_TEST_ACTOR_DIRECTORY") ||
            PathUtils.join(root, "tools/chrome-extension-runtime"),
        ),
      ),
    );
  ChromeUtils.registerWindowActor("CorpusAutomation", {
    child: {
      esModuleURI:
        "resource://floorp-automation-corpus/CorpusAutomationChild.sys.mjs",
    },
    allFrames: false,
    safeForUntrustedWebProcess: true,
  });
  const window =
      /** @type {import("./native-test-types.ts").NativeCorpusWindow} */ (/** @type {unknown} */ (Services
        .wm.getMostRecentWindow("navigator:browser"))),
    before = new Set(window.gBrowser.tabs);
  let addon, devtools;
  try {
    const installed = await installFrozenCorpus(root, record, output);
    ({ addon } = installed);
    Object.assign(item, {
      unchangedScriptCount: installed.unchangedScriptCount,
      unchangedScriptsSha256: installed.unchangedScriptsSha256,
      sourceIntegrity: installed.sourceIntegrity,
      nativeEvidence: installed.nativeEvidence,
      warnings: installed.warnings,
    });
    const extension = ExtensionParent.GlobalManager.extensionMap.get(
      record.id + "@chromium-extension",
    );
    if (!extension) throw Error("Installed native extension policy missing");
    await until(
      () => extension.backgroundContext?.viewType === "background_worker",
      "actual Web Scraper native ServiceWorker",
    );
    const target = window.gBrowser.addTab(url, {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
    window.gBrowser.selectedTab = target;
    await until(
      () =>
        target.linkedBrowser.browsingContext.currentWindowGlobal?.documentURI
          .spec === url,
      "authored product-table document",
    );
    devtools = await attachNativeFloorpDevtools(target, extension);
    const panel = await devtools.selectPanel(
      "Web Scraper",
      "devtools_web_scraper_panel.html",
    );
    item.nativePanel = {
      id: panel.definition.id,
      label: panel.definition.label,
      viewType: panel.view.viewType,
      url: panel.view.uri?.spec,
    };
    const actor = () => {
      const value = panel.browser.browsingContext.currentWindowGlobal?.getActor(
        "CorpusAutomation",
      );
      if (!value) throw Error("Private Web Scraper panel actor unavailable");
      return /** @type {import("./automation-test-types.ts").AutomationActor} */ (value);
    };
    const snapshot = () =>
      boundedAutomationQuery(
        actor()
          ?.sendQuery("Snapshot"),
        "Snapshot",
      )
        .catch(() => null);
    /** @param {import("./automation-test-types.ts").ScraperControl} control @param {{sitemap?:import("./automation-test-types.ts").Sitemap}} [extra] */
    const control = (control, extra = {}) =>
      boundedAutomationQuery(
        actor().sendQuery("WebScraperControl", { control, ...extra }),
        "WebScraperControl",
      );
    await until(async () => {
      const state = await snapshot();
      return (
        state?.ready === "complete" &&
        state.text?.includes("Create new sitemap")
      );
    }, "actual initialized native extension panel");
    await control("createMenu");
    await control("importForm");
    const sitemap = webscraperSitemap(url);
    item.sitemap = sitemap;
    await until(async () => {
      const state = await snapshot();
      return state?.text?.includes("Sitemap JSON");
    }, "real sitemap import form");
    item.input = await control("sitemapJSON", { sitemap: item.sitemap });
    await control("import");
    await until(async () => {
      item.imported = await snapshot();
      item.importedControlState = await boundedAutomationQuery(
        actor().sendQuery("WebScraperControlState", { control: "sitemapMenu" }),
        "WebScraperControlState",
      );
      const state = item.importedControlState;
      return item.imported?.url.includes(
        "/sitemap/" + sitemap._id + "/selectors/",
      ) && state.url === item.imported.url && state.ready === "complete" &&
        state.selectorViewReady && state.activeSitemapId === sitemap._id &&
        state.visible && state.enabled && state.unobstructed;
    }, "genuine imported sitemap view and available menu");
    await control("sitemapMenu");
    await until(async () => {
      item.scrapeMenuControlState = await boundedAutomationQuery(
        actor().sendQuery("WebScraperControlState", { control: "scrapeForm" }),
        "WebScraperControlState",
      );
      const state = item.scrapeMenuControlState;
      return state.activeSitemapId === sitemap._id && state.visible &&
        state.enabled && state.unobstructed;
    }, "genuine visible scrape menu control");
    await control("scrapeForm");
    await until(async () => {
      const state = await snapshot();
      return state?.text?.includes("Start scraping");
    }, "real scrape form");
    await control("start");
    await pause(5000);
    await until(
      async () => {
        item.observed = await snapshot();
        if (
          item.observed?.rows?.length === 3 &&
          item.observed.text?.includes("Apple") &&
          item.observed.text?.includes("Pear")
        ) {
          return true;
        }
        if (item.observed?.text?.includes("Refresh")) await control("refresh");
        await pause(1000);
        return false;
      },
      "genuine Web Scraper extracted rows",
      60000,
    );
    if (!item.observed?.rows) throw Error("Actual extracted rows missing");
    const rows = item.observed.rows;
    item.rows = rows;
    if (
      JSON.stringify(rows.slice(1).map((row) => row.slice(-2))) !==
        JSON.stringify(expectedProducts)
    ) {
      throw Error("Real extracted product values mismatch");
    }
    if (rows.slice(1).some((row) => row[1] !== url)) {
      throw Error("Real extracted source URL mismatch");
    }
    item.checks = [
      "signed CRX hash and all scripts unchanged through production converter",
      "actual native ServiceWorker background",
      "native Gecko toolbox and real registered extension panel",
      "native mouse and text input importing authored sitemap JSON",
      "real extension Start scraping and Refresh Data",
      "exact Apple/Pear product data extracted from authored URL",
    ];
    item.result = "pass";
    console.log("[Actual corpus] Web Scraper genuine local extraction PASS");
  } catch (error) {
    item.error = String(error);
    throw error;
  } finally {
    await devtools?.close();
    await addon?.uninstall();
    for (const tab of [...window.gBrowser.tabs]) {
      if (!before.has(tab)) {
        window.gBrowser.removeTab(tab, { skipPermitUnload: true });
      }
    }
    ChromeUtils.unregisterWindowActor("CorpusAutomation");
    await IOUtils.writeJSON(PathUtils.join(output, "report.json"), report);
  }
}
export const runAllTests = runWebScraper;
