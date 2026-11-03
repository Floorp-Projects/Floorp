// SPDX-License-Identifier: MPL-2.0
import { invokeNativeCorpusAction } from "./corpus-native-action.mjs";
import { installFrozenCorpus } from "./corpus-install-fixture.mjs";
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
const { FileUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/FileUtils.sys.mjs",
);
/** @param {number} ms */
const pause = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
export async function runAllTests() {
  const root = Services.env.get("FLOORP_CORPUS_ROOT");
  const fixture = Services.env.get("FLOORP_CORPUS_FIXTURE");
  if (!root || !fixture) {
    throw Error("FLOORP_CORPUS_ROOT and FLOORP_CORPUS_FIXTURE are required");
  }
  const ids = Services.env.get("FLOORP_CORPUS_IDS").split(",").filter(Boolean);
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
  const recipes =
    /** @type {import("./sidepanel-test-types.ts").SidePanelRecipe[]} */ (await IOUtils
      .readJSON(
        PathUtils.join(
          root,
          "tools",
          "chrome-extension-runtime",
          "corpus-sidepanel-recipes.json",
        ),
      ));
  const output = Services.env.get("FLOORP_CORPUS_OUTPUT") || PathUtils.join(
    root,
    "_dist",
    "chrome-runtime-tests",
    `corpus-sidepanel-floorp-${Date.now()}`,
  );
  await IOUtils.makeDirectory(output);
  /** @type {import("./sidepanel-test-types.ts").SidePanelReport & {authoredFixture:import("./sidepanel-test-types.ts").NativeFixtureMetadata}} */
  const report = {
    stage: "actual-sidepanel-primary-operation",
    fixture,
    authoredFixture: JSON.parse(
      Services.env.get("FLOORP_CORPUS_FIXTURE_METADATA") || "{}",
    ),
    runtime: {
      version: Services.appinfo.version,
      buildId: Services.appinfo.appBuildID,
    },
    extensions: [],
    nativeQualified: false,
    passed: false,
    completed: false,
    output,
  };
  const save = () =>
    IOUtils.writeJSON(PathUtils.join(output, "report.json"), report);
  const handler = Services.io.getProtocolHandler("resource");
  if (!handler?.QueryInterface) {
    throw Error("Native resource protocol unavailable");
  }
  const resource = handler.QueryInterface(Ci.nsIResProtocolHandler);
  resource.setSubstitution(
    "floorp-sidepanel-corpus",
    Services.io.newFileURI(
      new FileUtils.File(
        Services.env.get("FLOORP_TEST_ACTOR_DIRECTORY") ||
          PathUtils.join(root, "tools", "chrome-extension-runtime"),
      ),
    ),
  );
  ChromeUtils.registerWindowActor("CorpusSidePanel", {
    child: {
      esModuleURI:
        "resource://floorp-sidepanel-corpus/CorpusSidePanelChild.sys.mjs",
    },
    allFrames: false,
    safeForUntrustedWebProcess: true,
  });
  const window =
    /** @type {import("./native-test-types.ts").NativeCorpusWindow|null} */ (Services
      .wm.getMostRecentWindow("navigator:browser"));
  if (!window) throw Error("Actual native browser window unavailable");
  const sentinel =
    "Floorp authored compatibility passage 73195: Arctic foxes cross the quiet bridge.";
  const originalProxy = Services.prefs.getIntPref("network.proxy.type", 0);
  Services.prefs.setIntPref("network.proxy.type", 0);
  const originalDns = Services.prefs.getStringPref(
    "network.dns.localDomains",
    "",
  );
  Services.prefs.setStringPref(
    "network.dns.localDomains",
    [originalDns, "corpus.floorp.test"].filter(Boolean).join(","),
  );
  /** @param {import("./native-test-types.ts").NativeCorpusTab} tab @returns {import("./sidepanel-test-types.ts").ProbeActor} */
  const actorFor = (tab) => {
    const global = tab.linkedBrowser.browsingContext.currentWindowGlobal;
    if (!global) throw Error("Private actor window closed");
    return /** @type {import("./sidepanel-test-types.ts").ProbeActor} */ (global
      .getActor("CorpusSidePanel"));
  };
  try {
    for (
      const record of corpus.extensions.filter((e) =>
        e.area === "sidePanel" && (!ids.length || ids.includes(e.id))
      )
    ) {
      /** @type {import("./sidepanel-test-types.ts").SidePanelEvidence} */
      const item = {
        id: record.id,
        name: record.name,
        version: record.version,
        packageSha256: record.packageSha256,
        result: "failed-runtime",
        nativeQualified: false,
        checks: [],
      };
      report.extensions.push(item);
      let addon;
      const originalTabs = new Set(window.gBrowser.tabs);
      try {
        const installed = await installFrozenCorpus(root, record, output);
        addon = installed.addon;
        const extension = installed.extension;
        Object.assign(item, {
          warnings: installed.warnings,
          conversion: installed.conversion,
          sourceIntegrity: installed.sourceIntegrity,
          unchangedScriptCount: installed.unchangedScriptCount,
          unchangedScriptsSha256: installed.unchangedScriptsSha256,
          nativeEvidence: installed.nativeEvidence,
        });
        item.result = "failed-operation";
        await pause(1500);
        const firstURL = fixture + record.id + "/";
        item.authoredURL = firstURL;
        item.authoredSourceSha256 = report.authoredFixture.pages?.find((page) =>
          page.id === record.id
        )?.sha256;
        const first = window.gBrowser.addTab(firstURL, {
          triggeringPrincipal: Services.scriptSecurityManager
            .getSystemPrincipal(),
        });
        const second = window.gBrowser.addTab(firstURL + "second", {
          triggeringPrincipal: Services.scriptSecurityManager
            .getSystemPrincipal(),
        });
        window.gBrowser.selectedTab = first;
        let authoredReady = false;
        for (let n = 0; n < 40; n++) {
          try {
            const state = await actorFor(first)?.sendQuery("Snapshot");
            item.authoredDocument = {
              url: state?.url,
              text: state?.text?.slice(0, 500),
              uri: first.linkedBrowser.currentURI.spec,
              remoteType: first.linkedBrowser.remoteType,
            };
            if (state?.text?.includes(sentinel)) {
              authoredReady = true;
              break;
            }
          } catch (error) {
            item.authoredDocument = {
              error: String(error),
              uri: first.linkedBrowser.currentURI.spec,
              remoteType: first.linkedBrowser.remoteType,
            };
          }
          await pause(100);
        }
        if (!authoredReady) {
          item.result = "harness-fixture-unavailable";
          throw Error("Controlled authored content did not load");
        }
        item.checks.push("controlled authored text verified in content tab");
        item.nativeAction = await invokeNativeCorpusAction(
          extension,
          window,
          first,
        );
        item.checks.push("real native toolbar action invoked on authored page");
        /** @type {import("./native-test-types.ts").NativeCorpusBackground|undefined} */
        let view;
        for (let n = 0; n < 45; n++) {
          view = [...extension.views].find((v) =>
            v.viewType === "sidebar" || v.viewType === "popup"
          );
          if (view) break;
          await pause(200);
        }
        const getActor =
          () => /** @type {import("./sidepanel-test-types.ts").ProbeActor} */ (view
            ?.xulBrowser?.browsingContext?.currentWindowGlobal?.getActor(
              "CorpusSidePanel",
            ) || actorFor(first));
        item.injectedPagePanel = !view;
        /** @type {import("./sidepanel-test-types.ts").SidePanelSnapshot|undefined} */
        let ui;
        for (let n = 0; n < 40; n++) {
          try {
            ui = await getActor()?.sendQuery("Snapshot");
            if (ui?.ready === "complete" && ui.text?.length > 10) break;
          } catch {
            // Retry while the real action view is still loading.
          }
          await pause(200);
        }
        if (!ui) throw Error("Actual action produced no inspectable UI");
        item.initialUI = ui;
        const recipe = recipes.find((r) => r.id === record.id) || {};
        if (recipe.readSelector) {
          item.readClicked = await getActor().sendQuery("Click", {
            selector: recipe.readSelector,
          });
          await pause(1000);
        }
        item.afterReadUI = await getActor().sendQuery("Snapshot");
        item.readEvidence = {
          domContainsAuthoredText: !item.injectedPagePanel &&
              item.afterReadUI.text?.includes(sentinel) || false,
        };
        const selector = recipe.draftSelector ||
          "textarea:not([readonly]):not([disabled]),[contenteditable=true]";
        const draft = `Floorp retained draft ${record.id.slice(0, 8)} 84219`;
        const typed = await getActor().sendQuery("TypeDraft", {
          selector,
          text: draft,
        });
        item.draftBefore = typed.value;
        if (typed.value?.includes(draft)) {
          window.gBrowser.selectedTab = second;
          await pause(250);
          window.gBrowser.selectedTab = first;
          await pause(400);
          item.draftAfter =
            (await getActor().sendQuery("ReadDraft", { selector })).value;
          item.draftRetained = item.draftAfter === item.draftBefore;
          if (recipe.submitForRead) {
            await getActor().sendQuery("PressEnter", { selector });
            await pause(1500);
            item.readAttemptUI = await getActor().sendQuery("Snapshot");
          }
        }
        item.finalUI = await getActor().sendQuery("Snapshot");
        if (item.readEvidence.domContainsAuthoredText && item.draftRetained) {
          item.result = "passed";
          item.nativeQualified =
            installed.nativeEvidence.candidateIdentityVerified &&
            installed.nativeEvidence.nativeBackgroundVerified;
        } else if (
          /login|log in|sign in|sign up|account|premium/i.test(
            item.readAttemptUI?.text || item.finalUI.text || "",
          )
        ) {
          item.result = "blocked-account";
          item.blocker =
            "Unchanged extension UI requires an account before authored-page read can be verified";
        } else {
          item.blocker =
            "Authored-page read and retained draft were not both demonstrated through the actual extension UI";
        }
      } catch (error) {
        item.error = String(error);
      } finally {
        await addon?.uninstall();
        for (const tab of [...window.gBrowser.tabs]) {
          if (!originalTabs.has(tab)) {
            window.gBrowser.removeTab(tab, { skipPermitUnload: true });
          }
        }
        await save();
        console.log(
          `[Sidepanel corpus] ${item.name}: ${item.result} ${
            item.blocker || item.error || ""
          }`,
        );
      }
    }
  } finally {
    ChromeUtils.unregisterWindowActor("CorpusSidePanel");
    Services.prefs.setIntPref("network.proxy.type", originalProxy);
    if (originalDns) {
      Services.prefs.setStringPref("network.dns.localDomains", originalDns);
    } else Services.prefs.clearUserPref("network.dns.localDomains");
    report.completed = true;
    report.passed = report.extensions.length > 0 &&
      report.extensions.every((item) => item.result === "passed");
    report.nativeQualifiedCount = report.extensions.filter((item) =>
      item.nativeQualified === true
    ).length;
    report.observedAccountBlockedCount = report.extensions.filter((item) =>
      item.result === "blocked-account"
    ).length;
    await save();
    console.log(`[Sidepanel corpus] report ${output}/report.json`);
  }
  const failed = report.extensions.filter((item) =>
    !["passed", "blocked-account"].includes(item.result)
  );
  if (!report.extensions.length || failed.length) {
    throw Error(
      `SidePanel primary operation failed: ${failed.length} failed rows; ${report.extensions.length} selected`,
    );
  }
}
