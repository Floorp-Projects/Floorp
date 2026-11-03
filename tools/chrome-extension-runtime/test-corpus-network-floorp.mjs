// SPDX-License-Identifier: MPL-2.0
// Production conversion of immutable products; primary operations use their own UI.
import { invokeNativeCorpusAction } from "./corpus-native-action.mjs";
import { installFrozenCorpus } from "./corpus-install-fixture.mjs";
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
const { ExtensionPermissions } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionPermissions.sys.mjs",
);
const { FileUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/FileUtils.sys.mjs",
);
/** @param {number} ms */
const delay = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
export async function runAllTests() {
  const root = Services.env.get("FLOORP_CORPUS_ROOT");
  const fixture = Services.env.get("FLOORP_CORPUS_FIXTURE");
  const proxyPort = Services.env.get("FLOORP_CORPUS_PROXY_PORT");
  const ids = Services.env.get("FLOORP_CORPUS_IDS").split(",").filter(Boolean);
  if (!root || !fixture || !proxyPort)
    throw Error("Network corpus fixture environment missing");
  /** @type {import("./network-test-types.ts").NetworkCorpus} */
  const corpus = await IOUtils.readJSON(
    PathUtils.join(
      root,
      "tools",
      "chrome-extension-runtime",
      "corpus-lock.json",
    ),
  );
  const output = Services.env.get("FLOORP_CORPUS_OUTPUT");
  if (!output) throw Error("Network corpus output missing");
  await IOUtils.makeDirectory(output);
  /** @type {import("./network-test-types.ts").NetworkReport} */
  const report = {
    nativeQualified: false,
    nativeCandidateIdentity: null,
    qualificationMode:
      Services.env.get("FLOORP_TEST_REQUIRE_NATIVE_CORPUS") === "1"
        ? "candidate"
        : "historical",
    stage: "actual-package-primary-operation",
    browserMode: Services.env.get("FLOORP_CORPUS_BROWSER_MODE"),
    fixture,
    proxyPort,
    runtime: {
      version: Services.appinfo.version,
      buildId: Services.appinfo.appBuildID,
      nativeWorkerPolicy: WebExtensionPolicy.backgroundServiceWorkerEnabled,
      workerWebIDLCompiled:
        ChromeUtils.importESModule(
          "resource://gre/modules/AppConstants.sys.mjs",
        ).AppConstants.MOZ_WEBEXT_WEBIDL_ENABLED === true,
    },
    primaryOperationPassRate: null,
    extensions: [],
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
    "floorp-network-corpus",
    Services.io.newFileURI(
      new FileUtils.File(
        Services.env.get("FLOORP_TEST_ACTOR_DIRECTORY") ||
          PathUtils.join(root, "tools", "chrome-extension-runtime"),
      ),
    ),
  );
  ChromeUtils.registerWindowActor("CorpusNetwork", {
    child: {
      esModuleURI:
        "resource://floorp-network-corpus/CorpusNetworkChild.sys.mjs",
    },
    allFrames: false,
    safeForUntrustedWebProcess: true,
  });
  const candidateWindow =
    /** @type {import("./network-test-types.ts").NativeBrowserWindow | null} */ (
      Services.wm.getMostRecentWindow("navigator:browser")
    );
  if (!candidateWindow) throw Error("Browser window missing");
  const window = candidateWindow;
  const oldProxy = Services.prefs.getIntPref("network.proxy.type", 0);
  const oldDns = Services.prefs.getStringPref("network.dns.localDomains", "");
  Services.prefs.setIntPref("network.proxy.type", 0);
  Services.prefs.setStringPref(
    "network.dns.localDomains",
    [oldDns, "bypass.floorp-proxy.test", "routed.floorp-proxy.test"]
      .filter(Boolean)
      .join(","),
  );
  /** @param {import("./network-test-types.ts").NativeTab} tab */
  const actorFor = (tab) => {
    const global = tab.linkedBrowser.browsingContext.currentWindowGlobal;
    if (!global) throw Error("Current native document missing");
    return /** @type {import("./network-test-types.ts").NativeActor} */ (
      global.getActor("CorpusNetwork")
    );
  };
  /** @template T
   * @param {() => T | false | null | undefined | Promise<T | false | null | undefined>} read
   * @param {string} label
   * @param {number} [timeout]
   * @returns {Promise<T>} */
  async function until(read, label, timeout = 15000) {
    const deadline = Date.now() + timeout;
    while (Date.now() < deadline) {
      try {
        const value = await read();
        if (value) return value;
      } catch {
        /* Native document and actor creation may still be in progress. */
      }
      await delay(100);
    }
    throw Error("Timed out waiting for " + label);
  }
  /** @param {string} url @param {nsIPrincipal} principal */
  async function open(url, principal) {
    const tab = window.gBrowser.addTab(url, { triggeringPrincipal: principal });
    window.gBrowser.selectedTab = tab;
    await until(async () => {
      const data = await actorFor(tab)?.sendQuery("Snapshot");
      return (
        data?.ready === "complete" && data.url.startsWith(url.split("#")[0])
      );
    }, url);
    return { tab, actor: () => actorFor(tab) };
  }
  /** @param {import("./network-test-types.ts").NativePage} page */
  const snapshot = (page) => page.actor().sendQuery("Snapshot");
  /** @param {import("./network-test-types.ts").NativePage} page @param {string} selector @param {string} [text] @param {boolean} [optional] */
  async function click(page, selector, text, optional) {
    const result = await page
      .actor()
      .sendQuery("Click", { selector, text, optional });
    await delay(100);
    return result;
  }
  /** @param {import("./network-test-types.ts").NativePage} page @param {string} selector @param {string} value @param {boolean} [optional] */
  const fill = (page, selector, value, optional) =>
    page.actor().sendQuery("Fill", { selector, value, optional });
  /** @param {import("./network-test-types.ts").NativeExtension} extension @param {import("./network-test-types.ts").NativeTabPage} page */
  async function action(extension, page) {
    const currentRecord = report.extensions.at(-1);
    if (!currentRecord) throw Error("Current corpus record missing");
    currentRecord.actionDiagnostics = await invokeNativeCorpusAction(
      extension,
      window,
      page.tab,
    );
    await save();
    const view = await until(
      () =>
        [...extension.views].find(
          (view) => view.viewType === "popup" && !view.unloaded,
        ),
      "actual product popup",
    );
    const result = {
      actor: () => {
        const global = view.xulBrowser?.browsingContext.currentWindowGlobal;
        if (!global) throw Error("Current popup document missing");
        return /** @type {import("./network-test-types.ts").NativeActor} */ (
          global.getActor("CorpusNetwork")
        );
      },
    };
    await until(
      async () => (await snapshot(result))?.ready === "complete",
      "loaded product popup",
    );
    await delay(500);
    return result;
  }
  /** @param {import("./network-test-types.ts").NativePage} page @param {string} url */
  const fetch = (page, url) => page.actor().sendQuery("Fetch", { url });
  /** @param {import("./network-test-types.ts").NativePage} page @param {import("./network-test-types.ts").NetworkRecord} record */
  async function routing(page, record) {
    record.routed = await until(async () => {
      const value = await fetch(page, "http://corpus-proxy.invalid/proxied");
      return value.role === "proxy" && value;
    }, "actual product proxy routing");
    record.bypassed = await fetch(
      page,
      fixture.replace("127.0.0.1", "bypass.floorp-proxy.test") + "bypassed",
    );
    if (record.bypassed.role !== "origin")
      throw Error("Product bypass did not reach actual origin");
    record.checks.push(
      "Product UI profile routes actual HTTP to controlled proxy and bypasses authored host",
    );
  }
  /** @param {import("./network-test-types.ts").NativePage} page @param {import("./network-test-types.ts").NetworkRecord} record */
  async function restored(page, record) {
    record.restored = await until(async () => {
      const value = await fetch(
        page,
        fixture.replace("127.0.0.1", "routed.floorp-proxy.test") + "restored",
      );
      return value.role === "origin" && value;
    }, "actual product proxy disable");
    record.checks.push(
      "Product Direct/Disable UI stops actual controlled proxy routing",
    );
  }
  await save();
  try {
    for (const selected of corpus.extensions.filter((record) =>
      ids.includes(record.id),
    )) {
      /** @type {import("./network-test-types.ts").NetworkRecord} */
      const record = {
        id: selected.id,
        name: selected.name,
        version: selected.version,
        packageSha256: selected.packageSha256,
        primaryOperation: selected.primaryOperation,
        primaryOperationResult: "not-run",
        nativeQualified: false,
        checks: [],
        trace: [],
      };
      report.extensions.push(record);
      const oldTabs = new Set(window.gBrowser.tabs);
      let addon;
      let qualifyAfterCleanup = false;
      /** @type {import("./network-test-types.ts").NativeTabPage | undefined} */
      let baselinePage;
      record.stage = "package-validation";
      await save();
      try {
        if (record.name === "uBlock Origin Lite") {
          baselinePage = await open(
            fixture,
            Services.scriptSecurityManager.getSystemPrincipal(),
          );
          record.baselineAdLoaded = await until(() => {
            if (!baselinePage) throw Error("Baseline page missing");
            return baselinePage.actor().sendQuery("AdState");
          }, "authored script without product");
        }
        record.stage = "production-conversion";
        await save();
        const installed = await installFrozenCorpus(root, selected, output, {
          beforeInstall: async () => {
            await ExtensionPermissions.add(
              selected.id + "@chromium-extension",
              {
                permissions: ["internal:privateBrowsingAllowed"],
                origins: [],
              },
            );
          },
        });
        addon = installed.addon;
        record.conversion = installed.conversion;
        record.warnings = installed.warnings;
        record.unchangedScriptCount = installed.unchangedScriptCount;
        record.unchangedScriptsSha256 = installed.unchangedScriptsSha256;
        record.sourceIntegrity = installed.sourceIntegrity;
        record.nativeEvidence = installed.nativeEvidence;
        report.nativeCandidateIdentity =
          installed.nativeEvidence.candidateIdentity ?? null;
        const extension =
          /** @type {import("./network-test-types.ts").NativeExtension} */ (
            installed.extension
          );
        record.stage = "product-ui";
        record.background = extension.manifest.background;
        record.nativeWorkerVerified =
          installed.nativeEvidence.nativeWorkerVerified;
        record.backgroundViewType = installed.nativeEvidence.backgroundViewType;
        await save();
        await delay(900);
        for (const tab of [...window.gBrowser.tabs])
          if (!oldTabs.has(tab) && tab !== baselinePage?.tab)
            window.gBrowser.removeTab(tab, { skipPermitUnload: true });
        const page =
          baselinePage ||
          (await open(
            fixture,
            Services.scriptSecurityManager.getSystemPrincipal(),
          ));
        if (record.name === "ZeroOmega") {
          const options = await open(
            extension.baseURI.resolve("options.html#!/profile/proxy"),
            extension.principal,
          );
          await delay(800);
          await click(
            options,
            ".modal-footer button[ng-click*=skip]",
            undefined,
            true,
          );
          await until(
            async () =>
              !(await options
                .actor()
                .sendQuery("Exists", { selector: ".modal-backdrop" })),
            "actual product guide dismissal",
          );
          await click(options, ".nav-profile a", "proxy");
          await until(
            () =>
              options.actor().sendQuery("Exists", {
                selector: 'input[ng-model="proxyEditors[scheme].host"]',
              }),
            "product proxy form",
          );
          await fill(
            options,
            'input[ng-model="proxyEditors[scheme].host"]',
            "127.0.0.1",
          );
          await fill(
            options,
            'input[ng-model="proxyEditors[scheme].port"]',
            proxyPort,
          );
          await fill(
            options,
            'textarea[ng-model="bypassList"]',
            "bypass.floorp-proxy.test",
          );
          await click(options, 'a[ng-click="applyOptions()"]');
          await delay(600);
          record.trace.push(await snapshot(options));
          await click(await action(extension, page), "a", "proxy");
          await routing(page, record);
          await click(await action(extension, page), "#js-direct");
          await restored(page, record);
        } else if (record.name === "FoxyProxy") {
          const options = await open(
            extension.baseURI.resolve("content/options.html"),
            extension.principal,
          );
          await click(options, "#nav4");
          await click(options, '.proxy-head button[data-i18n="add"]');
          for (const [key, value] of Object.entries({
            title: "Corpus local proxy",
            hostname: "127.0.0.1",
            port: proxyPort,
            type: "http",
          }))
            await fill(options, `.proxy-div [data-id="${key}"]`, value);
          await fill(options, "#passthrough", "bypass.floorp-proxy.test", true);
          await click(options, '.proxy-section button[type="submit"]');
          await delay(600);
          record.trace.push(await snapshot(options));
          await click(
            await action(extension, page),
            `input[value="127.0.0.1:${proxyPort}"]`,
          );
          await routing(page, record);
          const restore = await action(extension, page);
          await click(restore, 'input[value="disable"]');
          record.restoredConfig = await until(async () => {
            const value = await restore.actor().sendQuery("ProxySettings");
            return value.value.mode === "system" && value;
          }, "actual product Disable system configuration");
          record.restored = await fetch(
            page,
            fixture.replace("127.0.0.1", "routed.floorp-proxy.test") +
              "restored",
          );
          if (record.restored.role === "proxy")
            throw Error("Product Disable UI left controlled proxy active");
          record.checks.push(
            "Product Disable UI selects system configuration and stops controlled proxy routing",
          );
        } else if (record.name === "User Agent Switcher") {
          const popup = await action(extension, page);
          record.trace.push(await snapshot(popup));
          await click(popup, "a", "Firefox");
          await click(popup, "a", "Windows Firefox 33");
          record.echo = await until(async () => {
            const value = await fetch(page, fixture + "echo?ua=selected");
            return (
              /Firefox\/33/.test(value.headers?.["user-agent"] ?? "") && value
            );
          }, "actual User-Agent change");
          record.checks.push(
            "Actual selected product profile changes real HTTP User-Agent",
          );
        } else if (record.name === "uBlock Origin Lite") {
          page.tab.linkedBrowser.reload();
          await delay(1500);
          record.blocked = !(await page.actor().sendQuery("AdState"));
          if (!record.blocked)
            throw Error("Actual product did not block controlled ad script");
          const popup = await action(extension, page);
          record.trace.push(await snapshot(popup));
          await click(popup, '.filteringModeSlider span[data-level="0"]');
          await delay(1000);
          record.trace.push(await snapshot(popup));
          await until(
            () => page.actor().sendQuery("AdState"),
            "actual controlled script allowed by No filtering UI and product reload",
          );
          record.checks.push(
            "Actual bundled static rules block authored script; No filtering UI allows it after native reload",
          );
        } else {
          const popup = await action(extension, page);
          await delay(1500);
          record.trace.push(await snapshot(popup));
          if (record.name === "Requestly") {
            await click(popup, "button", "Modify headers");
            const tab = await until(
              () =>
                [...window.gBrowser.tabs].find((tab) =>
                  tab.linkedBrowser.currentURI.spec.startsWith(
                    "https://app.requestly.io/",
                  ),
                ),
              "actual Requestly remote rule editor",
            );
            const editor = { tab, actor: () => actorFor(tab) };
            await delay(15000);
            const state = await snapshot(editor);
            record.trace.push(state);
            record.primaryOperationResult = state.text?.includes(
              "Running Requestly in automation without explicit permission is prohibited",
            )
              ? "blocked-product-automation-restriction"
              : "blocked-product-setup";
            record.error =
              "Actual remote Requestly editor did not demonstrate a primary operation; no automation restriction bypass or rule submission performed";
          } else if (record.name === "Windscribe") {
            await click(popup, "button", "Login");
            await delay(500);
            record.trace.push(await snapshot(popup));
            record.primaryOperationResult = "blocked-account-required";
            record.error =
              "Actual product requires VPN account login; no credentials supplied";
          } else if (record.name === "Browsec") {
            record.primaryOperationResult = record.trace
              .at(-1)
              ?.text?.includes("Terms of Service")
              ? "blocked-terms-consent-required"
              : "failed";
            record.error =
              "Actual product requires accepting third party Terms of Service/Privacy Policy before VPN use; no agreement accepted on the user's behalf";
          } else {
            record.primaryOperationResult = "blocked-product-setup";
            record.error =
              "Actual product primary operation has not been demonstrated";
          }
        }
        if (record.primaryOperationResult === "not-run")
          record.primaryOperationResult = "passed";
        qualifyAfterCleanup =
          record.primaryOperationResult === "passed" &&
          installed.nativeEvidence.candidateIdentityVerified === true &&
          installed.nativeEvidence.nativeBackgroundVerified === true;
      } catch (error) {
        record.primaryOperationResult = "failed";
        record.error = String(error);
      } finally {
        await addon?.uninstall();
        for (const tab of [...window.gBrowser.tabs])
          if (!oldTabs.has(tab))
            window.gBrowser.removeTab(tab, { skipPermitUnload: true });
        record.nativeQualified = qualifyAfterCleanup;
        await save();
        console.log(
          `[Network corpus] ${record.name}: ${record.primaryOperationResult} ${record.error || ""}`,
        );
      }
    }
  } finally {
    ChromeUtils.unregisterWindowActor("CorpusNetwork");
    resource.setSubstitution("floorp-network-corpus", null);
    Services.prefs.setIntPref("network.proxy.type", oldProxy);
    if (oldDns)
      Services.prefs.setStringPref("network.dns.localDomains", oldDns);
    else Services.prefs.clearUserPref("network.dns.localDomains");
    report.nativeQualified =
      report.extensions.length > 0 &&
      report.extensions.every((record) => record.nativeQualified === true);
    await save();
    console.log(`[Network corpus] report ${output}/report.json`);
  }
  const incomplete = report.extensions.filter(
    (record) => record.primaryOperationResult !== "passed",
  );
  if (!report.extensions.length || incomplete.length) {
    throw Error(
      "Actual network corpus primary operations incomplete: " +
        incomplete
          .map((record) => `${record.name}: ${record.primaryOperationResult}`)
          .join(", "),
    );
  }
}
