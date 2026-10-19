// SPDX-License-Identifier: MPL-2.0
import { zipSync } from "fflate/browser";
import {
  installFrozenCorpus,
  registerCorpusOperations,
  sha256,
} from "./corpus-install-fixture.mjs";
const { AddonManager } = ChromeUtils.importESModule(
  "resource://gre/modules/AddonManager.sys.mjs",
);
const { FileUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/FileUtils.sys.mjs",
);
/** @type {{ExtensionParent: import("./native-test-types.ts").NativeCorpusExtensionParent}} */
const { ExtensionParent } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionParent.sys.mjs",
);
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
/** @param {number} ms */
const delay = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
/** @template T @param {() => T | Promise<T>} read @param {string} label */
async function until(read, label) {
  const deadline = Date.now() + 25000;
  while (Date.now() < deadline) {
    const value = await read();
    if (value) return value;
    await delay(100);
  }
  throw Error("Timed out waiting for " + label);
}
export async function runAllTests() {
  const root = Services.env.get("FLOORP_CORPUS_ROOT");
  if (!root) throw Error("FLOORP_CORPUS_ROOT is required");
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
  const id = "jjmflmamggggndanpgfnpelongoepncg";
  const frozen = corpus.extensions.find((item) => item.id === id);
  if (!frozen) throw Error("Frozen extension missing");
  const output = PathUtils.join(
    root,
    "_dist",
    "chrome-runtime-tests",
    "corpus-management-floorp-" + Date.now(),
  );
  await IOUtils.makeDirectory(output);
  const fixtureFiles = {
    "manifest.json": JSON.stringify({
      manifest_version: 3,
      name: "Authored lifecycle probe",
      version: "1.0",
      permissions: ["management"],
      background: { scripts: ["background.js"] },
      browser_specific_settings: {
        gecko: { id: "authored-lifecycle@floorp.test" },
      },
      content_scripts: [{
        matches: ["http://127.0.0.1/*"],
        js: ["content.js"],
      }],
    }),
    "content.js":
      "document.documentElement.dataset.authoredLifecycleProbe = 'active';",
    "background.js":
      `document.title='ordinary-management-ready';window.addEventListener('message',async({data})=>{if(!data?.ordinaryManagementProbe)return;try{await browser.management.setEnabled(data.target,false);document.title=JSON.stringify({unexpectedlyAllowed:true})}catch(error){document.title=JSON.stringify({error:String(error)})}});`,
  };
  /** @type {import("./browser-tools-test-types.ts").NativeBrowserToolsReport} */
  const report = {
    stage: "actual-package-primary-operation",
    output,
    runtime: {
      version: Services.appinfo.version,
      buildId: Services.appinfo.appBuildID,
      workerPolicy: WebExtensionPolicy.backgroundServiceWorkerEnabled,
    },
    fixture: { files: {}, pages: [] },
    extension: {
      id,
      name: frozen.name,
      version: frozen.version,
      packageSha256: frozen.packageSha256,
      primaryOperation: frozen.primaryOperation,
      primaryOperationResult: "not-run",
      checks: [],
      snapshots: [],
    },
  };
  const save = () =>
    IOUtils.writeJSON(PathUtils.join(output, "report.json"), report);
  /** @type {Record<string, Uint8Array>} */
  const bytes = {};
  const fileHashes = report.fixture.files;
  if (!fileHashes) throw Error("Fixture hash ledger missing");
  for (const [name, text] of Object.entries(fixtureFiles)) {
    bytes[name] = new TextEncoder().encode(text);
    fileHashes[name] = await sha256(bytes[name]);
  }
  const path = PathUtils.join(output, "authored-lifecycle.xpi");
  await IOUtils.write(path, zipSync(bytes));
  registerCorpusOperations(root);
  const window =
    /** @type {import("./native-test-types.ts").NativeCorpusWindow} */ (Services
      .wm.getMostRecentWindow("navigator:browser"));
  const originalTabs = new Set(window.gBrowser.tabs);
  /** @type {import("./native-test-types.ts").NativeCorpusAddon | undefined} */
  let authored, addon;
  /** @param {import("./native-test-types.ts").NativeCorpusTab} tab */
  const actor = (tab) => {
    const actor = tab.linkedBrowser.browsingContext.currentWindowGlobal
      ?.getActor("CorpusOperations");
    if (!actor) throw Error("Corpus actor unavailable");
    return actor;
  };
  /** @param {import("./native-test-types.ts").NativeCorpusTab} tab @returns {Promise<import("./browser-tools-test-types.ts").OperationSnapshot>} */
  const snapshot = (tab) => actor(tab).sendQuery("Snapshot");
  try {
    const authoredAddon = await AddonManager.installTemporaryAddon(
      new FileUtils.File(path),
    );
    authored = authoredAddon;
    const installed = await installFrozenCorpus(root, frozen, output);
    addon = installed.addon;
    report.extension.unchangedScriptCount = installed.unchangedScriptCount;
    report.extension.unchangedScriptsSha256 = installed.unchangedScriptsSha256;
    const ordinary = ExtensionParent.GlobalManager.extensionMap.get(
      "authored-lifecycle@floorp.test",
    );
    const ordinaryBrowser = await until(
      () => {
        const browser = ordinary?.backgroundContext?.xulBrowser;
        return browser?.contentTitle === "ordinary-management-ready"
          ? browser
          : undefined;
      },
      "ordinary Firefox API control",
    );
    ordinaryBrowser.messageManager.loadFrameScript(
      "data:application/javascript," +
        encodeURIComponent(
          `content.postMessage(${
            JSON.stringify({
              ordinaryManagementProbe: true,
              target: id + "@chromium-extension",
            })
          },'*')`,
        ),
      false,
    );
    report.fixture.ordinaryFirefoxControl = await until(() => {
      try {
        return /** @type {{error?: string, unexpectedlyAllowed?: boolean}} */ (JSON
          .parse(ordinaryBrowser.contentTitle));
      } catch {
        return undefined;
      }
    }, "ordinary Firefox management denial");
    if (
      !report.fixture.ordinaryFirefoxControl.error?.includes(
        "setEnabled can only",
      ) || !addon.isActive
    ) throw Error("Ordinary Firefox management boundary changed");
    const fixture = window.wrappedJSObject.__floorpRuntimeFixtureURL;
    report.fixture.url = fixture;
    const page = window.gBrowser.addTab(fixture, {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
    await until(
      async () => (await snapshot(page).catch(() => null))?.probe === "active",
      "initial authored script",
    );
    const extension = ExtensionParent.GlobalManager.extensionMap.get(
      id + "@chromium-extension",
    );
    if (!extension) throw Error("Native extension policy missing");
    const ui = window.gBrowser.addTab(extension.baseURI.resolve("index.html"), {
      triggeringPrincipal: extension.principal,
    });
    window.gBrowser.selectedTab = ui;
    await until(
      async () =>
        (await snapshot(ui).catch(() => null))?.text?.includes(
          "Authored lifecycle probe",
        ),
      "real Extensity extension list",
    );
    report.extension.before = await snapshot(ui);
    const click = () =>
      actor(ui).sendQuery("Click", {
        selector: "li.extension",
        text: "Authored lifecycle probe",
      });
    await click();
    await until(() => authoredAddon.userDisabled, "Extensity disable");
    page.linkedBrowser.reload();
    await until(async () => {
      const value = await snapshot(page).catch(() => null);
      return value?.ready === "complete" && !value.probe;
    }, "content script absent after disable");
    report.extension.checks.push(
      "Trusted click on unchanged Extensity UI disabled the authored extension and stopped its actual content script",
    );
    await click();
    await until(
      () => !authoredAddon.userDisabled && authoredAddon.isActive,
      "Extensity re-enable",
    );
    page.linkedBrowser.reload();
    await until(
      async () => (await snapshot(page).catch(() => null))?.probe === "active",
      "content script restored after enable",
    );
    report.extension.checks.push(
      "Second trusted UI click re-enabled the extension and restored its actual content script",
    );
    report.extension.after = await snapshot(ui);
    report.extension.primaryOperationResult = "passed";
  } catch (error) {
    report.extension.primaryOperationResult = "failed";
    report.extension.error = String(error);
    throw error;
  } finally {
    await addon?.uninstall();
    await authored?.uninstall();
    for (const tab of [...window.gBrowser.tabs]) {
      if (!originalTabs.has(tab)) window.gBrowser.removeTab(tab);
    }
    ChromeUtils.unregisterWindowActor("CorpusOperations");
    await save();
    console.log(
      "[Actual corpus] Extensity " + report.extension.primaryOperationResult +
        " " + output,
    );
  }
}
