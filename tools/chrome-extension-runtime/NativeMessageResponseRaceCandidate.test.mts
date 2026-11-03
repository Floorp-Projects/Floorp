// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { zipSync } from "fflate/browser";
import {
  assert,
  runTests,
} from "../../browser-features/chrome/test/utils/test_harness.ts";
import {
  record,
  replaceStrings,
  same,
} from "./native-message-json-assertions.mts";
import {
  responseRaceProbeSource,
  responseRaceReferenceSource,
} from "./native-response-race-sources.mjs";
import {
  messageControllerSource,
  messageDocumentControllerSource,
  messageWorkerControllerSource,
} from "./native-response-race-controller.mjs";
import type {
  MessagingCandidateAddonManager,
  MessagingCandidateParent,
} from "./native-messaging-candidate-types.ts";
import type {
  MessageControlReply,
  MessageGroup,
  MessageJSONBrowser,
  MessageJSONConfig,
  MessageJSONFrameMessage,
  MessageJSONTab,
  MessageJSONWindow,
  MessageRealm,
  MessageRecord,
  MessageResource,
} from "./native-message-json-test-types.ts";
import type {
  ResponseRaceEvidence,
  ResponseRaceReference,
} from "./native-response-race-test-types.ts";
const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
const pause = (ms: number) => new Promise((resolve) => setTimeout(resolve, ms));
async function until<Value>(
  read: () => Value | false | undefined | Promise<Value | false | undefined>,
  label: string,
): Promise<Value> {
  const end = Date.now() + 20000;
  while (Date.now() < end) {
    const value = await read();
    if (value) return value;
    await pause(20);
  }
  throw Error("Timed out waiting for " + label);
}
async function documentIsVisible(
  browser: MessageJSONBrowser,
): Promise<boolean> {
  const channel = "floorp-owned-message-visibility-" +
    Services.uuid.generateUUID();
  return await new Promise((resolve) => {
    let active = true;
    const finish = (visible: boolean) => {
      if (!active) return;
      active = false;
      clearTimeout(timer);
      browser.messageManager.removeMessageListener(channel, listener);
      resolve(visible);
    };
    const listener = (message: MessageJSONFrameMessage) =>
      finish(message.data === "visible");
    const timer = setTimeout(() => finish(false), 1000);
    browser.messageManager.addMessageListener(channel, listener);
    browser.messageManager.loadFrameScript(
      "data:application/javascript," + encodeURIComponent(
        "sendAsyncMessage(" + JSON.stringify(channel) +
          ",content.document.visibilityState);",
      ),
      false,
    );
  });
}
async function activateOwnedControlTab(
  window: MessageJSONWindow,
  tab: MessageJSONTab,
): Promise<void> {
  window.gBrowser.selectedTab = tab;
  await until(
    async () =>
      window.gBrowser.selectedTab === tab &&
      tab.linkedBrowser.docShellIsActive &&
      await documentIsVisible(tab.linkedBrowser),
    "active visible owned control document",
  );
}
const raceReference = JSON.parse(
  responseRaceReferenceSource,
) as ResponseRaceReference;
const opposite = (realm: MessageRealm): MessageRealm =>
  realm === "document" ? "worker" : "document";
