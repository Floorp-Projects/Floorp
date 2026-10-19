// SPDX-License-Identifier: MPL-2.0
import { zipSync } from "fflate/browser";
import { installFrozenCorpus, registerCorpusOperations, sha256 } from "./corpus-install-fixture.mjs";
const { AddonManager } = ChromeUtils.importESModule("resource://gre/modules/AddonManager.sys.mjs");
const { FileUtils } = ChromeUtils.importESModule("resource://gre/modules/FileUtils.sys.mjs");
const { ExtensionParent } = ChromeUtils.importESModule("resource://gre/modules/ExtensionParent.sys.mjs");
const { setTimeout } = ChromeUtils.importESModule("resource://gre/modules/Timer.sys.mjs");
const delay = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
async function until(read, label) {
  const deadline = Date.now() + 25000;
  while (Date.now() < deadline) { const value = await read(); if (value) return value; await delay(100); }
  throw Error("Timed out waiting for " + label);
}
export async function runAllTests() {
  const root = Services.env.get("FLOORP_CORPUS_ROOT");
  if (!root) throw Error("FLOORP_CORPUS_ROOT is required");
  const corpus = await IOUtils.readJSON(PathUtils.join(root, "tools", "chrome-extension-runtime", "corpus-lock.json"));
  const id = "jjmflmamggggndanpgfnpelongoepncg";
  const frozen = corpus.extensions.find((item) => item.id === id);
  const output = PathUtils.join(root, "_dist", "chrome-runtime-tests", "corpus-management-floorp-" + Date.now());
  await IOUtils.makeDirectory(output);
  const fixtureFiles = {
    "manifest.json": JSON.stringify({ manifest_version: 3, name: "Authored lifecycle probe", version: "1.0", browser_specific_settings: { gecko: { id: "authored-lifecycle@floorp.test" } }, content_scripts: [{ matches: ["http://127.0.0.1/*"], js: ["content.js"] }] }),
    "content.js": "document.documentElement.dataset.authoredLifecycleProbe = 'active';",
  };
  const report = {
    stage: "actual-package-primary-operation", output,
    runtime: { version: Services.appinfo.version, buildId: Services.appinfo.appBuildID, workerPolicy: WebExtensionPolicy.backgroundServiceWorkerEnabled },
    fixture: { files: {} },
    extension: { id, name: frozen.name, version: frozen.version, packageSha256: frozen.packageSha256, primaryOperation: frozen.primaryOperation, primaryOperationResult: "not-run", checks: [] },
  };
  const save = () => IOUtils.writeJSON(PathUtils.join(output, "report.json"), report);
  const bytes = {};
  for (const [name, text] of Object.entries(fixtureFiles)) {
    bytes[name] = new TextEncoder().encode(text);
    report.fixture.files[name] = await sha256(bytes[name]);
  }
  const path = PathUtils.join(output, "authored-lifecycle.xpi");
  await IOUtils.write(path, zipSync(bytes));
  registerCorpusOperations(root);
  const window = Services.wm.getMostRecentWindow("navigator:browser");
  const originalTabs = new Set(window.gBrowser.tabs);
  let authored, addon;
  const actor = (tab) => tab.linkedBrowser.browsingContext.currentWindowGlobal?.getActor("CorpusOperations");
  const snapshot = (tab) => actor(tab)?.sendQuery("Snapshot");
  try {
    authored = await AddonManager.installTemporaryAddon(new FileUtils.File(path));
    const installed = await installFrozenCorpus(root, frozen, output);
    addon = installed.addon;
    report.extension.unchangedScriptCount = installed.unchangedScriptCount;
    report.extension.unchangedScriptsSha256 = installed.unchangedScriptsSha256;
    const fixture = window.wrappedJSObject.__floorpRuntimeFixtureURL;
    report.fixture.url = fixture;
    const page = window.gBrowser.addTab(fixture, { triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal() });
    await until(async () => (await snapshot(page).catch(() => null))?.probe === "active", "initial authored script");
    const extension = ExtensionParent.GlobalManager.extensionMap.get(id + "@chromium-extension");
    if (!extension) throw Error("Native extension policy missing");
    const ui = window.gBrowser.addTab(extension.baseURI.resolve("index.html"), { triggeringPrincipal: extension.principal });
    window.gBrowser.selectedTab = ui;
    await until(async () => (await snapshot(ui).catch(() => null))?.text?.includes("Authored lifecycle probe"), "real Extensity extension list");
    report.extension.before = await snapshot(ui);
    const click = () => actor(ui).sendQuery("Click", { selector: "li.extension", text: "Authored lifecycle probe" });
    await click();
    await until(() => authored.userDisabled, "Extensity disable");
    page.linkedBrowser.reload();
    await until(async () => { const value = await snapshot(page).catch(() => null); return value?.ready === "complete" && !value.probe; }, "content script absent after disable");
    report.extension.checks.push("Trusted click on unchanged Extensity UI disabled the authored extension and stopped its actual content script");
    await click();
    await until(() => !authored.userDisabled && authored.isActive, "Extensity re-enable");
    page.linkedBrowser.reload();
    await until(async () => (await snapshot(page).catch(() => null))?.probe === "active", "content script restored after enable");
    report.extension.checks.push("Second trusted UI click re-enabled the extension and restored its actual content script");
    report.extension.after = await snapshot(ui);
    report.extension.primaryOperationResult = "passed";
  } catch (error) {
    report.extension.primaryOperationResult = "failed";
    report.extension.error = String(error);
    throw error;
  } finally {
    await addon?.uninstall(); await authored?.uninstall();
    for (const tab of [...window.gBrowser.tabs]) if (!originalTabs.has(tab)) window.gBrowser.removeTab(tab);
    ChromeUtils.unregisterWindowActor("CorpusOperations");
    await save();
    console.log("[Actual corpus] Extensity " + report.extension.primaryOperationResult + " " + output);
  }
}
