/// <reference path="../../libs/@types/gecko/index.d.ts" />
// SPDX-License-Identifier: MPL-2.0
import { zipSync } from "fflate/browser";
import { transformManifest } from "../../browser-features/modules/modules/chrome-web-store/ManifestTransformer.sys.mts";
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

/** @param {Record<string,string>} files @param {string} id @param {string} outputPath */
export async function runComparison(files, id, outputPath) {
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  );
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  );
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  const manifest = transformManifest(JSON.parse(files["manifest.json"]), id);
  // This private-profile fixture uses Gecko's actual browser alert renderer.
  // It does not depend on an OS notification daemon in a headless environment.
  const useBrowserAlerts = manifest.permissions?.includes("notifications");
  const hadAlertPref = Services.prefs.prefHasUserValue(
    "alerts.useSystemBackend",
  );
  const previousAlertPref = Services.prefs.getBoolPref(
    "alerts.useSystemBackend",
  );
  if (useBrowserAlerts) {
    Services.prefs.setBoolPref("alerts.useSystemBackend", false);
  }
  const encoded = Object.fromEntries(
    Object.entries({ ...files, "manifest.json": JSON.stringify(manifest) })
      .map(([path, content]) => [path, new TextEncoder().encode(content)]),
  );
  const path = PathUtils.join(
    PathUtils.tempDir,
    `floorp-comparison-${Services.uuid.generateUUID()}.xpi`,
  );
  await IOUtils.write(path, zipSync(encoded));
  let addon, tab;
  try {
    addon = await AddonManager.installTemporaryAddon(new FileUtils.File(path));
    const extension = ExtensionParent.GlobalManager.extensionMap.get(
      `${id}@chromium-extension`,
    );
    const browserWindow =
      /** @type {import("./crx-test-types.ts").BrowserWindow} */ (/** @type {unknown} */ (Services
        .wm.getMostRecentWindow("navigator:browser")));
    tab = browserWindow.gBrowser.addTab(
      extension.baseURI.resolve("probe.html"),
      {
        triggeringPrincipal: extension.principal,
      },
    );
    const deadline = Date.now() + 20000;
    while (Date.now() < deadline) {
      try {
        const value = JSON.parse(tab.linkedBrowser.contentTitle);
        if (typeof value.passed === "boolean") {
          await IOUtils.writeJSON(outputPath, {
            ...value,
            transformedBackground: manifest.background,
            runtime: {
              version: Services.appinfo.version,
              buildId: Services.appinfo.appBuildID,
              nativeWorkerPolicy:
                WebExtensionPolicy.backgroundServiceWorkerEnabled,
            },
          });
          if (!value.passed) throw new Error(value.error);
          return;
        }
      } catch (error) {
        if (error instanceof SyntaxError) { /* Wait for fixture results. */ }
        else throw error;
      }
      await new Promise((resolve) => setTimeout(resolve, 30));
    }
    throw new Error("Timed out waiting for Floorp comparison fixture");
  } finally {
    if (tab) {
      const browserWindow =
        /** @type {import("./crx-test-types.ts").BrowserWindow} */ (/** @type {unknown} */ (Services
          .wm.getMostRecentWindow("navigator:browser")));
      browserWindow.gBrowser.removeTab(tab);
    }
    await addon?.uninstall();
    await IOUtils.remove(path);
    if (useBrowserAlerts) {
      if (hadAlertPref) {
        Services.prefs.setBoolPref(
          "alerts.useSystemBackend",
          previousAlertPref,
        );
      } else Services.prefs.clearUserPref("alerts.useSystemBackend");
    }
  }
}
