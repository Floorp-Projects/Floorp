// SPDX-License-Identifier: MPL-2.0
/** @import {Truthy, NativeBinaryInput, CorpusLock, CorpusReport, CorpusOperation, DevtoolsSnapshot, NativeWindowHandle, NativeBrowserHandle, ActorClickData, DevtoolsActorResponses, AssessmentExtensionParentModule, ResourceProtocolHandler} from "./assessment-test-types.ts" */
import { unzipSync } from "fflate/browser";
import { CRXConverterClass } from "../../browser-features/modules/modules/chrome-web-store/CRXConverter.sys.mts";
import { attachNativeFloorpDevtools } from "./floorp-devtools-panel.mjs";
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
/** @template {keyof DevtoolsActorResponses} Name @param {NativeBrowserHandle} browser @param {Name} name @param {ActorClickData} [data] @returns {Promise<DevtoolsActorResponses[Name]>} */
async function query(browser, name, data) {
  const actor = browser.browsingContext.currentWindowGlobal?.getActor(
    "CorpusDevtools",
  );
  if (!actor) throw Error("Private fixture actor is unavailable");
  let timer;
  try {
    return /** @type {DevtoolsActorResponses[Name]} */ (await Promise.race([
      actor.sendQuery(name, data),
      new Promise((_, reject) => {
        timer = setTimeout(
          () => reject(Error("Private fixture query deadline " + name)),
          5000,
        );
      }),
    ]));
  } finally {
    clearTimeout(timer);
  }
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
  const root = Services.env.get("FLOORP_CORPUS_ROOT"),
    output = Services.env.get("FLOORP_CORPUS_OUTPUT"),
    fixture = Services.env.get("FLOORP_CORPUS_FIXTURE");
  if (!root || !output || !fixture) {
    throw Error("Private corpus paths are required");
  }
  const selected = Services.env.get("FLOORP_CORPUS_IDS").split(",");
  /** @type {CorpusLock} */
  const corpus = await IOUtils.readJSON(
    PathUtils.join(
      root,
      "tools",
      "chrome-extension-runtime",
      "corpus-lock.json",
    ),
  );
  /** @type {CorpusReport<DevtoolsSnapshot>} */
  const report = {
    stage: "actual-package-devtools-primary-operation",
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
  const handler =
    /** @type {ResourceProtocolHandler} */ (Services.io.getProtocolHandler(
      "resource",
    )).QueryInterface(
      Ci.nsIResProtocolHandler,
    );
  handler.setSubstitution(
    "floorp-devtools-corpus",
    Services.io.newFileURI(
      new FileUtils.File(Services.env.get("FLOORP_TEST_ACTOR_DIRECTORY")),
    ),
  );
  ChromeUtils.registerWindowActor("CorpusDevtools", {
    child: {
      esModuleURI:
        "resource://floorp-devtools-corpus/CorpusDevtoolsChild.sys.mjs",
    },
    allFrames: true,
    safeForUntrustedWebProcess: true,
  });
  const win =
    /** @type {NativeWindowHandle} */ (/** @type {unknown} */ (Services.wm
      .getMostRecentWindow("navigator:browser")));
  const principal = Services.scriptSecurityManager.getSystemPrincipal();
  let failed = false;
  try {
    for (
      const record of corpus.extensions.filter((record) =>
        selected.includes(record.id)
      )
    ) {
      /** @type {CorpusOperation<DevtoolsSnapshot>} */
      /** @type {CorpusOperation<DevtoolsSnapshot>} */
      const item = {
        id: record.id,
        name: record.name,
        version: record.version,
        packageSha256: record.packageSha256,
        result: "failed",
        checks: [],
        primaryOperationResult: "failed",
      };
      report.extensions.push(item);
      const originalTabs = new Set(win.gBrowser.tabs);
      let addon;
      /** @type {import("./assessment-test-types.ts").NativeFloorpFrontend|undefined} */ let native;
      try {
        if (record.status !== "frozen") {
          throw Error("Original signed extension is not frozen");
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
          throw Error("Frozen signed CRX digest differs");
        }
        const conversion = await new CRXConverterClass().convert(
          bytes.slice().buffer,
          record.id,
          { id: record.id, name: record.name, version: record.version },
        );
        item.conversion = conversion.report;
        item.warnings = conversion.warnings;
        if (!conversion.success || !conversion.xpiData) {
          throw Error(
            conversion.error?.message || "Production conversion failed",
          );
        }
        const data = new DataView(
            bytes.slice().buffer,
            bytes.byteOffset,
            bytes.byteLength,
          ),
          version = data.getUint32(4, true);
        const offset = version === 3
          ? 12 + data.getUint32(8, true)
          : version === 2
          ? 16 + data.getUint32(8, true) + data.getUint32(12, true)
          : -1;
        if (offset < 12 || offset >= bytes.length) {
          throw Error("Invalid CRX archive offset");
        }
        /** @param {string} name */
        const script = (name) => /\.(?:js|mjs|cjs)$/i.test(name);
        const originals = unzipSync(bytes.subarray(offset), {
            filter: (entry) => script(entry.name),
          }),
          converted = unzipSync(new Uint8Array(conversion.xpiData));
        const names = Object.keys(originals).sort();
        if (
          JSON.stringify(names) !==
            JSON.stringify(Object.keys(converted).filter(script).sort())
        ) throw Error("Converted script inventory differs from signed CRX");
        const hashes = [];
        for (const name of names) {
          const hash = await digest(originals[name]);
          if (hash !== await digest(converted[name])) {
            throw Error("Signed script changed: " + name);
          }
          hashes.push(name + ":" + hash);
        }
        item.unchangedScriptCount = names.length;
        item.unchangedScriptsSha256 = await digest(
          new TextEncoder().encode(hashes.join("\n")),
        );
        const path = PathUtils.join(output, record.id + ".xpi");
        await IOUtils.write(path, new Uint8Array(conversion.xpiData));
        addon = await AddonManager.installTemporaryAddon(
          new FileUtils.File(path),
        );
        const extension = ExtensionParent.GlobalManager.extensionMap.get(
          record.id + "@chromium-extension",
        );
        if (!extension) throw Error("Actual installed extension is missing");
        item.background = extension.manifest.background;
        const tab = win.gBrowser.addTab(fixture, {
          triggeringPrincipal: principal,
        });
        win.gBrowser.selectedTab = tab;
        item.appBefore = await until(async () => {
          const value = await query(tab.linkedBrowser, "Snapshot").catch(() =>
            null
          );
          return value?.reactCount === "React count: 0" && value;
        }, "actual framework rendering");
        const isReact = record.id === "fmkadmapgofadopljbjfkapdkoienihi";
        if (!isReact && record.id !== "lmhkpmbekcpmknklioeibfkpmmfibljd") {
          throw Error("Unsupported authored DevTools corpus recipe");
        }
        native = await attachNativeFloorpDevtools(tab, extension);
        const activeNative = native;
        const panel = await activeNative.selectPanel(
          isReact ? "Components ⚛" : "Redux",
          isReact ? "panel.html" : "devpanel.html",
        );
        item.nativePanel = {
          id: panel.definition.id,
          label: panel.definition.label,
          extensionId: panel.definition.extensionId,
          url: panel.view.uri?.spec,
        };
        item.initialPanel = await until(async () => {
          const value = await query(panel.browser, "Snapshot");
          return value?.text?.includes(
            isReact ? "FloorpCounterLabel" : "Floorp authored Redux store",
          ) && value;
        }, "actual registered native component or store panel");
        if (isReact) {
          await query(panel.browser, "Click", {
            selector: '[data-testname="ComponentTreeListItem"]',
            text: "FloorpCounterLabel",
          });
          item.propsBefore = await until(async () => {
            const value = await query(panel.browser, "Snapshot");
            return value?.props?.inputs[0]?.value === "count" &&
              value.props.inputs[1]?.value === "0" && value;
          }, "actual native component props count0");
          win.gBrowser.selectedTab = tab;
          await query(tab.linkedBrowser, "Click", {
            selector: "#react-increment",
          });
          item.appAfter = await until(async () => {
            const value = await query(tab.linkedBrowser, "Snapshot");
            return value?.reactCount === "React count: 1" && value;
          }, "real authored React state update");
          item.propsAfter = await until(async () => {
            const value = await query(panel.browser, "Snapshot");
            return value?.props?.inputs[0]?.value === "count" &&
              value.props.inputs[1]?.value === "1" && value;
          }, "actual native component props count1");
        } else {
          await query(panel.browser, "Click", { selector: "*", text: "State" });
          item.stateBefore = await until(async () => {
            const value = await query(panel.browser, "Snapshot");
            return /^count\(pin\):0$/m.test(value.text) && value;
          }, "actual native Redux State count0");
          win.gBrowser.selectedTab = tab;
          await query(tab.linkedBrowser, "Click", {
            selector: "#redux-increment",
          });
          item.appAfter = await until(async () => {
            const value = await query(tab.linkedBrowser, "Snapshot");
            return value?.reduxCount === "Redux count: 1" && value;
          }, "real authored Redux state update");
          item.stateAfter = await until(async () => {
            const value = await query(panel.browser, "Snapshot");
            return /^count\(pin\):1$/m.test(value.text) &&
              value.text.includes("AUTHORED_INCREMENT") && value;
          }, "actual native Redux action and State count1");
        }
        await addon.uninstall();
        addon = null;
        await until(
          () => !activeNative.panels().length,
          "native extension panels removed by uninstall",
        );
        item.checks = [
          "Signed product scripts unchanged through production converter",
          isReact
            ? "Native component tree and actual prop count0→1 after genuine native app input"
            : "Native Redux action history and State count0→1 after genuine native app input",
          "Actual uninstall removes registered native extension panels",
        ];
        item.result = item.primaryOperationResult = "passed";
      } catch (error) {
        failed = true;
        item.error = String(
          error instanceof Error ? error.stack || error.message : error,
        );
        if (native) {
          item.registeredPanels = native.panels().map((panel) => ({
            id: panel.id,
            label: panel.label,
            extensionId: panel.extensionId,
          }));
        }
      } finally {
        if (native) await native.close();
        if (addon) await addon.uninstall();
        for (const tab of [...win.gBrowser.tabs]) {
          if (!originalTabs.has(tab)) win.gBrowser.removeTab(tab);
        }
        await save();
      }
    }
  } finally {
    await save();
    ChromeUtils.unregisterWindowActor("CorpusDevtools");
    handler.setSubstitution("floorp-devtools-corpus", null);
  }
  if (failed) {
    throw Error(
      "Actual frozen DevTools primary operation failed; see " + output,
    );
  }
}
