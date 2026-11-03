// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { zipSync } from "fflate/browser";
import {
  assert,
  assertEquals,
  runTests,
} from "../../browser-features/chrome/test/utils/test_harness.ts";
import {
  ownedWebRequestControllerSource,
  ownedWebRequestExpectedBody,
  ownedWebRequestProbeSource,
} from "./native-webrequest-batch-probe.mjs";
import type {
  MessagingCandidateAddon,
  MessagingCandidateAddonManager,
  MessagingCandidateParent,
  MessagingCandidateTab,
} from "./native-messaging-candidate-types.ts";
import type {
  OwnedWebRequestCase,
  OwnedWebRequestKind,
  OwnedWebRequestObservation,
  OwnedWebRequestReport,
  OwnedWebRequestWindow,
} from "./native-webrequest-batch-test-types.ts";
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
async function until<Value>(
  read: () => Value | false | undefined,
  label: string,
): Promise<Value> {
  const deadline = Date.now() + 20000;
  while (Date.now() < deadline) {
    const value = read();
    if (value) return value;
    await new Promise((resolve) => setTimeout(resolve, 30));
  }
  throw Error("Timed out waiting for " + label);
}
function titleObservation(
  tab: MessagingCandidateTab,
): OwnedWebRequestObservation | undefined {
  try {
    return JSON.parse(
      tab.linkedBrowser.contentTitle,
    ) as OwnedWebRequestObservation;
  } catch {
    return undefined;
  }
}
function assertObservation(
  item: OwnedWebRequestCase,
  expectedBody: string,
): void {
  const value = item.observation;
  assert(value, "The public extension probe returned its actual observation");
  assert(!value.error, "Public extension probe completed: " + value.error);
  assertEquals(value.kind, item.kind, "Actual probe variant");
  const worker = item.kind !== "ordinary-document";
  assertEquals(value.isWorker, worker, "Actual ServiceWorkerGlobalScope realm");
  assertEquals(value.hasDocument, !worker, "Actual document availability");
  assertEquals(
    value.importWitness,
    item.kind,
    "Actual classic/module/document script execution",
  );
  assertEquals(
    value.runtimeId,
    item.expectedRuntimeId,
    "Actual extension identity",
  );
  if (worker) {
    assertEquals(
      item.backgroundViewType,
      "background_worker",
      "Native background context",
    );
  }
  assert(value.response, "Actual owned HTTP fetch completed");
  assertEquals(value.response.status, 200, "Actual owned HTTP response status");
  assertEquals(value.response.url, value.url, "Actual owned HTTP response URL");
  assertEquals(
    value.response.redirected,
    false,
    "Owned HTTP fetch did not redirect",
  );
  assertEquals(
    value.response.body,
    expectedBody,
    "Actual response matches the authored HTTP fixture bytes",
  );
  assertEquals(
    JSON.stringify(value.records.map((record) => record.name)),
    JSON.stringify([
      "onBeforeRequest",
      "onBeforeSendHeaders",
      "onResponseStarted",
      "onCompleted",
    ]),
    "Every actual nonblocking webRequest event arrives once in network order",
  );
  const requestId = value.records[0].requestId;
  assert(requestId.length > 0, "Actual webRequest requestId is nonempty");
  const type = value.records[0].type;
  for (const record of value.records) {
    assertEquals(
      record.url,
      value.url,
      "Event refers to the exact owned request",
    );
    assertEquals(
      record.requestId,
      requestId,
      "Events retain the same actual requestId",
    );
    assertEquals(record.method, "GET", "Actual request method");
    assertEquals(record.type, type, "Events retain the actual resource type");
    assertEquals(record.error, null, "No network error was observed");
    assert(
      Number.isFinite(record.timeStamp),
      "Actual event timestamp is finite",
    );
  }
  assertEquals(value.records[2].statusCode, 200, "Response-start event status");
  assertEquals(value.records[3].statusCode, 200, "Completion event status");
  assertEquals(
    JSON.stringify(value.cleanupErrors),
    "[]",
    "Every actual webRequest listener is removed without error",
  );
  assertEquals(
    value.invalidAdds.length,
    2,
    "Both required-filter invalid add cases execute",
  );
  assertEquals(
    JSON.stringify(value.invalidAdds.map((item) => item.kind)),
    JSON.stringify(["missing-filter", "non-array-urls"]),
    "The invalid add cases preserve required-filter and type validation",
  );
  for (const item of value.invalidAdds) {
    assert(
      typeof item.error === "string" && item.error.length > 0,
      "Invalid addListener still rejects: " + item.kind,
    );
    assertEquals(
      item.registered,
      false,
      "Invalid addListener does not register its callback",
    );
  }
  const eventNames = [
    "onBeforeRequest",
    "onBeforeSendHeaders",
    "onResponseStarted",
    "onCompleted",
    "onErrorOccurred",
  ];
  assertEquals(
    JSON.stringify(value.registrations.map((item) => item.name)),
    JSON.stringify(eventNames),
    "Every real event uses its valid required filter",
  );
  assertEquals(
    JSON.stringify(value.removals.map((item) => item.name)),
    JSON.stringify(eventNames),
    "Every registered event attempts its public zero-option removal",
  );
  for (const item of value.registrations) {
    assertEquals(
      item.registered,
      true,
      "The valid public listener was registered: " + item.name,
    );
  }
  for (const item of value.removals) {
    assertEquals(
      item.before,
      true,
      "Public hasListener observes the listener before removal: " + item.name,
    );
    assertEquals(
      item.after,
      false,
      "Public hasListener observes the listener removed: " + item.name,
    );
  }
}
async function exercise(): Promise<void> {
  const { AppConstants } = ChromeUtils.importESModule(
    "resource://gre/modules/AppConstants.sys.mjs",
  );
  assert(
    AppConstants.MOZ_WEBEXT_WEBIDL_ENABLED,
    "Actual native Worker bindings are compiled",
  );
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  ) as unknown as { AddonManager: MessagingCandidateAddonManager };
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  ) as unknown as { ExtensionParent: MessagingCandidateParent };
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  const window = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as unknown as OwnedWebRequestWindow;
  const fixtureURL = window.wrappedJSObject.__floorpRuntimeFixtureURL;
  assert(
    typeof fixtureURL === "string",
    "Unchanged browser runner provides its real loopback HTTP fixture",
  );
  const expectedBody = ownedWebRequestExpectedBody;
  const report: OwnedWebRequestReport = {
    passed: false,
    runId: Services.env.get("FLOORP_TEST_RUN_ID"),
    fixtureURL,
    cases: [],
  };
  const reportPath = PathUtils.join(
    Services.dirsvc.get("ProfD", Ci.nsIFile).path,
    "native-webrequest-process-batch.json",
  );
  const save = () => IOUtils.writeJSON(reportPath, report);
  for (
    const kind of [
      "ordinary-document",
      "classic",
      "module",
    ] as const satisfies readonly OwnedWebRequestKind[]
  ) {
    const original = kind === "classic"
      ? "acacacacacacacacacacacacacacacac"
      : "bdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbd";
    const worker = kind !== "ordinary-document";
    const id = worker
      ? original + "@chromium-extension"
      : "ordinary-webrequest-process-control@example.test";
    const item: OwnedWebRequestCase = {
      kind,
      addonId: id,
      expectedRuntimeId: worker ? original : id,
      passed: false,
      cleanupErrors: [],
    };
    report.cases.push(item);
    let addon: MessagingCandidateAddon | undefined;
    let tab: MessagingCandidateTab | undefined;
    const packagePath = PathUtils.join(
      PathUtils.tempDir,
      "owned-webrequest-" + Services.uuid.generateUUID() + ".xpi",
    );
    try {
      const token = Services.uuid.generateUUID().toString();
      const url = fixtureURL + "corpus-content.html?ownedWebRequest=" +
        encodeURIComponent(token);
      const config = {
        kind,
        token,
        url,
        origin: new URL(fixtureURL).origin,
        resultKey: "owned-webrequest-result",
        gateKey: "owned-webrequest-gate",
      };
      const manifest = {
        manifest_version: worker ? 3 : 2,
        name: "Owned actual HTTP webRequest " + kind,
        version: "1",
        permissions: worker
          ? ["storage", "webRequest"]
          : ["storage", "webRequest", "http://127.0.0.1/*"],
        ...(worker
          ? {
            host_permissions: ["http://127.0.0.1/*"],
            background: {
              service_worker: "worker.js",
              ...(kind === "module" ? { type: "module" } : {}),
            },
          }
          : {}),
        browser_specific_settings: {
          gecko: {
            id,
            ...(worker
              ? {
                floorp_chrome_extension: {
                  schema_version: 1,
                  original_id: original,
                },
              }
              : {}),
          },
        },
      };
      const configSource = "const ownedWebRequestConfig=" +
        JSON.stringify(config) + ";\n";
      const marker = "globalThis.ownedWebRequestImportWitness=" +
        JSON.stringify(kind) + ";\n";
      const files = {
        "manifest.json": JSON.stringify(manifest),
        "control.html":
          '<!doctype html><meta charset="utf-8"><script src="control.js"></script>',
        "control.js": configSource + ownedWebRequestControllerSource +
          (worker ? "" : marker + ownedWebRequestProbeSource +
            "runOwnedWebRequestProbe(ownedWebRequestConfig);"),
        "worker.js": (kind === "module"
          ? 'import "./import-marker.js";\n'
          : 'importScripts("import-marker.js");\n') +
          configSource + ownedWebRequestProbeSource +
          "runOwnedWebRequestProbe(ownedWebRequestConfig);",
        "import-marker.js": marker,
      };
      await IOUtils.write(
        packagePath,
        zipSync(
          Object.fromEntries(
            Object.entries(files).map((
              [name, source],
            ) => [name, new TextEncoder().encode(source)]),
          ),
        ),
      );
      addon = await AddonManager.installTemporaryAddon(
        new FileUtils.File(packagePath),
      );
      const extension = await until(
        () =>
          ExtensionParent.GlobalManager.extensionMap.get(id),
        "owned " + kind + " extension",
      );
      tab = window.gBrowser.addTab(extension.baseURI.resolve("control.html"), {
        triggeringPrincipal: extension.principal,
      });
      const ownedTab = tab;
      const ready = await until(() => {
        const value = titleObservation(ownedTab);
        return value?.phase === "ready" || value?.phase === "complete" ||
            value?.phase === "controller-error"
          ? value
          : false;
      }, kind + " public storage ready witness");
      if (worker) {
        item.backgroundViewType = extension.backgroundContext?.viewType;
      }
      if (ready.phase !== "ready") {
        item.observation = ready;
        throw Error(
          "Probe did not reach its pre-request ready state: " +
            JSON.stringify(ready),
        );
      }
      ownedTab.linkedBrowser.messageManager.loadFrameScript(
        "data:application/javascript," +
          encodeURIComponent(
            "content.postMessage(" +
              JSON.stringify({ target: "owned-webrequest-release", token }) +
              ",'*');",
          ),
        false,
      );
      item.observation = await until(() => {
        const value = titleObservation(ownedTab);
        return value?.phase === "complete" ||
            value?.phase === "controller-error"
          ? value
          : false;
      }, kind + " actual HTTP and event observation");
      assertObservation(item, expectedBody);
      item.passed = true;
    } catch (error) {
      item.error = String(error);
      if (!item.observation && tab) item.observation = titleObservation(tab);
    } finally {
      try {
        if (tab?.isConnected) window.gBrowser.removeTab(tab);
      } catch (error) {
        item.cleanupErrors.push(String(error));
      }
      try {
        if (addon) await addon.uninstall();
      } catch (error) {
        item.cleanupErrors.push(String(error));
      }
      try {
        await IOUtils.remove(packagePath, { ignoreAbsent: true });
      } catch (error) {
        item.cleanupErrors.push(String(error));
      }
      if (item.cleanupErrors.length) item.passed = false;
      await save();
    }
  }
  report.passed = report.cases.every((item) => item.passed);
  if (!report.passed) {
    report.error = report.cases.filter((item) => !item.passed).map((item) =>
      item.kind + ": " + (item.error ?? item.cleanupErrors.join("; "))
    ).join("; ");
  }
  await save();
  console.info("NATIVE_WEBREQUEST_PROCESS_BATCH_REPORT " + reportPath);
  assert(
    report.passed,
    report.error ?? "Every real HTTP webRequest case passed",
  );
}
export function runAllTests(): Promise<void> {
  return runTests("NativeWebRequestBatch.test.mts", [{
    name:
      "real HTTP events in ordinary Firefox document and native classic/module Workers",
    fn: exercise,
  }]);
}
