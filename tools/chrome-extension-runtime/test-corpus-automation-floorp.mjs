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
    const options = window.gBrowser.addTab(
      extension.baseURI.resolve("options.html"),
      { triggeringPrincipal: extension.principal },
    );
    await until(async () => {
      const state = await boundedAutomationQuery(
        actor(options)
          ?.sendQuery("Snapshot"),
        "Snapshot",
      )
        .catch(() => null);
      return state?.ready === "complete" && state?.text?.includes("Settings");
    }, "real extension options");
    item.configuration = await boundedAutomationQuery(
      actor(options).sendQuery("ConfigureUIVision", {
        url,
      }),
      "ConfigureUIVision",
    );
    window.gBrowser.removeTab(options);
    window.gBrowser.selectedTab = target;
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
    item.macro = uiVisionMacro(url);
    await boundedAutomationQuery(
      actor(target).sendQuery("RunUIVision", { url, macro: item.macro }),
      "RunUIVision",
    );
    await until(
      async () => {
        item.observed = await boundedAutomationQuery(
          actor(target).sendQuery("Snapshot"),
          "Snapshot",
        );
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
        const target = [...window.gBrowser.tabs].find(
          (tab) => tab.linkedBrowser.currentURI.spec === url,
        );
        if (!target) return false;
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
