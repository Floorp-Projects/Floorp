// SPDX-License-Identifier: MPL-2.0
/** @import {Truthy, CorpusLock, CorpusReport, CorpusOperation, DevtoolsSnapshot, NativeWindowHandle, NativeBrowserHandle, ActorClickData, DevtoolsActorResponses, ResourceProtocolHandler} from "./assessment-test-types.ts" */
import { installFrozenCorpus } from "./corpus-install-fixture.mjs";
import { attachNativeFloorpDevtools } from "./floorp-devtools-panel.mjs";
const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
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
    nativeQualified: false,
    qualificationMode:
      Services.env.get("FLOORP_TEST_REQUIRE_NATIVE_CORPUS") === "1"
        ? "candidate"
        : "historical",
    stage: "actual-package-devtools-primary-operation",
    runtime: {
      version: Services.appinfo.version,
      buildId: Services.appinfo.appBuildID,
      nativeWorkerPolicy: WebExtensionPolicy.backgroundServiceWorkerEnabled,
      workerWebIDLCompiled: ChromeUtils.importESModule(
        "resource://gre/modules/AppConstants.sys.mjs",
      ).AppConstants.MOZ_WEBEXT_WEBIDL_ENABLED === true,
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
        nativeQualified: false,
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
        const installed = await installFrozenCorpus(root, record, output);
        addon = installed.addon;
        const extension = installed.extension;
        Object.assign(item, {
          unchangedScriptCount: installed.unchangedScriptCount,
          unchangedScriptsSha256: installed.unchangedScriptsSha256,
          sourceIntegrity: installed.sourceIntegrity,
          nativeEvidence: installed.nativeEvidence,
          nativeCandidateIdentity: installed.nativeEvidence.candidateIdentity ??
            null,
          conversion: installed.conversion,
          warnings: installed.warnings,
          background: installed.background,
        });
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
        item.nativeQualified = item.primaryOperationResult === "passed" &&
          item.nativeEvidence?.candidateIdentityVerified === true &&
          item.nativeEvidence.nativeBackgroundVerified === true;
        await save();
      }
    }
  } finally {
    await save();
    ChromeUtils.unregisterWindowActor("CorpusDevtools");
    handler.setSubstitution("floorp-devtools-corpus", null);
  }
  report.nativeQualified = report.extensions.length > 0 &&
    report.extensions.every((item) => item.nativeQualified === true);
  await save();
  if (failed) {
    throw Error(
      "Actual frozen DevTools primary operation failed; see " + output,
    );
  }
}
