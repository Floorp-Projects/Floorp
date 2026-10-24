// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { zipSync } from "fflate/browser";
import {
  assert,
  assertEquals,
  runTests,
} from "../../browser-features/chrome/test/utils/test_harness.ts";
import { nativePortErrorsProbeSource } from "./native-port-errors-probe.mjs";
import { assertNativePortErrors } from "./native-port-errors-assertions.mts";
import type {
  MessagingCandidateAddon,
  MessagingCandidateAddonManager,
  MessagingCandidateParent,
  MessagingCandidateTab,
  MessagingCandidateWindow,
} from "./native-messaging-candidate-types.ts";
import type {
  NativePortErrorResponse,
  NativePortErrorsReport,
} from "./native-port-errors-test-types.ts";
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
  ) as unknown as MessagingCandidateWindow;
  const report: NativePortErrorsReport = {
    passed: false,
    runId: Services.env.get("FLOORP_TEST_RUN_ID"),
    cases: [],
  };
  const reportPath = PathUtils.join(
    Services.dirsvc.get("ProfD", Ci.nsIFile).path,
    "native-port-errors.json",
  );
  let failure: Error | undefined;
  const addons: MessagingCandidateAddon[] = [],
    tabs: MessagingCandidateTab[] = [],
    packages: string[] = [];
  try {
    for (const caller of ["converted", "ordinary"] as const) {
      const original = "aaaabbbbccccddddeeeeffffgggghhhh",
        converted = caller === "converted";
      const id = converted
        ? original + "@chromium-extension"
        : "ordinary-native-port-errors@example.test";
      const manifest = {
        manifest_version: 3,
        name: "Owned actual Port error " + caller,
        version: "1",
        permissions: ["nativeMessaging"],
        ...(converted ? { background: { service_worker: "worker.js" } } : {}),
        browser_specific_settings: {
          gecko: {
            id,
            ...(converted
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
      const files = {
        "manifest.json": JSON.stringify(manifest),
        "control.html":
          '<!doctype html><meta charset="utf-8"><script src="control.js"></script>',
        "worker.js": nativePortErrorsProbeSource +
          `chrome.runtime.onMessage.addListener(request=>request.target==='owned-native-port-errors'?probeNativePortErrors(request):undefined);`,
        "control.js": nativePortErrorsProbeSource +
          `window.addEventListener('message',async event=>{if(event.data?.target!=='owned-native-port-errors')return;const requestId=event.data.requestId;try{const result=event.data.mode==='worker'?await chrome.runtime.sendMessage(event.data):await probeNativePortErrors(event.data);document.title=JSON.stringify({requestId,result});}catch(error){document.title=JSON.stringify({requestId,error:String(error)});}});document.title='owned-native-port-errors-ready';`,
      };
      const packagePath = PathUtils.join(
        PathUtils.tempDir,
        "native-port-errors-" + Services.uuid.generateUUID() + ".xpi",
      );
      packages.push(packagePath);
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
      addons.push(
        await AddonManager.installTemporaryAddon(
          new FileUtils.File(packagePath),
        ),
      );
      const extension = await until(
        () => ExtensionParent.GlobalManager.extensionMap.get(id),
        "owned actual extension",
      );
      if (converted) {
        await until(
          () =>
            extension.backgroundState === "running" &&
            extension.backgroundContext?.viewType === "background_worker",
          "actual native Worker",
        );
      }
      const tab = window.gBrowser.addTab(
        extension.baseURI.resolve("control.html"),
        { triggeringPrincipal: extension.principal },
      );
      tabs.push(tab);
      await until(
        () =>
          tab.linkedBrowser.contentTitle === "owned-native-port-errors-ready",
        "owned extension document",
      );
      for (
        const mode of converted
          ? ["document", "worker"] as const
          : ["document"] as const
      ) {
        for (const normal of converted ? [true, false] : [false]) {
          for (const throwFirst of [false, true]) {
            const requestId = Services.uuid.generateUUID().toString();
            const request = {
              target: "owned-native-port-errors",
              requestId,
              mode,
              normal,
              throwFirst,
              token: report.runId,
              hostName: "floorp_owned_missing_" + report.runId,
            };
            tab.linkedBrowser.messageManager.loadFrameScript(
              "data:application/javascript," +
                encodeURIComponent(
                  "content.postMessage(" + JSON.stringify(request) + ",'*');",
                ),
              false,
            );
            const response = await until(() => {
              try {
                const value = JSON.parse(
                  tab.linkedBrowser.contentTitle,
                ) as NativePortErrorResponse;
                return value.requestId === requestId ? value : false;
              } catch {
                return false;
              }
            }, caller + " " + mode + " public port callback scopes");
            assert(
              !response.error,
              "Actual public Port probe completed: " + response.error,
            );
            assert(
              response.result,
              "Actual public Port probe returned observations",
            );
            assertEquals(
              response.result.isWorker,
              mode === "worker",
              "Public Port probe ran in the requested actual context",
            );
            assertEquals(
              response.result.runtimeId,
              converted ? original : id,
              "Public Port probe uses its expected Chrome or Firefox identity",
            );
            report.cases.push({ caller, mode, result: response.result });
            assertNativePortErrors(response.result, !converted);
          }
        }
      }
    }
    report.passed = true;
  } catch (error) {
    report.error = String(error);
    failure = error instanceof Error ? error : Error(String(error));
  } finally {
    const errors: string[] = [];
    for (const tab of tabs.reverse()) {
      try {
        if (tab.isConnected) window.gBrowser.removeTab(tab);
      } catch (error) {
        errors.push(String(error));
      }
    }
    for (const addon of addons.reverse()) {
      try {
        await addon.uninstall();
      } catch (error) {
        errors.push(String(error));
      }
    }
    for (const path of packages) {
      try {
        await IOUtils.remove(path, { ignoreAbsent: true });
      } catch (error) {
        errors.push(String(error));
      }
    }
    if (errors.length) {
      report.passed = false;
      report.error = errors.join("; ");
      failure ??= Error(report.error);
    }
    await IOUtils.writeJSON(reportPath, report);
    console.info("NATIVE_PORT_ERRORS_REPORT " + reportPath);
  }
  if (failure) throw failure;
}
export function runAllTests(): Promise<void> {
  return runTests("NativePortErrorsCandidate.test.mts", [{
    name:
      "actual Chrome and Firefox Port error scopes across document and Worker callbacks",
    fn: exercise,
  }]);
}
