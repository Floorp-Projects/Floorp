// SPDX-License-Identifier: MPL-2.0
import { installFrozenCorpus } from "./corpus-install-fixture.mjs";
import { runLanguageToolOperation } from "./corpus-languagetool-operation.mjs";
const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
const { FileUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/FileUtils.sys.mjs",
);
const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs",
);
/** @param {number} ms */
const pause = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
/** @template T
 * @param {() => T | false | null | undefined | Promise<T | false | null | undefined>} read
 * @param {string} label
 * @returns {Promise<T>} */
async function until(read, label) {
  const end = Date.now() + 30000;
  while (Date.now() < end) {
    const value = await read();
    if (value) return value;
    await pause(100);
  }
  throw Error("Timed out waiting for " + label);
}
export async function runAllTests() {
  const output = Services.env.get("FLOORP_LANGUAGETOOL_OUTPUT"),
    root = Services.env.get("FLOORP_CORPUS_ROOT");
  if (!output || !root) {
    throw Error("Private LanguageTool fixture output/root required");
  }
  const id = "oldceeleldhonbafppcapldpdifcinji";
  /** @type {import("./network-test-types.ts").LanguageFixture} */
  const fixture = await IOUtils.readJSON(
    PathUtils.join(output, "fixture.json"),
  );
  const corpus =
    /** @type {import("./network-test-types.ts").NetworkCorpus} */ (
      await IOUtils.readJSON(
        PathUtils.join(
          root,
          "tools",
          "chrome-extension-runtime",
          "corpus-lock.json",
        ),
      )
    );
  const record = corpus.extensions.find((item) => item.id === id);
  if (!record) throw Error("Frozen LanguageTool record missing");
  /** @type {import("./languagetool-native-test-types.ts").NativeLanguageItem} */
  const item = {
    id,
    name: record.name,
    version: record.version,
    packageSha256: record.packageSha256,
    primaryOperation: record.primaryOperation,
    primaryOperationResult: "not-run",
    nativeQualified: false,
    cleanupCompleted: false,
    checks: [],
  };
  /** @type {import("./languagetool-native-test-types.ts").NativeLanguageReport} */
  const report = {
    stage: "actual-package-primary-operation",
    runtime: {
      version: Services.appinfo.version,
      buildId: Services.appinfo.appBuildID,
      nativeWorkerCompiled: AppConstants.MOZ_WEBEXT_WEBIDL_ENABLED === true,
    },
    fixture,
    extensions: [item],
    primaryOperationPassRate: null,
    output,
  };
  const save = () =>
    IOUtils.writeJSON(PathUtils.join(output, "report.json"), report);
  const resource =
    /** @type {import("./network-test-types.ts").ResourceProtocol} */ (
      Cc["@mozilla.org/network/protocol;1?name=resource"].getService(
        Ci.nsIResProtocolHandler,
      )
    );
  resource.setSubstitution(
    "floorp-languagetool-corpus",
    Services.io.newFileURI(
      new FileUtils.File(Services.env.get("FLOORP_TEST_ACTOR_DIRECTORY")),
    ),
  );
  ChromeUtils.registerWindowActor("CorpusLanguageTool", {
    child: {
      esModuleURI:
        "resource://floorp-languagetool-corpus/CorpusLanguageToolChild.sys.mjs",
    },
    safeForUntrustedWebProcess: true,
    allFrames: false,
    matches: [
      "http://127.0.0.1/*",
      "http://localhost/*",
      "https://languagetool.org/*",
    ],
  });
  const candidateWindow =
    /** @type {import("./network-test-types.ts").NativeBrowserWindow | null} */ (
      Services.wm.getMostRecentWindow("navigator:browser")
    );
  if (!candidateWindow) throw Error("Native browser window missing");
  const win = candidateWindow;
  const principal = Services.scriptSecurityManager.getSystemPrincipal();
  const originalTabs = new Set(win.gBrowser.tabs);
  item.nativeServiceRequests = [];
  const transportObserver = {
    /** @param {nsISupports} subject @param {string} topic */
    observe(subject, topic) {
      try {
        if (!subject.QueryInterface) return;
        const channel = subject.QueryInterface(Ci.nsIHttpChannel);
        if (
          !/^https:\/\/(?:api(?:-fallback)?\.languagetool\.org|api\.languagetoolplus\.com)\/v2\/check(?:\?|$)/
            .test(
              channel.URI.spec,
            )
        ) {
          return;
        }
        (item.nativeServiceRequests ??= []).push({
          topic,
          url: channel.URI.spec.split(/[?#]/)[0],
          method: channel.requestMethod,
          addonId: channel.loadInfo.triggeringPrincipal?.addonId,
          ...(topic === "http-on-examine-response"
            ? { status: channel.responseStatus }
            : {}),
        });
      } catch {
        /* Other observer subjects are irrelevant to this owned fixture. */
      }
    },
  };
  for (const topic of ["http-on-modify-request", "http-on-examine-response"]) {
    Services.obs.addObserver(transportObserver, topic);
  }
  /** @type {import("./native-test-types.ts").NativeCorpusAddon | null | undefined} */
  let addon;
  /** @type {import("./network-test-types.ts").NativeTab | undefined} */
  let tab;
  /** @template {"Snapshot" | "Click" | "Type"} N
   * @param {import("./network-test-types.ts").NativeTab} target
   * @param {N} name
   * @param {import("./network-test-types.ts").UIControl} [data]
   * @returns {Promise<import("./network-test-types.ts").LanguageResults[N]>} */
  async function query(target, name, data) {
    const global = target.linkedBrowser.browsingContext.currentWindowGlobal;
    if (!global) throw Error("Current native document missing");
    const actor =
      /** @type {import("./network-test-types.ts").LanguageActor} */ (
        global.getActor("CorpusLanguageTool")
      );
    /** @type {number | undefined} */
    let timer;
    try {
      /** @type {Promise<never>} */
      const timeout = new Promise((_, reject) => {
        timer = setTimeout(
          () => reject(Error("Native LanguageTool actor timed out " + name)),
          5000,
        );
      });
      return await Promise.race([actor.sendQuery(name, data), timeout]);
    } finally {
      if (timer !== undefined) clearTimeout(timer);
    }
  }

  /** @param {string} url */
  function navigate(url) {
    const target = tab;
    if (!target) throw Error("Grammar tab missing");
    const old = target.linkedBrowser.browsingContext.currentWindowGlobal
      ?.innerWindowId;
    target.linkedBrowser.loadURI(Services.io.newURI(url), {
      triggeringPrincipal: principal,
    });
    return until(async () => {
      const value = await query(target, "Snapshot").catch(() => null);
      return (
        target.linkedBrowser.browsingContext.currentWindowGlobal
            ?.innerWindowId !== old &&
        value?.ready === "complete" &&
        value.url === url &&
        value
      );
    }, "new native grammar document");
  }
  try {
    tab = win.gBrowser.addTab(fixture.url, { triggeringPrincipal: principal });
    win.gBrowser.selectedTab = tab;
    const editorTab = tab;
    item.baseline = await until(async () => {
      const value = await query(editorTab, "Snapshot").catch(() => null);
      return value?.ready === "complete" && value.url === fixture.url && value;
    }, "authored native grammar editor");
    const installed = await installFrozenCorpus(root, record, output);
    addon = installed.addon;
    item.conversion = installed.conversion;
    item.warnings = installed.warnings;
    item.sourceIntegrity = installed.sourceIntegrity;
    item.nativeEvidence = installed.nativeEvidence;
    item.background = installed.background;
    await until(
      () =>
        installed.extension.backgroundContext?.viewType === "background_worker",
      "genuine compiled LanguageTool background_worker",
    );
    if (
      !report.runtime.nativeWorkerCompiled ||
      installed.background?.selected !== "service_worker"
    ) {
      throw Error(
        "Primary operation requires production-selected compiled service worker",
      );
    }
    item.nativeWorkerVerified = true;
    const welcome = await until(
      () =>
        [...win.gBrowser.tabs].find(
          (candidate) =>
            candidate !== tab &&
            candidate.linkedBrowser.currentURI.spec.startsWith(
              "https://languagetool.org/webextension/welcome",
            ),
        ),
      "actual product welcome page",
    );
    item.remoteSetupAttempt = { url: welcome.linkedBrowser.currentURI.spec };
    welcome.linkedBrowser.loadURI(
      Services.io.newURI(fixture.onboardingHost.url),
      { triggeringPrincipal: principal },
    );
    const notice = await until(async () => {
      const value = await query(welcome, "Snapshot").catch(() => null);
      return (
        value?.url === fixture.onboardingHost.url &&
        value.ready === "complete" &&
        value.body.includes("Okay, got it") &&
        value
      );
    }, "actual synthetic-text data transmission notice");
    item.onboarding = notice;
    if (
      !notice.body.includes(
        "This extension will check your text by sending it to LanguageTool.org over an encrypted connection.",
      ) ||
      !notice.body.includes("Your text will not be stored") ||
      /agree to.*terms|purchase|subscribe|create.*account/i.test(notice.body)
    ) {
      item.primaryOperationResult = "blocked-unreviewed-consent";
      throw Error(
        "Actual notice differs from authorized reversible synthetic-text setup",
      );
    }
    win.gBrowser.selectedTab = welcome;
    await query(welcome, "Click", { menuText: "Okay, got it" });
    item.consent = {
      notice: notice.body,
      nativeControl: "Okay, got it",
      scope:
        "Disposable browser profile; authored synthetic sentence only; no account",
    };
    await pause(500);
    if ([...win.gBrowser.tabs].includes(welcome)) {
      win.gBrowser.removeTab(welcome, { skipPermitUnload: true });
    }
    win.gBrowser.selectedTab = tab;
    await navigate(fixture.url);
    const verifyRemoval = await runLanguageToolOperation({
      item,
      url: fixture.url,
      text: fixture.text,
      until,
      pause,
      navigate,
      snapshot: () => query(editorTab, "Snapshot"),
      click: ({ selector, text }) =>
        query(editorTab, "Click", { selector, text }),
      type: (text) => query(editorTab, "Type", { selector: "#editor", text }),
    });
    if (!addon) throw Error("Installed LanguageTool addon missing");
    await addon.uninstall();
    addon = null;
    await verifyRemoval();
    if (
      !item.nativeServiceRequests?.some((request) => request.status === 200)
    ) {
      throw Error(
        "Actual native LanguageTool service response was not observed",
      );
    }
    item.checks.push(
      "The native HTTP channel observed the actual LanguageTool HTTPS check service returning status 200",
    );
    item.primaryOperationResult = "passed";
  } catch (error) {
    if (item.primaryOperationResult === "not-run") {
      item.primaryOperationResult = "failed";
    }
    item.error = String(error);
    if (tab) item.lastSnapshot = await query(tab, "Snapshot").catch(() => null);
  } finally {
    /** @type {string[]} */
    const cleanupErrors = [];
    /** @param {() => void | Promise<void>} operation */
    const cleanup = async (operation) => {
      try {
        await operation();
      } catch (error) {
        cleanupErrors.push(String(error));
      }
    };
    for (
      const topic of ["http-on-modify-request", "http-on-examine-response"]
    ) {
      await cleanup(() =>
        Services.obs.removeObserver(transportObserver, topic)
      );
    }
    await cleanup(async () => {
      await addon?.uninstall();
    });
    for (const candidate of [...win.gBrowser.tabs]) {
      if (!originalTabs.has(candidate)) {
        await cleanup(() =>
          win.gBrowser.removeTab(candidate, { skipPermitUnload: true })
        );
      }
    }
    await cleanup(() =>
      ChromeUtils.unregisterWindowActor("CorpusLanguageTool")
    );
    await cleanup(() =>
      resource.setSubstitution("floorp-languagetool-corpus", null)
    );
    item.cleanupCompleted = cleanupErrors.length === 0;
    if (cleanupErrors.length) item.cleanupError = cleanupErrors.join("\n");
    item.nativeQualified = item.primaryOperationResult === "passed" &&
      item.cleanupCompleted &&
      item.nativeEvidence?.candidateIdentityVerified === true &&
      item.nativeEvidence.nativeBackgroundVerified === true;
    await save();
  }

  if (item.primaryOperationResult !== "passed" || !item.cleanupCompleted) {
    throw Error(
      "Real LanguageTool primary operation failed: " +
        (item.error || item.cleanupError),
    );
  }
}
