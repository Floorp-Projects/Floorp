/// <reference path="../../libs/@types/gecko/index.d.ts" />
// SPDX-License-Identifier: MPL-2.0
import {
  automationIds,
  selectAutomationIds,
} from "./automation-corpus-selection.mjs";
import { boundedAutomationQuery } from "./automation-query.mjs";
import { invokeNativeCorpusAction } from "./corpus-native-action.mjs";
import { installFrozenCorpus } from "./corpus-install-fixture.mjs";
import { runWebScraper } from "./test-corpus-webscraper-floorp.mjs";
import {
  automaId,
  automaWorkflow,
  browserflowId,
  expectedAutomaCustomer,
  expectedCustomer,
  uiVisionId,
  uiVisionMacro,
  webscraperId,
} from "./corpus-automation-recipes.mjs";
/** @type {{ExtensionParent:import("./native-test-types.ts").NativeCorpusExtensionParent}} */
const { ExtensionParent } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionParent.sys.mjs",
);
const { FileUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/FileUtils.sys.mjs",
);
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
/** @param {number} ms */
const pause = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
/** @template T @param {()=>T|false|null|undefined|Promise<T|false|null|undefined>} read @param {string} description @param {number} [timeout] @returns {Promise<T>} */
async function until(read, description, timeout = 30000) {
  const deadline = Date.now() + timeout;
  while (Date.now() < deadline) {
    const value = await read();
    if (value) return value;
    await pause(100);
  }
  throw Error("Timed out waiting for " + description);
}
function resourceProtocol() {
  const handler = Services.io.getProtocolHandler("resource");
  if (!handler.QueryInterface) {
    throw Error("Native resource protocol interface unavailable");
  }
  return handler.QueryInterface(Ci.nsIResProtocolHandler);
}
async function runUIVision() {
  const root = Services.env.get("FLOORP_CORPUS_ROOT"),
    url = Services.env.get("FLOORP_CORPUS_FIXTURE"),
    output = PathUtils.join(
      Services.env.get("FLOORP_CORPUS_OUTPUT"),
      "ui-vision",
    );
  if (!root || !url || !output) {
    throw Error("Automation corpus fixture environment missing");
  }
  await IOUtils.makeDirectory(output);
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
  const record = corpus.extensions.find((record) => record.id === uiVisionId);
  if (!record) throw Error("Frozen automation record missing");
  /** @type {import("./automation-test-types.ts").AutomationItem} */
  const item = {
    id: record.id,
    name: record.name,
    version: record.version,
    packageSha256: record.packageSha256,
    result: "failed",
  };
  /** @type {import("./automation-test-types.ts").AutomationReport} */
  const report = {
    stage: "frozen-automation-primary-operation",
    runtime: {
      version: Services.appinfo.version,
      buildId: Services.appinfo.appBuildID,
    },
    fixture: JSON.parse(
      Services.env.get("FLOORP_CORPUS_FIXTURE_METADATA") || "{}",
    ),
    extensions: [item],
    output,
  };
  const resource = resourceProtocol();
  resource.setSubstitution(
    "floorp-automation-corpus",
    Services.io.newFileURI(
      new FileUtils.File(
        Services.env.get("FLOORP_TEST_ACTOR_DIRECTORY") ||
          PathUtils.join(root, "tools", "chrome-extension-runtime"),
      ),
    ),
  );
  ChromeUtils.registerWindowActor("CorpusAutomation", {
    child: {
      esModuleURI:
        "resource://floorp-automation-corpus/CorpusAutomationChild.sys.mjs",
    },
    allFrames: false,
    safeForUntrustedWebProcess: true,
  });
  const window =
      /** @type {import("./native-test-types.ts").NativeCorpusWindow} */ (/** @type {unknown} */ (Services
        .wm.getMostRecentWindow("navigator:browser"))),
    before = new Set(window.gBrowser.tabs);
  /** @param {import("./native-test-types.ts").NativeCorpusTab} tab @returns {import("./automation-test-types.ts").AutomationActor} */
  const actor = (tab) => {
    const value = tab.linkedBrowser.browsingContext.currentWindowGlobal
      ?.getActor("CorpusAutomation");
    if (!value) throw Error("Private automation actor unavailable");
    return /** @type {import("./automation-test-types.ts").AutomationActor} */ (value);
  };
  let addon;
  try {
    const installed = await installFrozenCorpus(root, record, output);
    ({ addon } = installed);
    Object.assign(item, {
      unchangedScriptCount: installed.unchangedScriptCount,
      unchangedScriptsSha256: installed.unchangedScriptsSha256,
      sourceIntegrity: installed.sourceIntegrity,
      nativeEvidence: installed.nativeEvidence,
      warnings: installed.warnings,
    });
    const extension = ExtensionParent.GlobalManager.extensionMap.get(
      `${record.id}@chromium-extension`,
    );
    if (!extension) throw Error("Installed native extension policy missing");
    await until(
      () => extension.backgroundContext?.viewType === "background_worker",
      "genuine native extension Worker",
    );
    const target = window.gBrowser.addTab(url, {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
    window.gBrowser.selectedTab = target;
    await until(async () => {
      const state = await boundedAutomationQuery(
        actor(target)
          ?.sendQuery("Snapshot"),
        "Snapshot",
      )
        .catch(() => null);
      return state?.text?.includes("Automation target") && state;
    }, "authored input/click target");
    item.nativeAction = await invokeNativeCorpusAction(
      extension,
      window,
      target,
    );
    const panel = await until(
      () =>
        [...extension.views].find(
          (view) => view.viewType === "sidebar" || view.viewType === "popup",
        ),
      "real toolbar action side panel",
    );
    const panelActor = () => {
      const value = panel.xulBrowser?.browsingContext.currentWindowGlobal
        ?.getActor("CorpusAutomation");
      if (!value) throw Error("Private automation panel actor unavailable");
      return /** @type {import("./automation-test-types.ts").AutomationActor} */ (value);
    };
    await until(async () => {
      const state = await boundedAutomationQuery(
        panelActor()
          ?.sendQuery("Snapshot"),
        "Snapshot",
      )
        .catch(() => null);
      return state?.ready === "complete" && (state?.text?.length ?? 0) > 20;
    }, "real extension macro UI");
    item.onboarding = await boundedAutomationQuery(
      panelActor().sendQuery("AcknowledgeUIVisionWelcome"),
      "AcknowledgeUIVisionWelcome",
    );
    if (
      item.onboarding.shown && (!item.onboarding.clicked ||
        !item.onboarding.events.some((event) =>
          event.type === "click" && event.trusted
        ))
    ) {
      throw Error("Genuine UI.Vision welcome acknowledgement did not complete");
    }
    await until(async () => {
      const state = await boundedAutomationQuery(
        panelActor().sendQuery("Snapshot"),
        "Snapshot",
      );
      return !state.text.includes("Your macros are all still here");
    }, "actual UI.Vision welcome dialog to close");
    const options = window.gBrowser.addTab(
      extension.baseURI.resolve("options.html#api"),
      { triggeringPrincipal: extension.principal },
    );
    window.gBrowser.selectedTab = options;
    await until(async () => {
      const state = await boundedAutomationQuery(
        actor(options).sendQuery("Snapshot"),
        "Snapshot",
      ).catch(() => null);
      return state?.ready === "complete" &&
        state.text.includes("Run embedded macros from public websites");
    }, "real UI.Vision API settings");
    item.websiteOperator = {
      before: await boundedAutomationQuery(
        actor(options).sendQuery("ReadUIVisionWebsiteConfiguration"),
        "ReadUIVisionWebsiteConfiguration",
      ),
      steps: [],
    };
    /** @param {import("./automation-test-types.ts").UIVisionWebsiteControl} control */
    const readyControl = (control) =>
      until(async () => {
        const state = await boundedAutomationQuery(
          actor(options).sendQuery("UIVisionWebsiteControlState", {
            uiVisionControl: control,
          }),
          "UIVisionWebsiteControlState",
        );
        return state.ready === "complete" && state.present && state.visible &&
          state.enabled && state.unobstructed &&
          ((control !== "whitelistURL" && control !== "saveWhitelist") ||
            state.dialogTitle === "Embedded Macros Website Whitelist") &&
          state;
      }, "visible unobstructed UI.Vision website control " + control);
    /** @param {import("./automation-test-types.ts").UIVisionWebsiteControl} control */
    const useControl = async (control) => {
      await readyControl(control);
      const step = await boundedAutomationQuery(
        actor(options).sendQuery("UIVisionWebsiteControl", {
          uiVisionControl: control,
          url,
        }),
        "UIVisionWebsiteControl",
      );
      item.websiteOperator?.steps.push(step);
      return step;
    };
    if (!(await readyControl("websiteRuns")).checked) {
      await useControl("websiteRuns");
    }
    await until(
      async () => (await readyControl("websiteRuns")).checked,
      "actual website-run checkbox enabled",
    );
    await useControl("editWhitelist");
    await useControl("whitelistURL");
    await until(
      async () => (await readyControl("whitelistURL")).value === url,
      "actual whitelist textarea contains only authored loopback operation",
    );
    await useControl("saveWhitelist");
    await until(async () => {
      const state = await boundedAutomationQuery(
        actor(options).sendQuery("UIVisionWebsiteControlState", {
          uiVisionControl: "websiteRuns",
        }),
        "UIVisionWebsiteControlState",
      );
      const config = await boundedAutomationQuery(
        actor(options).sendQuery("ReadUIVisionWebsiteConfiguration"),
        "ReadUIVisionWebsiteConfiguration",
      );
      if (item.websiteOperator) item.websiteOperator.after = config;
      return !state.dialogVisible && state.checked &&
        config.allowRunFromHttpSchema === true &&
        config.websiteInvokePolicyVersion === 1 &&
        Array.isArray(config.websiteWhiteList) &&
        config.websiteWhiteList.length === 1 &&
        config.websiteWhiteList[0] === url;
    }, "vendor settings persisted after trusted website-run controls");
    window.gBrowser.removeTab(options);
    window.gBrowser.selectedTab = target;
    item.macro = uiVisionMacro(url);
    const observerToken = Services.uuid.generateUUID().toString().slice(1, -1);
    const beforeInnerWindowId = target.linkedBrowser.browsingContext
      .currentWindowGlobal?.innerWindowId;
    if (!beforeInnerWindowId) {
      throw Error("Owned macro target has no current document");
    }
    item.invocation = {
      dispatchedDocumentedBridge: false,
      witnessMs: 3000,
      events: [],
      observerToken,
      transport: "bounded-owned-document-console",
      documentChanges: [],
    };
    try {
      item.invocation = {
        ...await boundedAutomationQuery(
          actor(target).sendQuery("RunUIVision", {
            url,
            macro: item.macro,
            witnessToken: observerToken,
          }),
          "RunUIVision",
        ),
        documentChanges: [],
      };
    } catch (error) {
      const currentInnerWindowId =
        target.linkedBrowser.browsingContext.currentWindowGlobal
          ?.innerWindowId ?? null;
      const isLoadingDocument =
        /** @type {import("./automation-test-types.ts").UIVisionNavigationBrowser} */ (
          target.linkedBrowser
        ).webProgress.isLoadingDocument;
      if (
        target.linkedBrowser.currentURI.spec !== url ||
        !/(?:Actor|actor|WindowGlobal|document).*(?:destroyed|unavailable|navigated|unloaded)|IPC channel.*closed/
          .test(String(error)) ||
        (!(currentInnerWindowId &&
          currentInnerWindowId !== beforeInnerWindowId) &&
          !isLoadingDocument)
      ) throw error;
      item.invocation.documentChanges?.push({
        error: String(error).slice(0, 1000),
        beforeInnerWindowId,
        currentInnerWindowId,
        isLoadingDocument,
        url: target.linkedBrowser.currentURI.spec,
      });
    }
    // Keep the diagnostic interval in the browser host. An expected document
    // navigation may end the old observer; only the unchanged primary decides.
    await pause(3000);
    const engineView = {
      method: "actual-native-UI-Vision-Data-Logs-tabs",
      steps:
        /** @type {import("./automation-test-types.ts").UIVisionLogControlObservation[]} */ ([]),
      last:
        /** @type {import("./automation-test-types.ts").UIVisionLogControlObservation|null} */ (null),
    };
    item.engineView = engineView;
    /** @param {import("./automation-test-types.ts").UIVisionLogControl} control */
    const readyLogControl = async (control) => {
      /** @type {import("./automation-test-types.ts").UIVisionLogControlObservation|undefined} */
      let previous;
      return await until(async () => {
        const state = await boundedAutomationQuery(
          panelActor().sendQuery("UIVisionLogControlState", {
            uiVisionLogControl: control,
          }),
          "UIVisionLogControlState",
        );
        engineView.last = state;
        const usable = state.ready === "complete" && state.visible &&
          state.enabled && state.unobstructed;
        const stable = usable && previous && previous.visible &&
          previous.enabled && previous.unobstructed &&
          Math.abs(state.rect.x - previous.rect.x) <= 0.1 &&
          Math.abs(state.rect.y - previous.rect.y) <= 0.1 &&
          Math.abs(state.rect.width - previous.rect.width) <= 0.1 &&
          Math.abs(state.rect.height - previous.rect.height) <= 0.1;
        previous = usable ? state : undefined;
        return stable && state;
      }, "stable genuine UI.Vision " + control + " tab");
    };
    try {
      for (const control of /** @type {const} */ (["data", "logs"])) {
        const state = await readyLogControl(control);
        if (!state.selected) {
          engineView.steps.push(
            await boundedAutomationQuery(
              panelActor().sendQuery("UIVisionLogControl", {
                uiVisionLogControl: control,
              }),
              "UIVisionLogControl",
            ),
          );
        }
        await until(async () => {
          const active = await readyLogControl(control);
          return active.selected && active.viewVisible;
        }, "actual selected UI.Vision " + control + " view");
      }
    } catch (error) {
      item.engineLog = (await boundedAutomationQuery(
        panelActor().sendQuery("Snapshot"),
        "Snapshot",
      ).catch(() => null))?.text;
      throw error;
    }

    await until(
      async () => {
        const beforeSnapshotGlobal =
          target.linkedBrowser.browsingContext.currentWindowGlobal;
        const beforeSnapshotInnerWindowId = beforeSnapshotGlobal?.innerWindowId;
        const beforeSnapshotURL = beforeSnapshotGlobal?.documentURI?.spec;
        try {
          item.observed = await boundedAutomationQuery(
            actor(target).sendQuery("Snapshot"),
            "Snapshot",
          );
        } catch (error) {
          const browser = target.linkedBrowser;
          const currentInnerWindowId =
            browser.browsingContext.currentWindowGlobal?.innerWindowId ?? null;
          const isLoadingDocument =
            /** @type {import("./automation-test-types.ts").UIVisionNavigationBrowser} */ (browser)
              .webProgress.isLoadingDocument;
          // The macro's real open command can replace the actor during this
          // read-only query. Retry only that exact owned-document transition.
          if (
            String(error) !==
              "AbortError: Actor 'CorpusAutomation' destroyed before query 'Snapshot' was resolved" ||
            !beforeSnapshotInnerWindowId || beforeSnapshotURL !== url ||
            browser.currentURI.spec !== url ||
            (!(currentInnerWindowId &&
              currentInnerWindowId !== beforeSnapshotInnerWindowId) &&
              !isLoadingDocument)
          ) throw error;
          item.invocation?.documentChanges?.push({
            error: String(error),
            beforeInnerWindowId: beforeSnapshotInnerWindowId,
            currentInnerWindowId,
            isLoadingDocument,
            url: browser.currentURI.spec,
          });
          return false;
        }
        item.engineLog = (await boundedAutomationQuery(
          panelActor().sendQuery("Snapshot"),
          "Snapshot",
        )).text;
        return (
          item.observed.operation === expectedCustomer &&
          item.engineLog.includes("Macro completed")
        );
      },
      "genuine extension open/type/click/assertText macro completion",
      60000,
    );
    if (
      !item.observed || item.observed.input !== expectedCustomer ||
      item.observed.result !== `Submitted: ${expectedCustomer}` ||
      item.observed.clickCount !== "1" ||
      !item.engineLog || item.engineLog.includes("[error]")
    ) {
      throw Error("Real macro target/result/completion assertions failed");
    }
    item.checks = [
      "frozen signed CRX digest",
      "all signed scripts unchanged after production CRX conversion",
      "actual native ServiceWorker background",
      "actual toolbar action and side panel",
      "documented SaveAndRunMacro import bridge",
      "real macro open/type/click/assertText",
      "target DOM input and one click",
      "successful extension engine log",
    ];
    item.result = "pass";
    console.log("[Actual corpus] UI.Vision real macro PASS");
  } catch (error) {
    item.error = String(error);
    throw error;
  } finally {
    await addon?.uninstall();
    for (const tab of [...window.gBrowser.tabs]) {
      if (!before.has(tab)) {
        window.gBrowser.removeTab(tab, { skipPermitUnload: true });
      }
    }
    ChromeUtils.unregisterWindowActor("CorpusAutomation");
    await IOUtils.writeJSON(PathUtils.join(output, "report.json"), report);
    console.log("REPORT " + PathUtils.join(output, "report.json"));
  }
}
async function runAutoma() {
  const root = Services.env.get("FLOORP_CORPUS_ROOT"),
    url = Services.env.get("FLOORP_CORPUS_FIXTURE") + "?automa";
  const output = PathUtils.join(
    Services.env.get("FLOORP_CORPUS_OUTPUT"),
    "automa",
  );
  await IOUtils.makeDirectory(output);
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
  const record = corpus.extensions.find((record) => record.id === automaId);
  if (!record) throw Error("Frozen automation record missing");
  /** @type {import("./automation-test-types.ts").AutomationItem} */
  const item = {
    id: record.id,
    name: record.name,
    version: record.version,
    packageSha256: record.packageSha256,
    result: "failed",
  };
  /** @type {import("./automation-test-types.ts").AutomationReport} */
  const report = {
    stage: "frozen-automation-primary-operation",
    fixture: JSON.parse(
      Services.env.get("FLOORP_CORPUS_FIXTURE_METADATA") || "{}",
    ),
    extensions: [item],
    output,
  };
  resourceProtocol()
    .setSubstitution(
      "floorp-automation-corpus",
      Services.io.newFileURI(
        new FileUtils.File(
          Services.env.get("FLOORP_TEST_ACTOR_DIRECTORY") ||
            PathUtils.join(root, "tools", "chrome-extension-runtime"),
        ),
      ),
    );
  ChromeUtils.registerWindowActor("CorpusAutomation", {
    child: {
      esModuleURI:
        "resource://floorp-automation-corpus/CorpusAutomationChild.sys.mjs",
    },
    allFrames: false,
    safeForUntrustedWebProcess: true,
  });
  const window =
      /** @type {import("./native-test-types.ts").NativeCorpusWindow} */ (/** @type {unknown} */ (Services
        .wm.getMostRecentWindow("navigator:browser"))),
    before = new Set(window.gBrowser.tabs);
  /** @param {import("./native-test-types.ts").NativeCorpusTab} tab @returns {import("./automation-test-types.ts").AutomationActor} */
  const actor = (tab) => {
    const value = tab.linkedBrowser.browsingContext.currentWindowGlobal
      ?.getActor("CorpusAutomation");
    if (!value) throw Error("Private automation actor unavailable");
    return /** @type {import("./automation-test-types.ts").AutomationActor} */ (value);
  };
  let addon;
  try {
    const installed = await installFrozenCorpus(root, record, output);
    ({ addon } = installed);
    Object.assign(item, {
      unchangedScriptCount: installed.unchangedScriptCount,
      unchangedScriptsSha256: installed.unchangedScriptsSha256,
      sourceIntegrity: installed.sourceIntegrity,
      nativeEvidence: installed.nativeEvidence,
      warnings: installed.warnings,
    });
    const extension = ExtensionParent.GlobalManager.extensionMap.get(
      `${record.id}@chromium-extension`,
    );
    if (!extension) throw Error("Installed native extension policy missing");
    await until(
      () => extension.backgroundContext?.viewType === "background_worker",
      "actual Automa module ServiceWorker",
    );
    const ui = window.gBrowser.addTab(extension.baseURI.resolve("popup.html"), {
      triggeringPrincipal: extension.principal,
    });
    window.gBrowser.selectedTab = ui;
    /** @type {import("./automation-test-types.ts").AutomaWorkflow|undefined} */
    let template;
    await until(async () => {
      try {
        const stored = await boundedAutomationQuery(
          actor(ui)?.sendQuery("ReadAutomaConfiguration"),
          "ReadAutomaConfiguration",
        );
        template = Object.values(stored?.workflows || {}).find(
          (workflow) =>
            workflow.drawflow?.nodes?.some((node) => node.label === "forms") &&
            workflow.drawflow.nodes.some(
              (node) => node.label === "event-click",
            ),
        );
        return template;
      } catch {
        return false;
      }
    }, "actual packaged Automa example graph");
    if (!template) throw Error("Genuine packaged Automa template missing");
    const workflow = automaWorkflow(template, url);
    item.workflow = workflow;
    await boundedAutomationQuery(
      actor(ui).sendQuery("WriteAutomaWorkflow", {
        workflow: item.workflow,
      }),
      "WriteAutomaWorkflow",
    );
    ui.linkedBrowser.reload();
    await until(async () => {
      const state = await boundedAutomationQuery(
        actor(ui)
          ?.sendQuery("Snapshot"),
        "Snapshot",
      )
        .catch(() => null);
      return state?.text?.includes(workflow.name) && state;
    }, "real authored workflow UI card");
    item.execute = await boundedAutomationQuery(
      actor(ui).sendQuery("RunAutoma", {
        name: workflow.name,
      }),
      "RunAutoma",
    );
    await until(
      async () => {
        const targets =
          /** @type {import("./native-test-types.ts").NativeCorpusWindow[]} */ ([
            ...Services.wm.getEnumerator("navigator:browser"),
          ]).filter((browserWindow) => !!browserWindow.gBrowser)
            .flatMap((browserWindow) => [...browserWindow.gBrowser.tabs])
            .filter((tab) => tab.linkedBrowser.currentURI.spec === url);
        if (!targets.length) return false;
        if (targets.length !== 1) {
          throw Error("Expected one exact authored Automa target tab");
        }
        const target = targets[0];
        item.observed = await boundedAutomationQuery(
          actor(target).sendQuery("Snapshot"),
          "Snapshot",
        );
        return item.observed.operation === expectedAutomaCustomer;
      },
      "genuine Automa new-tab/forms/event-click effects",
      60000,
    );
    if (
      !item.observed || item.observed.input !== expectedAutomaCustomer ||
      item.observed.result !== `Submitted: ${expectedAutomaCustomer}` ||
      item.observed.clickCount !== "1"
    ) {
      throw Error("Actual Automa input and single click result mismatch");
    }
    item.checks = [
      "signed CRX hash",
      "all signed scripts unchanged after production conversion",
      "actual module ServiceWorker background",
      "packaged sample graph with authored URL/selectors/input data",
      "native mouse click on real Execute control",
      "real engine opens page/types/clicks",
      "actual target DOM input and exactly one click",
    ];
    item.result = "pass";
    console.log("[Actual corpus] Automa real authored macro PASS");
  } catch (error) {
    item.error = String(error);
    throw error;
  } finally {
    await addon?.uninstall();
    for (const tab of [...window.gBrowser.tabs]) {
      if (!before.has(tab)) {
        window.gBrowser.removeTab(tab, { skipPermitUnload: true });
      }
    }
    ChromeUtils.unregisterWindowActor("CorpusAutomation");
    await IOUtils.writeJSON(PathUtils.join(output, "report.json"), report);
  }
}
async function runBrowserflow() {
  const root = Services.env.get("FLOORP_CORPUS_ROOT"),
    url = Services.env.get("FLOORP_CORPUS_FIXTURE");
  const output = PathUtils.join(
    Services.env.get("FLOORP_CORPUS_OUTPUT"),
    "browserflow",
  );
  await IOUtils.makeDirectory(output);
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
  const record = corpus.extensions.find(
    (record) => record.id === browserflowId,
  );
  if (!record) throw Error("Frozen automation record missing");
  /** @type {import("./automation-test-types.ts").AutomationItem} */
  const item = {
    id: record.id,
    name: record.name,
    version: record.version,
    packageSha256: record.packageSha256,
    result: "failed",
  };
  /** @type {import("./automation-test-types.ts").AutomationReport} */
  const report = {
    stage: "frozen-automation-account-boundary",
    extensions: [item],
    output,
  };
  resourceProtocol()
    .setSubstitution(
      "floorp-automation-corpus",
      Services.io.newFileURI(
        new FileUtils.File(
          Services.env.get("FLOORP_TEST_ACTOR_DIRECTORY") ||
            PathUtils.join(root, "tools", "chrome-extension-runtime"),
        ),
      ),
    );
  ChromeUtils.registerWindowActor("CorpusAutomation", {
    child: {
      esModuleURI:
        "resource://floorp-automation-corpus/CorpusAutomationChild.sys.mjs",
    },
    allFrames: false,
    safeForUntrustedWebProcess: true,
  });
  const window =
      /** @type {import("./native-test-types.ts").NativeCorpusWindow} */ (/** @type {unknown} */ (Services
        .wm.getMostRecentWindow("navigator:browser"))),
    before = new Set(window.gBrowser.tabs);
  let addon;
  try {
    const installed = await installFrozenCorpus(root, record, output);
    ({ addon } = installed);
    Object.assign(item, {
      unchangedScriptCount: installed.unchangedScriptCount,
      unchangedScriptsSha256: installed.unchangedScriptsSha256,
      sourceIntegrity: installed.sourceIntegrity,
      nativeEvidence: installed.nativeEvidence,
      warnings: installed.warnings,
    });
    const extension = ExtensionParent.GlobalManager.extensionMap.get(
      `${record.id}@chromium-extension`,
    );
    if (!extension) throw Error("Installed native extension policy missing");
    await until(
      () => extension.backgroundContext?.viewType === "background_worker",
      "actual Browserflow ServiceWorker",
    );
    const target = window.gBrowser.addTab(url + "?browserflow", {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
    await until(
      () =>
        target.linkedBrowser.browsingContext.currentWindowGlobal?.documentURI
          .spec ===
          url + "?browserflow",
      "authored Browserflow target",
    );
    window.gBrowser.selectedTab = target;
    item.nativeAction = await invokeNativeCorpusAction(
      extension,
      window,
      target,
    );
    const popup = await until(
      () => [...extension.views].find((view) => view.viewType === "popup"),
      "actual Browserflow toolbar popup",
    );
    await until(async () => {
      const actor =
        /** @type {import("./automation-test-types.ts").AutomationActor|undefined} */ (popup
          .xulBrowser?.browsingContext.currentWindowGlobal?.getActor(
            "CorpusAutomation",
          ));
      item.observed = await boundedAutomationQuery(
        actor?.sendQuery("Snapshot"),
        "Snapshot",
      ).catch(() => null);
      return (
        item.observed?.ready === "complete" &&
        item.observed.text?.includes("Sign in")
      );
    }, "real Browserflow sign-in boundary");
    item.result = "blocked-account";
    item.checks = [
      "signed CRX digest and unchanged scripts through production conversion",
      "actual native ServiceWorker background",
      "real toolbar action popup",
      "same sign-in-only account boundary as genuine Chrome reference",
    ];
    item.reason =
      "The immutable signed package exposes only Sign in before cloud authentication; no local automation editor or Execute control is available.";
    console.log("[Actual corpus] Browserflow real account boundary observed");
  } catch (error) {
    item.error = String(error);
    throw error;
  } finally {
    await addon?.uninstall();
    for (const tab of [...window.gBrowser.tabs]) {
      if (!before.has(tab)) {
        window.gBrowser.removeTab(tab, { skipPermitUnload: true });
      }
    }
    ChromeUtils.unregisterWindowActor("CorpusAutomation");
    await IOUtils.writeJSON(PathUtils.join(output, "report.json"), report);
  }
}
export async function runAllTests() {
  const root = Services.env.get("FLOORP_CORPUS_ROOT"),
    output = Services.env.get("FLOORP_CORPUS_OUTPUT");
  if (!root || !output) {
    throw Error("Automation corpus root and output are required");
  }
  /** @type {import("./corpus-types.ts").FrozenCorpus} */
  const corpus = await IOUtils.readJSON(
    PathUtils.join(
      root,
      "tools",
      "chrome-extension-runtime",
      "corpus-lock.json",
    ),
  );
  const selected = selectAutomationIds(
    Services.env.get("FLOORP_CORPUS_IDS"),
    corpus,
  );
  const operations = new Map([
    [uiVisionId, { product: "ui-vision", run: runUIVision }],
    [automaId, { product: "automa", run: runAutoma }],
    [browserflowId, { product: "browserflow", run: runBrowserflow }],
    [webscraperId, { product: "web-scraper", run: runWebScraper }],
  ]);
  const failures = [];
  /** @type {import("./automation-test-types.ts").AutomationReport[]} */
  const reports = [];
  for (const id of selected) {
    const operation = operations.get(id);
    if (!operation) throw Error("Automation operation missing for " + id);
    try {
      await operation.run();
    } catch (error) {
      failures.push(id + ": " + String(error));
    }
    const path = PathUtils.join(output, operation.product, "report.json");
    if (!await IOUtils.exists(path)) {
      failures.push("Missing selected automation report for " + id);
      continue;
    }
    /** @type {import("./automation-test-types.ts").AutomationReport} */
    const report = await IOUtils.readJSON(path);
    if (report.extensions.length !== 1 || report.extensions[0].id !== id) {
      failures.push("Selected automation report ID mismatch for " + id);
      continue;
    }
    reports.push(report);
  }
  await IOUtils.writeJSON(PathUtils.join(output, "report.json"), {
    stage: "frozen-automation-primary-operation",
    runtime: {
      version: Services.appinfo.version,
      buildId: Services.appinfo.appBuildID,
    },
    selectedIds: selected,
    unattemptedIds: automationIds.filter((id) => !selected.includes(id)),
    extensions: reports.flatMap((report) => report.extensions),
    failures,
    output,
  });
  if (failures.length) throw Error(failures.join("; "));
}
