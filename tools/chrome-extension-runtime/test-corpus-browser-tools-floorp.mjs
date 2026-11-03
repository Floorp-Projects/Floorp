// SPDX-License-Identifier: MPL-2.0
import {
  installFrozenCorpus,
  registerCorpusOperations,
  sha256,
} from "./corpus-install-fixture.mjs";
import { invokeNativeCorpusAction } from "./corpus-native-action.mjs";
/** @type {{ExtensionParent: import("./native-test-types.ts").NativeCorpusExtensionParent}} */
const { ExtensionParent } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionParent.sys.mjs",
);
const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
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
/**
 * @template {keyof import("./browser-tools-test-types.ts").OperationReplies} Name
 * @param {import("./native-test-types.ts").NativeCorpusBrowser} browser
 * @param {Name} name
 * @param {import("./browser-tools-test-types.ts").OperationRequest} [data]
 * @returns {Promise<import("./browser-tools-test-types.ts").OperationReplies[Name]>}
 */
async function query(browser, name, data) {
  const actor = browser.browsingContext.currentWindowGlobal?.getActor(
    "CorpusOperations",
  );
  if (!actor) throw Error("Private corpus document actor unavailable");
  /** @type {number | undefined} */
  let timer;
  try {
    return /** @type {import("./browser-tools-test-types.ts").OperationReplies[Name]} */ (await Promise
      .race([
        actor.sendQuery(name, data),
        new Promise((_resolve, reject) => {
          timer = setTimeout(
            () => reject(Error("Private corpus actor query deadline")),
            5000,
          );
        }),
      ]));
  } finally {
    if (timer !== undefined) clearTimeout(timer);
  }
}
/** @returns {import("./native-test-types.ts").NativeCorpusWindow[]} */
function nativeWindows() {
  return /** @type {import("./native-test-types.ts").NativeCorpusWindow[]} */ ([
    ...Services.wm.getEnumerator("navigator:browser"),
  ]).filter((window) => !!window.gBrowser);
}
export async function runAllTests() {
  const root = Services.env.get("FLOORP_CORPUS_ROOT");
  if (!root) throw Error("FLOORP_CORPUS_ROOT is required");
  const id = Services.env.get("FLOORP_CORPUS_IDS") ||
    "chphlpgkkbolifaimnlloiipkdnihall";
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
  const record = corpus.extensions.find((e) => e.id === id);
  if (!record) throw Error("Frozen extension missing");
  const output = PathUtils.join(
    root,
    "_dist",
    "chrome-runtime-tests",
    "corpus-browser-tools-floorp-" + Date.now(),
  );
  await IOUtils.makeDirectory(output);
  /** @type {import("./browser-tools-test-types.ts").NativeBrowserToolsReport} */
  const report = {
    stage: "actual-package-primary-operation",
    output,
    fixture: { pages: [] },
    runtime: {
      version: Services.appinfo.version,
      buildId: Services.appinfo.appBuildID,
      workerPolicy: WebExtensionPolicy.backgroundServiceWorkerEnabled,
    },
    extension: {
      id,
      name: record.name,
      version: record.version,
      packageSha256: record.packageSha256,
      primaryOperation: record.primaryOperation,
      primaryOperationResult: "not-run",
      checks: [],
      snapshots: [],
    },
  };
  report.runtime.workerWebIDLCompiled =
    ChromeUtils.importESModule("resource://gre/modules/AppConstants.sys.mjs")
      .AppConstants.MOZ_WEBEXT_WEBIDL_ENABLED === true;
  const save = () =>
    IOUtils.writeJSON(PathUtils.join(output, "report.json"), report);
  registerCorpusOperations(root);
  const window =
    /** @type {import("./native-test-types.ts").NativeCorpusWindow} */ (Services
      .wm.getMostRecentWindow("navigator:browser"));
  const initialWindows = new Set(nativeWindows());
  const initialTabs = new Set(window.gBrowser.tabs);
  /** @param {import("./native-test-types.ts").NativeCorpusBrowser} browser @param {string} label */
  const snapshot = async (browser, label) => {
    const value = await query(browser, "Snapshot");
    report.extension.snapshots.push({ label, ...value });
    await save();
    return value;
  };
  /** @param {import("./native-test-types.ts").NativeCorpusBrowser} browser @param {string} text */
  const click = (browser, text) =>
    query(browser, "Click", {
      selector: "button,a,div,span",
      text,
      exact: true,
    });
  /** @type {import("./native-test-types.ts").NativeCorpusAddon | undefined} */
  let addon;
  try {
    const installed = await installFrozenCorpus(root, record, output);
    addon = installed.addon;
    report.extension.unchangedScriptCount = installed.unchangedScriptCount;
    report.extension.unchangedScriptsSha256 = installed.unchangedScriptsSha256;
    report.extension.sourceIntegrity = installed.sourceIntegrity;
    report.extension.nativeEvidence = installed.nativeEvidence;
    report.extension.backgroundSelection = installed.background;
    const extension = ExtensionParent.GlobalManager.extensionMap.get(
      id + "@chromium-extension",
    );
    if (!extension) throw Error("Actual native extension policy missing");
    await delay(1800);
    if (installed.background?.selected === "service_worker") {
      await until(
        () => extension.backgroundContext?.viewType === "background_worker",
        "actual selected native Worker background",
      );
    }
    report.extension.backgroundContextType =
      extension.backgroundContext?.viewType || null;
    if (record.name === "Momentum") {
      const entry = extension.baseURI.resolve("index.html");
      report.fixture = {
        pages: [],
        entry,
        entrySha256: await sha256(
          await IOUtils.read(
            PathUtils.join(
              root,
              "_dist",
              "chrome-extension-corpus",
              "chrome-unpacked",
              id,
              "index.html",
            ),
          ),
        ),
      };
      const tab = window.gBrowser.addTab(entry, {
        triggeringPrincipal: extension.principal,
      });
      window.gBrowser.selectedTab = tab;
      const browser = tab.linkedBrowser;
      await until(async () => {
        const state = await query(browser, "Snapshot").catch(() => null);
        return state?.controls?.some((control) => control.id === "name");
      }, "Momentum's actual name onboarding");
      await snapshot(browser, "advertised-view");
      report.extension.momentumStage = "native-name-input";
      await query(browser, "Type", {
        selector: "#name",
        text: "Authored User",
      });
      await snapshot(browser, "after-native-name-input");
      report.extension.momentumStage = "native-continue-click";
      await until(async () => {
        const state = await query(browser, "Snapshot").catch(() => null);
        return state?.controls?.some((control) =>
          control.label?.trim() === "Continue"
        );
      }, "Momentum actual Continue control");
      const hit = await query(browser, "InspectHit", {
        selector: "button",
        text: "Continue",
        exact: true,
      });
      if (!hit.unobstructed) {
        report.extension.pointerOcclusion = hit;
        report.extension.continueKeyboard = await query(browser, "PressKey", {
          key: "Enter",
        });
        if (report.extension.continueKeyboard.focusBefore.id !== "name") {
          throw Error(
            "Native name input lost focus before keyboard submission",
          );
        }
      } else {report.extension.continueClick = await query(browser, "Click", {
          selector: "button",
          text: "Continue",
          exact: true,
        });}
      await until(async () => {
        const state = await query(browser, "Snapshot").catch(() => null);
        return state &&
          !state.controls.some((control) => control.id === "name");
      }, "Momentum name submission");
      await snapshot(browser, "after-name-setting");
      report.extension.momentumStage = "native-stay-logged-out-click";
      await until(async () => {
        const state = await query(browser, "Snapshot").catch(() => null);
        return state?.text?.includes("Stay logged out");
      }, "Momentum guest login choice");
      const guestHit = await query(browser, "InspectHit", {
        selector: "button,a,div,span",
        text: "Stay logged out",
        exact: true,
      });
      report.extension.guestHit = guestHit;
      if (!guestHit.unobstructed) {
        report.extension.guestPointerOcclusion = guestHit;
        report.extension.guestKeyboardFocus = [];
        let reached = false;
        for (let index = 0; index < 24; index++) {
          const step = await query(browser, "PressKey", { key: "Tab" });
          report.extension.guestKeyboardFocus.push(step);
          if (step.focusAfter?.text === "Stay logged out") {
            reached = true;
            break;
          }
        }
        if (!reached) {
          throw Error(
            "Actual guest choice is covered by the portal overlay and not reachable through native Tab navigation",
          );
        }
        report.extension.guestKeyboard = await query(browser, "PressKey", {
          key: "Enter",
        });
      } else {report.extension.guestClick = await click(
          browser,
          "Stay logged out",
        );}
      await delay(500);
      await snapshot(browser, "after-native-guest-choice");
      await until(async () => {
        const state = await query(browser, "Snapshot").catch(() => null);
        return state?.text?.includes("Authored User") &&
          !state.text.includes("How would you like to log in");
      }, "Momentum's actual guest dashboard");
      await snapshot(browser, "guest-dashboard");
      const previousInnerWindowId = await until(
        () => browser.browsingContext.currentWindowGlobal?.innerWindowId,
        "native dashboard inner window",
      );
      browser.reload();
      const reloadedGlobal = await until(() => {
        const global = browser.browsingContext.currentWindowGlobal;
        return global && global.innerWindowId !== previousInnerWindowId
          ? global
          : undefined;
      }, "Momentum's distinct reloaded document");
      report.extension.reloadWindows = {
        before: previousInnerWindowId,
        after: reloadedGlobal.innerWindowId,
      };
      await until(async () => {
        const state = await query(browser, "Snapshot").catch(() => null);
        return state?.ready === "complete" &&
          state.text?.includes("Authored User") &&
          !state.controls.some((control) => control.id === "name");
      }, "Momentum persisted authored name in the fresh guest dashboard");
      await snapshot(browser, "after-native-reload");
      report.extension.checks.push(
        "Actual name input and Continue UI saved the authored name",
      );
      report.extension.checks.push(
        "Actual Stay logged out UI opened the guest dashboard without an external account",
      );
      report.extension.checks.push(
        "A distinct native document after reload retained the authored name",
      );
      report.extension.primaryOperationResult = "passed";
      return;
    }
    const base = window.wrappedJSObject.__floorpRuntimeFixtureURL;
    const pages = ["corpus-tabs-first.html", "corpus-tabs-second.html"];
    const authoredTabs = [];
    report.fixture = { pages: [] };
    for (const page of pages) {
      const bytes = await IOUtils.read(
        PathUtils.join(
          root,
          "tools",
          "chrome-extension-runtime",
          "fixtures",
          page,
        ),
      );
      const url = base + page;
      report.fixture.pages.push({ url, sha256: await sha256(bytes) });
      const tab = window.gBrowser.addTab(url, {
        triggeringPrincipal: Services.scriptSecurityManager
          .getSystemPrincipal(),
      });
      authoredTabs.push(tab);
      await until(
        () => tab.linkedBrowser.contentTitle.includes("Authored saved session"),
        "authored tab",
      );
    }
    if (record.name === "Toby") {
      report.extension.action = await invokeNativeCorpusAction(
        extension,
        window,
        authoredTabs[0],
      );
      const panel = await until(
        () =>
          [...extension.views].find((view) =>
            view.viewType === "sidebar" && !view.unloaded
          )?.xulBrowser,
        "actual native Toby side panel",
      );
      await until(
        async () =>
          (await query(panel, "Snapshot").catch(() => null))?.ready ===
            "complete",
        "Toby panel document",
      );
      const initial = await snapshot(panel, "actual-Toby-view");
      if (
        initial.controls.some((control) =>
          control.label?.trim() === "Start" || control.text?.trim() === "Start"
        )
      ) {
        await click(panel, "Start");
      }
      await until(
        async () =>
          (await query(panel, "Snapshot")).text.includes(
            "Sign in to save tabs to Toby",
          ),
        "actual Toby account prerequisite",
      );
      await snapshot(panel, "actual-Toby-account-prerequisite");
      report.extension.primaryOperationResult = "blocked-account";
      report.extension.blocker =
        "The actual native panel requires a Toby account before saving tabs; no collection operation was counted as passed.";
      report.extension.checks.push(
        "Observed the genuine account prerequisite in the original extension side panel",
      );
      return;
    }
    if (record.name === "Cookie Editor") {
      const source = authoredTabs[0];
      report.extension.action = await invokeNativeCorpusAction(
        extension,
        window,
        source,
      );
      const popup = await until(
        () =>
          [...extension.views].find((view) =>
            view.viewType === "popup" && !view.unloaded
          )?.xulBrowser,
        "actual Cookie-Editor action popup",
      );
      await until(
        async () =>
          (await query(popup, "Snapshot").catch(() => null))?.text?.includes(
            "Cookie-Editor",
          ),
        "actual cookie popup content",
      );
      const initial = await snapshot(popup, "actual-native-cookie-popup");
      /** @param {string} selector @param {string} [text] */
      const control = (selector, text) =>
        query(popup, "Click", { selector, text, exact: true });
      if (initial.text.includes("Request permission")) {
        await control("#request-permission");
        const notification = await until(() => {
          const node =
            /** @type {import("./native-test-types.ts").NativePermissionNotification | null} */ (window
              .PopupNotifications.panel.querySelector(
                "popupnotification[popupid='addon-webext-permissions']",
              ));
          return window.PopupNotifications.panel.state === "open" ? node : null;
        }, "actual cookie host permission prompt");
        const allow = notification.button ||
          notification.shadowRoot?.querySelector(
            ".popup-notification-primary-button",
          ) || notification.querySelector(".popup-notification-primary-button");
        if (!allow) {
          throw Error("Actual native permission Allow button missing");
        }
        allow.click();
        report.extension.checks.push(
          "Approved the genuine product-requested native host permission prompt",
        );
      }
      await until(
        async () =>
          (await query(popup, "Snapshot")).text?.includes(
            "does not have any cookies",
          ),
        "actual empty-cookie page after permission",
      );
      await delay(450);
      await control("#create-cookie");
      await query(popup, "Type", {
        selector: "#name-create",
        text: "floorp_authored_cookie",
      });
      await query(popup, "Type", {
        selector: "#value-create",
        text: "created-through-real-ui",
      });
      await control("#save-create-cookie");
      await until(
        async () =>
          (await query(source.linkedBrowser, "Snapshot")).cookie?.includes(
            "floorp_authored_cookie=created-through-real-ui",
          ),
        "created cookie independently observed on authored page",
      );
      await snapshot(popup, "actual-created-cookie");
      await control(".header-name", "floorp_authored_cookie");
      await query(popup, "Type", {
        selector: ".input-value",
        text: "edited-through-real-ui",
        replace: true,
      });
      await control("button.save");
      await until(
        async () =>
          (await query(source.linkedBrowser, "Snapshot")).cookie?.includes(
            "floorp_authored_cookie=edited-through-real-ui",
          ),
        "edited cookie independently observed on authored page",
      );
      await snapshot(popup, "actual-edited-cookie");
      await control("button.delete");
      await until(
        async () =>
          !(await query(source.linkedBrowser, "Snapshot")).cookie?.includes(
            "floorp_authored_cookie=",
          ),
        "deleted cookie independently observed on authored page",
      );
      report.extension.checks.push(
        "Actual product UI created, edited, and deleted the cookie; authored page independently observed each effect",
      );
      report.extension.primaryOperationResult = "passed";
      return;
    }
    if (record.name === "Session Buddy") {
      const tab = window.gBrowser.addTab(
        extension.baseURI.resolve("session-buddy.html"),
        { triggeringPrincipal: extension.principal },
      );
      window.gBrowser.selectedTab = tab;
      const browser = tab.linkedBrowser;
      await until(
        async () =>
          (await query(browser, "Snapshot").catch(() => null))?.text?.includes(
            "Save tabs to create a collection",
          ),
        "actual Session Buddy initial collection UI",
      );
      const initial = await snapshot(browser, "actual-session-view");
      if (initial.text.includes("Thanks for installing Session Buddy")) {
        await query(browser, "Click", {
          selector: "button",
          text: "Close",
          exact: true,
        });
      }
      await query(browser, "Click", {
        selector: "button",
        text: "Save",
        exact: true,
      });
      await until(
        async () =>
          (await query(browser, "Snapshot")).controls.some((control) =>
            control.id === "set-editor-title"
          ),
        "actual collection title dialog",
      );
      await query(browser, "Type", {
        selector: "#set-editor-title",
        text: "Authored Session",
        replace: true,
      });
      await query(browser, "Click", {
        selector: "button",
        text: "OK",
        exact: true,
      });
      await until(async () => {
        const state = await query(browser, "Snapshot");
        return state.text.includes("Authored Session") &&
          state.text.includes("Authored saved session /first") &&
          state.text.includes("Authored saved session /second");
      }, "actual saved authored collection");
      await snapshot(browser, "actual-saved-collection");
      for (const source of authoredTabs) window.gBrowser.removeTab(source);
      const previous = await until(
        () => browser.browsingContext.currentWindowGlobal?.innerWindowId,
        "native collection inner window",
      );
      browser.reload();
      await until(
        () =>
          browser.browsingContext.currentWindowGlobal?.innerWindowId !==
            previous,
        "distinct reloaded collection document",
      );
      await until(async () => {
        const state = await query(browser, "Snapshot").catch(() => null);
        return state?.ready === "complete" &&
          state.text.includes("Authored Session") &&
          state.text.includes("Authored saved session /first") &&
          state.text.includes("Authored saved session /second");
      }, "persisted collection after closing source tabs and reloading");
      await query(browser, "Click", {
        selector: "button",
        text: "Open",
        exact: true,
      });
      await until(
        () =>
          report.fixture.pages.every((page) =>
            nativeWindows().some((browserWindow) =>
              [...browserWindow.gBrowser.tabs].some((candidate) =>
                candidate.linkedBrowser.currentURI.spec === page.url
              )
            )
          ),
        "Session Buddy actual Open restored both exact authored URLs",
      );
      report.extension.checks.push(
        "Actual Save UI created named collection with both authored tabs; collection survived source-tab closure and distinct-document reload; actual Open restored both exact URLs",
      );
      report.extension.primaryOperationResult = "passed";
      return;
    }
    if (record.name !== "OneTab") {
      throw Error("Actual product UI recipe is not yet implemented");
    }
    const tab = window.gBrowser.addTab(
      extension.baseURI.resolve("onetab.html"),
      { triggeringPrincipal: extension.principal },
    );
    window.gBrowser.selectedTab = tab;
    const browser = tab.linkedBrowser;
    await until(
      async () =>
        (await query(browser, "Snapshot").catch(() => null))?.text?.includes(
          "Store all open windows here",
        ),
      "actual OneTab app UI",
    );
    await snapshot(browser, "actual-OneTab-view");
    await click(browser, "Store all open windows here");
    const approval = await until(async () => {
      const state = await query(browser, "Snapshot").catch(() => null);
      if (state?.text?.includes("Authored saved session /first")) {
        return { saved: true };
      }
      const panel = window.PopupNotifications.panel;
      const notification =
        /** @type {import("./native-test-types.ts").NativePermissionNotification | null} */ (panel
          .querySelector(
            "popupnotification[popupid='addon-webext-permissions']",
          ));
      if (notification && panel.state === "open") return { notification };
      return undefined;
    }, "OneTab save or actual optional permission prompt");
    if (approval.notification) {
      const notification = approval.notification;
      const primary = notification.button ||
        notification.shadowRoot?.querySelector(
          ".popup-notification-primary-button",
        ) || notification.querySelector(".popup-notification-primary-button");
      if (!primary) {
        throw Error("Actual native permission Allow control missing");
      }
      primary.click();
      report.extension.checks.push(
        "Accepted the genuine product-requested tabGroups permission prompt",
      );
    }
    await until(async () => {
      const state = await query(browser, "Snapshot");
      return state.text.includes("Authored saved session /first") &&
        state.text.includes("Authored saved session /second");
    }, "actual saved authored session");
    await snapshot(browser, "saved-session");
    await until(
      () =>
        ![...window.gBrowser.tabs].some((tab) =>
          report.fixture.pages.some((page) =>
            tab.linkedBrowser.currentURI.spec === page.url
          )
        ),
      "OneTab actually closed both authored tabs",
    );
    report.extension.checks.push(
      "Actual Store all open windows UI saved and closed both authored tabs",
    );
    await click(browser, "Restore all");
    await delay(500);
    const restore = await snapshot(browser, "after-restore-click");
    if (restore.text.includes("After restoring tabs:")) {
      await click(browser, "Remove them from your list");
      await click(browser, "Save choice");
    }
    await until(
      () =>
        report.fixture.pages.every((page) =>
          [...window.gBrowser.tabs].some((tab) =>
            tab.linkedBrowser.currentURI.spec === page.url
          )
        ),
      "native restored authored tabs",
    );
    report.extension.checks.push(
      "Actual Restore all UI recreated both authored tabs",
    );
    report.extension.primaryOperationResult = "passed";
  } catch (error) {
    report.extension.primaryOperationResult = "failed";
    report.extension.error = String(error);
    throw error;
  } finally {
    if (![...window.gBrowser.tabs].some((tab) => initialTabs.has(tab))) {
      // A real session manager intentionally closes the harness's original tabs.
      // Keep the browser alive during cleanup after its primary effects were checked.
      initialTabs.add(
        window.gBrowser.addTab("about:blank", {
          triggeringPrincipal: Services.scriptSecurityManager
            .getSystemPrincipal(),
        }),
      );
    }
    await addon?.uninstall();
    for (const browserWindow of nativeWindows()) {
      if (!initialWindows.has(browserWindow)) browserWindow.close();
    }
    for (const tab of [...window.gBrowser.tabs]) {
      if (!initialTabs.has(tab)) window.gBrowser.removeTab(tab);
    }
    ChromeUtils.unregisterWindowActor("CorpusOperations");
    await save();
    console.log(
      "[Actual corpus] " + record.name + " " +
        report.extension.primaryOperationResult + " " + output,
    );
  }
}
