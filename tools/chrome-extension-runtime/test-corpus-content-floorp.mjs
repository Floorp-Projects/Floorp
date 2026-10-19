// SPDX-License-Identifier: MPL-2.0
/** @import {Truthy, NativeBinaryInput, CorpusLock, CorpusReport, CorpusOperation, ContentSnapshot, NativeWindowHandle, NativeTabHandle, ActorHandle, ActorClickData, ContentActorResponses, AssessmentExtensionParentModule, ResourceProtocolHandler} from "./assessment-test-types.ts" */
import { unzipSync } from "fflate/browser";
import { CRXConverterClass } from "../../browser-features/modules/modules/chrome-web-store/CRXConverter.sys.mts";
const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
const { AddonManager } = ChromeUtils.importESModule(
  "resource://gre/modules/AddonManager.sys.mjs",
);
const { ExtensionParent } =
  /** @type {AssessmentExtensionParentModule} */ (ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  ));
const { FileUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/FileUtils.sys.mjs",
);
/** @param {number} ms @returns {Promise<void>} */
const pause = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
/** @template {keyof ContentActorResponses} Name @param {ActorHandle|undefined} actor @param {Name} name @param {ActorClickData} [data] @returns {Promise<ContentActorResponses[Name]>} */
async function query(actor, name, data) {
  if (!actor) throw Error("Harness actor unavailable: " + name);
  let timer;
  try {
    return /** @type {ContentActorResponses[Name]} */ (await Promise.race([
      actor.sendQuery(name, data),
      new Promise((_, reject) => {
        timer = setTimeout(
          () => reject(Error("Harness actor query timed out: " + name)),
          5000,
        );
      }),
    ]));
  } finally {
    clearTimeout(timer);
  }
}
/** @template T @param {()=>T|Promise<T>} read @param {string} label @returns {Promise<Truthy<T>>} */
async function until(read, label) {
  const end = Date.now() + 30000;
  while (Date.now() < end) {
    const value = await read();
    if (value) return /** @type {Truthy<T>} */ (value);
    await pause(100);
  }
  throw Error("Timed out waiting for " + label);
}
/** @param {NativeBinaryInput} bytes */
const digest = async (bytes) =>
  [
    ...new Uint8Array(
      await crypto.subtle.digest("SHA-256", new Uint8Array(bytes)),
    ),
  ].map(
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
  /** @type {CorpusLock} */
  const corpus = await IOUtils.readJSON(
    PathUtils.join(
      root,
      "tools",
      "chrome-extension-runtime",
      "corpus-lock.json",
    ),
  );
  /** @type {CorpusReport<ContentSnapshot>} */
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
  const resource =
    /** @type {ResourceProtocolHandler} */ (Services.io.getProtocolHandler(
      "resource",
    )).QueryInterface(
      Ci.nsIResProtocolHandler,
    );
  resource.setSubstitution(
    "floorp-content-corpus",
    Services.io.newFileURI(
      new FileUtils.File(
        Services.env.get("FLOORP_TEST_ACTOR_DIRECTORY") ||
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
  ChromeUtils.registerWindowActor("CorpusContentUI", {
    child: {
      esModuleURI:
        "resource://floorp-content-corpus/CorpusContentUIChild.sys.mjs",
    },
    allFrames: false,
    remoteTypes: ["extension"],
  });
  const win =
    /** @type {NativeWindowHandle} */ (/** @type {unknown} */ (Services.wm
      .getMostRecentWindow("navigator:browser")));
  const principal = Services.scriptSecurityManager.getSystemPrincipal();
  const converter = new CRXConverterClass();
  /** @param {NativeTabHandle} tab @returns {Promise<ContentSnapshot>} */
  const snapshot = (tab) =>
    query(
      tab.linkedBrowser.browsingContext.currentWindowGlobal?.getActor(
        "CorpusContent",
      ),
      "Snapshot",
    );
  try {
    for (
      const record of corpus.extensions.filter((item) =>
        selected.includes(item.id)
      )
    ) {
      /** @type {CorpusOperation<ContentSnapshot>} */
      /** @type {CorpusOperation<ContentSnapshot>} */
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
        try {
          item.baseline = await until(async () => {
            const value = await snapshot(tab).catch(() => null);
            return value?.ready === "complete" && value.text ? value : null;
          }, "authored baseline");
        } catch (error) {
          item.primaryOperationResult = "harness-fixture-unavailable";
          throw error;
        }
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
        const conversion = await converter.convert(
          bytes.slice().buffer,
          record.id,
          {
            id: record.id,
            name: record.name,
            version: record.version,
          },
        );
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
        const baseline = item.baseline;
        if (!baseline) throw Error("Authored page baseline is missing");
        if (record.name === "Stylus") {
          const editor = win.gBrowser.addTab(
            extension.baseURI.resolve("edit.html?domain=127.0.0.1"),
            { triggeringPrincipal: extension.principal },
          );
          win.gBrowser.selectedTab = editor;
          const ui = () =>
            editor.linkedBrowser.browsingContext.currentWindowGlobal?.getActor(
              "CorpusContentUI",
            );
          item.editorBefore = await until(async () => {
            const value = await query(ui(), "SnapshotUI").catch(() => null);
            return value?.ready === "complete" && value.hasEditor
              ? value
              : null;
          }, "actual Stylus editor");
          await query(ui(), "Type", {
            selector: "#name",
            text: "Floorp authored primary style",
          });
          await query(ui(), "Type", {
            selector: ".CodeMirror textarea",
            text: "#probe { background-color: rgb(12, 34, 56) !important; }",
          });
          item.editorAfterTyping = await query(ui(), "SnapshotUI");
          await query(ui(), "Click", { selector: "#save-button" });
          item.editorSaved = await until(async () => {
            const value = await query(ui(), "SnapshotUI").catch(() => null);
            return value?.url?.includes("id=") && value.saveDisabled
              ? value
              : null;
          }, "style saved through native editor controls");
          item.checks.push(
            "Authored CSS saved through the unchanged Stylus editor using native text and pointer input",
          );
          win.gBrowser.removeTab(editor);
          win.gBrowser.selectedTab = tab;
        } else if (["Violentmonkey", "Tampermonkey"].includes(record.name)) {
          const editorPath = record.name === "Violentmonkey"
            ? "options/index.html#scripts/_new"
            : "options.html#nav=new-user-script";
          const editor = win.gBrowser.addTab(
            extension.baseURI.resolve(editorPath),
            {
              triggeringPrincipal: extension.principal,
            },
          );
          win.gBrowser.selectedTab = editor;
          const ui = () =>
            editor.linkedBrowser.browsingContext.currentWindowGlobal?.getActor(
              "CorpusContentUI",
            );
          item.editorBefore = await until(async () => {
            const value = await query(ui(), "SnapshotUI").catch(() => null);
            return value?.hasEditor && value;
          }, "actual userscript editor");
          const code = [
            "// ==UserScript==",
            "// @name Floorp authored primary userscript",
            "// @namespace floorp-local-authored",
            "// @version 1.0",
            `// @include ${fixture}*`,
            "// @grant none",
            "// @inject-into content",
            "// @run-at document-end",
            "// ==/UserScript==",
            'document.documentElement.dataset.floorpUserScript = "authored-primary-operation";',
            'document.querySelector("#probe")?.style.setProperty("background-color", "rgb(12, 34, 56)", "important");',
          ].join("\n");
          item.authoredUserscriptSha256 = await digest(
            new TextEncoder().encode(code),
          );
          await query(ui(), "Type", {
            selector: ".CodeMirror textarea",
            text: code,
          });
          item.editorAfterTyping = await query(ui(), "SnapshotUI");
          if (item.editorAfterTyping.code !== code) {
            throw Error("Native editor input differs from authored userscript");
          }
          if (record.name === "Tampermonkey") {
            await query(ui(), "Click", { menuText: "File" });
          }
          await query(
            ui(),
            "Click",
            record.name === "Violentmonkey"
              ? { buttonText: "Save & Close" }
              : { menuText: "Save" },
          );
          if (record.name === "Tampermonkey") {
            await pause(500);
            item.afterNativeSave = await query(ui(), "SnapshotUI");
            editor.linkedBrowser.loadURI(
              Services.io.newURI(
                extension.baseURI.resolve("options.html#nav=dashboard"),
              ),
              { triggeringPrincipal: extension.principal },
            );
          }
          item.editorSaved = await until(async () => {
            const value = await query(ui(), "SnapshotUI").catch(() => null);
            return value && !value.hasEditor &&
              value.text?.includes("Floorp authored primary userscript") &&
              value;
          }, "actual saved userscript in native dashboard");
          item.checks.push(
            "Authored matching userscript saved through unchanged extension editor using native text and pointer input",
          );
          win.gBrowser.removeTab(editor);
          win.gBrowser.selectedTab = tab;
        } else if (record.name !== "Dark Reader") {
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
              (["Violentmonkey", "Tampermonkey"].includes(record.name)
                ? value?.userScript === "authored-primary-operation" &&
                  value.frameUserScript === "authored-primary-operation" &&
                  value.probe === "rgb(12, 34, 56)"
                : record.name === "Stylus"
                ? value?.probe === "rgb(12, 34, 56)"
                : value?.markers?.length && value.body !== baseline.body)
            ? value
            : null;
        }, "actual unchanged extension content effect");
        item.checks.push(
          `Actual signed-package ${record.name} script transformed the authored page`,
        );
        tab.linkedBrowser.loadURI(Services.io.newURI(fixture + "second"), {
          triggeringPrincipal: principal,
        });
        item.afterNavigation = await until(async () => {
          const value = await snapshot(tab).catch(() => null);
          return value?.url === fixture + "second" &&
              (["Violentmonkey", "Tampermonkey"].includes(record.name)
                ? value?.userScript === "authored-primary-operation" &&
                  value.frameUserScript === "authored-primary-operation" &&
                  value.probe === "rgb(12, 34, 56)"
                : record.name === "Stylus"
                ? value.probe === "rgb(12, 34, 56)"
                : value.markers.length && value.body !== baseline.body)
            ? value
            : null;
        }, "theme after native navigation");
        item.checks.push("Transformation persists after real page navigation");
        await addon.uninstall();
        addon = null;
        tab.linkedBrowser.reload();
        item.afterRemoval = await until(async () => {
          const value = await snapshot(tab).catch(() => null);
          return value?.body === baseline.body &&
              value.probe === baseline.probe && !value.markers.length
            ? value
            : null;
        }, "authored page restored after actual uninstall");
        item.checks.push("Uninstall removes the real transformation");
        item.primaryOperationResult = "passed";
      } catch (error) {
        if (item.primaryOperationResult !== "harness-fixture-unavailable") {
          item.primaryOperationResult = "failed";
        }
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
    ChromeUtils.unregisterWindowActor("CorpusContentUI");
    ChromeUtils.unregisterWindowActor("CorpusContent");
    resource.setSubstitution("floorp-content-corpus", null);
    await save();
  }
  console.log(
    "[Content corpus] REPORT " + PathUtils.join(output, "report.json"),
  );
  if (
    !report.extensions.length ||
    report.extensions.some((item) => item.primaryOperationResult !== "passed")
  ) {
    throw Error(
      "Content corpus primary operation did not pass; see " +
        PathUtils.join(output, "report.json"),
    );
  }
}
