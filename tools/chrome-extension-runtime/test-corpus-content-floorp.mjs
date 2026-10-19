// SPDX-License-Identifier: MPL-2.0
import { unzipSync } from "fflate/browser";
import { CRXConverterClass } from "../../browser-features/modules/modules/chrome-web-store/CRXConverter.sys.mts";
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
const { AddonManager } = ChromeUtils.importESModule(
  "resource://gre/modules/AddonManager.sys.mjs",
);
const { ExtensionParent } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionParent.sys.mjs",
);
const { FileUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/FileUtils.sys.mjs",
);
const pause = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
async function until(read, label) {
  const end = Date.now() + 30000;
  while (Date.now() < end) {
    const value = await read();
    if (value) return value;
    await pause(100);
  }
  throw Error("Timed out waiting for " + label);
}
const digest = async (bytes) =>
  [...new Uint8Array(await crypto.subtle.digest("SHA-256", bytes))].map(
    (byte) => byte.toString(16).padStart(2, "0"),
  ).join("");
export async function runAllTests() {
  const root = Services.env.get("FLOORP_CORPUS_ROOT");
  const fixture = Services.env.get("FLOORP_CORPUS_FIXTURE");
  const output = Services.env.get("FLOORP_CORPUS_OUTPUT");
  if (!root || !fixture || !output) {
    throw Error("Corpus root, fixture and output are required");
  }
  const selected = Services.env.get("FLOORP_CORPUS_IDS").split(",").filter(
    Boolean,
  );
  const corpus = await IOUtils.readJSON(
    PathUtils.join(
      root,
      "tools",
      "chrome-extension-runtime",
      "corpus-lock.json",
    ),
  );
  const report = {
    stage: "actual-package-primary-operation",
    runtime: {
      version: Services.appinfo.version,
      buildId: Services.appinfo.appBuildID,
      nativeWorkerPolicy: WebExtensionPolicy.backgroundServiceWorkerEnabled,
    },
    fixture: JSON.parse(
      Services.env.get("FLOORP_CORPUS_FIXTURE_METADATA") || "{}",
    ),
    extensions: [],
    primaryOperationPassRate: null,
    output,
  };
  const save = () =>
    IOUtils.writeJSON(PathUtils.join(output, "report.json"), report);
  const resource = Services.io.getProtocolHandler("resource").QueryInterface(
    Ci.nsIResProtocolHandler,
  );
  resource.setSubstitution(
    "floorp-content-corpus",
    Services.io.newFileURI(
      new FileUtils.File(
        PathUtils.join(root, "tools", "chrome-extension-runtime"),
      ),
    ),
  );
  ChromeUtils.registerWindowActor("CorpusContent", {
    child: {
      esModuleURI:
        "resource://floorp-content-corpus/CorpusContentChild.sys.mjs",
    },
    allFrames: true,
    safeForUntrustedWebProcess: true,
    matches: ["http://127.0.0.1/*"],
  });
  const win = Services.wm.getMostRecentWindow("navigator:browser");
  const principal = Services.scriptSecurityManager.getSystemPrincipal();
  const converter = new CRXConverterClass();
  const snapshot = async (tab) =>
    tab.linkedBrowser.browsingContext.currentWindowGlobal?.getActor(
      "CorpusContent",
    ).sendQuery("Snapshot");
  try {
    for (
      const record of corpus.extensions.filter((item) =>
        selected.includes(item.id)
      )
    ) {
      const item = {
        id: record.id,
        name: record.name,
        version: record.version,
        packageSha256: record.packageSha256,
        primaryOperation: record.primaryOperation,
        primaryOperationResult: "not-run",
        checks: [],
      };
      report.extensions.push(item);
      const originalTabs = new Set(win.gBrowser.tabs);
      let addon;
      try {
        if (record.status !== "frozen") {
          throw Error("Original signed package is unavailable");
        }
        const tab = win.gBrowser.addTab(fixture, {
          triggeringPrincipal: principal,
        });
        win.gBrowser.selectedTab = tab;
        item.baseline = await until(async () => {
          const value = await snapshot(tab).catch(() => null);
          return value?.ready === "complete" && value.text ? value : null;
        }, "authored baseline");
        const bytes = await IOUtils.read(
          PathUtils.join(
            root,
            "_dist",
            "chrome-extension-corpus",
            "packages",
            record.id + ".crx",
          ),
        );
        if (await digest(bytes) !== record.packageSha256) {
          throw Error("Frozen signed CRX digest mismatch");
        }
        const conversion = await converter.convert(bytes.buffer, record.id, {
          id: record.id,
          name: record.name,
          version: record.version,
        });
        item.warnings = conversion.warnings;
        item.conversion = conversion.report;
        if (!conversion.success || !conversion.xpiData) {
          throw Error(
            conversion.error?.message || "Production conversion failed",
          );
        }
        const files = unzipSync(new Uint8Array(conversion.xpiData));
        const unchanged = [];
        for (
          const [name, data] of Object.entries(files).filter(([name]) =>
            /\.(?:js|mjs|cjs)$/i.test(name)
          )
        ) {
          const original = await IOUtils.read(
            PathUtils.join(
              root,
              "_dist",
              "chrome-extension-corpus",
              "chrome-unpacked",
              record.id,
              ...name.split("/"),
            ),
          );
          const actual = await digest(data);
          if (actual !== await digest(original)) {
            throw Error("Signed script changed in conversion: " + name);
          }
          unchanged.push(name + ":" + actual);
        }
        item.unchangedScriptCount = unchanged.length;
        item.unchangedScriptsSha256 = await digest(
          new TextEncoder().encode(unchanged.sort().join("\n")),
        );
        const xpi = PathUtils.join(output, record.id + ".xpi");
        await IOUtils.write(xpi, new Uint8Array(conversion.xpiData));
        addon = await AddonManager.installTemporaryAddon(
          new FileUtils.File(xpi),
        );
        const extension = ExtensionParent.GlobalManager.extensionMap.get(
          record.id + "@chromium-extension",
        );
        if (!extension) throw Error("Installed native extension missing");
        item.background = extension.manifest.background;
        if (record.name !== "Dark Reader") {
          throw Error(
            "No primary-operation recipe implemented for " + record.name,
          );
        }
        const initial = tab.linkedBrowser.browsingContext.currentWindowGlobal
          ?.innerWindowId;
        tab.linkedBrowser.reload();
        item.transformed = await until(async () => {
          const value = await snapshot(tab).catch(() => null);
          return tab.linkedBrowser.browsingContext.currentWindowGlobal
                  ?.innerWindowId !== initial &&
              value?.markers?.length && value.body !== item.baseline.body
            ? value
            : null;
        }, "real unchanged Dark Reader theme");
        item.checks.push(
          "Actual signed-package Dark Reader script transformed the authored page",
        );
        tab.linkedBrowser.loadURI(Services.io.newURI(fixture + "second"), {
          triggeringPrincipal: principal,
        });
        item.afterNavigation = await until(async () => {
          const value = await snapshot(tab).catch(() => null);
          return value?.url === fixture + "second" && value.markers.length &&
              value.body !== item.baseline.body
            ? value
            : null;
        }, "theme after native navigation");
        item.checks.push("Transformation persists after real page navigation");
        await addon.uninstall();
        addon = null;
        tab.linkedBrowser.reload();
        item.afterRemoval = await until(async () => {
          const value = await snapshot(tab).catch(() => null);
          return value?.body === item.baseline.body && !value.markers.length
            ? value
            : null;
        }, "authored page restored after actual uninstall");
        item.checks.push("Uninstall removes the real transformation");
        item.primaryOperationResult = "passed";
      } catch (error) {
        item.primaryOperationResult = "failed";
        item.error = String(error);
        console.error("[Content corpus]", record.name, error);
      } finally {
        await addon?.uninstall();
        for (const tab of [...win.gBrowser.tabs]) {
          if (!originalTabs.has(tab)) {
            win.gBrowser.removeTab(tab, { skipPermitUnload: true });
          }
        }
        await save();
      }
    }
  } finally {
    ChromeUtils.unregisterWindowActor("CorpusContent");
    resource.setSubstitution("floorp-content-corpus", null);
    await save();
  }
  console.log(
    "[Content corpus] REPORT " + PathUtils.join(output, "report.json"),
  );
}
