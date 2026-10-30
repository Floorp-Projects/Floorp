// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { zipSync } from "fflate/browser";
import {
  assert,
  runTests,
} from "../../browser-features/chrome/test/utils/test_harness.ts";
import {
  decodeDescription,
  record,
  records,
  replaceStrings,
  same,
} from "./native-message-json-assertions.mts";
import {
  messageEdgeProbeSource,
  messageEdgeReferenceSource,
  messageJSONProbeSource,
  messageJSONReferenceSource,
  messageOverloadProbeSource,
  messageOverloadReferenceSource,
} from "./native-message-json-sources.mjs";
import {
  messageControllerSource,
  messageDocumentControllerSource,
  messageWorkerControllerSource,
  ordinaryMessageSource,
} from "./native-message-json-controller.mjs";
import type {
  MessagingCandidateAddonManager,
  MessagingCandidateParent,
} from "./native-messaging-candidate-types.ts";
import type {
  MessageControlReply,
  MessageEdgeReference,
  MessageGroup,
  MessageJSONBrowser,
  MessageJSONConfig,
  MessageJSONEvidence,
  MessageJSONFrameMessage,
  MessageJSONReference,
  MessageJSONTab,
  MessageJSONWindow,
  MessageOverloadReference,
  MessageRealm,
  MessageRecord,
  MessageResource,
} from "./native-message-json-test-types.ts";
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
const jsonReference = JSON.parse(
  messageJSONReferenceSource,
) as MessageJSONReference;
const overloadReference = JSON.parse(
  messageOverloadReferenceSource,
) as MessageOverloadReference;
const edgeReference = JSON.parse(
  messageEdgeReferenceSource,
) as MessageEdgeReference;
const jsonChromeID = String(
  (jsonReference.senderRows.find((row) =>
    row.caseId === "function" && row.api === "runtime.sendMessage" &&
    row.mode === "explicit-id"
  )?.observed as unknown[])[0],
);
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
  const configPath = Services.env.get("FLOORP_MESSAGE_JSON_CANDIDATE_CONFIG");
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
  const evidence: MessageJSONEvidence = {
    stage: "authored-actual-native-message-json-overloads",
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
      for (const kind of ["json", "overload", "edge"] as const) {
        const originalId = {
          json: "abcdefghijklmnopabcdefghijklmnop",
          overload: "aaaabbbbccccddddeeeeffffgggghhhh",
          edge: "hhhhggggffffeeeeddddccccbbbbaaaa",
        }[kind];
        const id = originalId + "@chromium-extension";
        const publicSource = {
          json: messageJSONProbeSource,
          overload: messageOverloadProbeSource,
          edge: messageEdgeProbeSource,
        }[kind];
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
        if (kind === "json") {
          const senders = group(scriptType + "/json-senders", 192),
            replies = group(scriptType + "/json-replies", 60);
          for (const expected of jsonReference.senderRows) {
            const realm = String(expected.realm) as MessageRealm,
              receiver = opposite(realm),
              caseId = String(expected.caseId),
              mode = String(expected.mode),
              port = expected.api === "runtime.Port.postMessage";
            const token = "owned-json-" + config.token + "-" +
              Services.uuid.generateUUID();
            await attempt(
              senders,
              realm + "/" + mode + "/" + caseId,
              async () => {
                if (!port) await request(receiver, "json-arm", [token]);
                const source = record(
                  await request(realm, port ? "json-port" : "json-send", [
                    token,
                    caseId,
                    port ? false : mode,
                  ]),
                );
                if (expected.outcome === "sync-throw") {
                  await pause(40);
                }
                const events = records(
                  await request(
                    receiver,
                    port ? "json-ports" : "json-messages",
                  ),
                ).filter((event) => event[port ? "name" : "id"] === token);
                const observed = port
                  ? events.flatMap((event) => {
                    assert(
                      Array.isArray(event.messages),
                      "Port observation array",
                    );
                    return event.messages.map(decodeDescription);
                  })
                  : events.map((event) => decodeDescription(event.observed));
                const identitiesValid = events.every((event) =>
                  event.realm === receiver && event.runtimeId === originalId &&
                  (port || event.senderId === originalId)
                );
                const actual = {
                  caseId: source.caseId,
                  api: source.api,
                  mode: source.mode ?? "port",
                  realm: source.realm,
                  runtimeId: source.runtimeId,
                  receiverIdentitiesValid: identitiesValid,
                  outcome: source.outcome,
                  observed,
                  error: source.error ?? null,
                  state: source.state,
                  returnedType: source.returnedType ?? null,
                  returnedThenable: source.returnedThenable ?? null,
                };
                compare(senders, realm + "/" + mode + "/" + caseId, actual, {
                  ...record(
                    replaceStrings(
                      expected,
                      new Map([[jsonChromeID, originalId]]),
                    ),
                  ),
                  runtimeId: originalId,
                  receiverIdentitiesValid: true,
                });
              },
            );
          }
          for (const expected of jsonReference.responseRows) {
            const realm = String(expected.senderRealm) as MessageRealm,
              receiver = opposite(realm),
              caseId = String(expected.caseId),
              mode = String(expected.mode),
              port = mode === "port";
            const token = "owned-json-response-" + config.token + "-" +
              Services.uuid.generateUUID();
            await attempt(
              replies,
              realm + "/" + mode + "/" + caseId,
              async () => {
                await request(
                  receiver,
                  port ? "json-arm-port-response" : "json-arm-response",
                  port ? [token, caseId] : [token, caseId, mode],
                );
                const source = record(
                  await request(
                    realm,
                    port ? "json-port" : "json-response-request",
                    port ? [token, caseId, true] : [token, caseId, mode],
                  ),
                );
                const events = records(
                  await request(
                    receiver,
                    port ? "json-ports" : "json-messages",
                  ),
                ).filter((event) => event[port ? "name" : "id"] === token);
                const event = events[0];
                const values = port
                  ? (Array.isArray(source.received)
                    ? source.received.map(decodeDescription)
                    : [])
                  : source.outcome === "resolved"
                  ? [decodeDescription(source.result)]
                  : [];
                const actual = {
                  caseId: source.caseId,
                  mode,
                  senderRealm: source.realm,
                  receiverRealm: event?.realm,
                  outcome: source.outcome,
                  values,
                  senderError: source.error ?? null,
                  receiverThrow: event?.responseThrow ?? null,
                  state: event?.payloadState,
                  runtimeId: source.runtimeId,
                  receiverRuntimeId: event?.runtimeId,
                  receiverEventCount: events.length,
                  receivedRequest: event
                    ? decodeDescription(
                      port ? (event.messages as unknown[])[0] : event.observed,
                    )
                    : null,
                  response: source.response,
                };
                compare(replies, realm + "/" + mode + "/" + caseId, actual, {
                  ...expected,
                  runtimeId: originalId,
                  receiverRuntimeId: originalId,
                  receiverEventCount: 1,
                  receivedRequest: { responseCase: caseId },
                  response: true,
                });
              },
            );
          }
        } else if (kind === "overload") {
          const present = group(scriptType + "/overload-receiver-present", 220),
            missing = group(scriptType + "/overload-no-receiver", 4);
          for (const expected of overloadReference.cases) {
            const realm = String(expected.realm) as MessageRealm,
              label = realm + "/" + expected.name + "/" + expected.mode;
            await attempt(present, label, async () => {
              const actual = record(
                await request(realm, "overload", [
                  expected.name,
                  expected.mode,
                ]),
              );
              compare(
                present,
                label,
                actual,
                replaceStrings(
                  expected,
                  new Map([[overloadReference.extensionId, originalId]]),
                ),
              );
            });
          }
          for (const realm of ["document", "worker"] as const) {
            const receiver = opposite(realm);
            try {
              await request(receiver, "overload-receiver", [false]);
              for (
                const expected of overloadReference.missingReceiverCases.filter(
                  (value) => value.realm === realm,
                )
              ) {
                await attempt(
                  missing,
                  realm + "/" + expected.mode,
                  async () => {
                    const actual = record(
                      await request(realm, "overload", [
                        expected.name,
                        expected.mode,
                      ]),
                    );
                    compare(
                      missing,
                      realm + "/" + expected.mode,
                      actual,
                      replaceStrings(
                        expected,
                        new Map([[overloadReference.extensionId, originalId]]),
                      ),
                    );
                  },
                );
              }
            } catch (error) {
              missing.failures.push(String(error));
            } finally {
              await request(receiver, "overload-receiver", [true]);
            }
          }
        } else {
          const responses = group(scriptType + "/edge-response-hooks", 28),
            options = group(scriptType + "/edge-options", 18),
            ports = group(scriptType + "/edge-port-hooks", 8);
          for (const expected of edgeReference.responses) {
            const expectedSender = record(expected.sender),
              realm = String(expectedSender.realm) as MessageRealm,
              token = "owned-edge-response-" + Services.uuid.generateUUID();
            await attempt(responses, realm + "/" + expected.kind, async () => {
              const sender = record(
                  await request(realm, "edge-run", [token, expected.kind]),
                ),
                receiver = record(
                  await request(opposite(realm), "edge-read", [token]),
                );
              compare(
                responses,
                realm + "/" + expected.kind,
                { kind: expected.kind, sender, receiver },
                replaceStrings(
                  expected,
                  new Map([[edgeReference.extensionId, originalId], [
                    String(expectedSender.id),
                    token,
                  ]]),
                ),
              );
            });
          }
          for (const expected of edgeReference.options) {
            const realm = String(expected.realm) as MessageRealm;
            await attempt(options, realm + "/" + expected.name, async () => {
              const actual = record(
                await request(realm, "edge-options", [expected.name]),
              );
              compare(
                options,
                realm + "/" + expected.name,
                actual,
                replaceStrings(
                  expected,
                  new Map([[edgeReference.extensionId, originalId]]),
                ),
              );
            });
          }
          for (const expected of edgeReference.ports) {
            const expectedSender = record(expected.sender),
              realm = String(expectedSender.realm) as MessageRealm,
              name = "owned-port-hook-" + Services.uuid.generateUUID();
            await attempt(ports, realm + "/" + expected.kind, async () => {
              const sender = record(
                  await request(realm, "edge-port", [name, expected.kind]),
                ),
                receiver = await request(opposite(realm), "edge-read-port", [
                  name,
                ]);
              compare(
                ports,
                realm + "/" + expected.kind,
                { kind: expected.kind, sender, receiver },
                replaceStrings(
                  expected,
                  new Map([[edgeReference.extensionId, originalId], [
                    String(expectedSender.name),
                    name,
                  ]]),
                ),
              );
            });
          }
        }
        if (tab.isConnected) window.gBrowser.removeTab(tab);
        await addon.uninstall();
        resource.addon = undefined;
        await IOUtils.remove(packagePath, { ignoreAbsent: true });
        resource.active = false;
      }
    }
    const ordinary = group("ordinary-firefox-structured-clone", 1),
      id = "ordinary-message-json-control@example.test";
    const packagePath = PathUtils.join(
      PathUtils.tempDir,
      "ordinary-message-json-" + Services.uuid.generateUUID() + ".xpi",
    );
    const files = {
      "manifest.json": JSON.stringify({
        manifest_version: 2,
        name: "Ordinary Firefox structured clone control",
        version: "1",
        background: { scripts: ["ordinary.js"] },
        browser_specific_settings: { gecko: { id } },
      }),
      "control.html": '<!doctype html><script src="ordinary.js"></script>',
      "ordinary.js": ordinaryMessageSource,
    };
    const ordinaryResource: MessageResource = { packagePath, active: true };
    resources.push(ordinaryResource);
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
    ordinaryResource.addon = addon;
    const extension = await until(
      () => ExtensionParent.GlobalManager.extensionMap.get(id),
      "ordinary Firefox extension",
    );
    await until(
      () =>
        extension.backgroundState === "running" &&
        !!extension.backgroundContext &&
        extension.backgroundContext.viewType !== "background_worker",
      "ordinary Firefox background document",
    );
    const tab = window.gBrowser.addTab(
      extension.baseURI.resolve("control.html"),
      { triggeringPrincipal: extension.principal },
    );
    ordinaryResource.tab = tab;
    await activateOwnedControlTab(window, tab);
    await until(
      () =>
        tab.linkedBrowser.contentTitle === "owned-ordinary-json-control-ready",
      "ordinary Firefox control document",
    );
    await attempt(ordinary, "message-and-Port", async () => {
      const requestId = Services.uuid.generateUUID().toString();
      tab.linkedBrowser.messageManager.loadFrameScript(
        "data:application/javascript," +
          encodeURIComponent(
            "content.postMessage(" +
              JSON.stringify({
                target: "floorp-owned-message-json",
                requestId,
              }) + ",'*');",
          ),
        false,
      );
      const reply = await until(() => {
        try {
          const value = JSON.parse(
            tab.linkedBrowser.contentTitle,
          ) as MessageControlReply;
          return value.requestId === requestId ? value : false;
        } catch {
          return false;
        }
      }, "ordinary structured clone reply");
      assert(
        !reply.error,
        "Ordinary structured clone succeeds: " + reply.error,
      );
      const snapshot = {
        date: "[object Date]",
        iso: "2026-10-19T12:34:56.000Z",
        map: "[object Map]",
        entries: [["key", "value"]],
        set: "[object Set]",
        setValues: ["owned"],
        typed: "[object Uint8Array]",
        typedValues: [3, 7, 255],
        bigint: "bigint",
        bigintText: "12",
        undefinedPresent: true,
        negativeZero: true,
        cycle: true,
        receiverRealm: "document",
      };
      compare(ordinary, "message-and-Port", record(reply.value), {
        realm: "document",
        runtimeId: id,
        hooks: 0,
        message: snapshot,
        port: snapshot,
      });
    });
    for (const item of evidence.groups) {
      if (item.observations.length !== item.expectedCount) {
        item.failures.push(
          "Expected " + item.expectedCount + " actual rows, received " +
            item.observations.length,
        );
      }
    }
    evidence.passed = evidence.groups.length === 15 &&
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
  return runTests("NativeMessageJSONCandidate.test.mts", [{
    name:
      "exact Chrome JSON, overload and reentrant contracts in genuine classic/module Workers and documents with ordinary Firefox controls",
    fn: exercise,
  }]);
}
