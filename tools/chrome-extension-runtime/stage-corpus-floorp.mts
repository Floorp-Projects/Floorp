// SPDX-License-Identifier: MPL-2.0
// Authentic signed package conversion/startup inspection; no primary-operation score.
/// <reference path="../../libs/@types/gecko/index.d.ts" />
import { CRXConverterClass } from "../../browser-features/modules/modules/chrome-web-store/CRXConverter.sys.mts";
import type {
  CorpusBrowserWindow,
  CorpusInspectionReport,
  CorpusInstalledAddon,
  FrozenCorpus,
} from "./corpus-types.ts";
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
const delay = (ms: number) => new Promise((resolve) => setTimeout(resolve, ms));

export async function runAllTests(): Promise<void> {
  const root = Services.env.get("FLOORP_CORPUS_ROOT");
  if (!root) throw new Error("Set FLOORP_CORPUS_ROOT to the Floorp checkout");
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
  ) as FrozenCorpus;
  const directory = PathUtils.join(root, "_dist", "chrome-extension-corpus");
  const output = PathUtils.join(directory, `floorp-inspection-${Date.now()}`);
  await IOUtils.makeDirectory(output);
  await IOUtils.makeDirectory(PathUtils.join(directory, "converted"));
  const file = new FileUtils.File(
    PathUtils.join(root, "tools", "chrome-extension-runtime"),
  );
  const handler = Services.io.getProtocolHandler("resource");
  if (!handler.QueryInterface) {
    throw new Error("Resource protocol handler is unavailable");
  }
  handler.QueryInterface(Ci.nsIResProtocolHandler)
    .setSubstitution("floorp-corpus-inspector", Services.io.newFileURI(file));
  ChromeUtils.registerWindowActor("CorpusInspector", {
    child: {
      esModuleURI:
        "resource://floorp-corpus-inspector/CorpusInspectorChild.sys.mjs",
    },
    remoteTypes: ["extension"],
    allFrames: false,
  });
  const report: CorpusInspectionReport = {
    stage: "authentic-package-conversion-and-startup-inspection",
    primaryOperationPassRate: null,
    runtime: {
      version: Services.appinfo.version,
      buildId: Services.appinfo.appBuildID,
      nativeWorkerPolicy: WebExtensionPolicy.backgroundServiceWorkerEnabled,
    },
    extensions: [],
  };
  const converter = new CRXConverterClass();
  const nativeWindow = Services.wm.getMostRecentWindow("navigator:browser");
  if (!nativeWindow) throw new Error("Browser window unavailable");
  const browserWindow = nativeWindow as unknown as CorpusBrowserWindow;
  for (const record of corpus.extensions) {
    if (selected.length && !selected.includes(record.id)) continue;
    const result: Record<string, unknown> = {
      id: record.id,
      name: record.name,
      version: record.version,
      primaryOperationResult: "not-run",
    };
    report.extensions.push(result);
    let addon: CorpusInstalledAddon | undefined;
    const tabs = new Set(browserWindow.gBrowser.tabs);
    try {
      if (record.status !== "frozen") {
        result.status = "unavailable-package";
        continue;
      }
      const bytes = new Uint8Array(
        await IOUtils.read(
          PathUtils.join(directory, "packages", `${record.id}.crx`),
        ),
      );
      const digest = [
        ...new Uint8Array(await crypto.subtle.digest("SHA-256", bytes)),
      ]
        .map((value) => value.toString(16).padStart(2, "0")).join("");
      if (digest !== record.packageSha256) {
        throw new Error("Frozen signed package hash mismatch");
      }
      const conversion = await converter.convert(bytes.buffer, record.id, {
        id: record.id,
        name: record.name,
        version: record.version || "",
        description: "Frozen real extension corpus",
      });
      result.conversion = conversion.report;
      result.warnings = conversion.warnings;
      if (!conversion.success || !conversion.xpiData) {
        throw new Error(conversion.error?.message || "Conversion failed");
      }
      const path = PathUtils.join(directory, "converted", `${record.id}.xpi`);
      await IOUtils.write(path, new Uint8Array(conversion.xpiData));
      addon = await AddonManager.installTemporaryAddon(
        new FileUtils.File(path),
      );
      const extension = ExtensionParent.GlobalManager.extensionMap.get(
        `${record.id}@chromium-extension`,
      );
      if (!extension) {
        throw new Error("Installed extension missing from native manager");
      }
      result.status = "installed";
      result.background = extension.manifest.background;
      result.backgroundViewType = extension.backgroundContext?.viewType;
      const manifest = extension.manifest;
      const entries = {
        popup: (manifest.action || manifest.browser_action)?.default_popup,
        sidePanel: manifest.side_panel?.default_path,
        options: manifest.options_ui?.page || manifest.options_page,
        devtools: manifest.devtools_page,
        newtab: manifest.chrome_url_overrides?.newtab,
      };
      result.entries = entries;
      const entry = entries.sidePanel || entries.popup || entries.options ||
        entries.devtools || entries.newtab;
      if (entry) {
        const uri = extension.baseURI.resolve(entry);
        const tab = browserWindow.gBrowser.addTab(uri, {
          triggeringPrincipal: extension.principal,
        });
        const deadline = Date.now() + 10000;
        while (Date.now() < deadline) {
          const global = tab.linkedBrowser.browsingContext?.currentWindowGlobal;
          if (global?.isCurrentGlobal && global.documentURI?.spec === uri) {
            try {
              const ui = await global.getActor("CorpusInspector").sendQuery(
                "Snapshot",
              );
              result.ui = ui;
              if (ui.readyState === "complete" && (ui.text?.length ?? 0) > 20) {
                break;
              }
            } catch { /* Wait for the extension document actor. */ }
          }
          await delay(100);
        }
      }
      console.log(`[Actual corpus] inspected ${record.name} ${record.version}`);
    } catch (error) {
      result.status = "inspection-error";
      result.error = String(error);
      console.error(`[Actual corpus] ${record.name}: ${error}`);
    } finally {
      await addon?.uninstall();
      for (const tab of [...browserWindow.gBrowser.tabs]) {
        if (!tabs.has(tab)) {
          browserWindow.gBrowser.removeTab(tab, { skipPermitUnload: true });
        }
      }
      await IOUtils.writeJSON(PathUtils.join(output, "report.json"), report);
    }
  }
  ChromeUtils.unregisterWindowActor("CorpusInspector");
  console.log(`[Actual corpus] report ${output}/report.json`);
}