function compare(
  group: MessageGroup,
  label: string,
  actual: MessageRecord,
  expected: unknown,
): void {
  group.observations.push({ label, ...actual });
  try {
    same(actual, expected, label);
  } catch (error) {
    group.failures.push(String(error));
  }
}
async function exercise(): Promise<void> {
  const configPath = Services.env.get("FLOORP_RESPONSE_RACE_CANDIDATE_CONFIG");
  assert(configPath, "Owned native message JSON configuration exists");
  const config = await IOUtils.readJSON(configPath) as MessageJSONConfig;
  const { AppConstants } = ChromeUtils.importESModule(
    "resource://gre/modules/AppConstants.sys.mjs",
  );
  assert(
    AppConstants.MOZ_WEBEXT_WEBIDL_ENABLED,
    "Native Worker bindings are compiled",
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
  ) as unknown as MessageJSONWindow;
  const evidence: ResponseRaceEvidence = {
    stage: "authored-actual-native-response-race",
    token: config.token,
    runId: Services.env.get("FLOORP_TEST_RUN_ID"),
    passed: false,
    groups: [],
    cleanupErrors: [],
  };
  const resources: MessageResource[] = [];
  let failure: Error | undefined;
  const group = (name: string, expectedCount: number): MessageGroup => {
    const value = { name, expectedCount, observations: [], failures: [] };
    evidence.groups.push(value);
    return value;
  };
  const attempt = async (
    target: MessageGroup,
    label: string,
    work: () => Promise<void>,
  ) => {
    try {
      await work();
    } catch (error) {
      target.failures.push(label + ": " + String(error));
      target.observations.push({ label, commandFailure: String(error) });
    }
  };
  try {
    for (const scriptType of ["classic", "module"] as const) {
      for (const kind of ["response-race"] as const) {
        const originalId = "bbbbaaaaddddcccceeeeffffgggghhhh";
        const id = originalId + "@chromium-extension";
        const publicSource = responseRaceProbeSource;
        const manifest = {
          manifest_version: 3,
          name: "Owned Chrome JSON " + kind + " " + scriptType,
          version: "1",
          background: {
            service_worker: "worker.js",
            ...(scriptType === "module" ? { type: "module" } : {}),
          },
          browser_specific_settings: {
            gecko: {
              id,
              floorp_chrome_extension: {
                schema_version: 1,
                original_id: originalId,
              },
            },
          },
        };
        const files = {
          "manifest.json": JSON.stringify(manifest),
          "control.html":
            '<!doctype html><meta charset="utf-8"><script src="control.js"></script>',
          "control.js": publicSource + "\n" + messageControllerSource +
            messageDocumentControllerSource,
          "worker.js": (scriptType === "module"
            ? "export const ownedModuleProof=import.meta.url;\n"
            : "") +
            publicSource + "\n" + messageControllerSource +
            messageWorkerControllerSource,
        };
        const packagePath = PathUtils.join(
          PathUtils.tempDir,
          "native-message-json-" + Services.uuid.generateUUID() + ".xpi",
        );
        const resource: MessageResource = { packagePath, active: true };
        resources.push(resource);
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
        const addon = await AddonManager.installTemporaryAddon(
          new FileUtils.File(packagePath),
        );
        resource.addon = addon;
        const extension = await until(
          () =>
            ExtensionParent.GlobalManager.extensionMap.get(id),
          "owned " + kind + " extension",
        );
        await until(
          () =>
            extension.backgroundState === "running" &&
            extension.backgroundContext?.viewType === "background_worker",
          "genuine native " + scriptType + " Worker",
        );
        const tab = window.gBrowser.addTab(
          extension.baseURI.resolve("control.html"),
          { triggeringPrincipal: extension.principal },
        );
        resource.tab = tab;
        const browser = tab.linkedBrowser;
        await activateOwnedControlTab(window, tab);
        const ready = await until(() => {
          try {
            const value = record(JSON.parse(browser.contentTitle));
            return value.ready === true ? value : false;
          } catch {
            return false;
          }
        }, "native Worker control handshake");
        const worker = record(ready.worker);
        same(worker.realm, "worker", "Control is an actual Worker");
        same(worker.runtimeId, originalId, "Control public identity");
        same(
          worker.moduleProof,
          scriptType === "module",
          "Module fixture requires module-only syntax",
        );
        const request = async (
          mode: MessageRealm,
          operation: string,
          args: unknown[] = [],
        ): Promise<unknown> => {
          const requestId = Services.uuid.generateUUID().toString();
          const command = {
            target: "floorp-owned-message-json",
            requestId,
            mode,
            operation,
            args,
          };
          browser.messageManager.loadFrameScript(
            "data:application/javascript," +
              encodeURIComponent(
                "content.postMessage(" + JSON.stringify(command) + ",'*');",
              ),
            false,
          );
          const response = await until(() => {
            try {
              const value = JSON.parse(
                browser.contentTitle,
              ) as MessageControlReply;
              return value.requestId === requestId ? value : false;
            } catch {
              return false;
            }
          }, operation + " in " + mode);
          assert(
            !response.error,
            "Owned public command completed: " + response.error,
          );
          return response.value;
        };
        const races = group(scriptType + "/response-races", 12);
        assert(
          raceReference.completed && raceReference.rows.length === 12,
          "Exact Chrome twelve-row race reference",
        );
        for (const expected of raceReference.rows) {
          const expectedSender = record(expected.sender);
          const realm = String(expectedSender.realm) as MessageRealm;
          const token = "owned-response-race-" + Services.uuid.generateUUID();
          await attempt(races, realm + "/" + expected.kind, async () => {
            const sender = record(
              await request(realm, "race-run", [token, expected.kind]),
            );
            const receiver = record(
              await request(opposite(realm), "race-read", [token]),
            );
            same(sender.realm, realm, "Actual sender realm");
            same(
              receiver.realm,
              opposite(realm),
              "Actual opposite receiver realm",
            );
            same(sender.runtimeId, originalId, "Sender public identity");
            same(receiver.runtimeId, originalId, "Receiver public identity");
            same(
              receiver.senderId,
              originalId,
              "Received public sender identity",
            );
            compare(
              races,
              realm + "/" + expected.kind,
              { kind: expected.kind, sender, receiver },
              replaceStrings(
                expected,
                new Map([[raceReference.extensionId, originalId], [
                  String(expectedSender.id),
                  token,
                ]]),
              ),
            );
          });
        }
        if (tab.isConnected) window.gBrowser.removeTab(tab);
        await addon.uninstall();
        resource.addon = undefined;
        await IOUtils.remove(packagePath, { ignoreAbsent: true });
        resource.active = false;
      }
    }
    for (const item of evidence.groups) {
      if (item.observations.length !== item.expectedCount) {
        item.failures.push(
          "Expected " + item.expectedCount + " actual rows, received " +
            item.observations.length,
        );
      }
    }
    evidence.passed = evidence.groups.length === 2 &&
      evidence.groups.every((item) => item.failures.length === 0);
    if (!evidence.passed) {
      failure = Error(
        evidence.groups.flatMap((item) =>
          item.failures.map((text) => item.name + ": " + text)
        ).join("\n"),
      );
    }
  } catch (error) {
    evidence.error = String(error);
    failure = error instanceof Error ? error : Error(String(error));
  } finally {
    for (const resource of resources.reverse()) {
      if (!resource.active) continue;
      try {
        if (resource.tab?.isConnected) window.gBrowser.removeTab(resource.tab);
      } catch (error) {
        evidence.cleanupErrors.push(String(error));
      }
      try {
        if (resource.addon) await resource.addon.uninstall();
      } catch (error) {
        evidence.cleanupErrors.push(String(error));
      }
      try {
        await IOUtils.remove(resource.packagePath, { ignoreAbsent: true });
      } catch (error) {
        evidence.cleanupErrors.push(String(error));
      }
    }
    if (evidence.cleanupErrors.length) {
      evidence.passed = false;
      failure ??= Error(evidence.cleanupErrors.join("; "));
    }
    await IOUtils.writeJSON(config.outputPath, evidence);
    console.info("NATIVE_MESSAGE_JSON_REPORT " + config.outputPath);
  }
  if (failure) throw failure;
}
export function runAllTests(): Promise<void> {
  return runTests("NativeMessageResponseRaceCandidate.test.mts", [{
    name:
      "exact Chrome response ownership races in genuine classic/module Workers and documents",
    fn: exercise,
  }]);
}
