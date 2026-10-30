// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { zipSync } from "fflate/browser";
import {
  assert,
  runTests,
} from "../../browser-features/chrome/test/utils/test_harness.ts";
import type {
  MessagingCandidateAddon,
  MessagingCandidateAddonManager,
  MessagingCandidateManifest,
  MessagingCandidateParent,
  MessagingCandidateTab,
  MessagingCandidateWindow,
} from "./native-messaging-candidate-types.ts";
import {
  quietNativeDocumentProbeSource,
  quietNativeWorkerProbeSource,
} from "./quiet-native-probe-sources.mjs";
import type {
  QuietDocumentResponse,
  QuietDocumentState,
  QuietLifetimeCase,
  QuietLifetimeConfig,
  QuietLifetimeEvidence,
  QuietProbeResult,
} from "./quiet-native-lifetime-types.ts";
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
const pause = (milliseconds: number) =>
  new Promise((resolve) => setTimeout(resolve, milliseconds));
async function until<Value>(
  read: () => Value | false | undefined | Promise<Value | false | undefined>,
  label: string,
): Promise<Value> {
  const end = Date.now() + 15000;
  while (Date.now() < end) {
    const value = await read();
    if (value) return value;
    await pause(25);
  }
  throw Error("Timed out waiting for " + label);
}
async function exercise(): Promise<void> {
  const config = await IOUtils.readJSON(
    Services.env.get("FLOORP_QUIET_NATIVE_LIFETIME_CONFIG"),
  ) as QuietLifetimeConfig;
  assert(
    config.quietMilliseconds === 36000,
    "Exact Chrome reference quiet interval",
  );
  assert(
    Services.appinfo.appBuildID === config.expectedBuildId,
    "Exact root-authorized native build",
  );
  const { AppConstants } = ChromeUtils.importESModule(
    "resource://gre/modules/AppConstants.sys.mjs",
  );
  assert(
    AppConstants.MOZ_WEBEXT_WEBIDL_ENABLED,
    "Genuine native Worker bindings compiled",
  );
  assert(
    WebExtensionPolicy.backgroundServiceWorkerEnabled === false,
    "Ordinary Firefox Worker policy unchanged",
  );
  const actualPlatform = AppConstants.platform === "win"
    ? "windows"
    : AppConstants.platform === "macosx"
    ? "mac"
    : AppConstants.platform;
  assert(actualPlatform === config.platform, "Actual host registration OS");
  const hostManifest = await IOUtils.readJSON(
    config.manifestPath,
  ) as MessagingCandidateManifest;
  assert(
    hostManifest.name === config.hostName &&
      hostManifest.path === config.hostPath,
    "Owned host registration paths",
  );
  assert(
    JSON.stringify(hostManifest.allowed_origins) ===
      JSON.stringify(["chrome-extension://abcdefghijklmnopabcdefghijklmnop/"]),
    "Only genuine Chrome identity allowed",
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
  const evidence: QuietLifetimeEvidence = {
    stage: "authored-real-native-port-quiet-lifetime",
    token: config.token,
    runId: Services.env.get("FLOORP_TEST_RUN_ID"),
    passed: false,
    cases: [],
    cleanupErrors: [],
  };
  const traffic = async () =>
    await IOUtils.exists(config.trafficPath)
      ? await IOUtils.readUTF8(config.trafficPath)
      : "";
  let failure: Error | undefined;
  try {
    for (const scriptType of ["classic", "module"] as const) {
      for (const native of [true, false]) {
        const item: QuietLifetimeCase = {
          scriptType,
          native,
          quietStartedAt: 0,
          passed: false,
        };
        evidence.cases.push(item);
        let addon: MessagingCandidateAddon | undefined,
          tab: MessagingCandidateTab | undefined;
        const packagePath = PathUtils.join(
          PathUtils.tempDir,
          "quiet-native-lifetime-" + Services.uuid.generateUUID() + ".xpi",
        );
        try {
          const id = "abcdefghijklmnopabcdefghijklmnop@chromium-extension";
          const manifest = {
            manifest_version: 3,
            name: "Owned quiet native lifetime " + scriptType + " " + native,
            version: "1",
            permissions: ["nativeMessaging"],
            background: {
              service_worker: "worker.js",
              ...(scriptType === "module" ? { type: "module" } : {}),
            },
            browser_specific_settings: {
              gecko: {
                id,
                floorp_chrome_extension: {
                  schema_version: 1,
                  original_id: "abcdefghijklmnopabcdefghijklmnop",
                },
              },
            },
          };
          const controller =
            `window.addEventListener('message',async event=>{const request=event.data;if(request?.target!=='owned-quiet-native-lifetime'||request.token!==${
              JSON.stringify(config.token)
            })return;try{const value=await ownedQuietNativeProbe[request.operation](request.options);document.title=JSON.stringify({requestId:request.requestId,value});}catch(error){document.title=JSON.stringify({requestId:request.requestId,error:String(error)});}});document.title='owned-quiet-native-ready';`;
          const files = {
            "manifest.json": JSON.stringify(manifest),
            "control.html":
              '<!doctype html><meta charset="utf-8"><script src="control.js"></script>',
            "control.js": quietNativeDocumentProbeSource + "\n" + controller,
            "worker.js": (scriptType === "module"
              ? "export const ownedQuietModuleProof=import.meta.url;\n"
              : "") + quietNativeWorkerProbeSource,
          };
          await IOUtils.write(
            packagePath,
            zipSync(
              Object.fromEntries(
                Object.entries(files).map((
                  [name, value],
                ) => [name, new TextEncoder().encode(value)]),
              ),
            ),
          );
          addon = await AddonManager.installTemporaryAddon(
            new FileUtils.File(packagePath),
          );
          const extension = await until(
            () =>
              ExtensionParent.GlobalManager.extensionMap.get(id),
            "owned extension",
          );
          await until(
            () =>
              extension.backgroundState === "running" &&
              extension.backgroundContext?.viewType === "background_worker",
            "actual native " + scriptType + " Worker",
          );
          tab = window.gBrowser.addTab(
            extension.baseURI.resolve("control.html"),
            { triggeringPrincipal: extension.principal },
          );
          await until(
            () =>
              tab?.linkedBrowser.contentTitle === "owned-quiet-native-ready",
            "owned extension document",
          );
          const request = async (
            operation: string,
            options?: object,
          ): Promise<QuietDocumentState | QuietProbeResult> => {
            const requestId = Services.uuid.generateUUID().toString();
            tab?.linkedBrowser.messageManager.loadFrameScript(
              "data:application/javascript," +
                encodeURIComponent(
                  "content.postMessage(" +
                    JSON.stringify({
                      target: "owned-quiet-native-lifetime",
                      token: config.token,
                      requestId,
                      operation,
                      options,
                    }) + ",'*');",
                ),
              false,
            );
            const result = await until(() => {
              try {
                const value = JSON.parse(
                  tab?.linkedBrowser.contentTitle ?? "",
                ) as QuietDocumentResponse;
                return value.requestId === requestId ? value : false;
              } catch {
                return false;
              }
            }, operation + " public probe");
            assert(!result.error, "Public probe completed: " + result.error);
            assert(result.value, "Public probe returned actual value");
            return result.value;
          };
          item.start = await request(
            "start",
            native ? { nativeHost: config.hostName } : {},
          ) as QuietDocumentState;
          assert(
            item.start.controlReady?.workerGeneration,
            "Actual Worker generation established",
          );
          assert(
            !item.start.controlDisconnected,
            "Control connected before quiet window",
          );
          if (native) {
            assert(
              item.start.initial?.host?.serial === 1,
              "Initial actual framed native response",
            );
            await until(async () => {
              const events = (await traffic()).trim().split("\n").filter(
                Boolean,
              ).map((line) =>
                JSON.parse(line) as {
                  event: string;
                  pid: number;
                  serial?: number;
                }
              );
              return events.some((event) =>
                event.event === "send" &&
                event.pid === item.start?.initial?.host?.pid &&
                event.serial === 1
              );
            }, "initial real host send log");
          }
          const before = await traffic();
          item.hostTrafficBytesBefore =
            new TextEncoder().encode(before).byteLength;
          item.quietStartedAt = Date.now();
          await pause(config.quietMilliseconds + 250);
          item.quietFinishedAt = Date.now();
          item.passiveAfterQuiet = await request(
            "snapshot",
          ) as QuietDocumentState;
          const after = await traffic();
          item.hostTrafficBytesAfter =
            new TextEncoder().encode(after).byteLength;
          assert(
            item.quietFinishedAt - item.quietStartedAt >=
              config.quietMilliseconds,
            "Full passive quiet window elapsed",
          );
          assert(
            item.passiveAfterQuiet.documentTime -
                item.passiveAfterQuiet.quietStartDocumentTime >=
              config.quietMilliseconds,
            "Public document clock confirms full quiet window",
          );
          assert(
            before === after,
            "No native host traffic or exit during quiet window",
          );
          if (native) {
            assert(
              !item.passiveAfterQuiet.controlDisconnected,
              "Native host keeps original Worker alive while quiet",
            );
            assert(
              extension.backgroundState === "running" &&
                extension.backgroundContext?.viewType === "background_worker",
              "Original genuine Worker remains running before probe",
            );
            item.probe = await request("probe") as QuietProbeResult;
            assert(
              !item.probe.response.error &&
                !item.probe.response.nativePortMissing,
              "Original native Port still usable",
            );
            assert(
              item.probe.response.workerGeneration ===
                item.start.controlReady?.workerGeneration,
              "Same Worker generation across quiet interval",
            );
            assert(
              item.probe.response.host?.pid === item.start.initial?.host?.pid &&
                item.probe.response.host?.serial === 2,
              "Same real host process and second framed response",
            );
            assert(
              item.probe.response.host?.message.command === "probe",
              "Actual host echoes probe frame",
            );
          } else {
            assert(
              item.passiveAfterQuiet.controlDisconnected,
              "Ordinary quiet Port does not keep Worker alive",
            );
            assert(
              extension.backgroundState === "stopped" &&
                !extension.backgroundContext,
              "Actual quiet Worker stopped without wake request",
            );
          }
          await request("stop");
          item.passed = true;
        } catch (error) {
          item.error = String(error);
          failure ??= error instanceof Error ? error : Error(String(error));
        } finally {
          try {
            if (tab?.isConnected) window.gBrowser.removeTab(tab);
          } catch (error) {
            evidence.cleanupErrors.push(String(error));
          }
          try {
            await addon?.uninstall();
            if (native && item.start?.initial?.host?.pid) {
              const pid = item.start.initial.host.pid;
              await until(
                async () =>
                  (await traffic()).trim().split("\n").filter(Boolean).map(
                    (line) =>
                      JSON.parse(line) as {
                        event: string;
                        pid: number;
                        clean?: boolean;
                      },
                  ).some((event) =>
                    event.event === "exit" && event.pid === pid &&
                    event.clean === true
                  ),
                "owned native host clean EOF",
              );
            }
          } catch (error) {
            evidence.cleanupErrors.push(String(error));
          }
          try {
            await IOUtils.remove(packagePath, { ignoreAbsent: true });
          } catch (error) {
            evidence.cleanupErrors.push(String(error));
          }
        }
      }
    }
    evidence.passed = evidence.cases.length === 4 &&
      evidence.cases.every((item) => item.passed) &&
      evidence.cleanupErrors.length === 0;
  } catch (error) {
    evidence.error = String(error);
    failure ??= error instanceof Error ? error : Error(String(error));
  } finally {
    await IOUtils.writeJSON(config.outputPath, evidence);
  }
  if (failure) throw failure;
  assert(
    evidence.passed,
    "All actual quiet lifetime cases and cleanup completed",
  );
}
export function runAllTests(): Promise<void> {
  return runTests("QuietNativePortLifetime", [{
    name:
      "Chrome quiet native host keeps Worker alive; quiet ordinary Port permits automatic idle",
    fn: exercise,
  }]);
}
