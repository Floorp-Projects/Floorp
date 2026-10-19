/// <reference path="../../libs/@types/gecko/index.d.ts" />
// SPDX-License-Identifier: MPL-2.0
import { invokeNativeCorpusAction } from "./corpus-native-action.mjs";
import { installFrozenCorpus, sha256 } from "./corpus-install-fixture.mjs";
const { ExtensionParent } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionParent.sys.mjs",
);
const { FileUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/FileUtils.sys.mjs",
);
const { Downloads } = ChromeUtils.importESModule(
  "resource://gre/modules/Downloads.sys.mjs",
);
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
/** @param {number} ms */
const delay = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
/** @template T @param {()=>Promise<T|false|null|undefined>|T|false|null|undefined} read @param {string} label @param {number} [timeout] @returns {Promise<T>} */
async function until(read, label, timeout = 25000) {
  const end = Date.now() + timeout;
  while (Date.now() < end) {
    const value = await read();
    if (value) return value;
    await delay(100);
  }
  throw Error("Timed out waiting for " + label);
}
export async function runAllTests() {
  const root = Services.env.get("FLOORP_CORPUS_ROOT"),
    url = Services.env.get("FLOORP_CORPUS_FIXTURE"),
    output = Services.env.get("FLOORP_CORPUS_OUTPUT");
  if (!root || !url || !output) {
    throw Error("Private authored media fixture environment missing");
  }
  const id = "lmjnegcaeklhafolokijcfjliaokphfk";
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
  const record = corpus.extensions.find((e) => e.id === id);
  if (!record) throw Error("Frozen Video DownloadHelper record missing");
  /** @type {import("./crx-test-types.ts").MediaItem & {input:import("./crx-test-types.ts").ClickEvidence[]}} */
  const item = {
    id,
    name: record.name,
    version: record.version,
    packageSha256: record.packageSha256,
    primaryOperationAsSelected: record.primaryOperation,
    actualAdvertisedOperation:
      "Discover media and save a playable file through the vendor v10 download engine",
    nativeCompanionApplicability: "not-applicable-v10",
    nativeCompanionSource:
      "https://github.com/aclap-dev/vdhcoapp/blob/f3736cee7013d4f9003884d9f8077c4c16d2ee4f/README.md",
    primaryOperationResult: "not-run",
    snapshots: [],
    input: [],
  };
  /** @type {import("./crx-test-types.ts").MediaReport} */
  const report = {
    stage: "actual-package-primary-operation",
    browser: "Floorp",
    runtime: {
      version: Services.appinfo.version,
      buildId: Services.appinfo.appBuildID,
    },
    fixture: JSON.parse(
      Services.env.get("FLOORP_CORPUS_FIXTURE_METADATA") || "{}",
    ),
    output,
    extension: item,
  };
  const save = () =>
    IOUtils.writeJSON(PathUtils.join(output, "report.json"), report);
  const directory = PathUtils.join(output, "downloads");
  await IOUtils.makeDirectory(directory);
  const prefNames = [
    "browser.download.dir",
    "browser.download.folderList",
    "browser.download.useDownloadDir",
    "media.autoplay.default",
  ];
  const oldPrefs = prefNames.map((name) => ({
    name,
    user: Services.prefs.prefHasUserValue(name),
    value: Services.prefs.getPrefType(name) === Services.prefs.PREF_STRING
      ? Services.prefs.getStringPref(name)
      : Services.prefs.getPrefType(name) === Services.prefs.PREF_BOOL
      ? Services.prefs.getBoolPref(name)
      : Services.prefs.getIntPref(name, 0),
  }));
  Services.prefs.setStringPref("browser.download.dir", directory);
  Services.prefs.setIntPref("browser.download.folderList", 2);
  Services.prefs.setBoolPref("browser.download.useDownloadDir", true);
  Services.prefs.setIntPref("media.autoplay.default", 0);
  const protocol = Services.io.getProtocolHandler("resource");
  if (!protocol.QueryInterface) {
    throw Error("Resource protocol lacks its XPCOM interface");
  }
  const resources =
    /** @type {{setSubstitution(name:string,uri:nsIURI|null):void}} */ (/** @type {unknown} */ (protocol
      .QueryInterface(Ci.nsIResProtocolHandler)));
  resources.setSubstitution(
    "floorp-downloadhelper-fixture",
    Services.io.newFileURI(
      new FileUtils.File(Services.env.get("FLOORP_TEST_ACTOR_DIRECTORY")),
    ),
  );
  ChromeUtils.registerWindowActor("CorpusDownloadHelper", {
    child: {
      esModuleURI:
        "resource://floorp-downloadhelper-fixture/CorpusDownloadHelperChild.sys.mjs",
    },
    allFrames: false,
    safeForUntrustedWebProcess: true,
  });
  const window =
      /** @type {import("./native-test-types.ts").NativeCorpusWindow} */ (/** @type {unknown} */ (Services
        .wm.getMostRecentWindow("navigator:browser"))),
    before = new Set(window.gBrowser.tabs);
  /** @param {import("./native-test-types.ts").NativeCorpusBrowser} browser */
  const actorFor = (browser) => {
    const actor = browser.browsingContext.currentWindowGlobal?.getActor(
      "CorpusDownloadHelper",
    );
    if (!actor) throw Error("Private media actor unavailable");
    return /** @type {import("./crx-test-types.ts").MediaActor} */ (actor);
  };
  /** @param {import("./native-test-types.ts").NativeCorpusBrowser} browser @param {string} label */
  const snapshot = async (browser, label) => {
    const state = await actorFor(browser).sendQuery("Snapshot");
    item.snapshots.push({ label, ...state });
    await save();
    return state;
  };
  let addon;
  try {
    const installed = await installFrozenCorpus(root, record, output);
    addon = installed.addon;
    Object.assign(item, {
      unchangedScriptCount: installed.unchangedScriptCount,
      unchangedScriptsSha256: installed.unchangedScriptsSha256,
      sourceIntegrity: installed.sourceIntegrity,
      nativeEvidence: installed.nativeEvidence,
      warnings: installed.warnings,
    });
    const extension =
      /** @type {import("./native-test-types.ts").NativeCorpusExtension|undefined} */ (ExtensionParent
        .GlobalManager.extensionMap.get(`${id}@chromium-extension`));
    if (!extension) throw Error("Actual installed extension policy missing");
    await until(
      () => extension.backgroundContext?.viewType === "background_worker",
      "actual extension ServiceWorker background",
    );
    await delay(1500);
    const tab = window.gBrowser.addTab(url, {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
    window.gBrowser.selectedTab = tab;
    item.playback = await until(async () => {
      const state = await actorFor(tab.linkedBrowser)?.sendQuery("Playback")
        .catch(() => null);
      return state && state.currentTime > 0.2 && state.width === 640 && state;
    }, "actual authored video playback");
    await delay(1500);
    item.nativeAction = await invokeNativeCorpusAction(extension, window, tab);
    const panel = await until(
      () =>
        [...extension.views].find((view) =>
          view.viewType === "popup" || view.viewType === "sidebar"
        ),
      "genuine vendor action popup",
    );
    const browser = panel.xulBrowser;
    if (!browser) throw Error("Genuine vendor panel browser is unavailable");
    const initial = await until(async () => {
      const state = await actorFor(browser)?.sendQuery("Snapshot").catch(() =>
        null
      );
      return state?.controls.some((c) => c.id === "action_download") && state;
    }, "actual discovered media Download control");
    await snapshot(browser, "actual-discovered-media-panel");
    if (initial.controls.some((c) => c.label === "Accept")) {
      item.input.push(
        await actorFor(browser).sendQuery("Click", { text: "Accept" }),
      );
      item.acceptedVendorTermsForAuthoredMedia = true;
      await delay(500);
    }
    item.input.push(
      await actorFor(browser).sendQuery("Click", { text: "Download" }),
    );
    await delay(1000);
    await snapshot(browser, "after-native-download-click");
    const list =
      /** @type {{getAll():Promise<import("./crx-test-types.ts").Download[]>}} */ (await Downloads
        .getList(Downloads.ALL));
    const download = await until(
      async () =>
        (await list.getAll()).find((download) =>
          download.succeeded &&
          /\.(mp4|mkv|webm)$/i.test(download.target.path) &&
          download.target.path.replaceAll("\\", "/").startsWith(
            directory.replaceAll("\\", "/") + "/",
          )
        ),
      "genuine completed video download",
      45000,
    );
    const bytes = await IOUtils.read(download.target.path);
    item.savedFile = {
      path: download.target.path,
      bytes: bytes.length,
      sha256: await sha256(bytes),
      source: download.source.url,
    };
    if (bytes.length < 100000) throw Error("Saved video is unexpectedly small");
    item.primaryOperationResult =
      "browser-download-completed-awaiting-media-validation";
  } catch (error) {
    item.primaryOperationResult = "failed";
    item.error = String(error);
    item.additionalErrors =
      typeof error === "object" && error !== null && "additionalErrors" in error
        ? error.additionalErrors
        : undefined;
    throw error;
  } finally {
    await addon?.uninstall();
    for (const tab of [...window.gBrowser.tabs]) {
      if (!before.has(tab)) {
        window.gBrowser.removeTab(tab, { skipPermitUnload: true });
      }
    }
    ChromeUtils.unregisterWindowActor("CorpusDownloadHelper");
    resources.setSubstitution("floorp-downloadhelper-fixture", null);
    for (const pref of oldPrefs) {
      if (!pref.user) Services.prefs.clearUserPref(pref.name);
      else if (typeof pref.value === "string") {
        Services.prefs.setStringPref(pref.name, pref.value);
      } else if (typeof pref.value === "boolean") {
        Services.prefs.setBoolPref(pref.name, pref.value);
      } else Services.prefs.setIntPref(pref.name, pref.value);
    }
    await save();
    console.log("REPORT " + PathUtils.join(output, "report.json"));
  }
}
