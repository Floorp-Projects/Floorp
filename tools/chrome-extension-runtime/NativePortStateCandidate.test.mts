// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { zipSync } from "fflate/browser";
import {
  incomingPortStateProbeSource,
  portStateStressProbeSource,
} from "./native-port-state-probe-sources.mjs";
import {
  assert,
  assertEquals,
  runTests,
} from "../../browser-features/chrome/test/utils/test_harness.ts";
import type {
  MessagingCandidateAddon,
  MessagingCandidateAddonManager,
  MessagingCandidateParent,
  MessagingCandidateTab,
  MessagingCandidateWindow,
} from "./native-messaging-candidate-types.ts";
import type {
  PortStateConfig,
  PortStateEvidence,
  PortStateHostEvent,
  PortStateIncoming,
  PortStateIncomingSender,
  PortStateResponse,
  PortStateResult,
} from "./native-port-state-test-types.ts";
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
const originalId = "abcdefghijklmnopabcdefghijklmnop";
const pause = (ms: number) => new Promise((resolve) => setTimeout(resolve, ms));
async function until<Value>(
  read: () => Value | false | undefined | Promise<Value | false | undefined>,
  label: string,
): Promise<Value> {
  const deadline = Date.now() + 20000;
  while (Date.now() < deadline) {
    const value = await read();
    if (value) return value;
    await pause(25);
  }
  throw Error("Timed out waiting for " + label);
}
async function readEvents(
  config: PortStateConfig,
): Promise<PortStateHostEvent[]> {
  if (!await IOUtils.exists(config.hostEventsPath)) return [];
  return (await IOUtils.readUTF8(config.hostEventsPath)).trim().split("\n")
    .filter(Boolean).map((line) => JSON.parse(line) as PortStateHostEvent);
}
const program = portStateStressProbeSource + "\n" +
  incomingPortStateProbeSource + `
async function dispatchPortState(request){
 if(request.operation==='state')return globalThis.probePortStateStress(request.hosts);
 if(request.operation==='incoming-start')return globalThis.startIncomingCloseStress(request.name);
 if(request.operation==='incoming-read')return globalThis.getIncomingCloseStress(request.name);
 throw Error('Unknown owned Port state operation');
}
`;
async function exercise(): Promise<void> {
  const configPath = Services.env.get("FLOORP_PORT_STATE_CANDIDATE_CONFIG");
  assert(configPath, "Owned native Port state configuration exists");
  const config = await IOUtils.readJSON(configPath) as PortStateConfig;
  const { AppConstants } = ChromeUtils.importESModule(
    "resource://gre/modules/AppConstants.sys.mjs",
  );
  assert(
    AppConstants.MOZ_WEBEXT_WEBIDL_ENABLED,
    "Native Worker bindings are compiled",
  );
  assertEquals(
    AppConstants.platform === "win"
      ? "windows"
      : AppConstants.platform === "macosx"
      ? "mac"
      : AppConstants.platform,
    config.platform,
    "Actual host OS matches owned registration",
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
  const report: PortStateEvidence = {
    stage: "authored-actual-port-local-state-stress",
    passed: false,
    token: config.token,
    runId: Services.env.get("FLOORP_TEST_RUN_ID"),
    states: [],
    incoming: [],
    hostEvents: [],
  };
  let addon: MessagingCandidateAddon | undefined,
    tab: MessagingCandidateTab | undefined,
    packagePath: string | undefined,
    failure: Error | undefined;
  try {
    const id = originalId + "@chromium-extension";
    const files = {
      "manifest.json": JSON.stringify({
        manifest_version: 3,
        name: "Owned genuine Chrome Port state stress",
        version: "1",
        permissions: ["nativeMessaging"],
        background: { service_worker: "worker.js" },
        browser_specific_settings: {
          gecko: {
            id,
            floorp_chrome_extension: {
              schema_version: 1,
              original_id: originalId,
            },
          },
        },
      }),
      "control.html":
        '<!doctype html><meta charset="utf-8"><script src="control.js"></script>',
      "worker.js": program +
        `chrome.runtime.onMessage.addListener(request=>request.target==='owned-port-state'?dispatchPortState(request):undefined);`,
      "control.js": program +
        `window.addEventListener('message',async event=>{if(event.data?.target!=='owned-port-state')return;const requestId=event.data.requestId;try{const result=event.data.mode==='worker'?await chrome.runtime.sendMessage(event.data):await dispatchPortState(event.data);document.title=JSON.stringify({requestId,result});}catch(error){document.title=JSON.stringify({requestId,error:String(error)});}});document.title='owned-port-state-ready';`,
    };
    packagePath = PathUtils.join(
      PathUtils.tempDir,
      "native-port-state-" + Services.uuid.generateUUID() + ".xpi",
    );
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
      () => ExtensionParent.GlobalManager.extensionMap.get(id),
      "owned installed extension",
    );
    await until(
      () =>
        extension.backgroundState === "running" &&
        extension.backgroundContext?.viewType === "background_worker",
      "genuine native background Worker",
    );
    tab = window.gBrowser.addTab(extension.baseURI.resolve("control.html"), {
      triggeringPrincipal: extension.principal,
    });
    const browser = tab.linkedBrowser;
    await until(
      () => browser.contentTitle === "owned-port-state-ready",
      "owned control document",
    );
    const request = async (
      mode: "document" | "worker",
      operation: string,
      name?: string,
    ): Promise<PortStateResponse["result"]> => {
      const requestId = Services.uuid.generateUUID().toString();
      const value = {
        target: "owned-port-state",
        requestId,
        mode,
        operation,
        name,
        hosts: config.hosts,
      };
      browser.messageManager.loadFrameScript(
        "data:application/javascript," +
          encodeURIComponent(
            "content.postMessage(" + JSON.stringify(value) + ",'*');",
          ),
        false,
      );
      const response = await until(() => {
        try {
          const reply = JSON.parse(browser.contentTitle) as PortStateResponse;
          return reply.requestId === requestId ? reply : false;
        } catch {
          return false;
        }
      }, mode + " " + operation);
      assert(!response.error, "Public probe completed: " + response.error);
      assert(
        response.result !== undefined,
        "Public probe returned observations",
      );
      return response.result;
    };
    for (const realm of ["document", "worker"] as const) {
      const result = await request(realm, "state") as PortStateResult;
      assertEquals(
        result.realm,
        realm,
        "Exact requested document or native Worker ran shared Chrome source",
      );
      assertEquals(
        result.runtimeId,
        originalId,
        "Public Chrome identity is correct",
      );
      assertEquals(
        result.cases.length,
        14,
        "All fourteen exact Chrome stress cases ran",
      );
      assertEquals(
        JSON.stringify(
          result.cases.map((item) => item.kind + "/" + item.variant),
        ),
        JSON.stringify([
          ...["normal", "missing", "forbidden"].flatMap((kind) =>
            [
              "late-listeners",
              "serialization",
              "no-public-disconnect-listener",
              "explicit-local-disconnect",
            ].map((variant) => kind + "/" + variant)
          ),
          "allowed/mutation-at-post",
          "allowed/established-later-task-close",
        ]),
        "Exact Chrome case identities and ordering are preserved",
      );
      report.states.push(result);
      for (const item of result.cases) {
        const label = realm + "/" + item.kind + "/" + item.variant;
        assert(!item.timeout, label + " completed");
        if (
          item.variant === "late-listeners" || item.variant === "serialization"
        ) {
          assertEquals(
            item.postThrow,
            null,
            label + ": same-task post accepted",
          );
          assertEquals(
            item.listeners?.length,
            2,
            label + ": two late-registered callbacks delivered",
          );
          assertEquals(
            JSON.stringify(item.listeners?.map((listener) => listener.index)),
            "[1,2]",
            label + ": both distinct late listener registrations ran",
          );
          for (const listener of item.listeners ?? []) {
            assert(
              !listener.initialTask && listener.samePort,
              label + ": callbacks run after initial task with original port",
            );
            assert(
              listener.postThrow,
              label + ": post inside disconnect callback throws",
            );
            assert(
              item.kind === "normal"
                ? listener.error === null
                : typeof listener.error === "string" &&
                  listener.error.length > 0,
              label + ": normal/error distinction preserved",
            );
          }
          if (item.variant === "serialization") {
            assert(
              item.cyclicThrow && item.bigIntThrow,
              label + ": JSON failures remain synchronous",
            );
          }
        } else if (item.variant === "no-public-disconnect-listener") {
          assertEquals(
            item.initialPostThrow,
            null,
            label + ": initial post accepted without disconnect listener",
          );
          assert(
            item.eventualPostThrow,
            label +
              ": eventual post throws without public notification listener",
          );
        } else if (item.variant === "explicit-local-disconnect") {
          assert(
            item.postThrow,
            label + ": local disconnect rejects immediate post",
          );
        } else if (item.variant === "mutation-at-post") {
          assertEquals(item.postThrow, null, label + ": initial send accepted");
          assertEquals(
            JSON.stringify(item.echo),
            JSON.stringify({
              command: "echo",
              value: "at-call",
              list: ["original"],
            }),
            label + ": payload serialized at call time",
          );
        } else if (item.variant === "established-later-task-close") {
          assertEquals(
            item.initialPostThrow,
            null,
            label + ": native host armed",
          );
          assertEquals(
            item.postThrow,
            null,
            label +
              ": current later task retains open state before disconnect delivery",
          );
          assert(
            item.disconnect?.samePort && !item.disconnect.callbackActive &&
              item.disconnect.postThrow,
            label + ": later notification marks port closed",
          );
        } else throw Error("Unknown observed stress variant: " + label);
      }
    }
    for (const receiver of ["worker", "document"] as const) {
      const source = receiver === "worker" ? "document" : "worker",
        name = "floorp-owned-incoming-close-" + receiver;
      const sender = await request(
        source,
        "incoming-start",
        name,
      ) as PortStateIncomingSender;
      assertEquals(
        sender.realm,
        source,
        "Incoming probe sender executes in exact requested realm",
      );
      assertEquals(
        sender.runtimeId,
        originalId,
        "Incoming sender uses expected public identity",
      );
      assert(
        !sender.timeout,
        "Sender observes incoming receiver callback before closing",
      );
      const incoming = await until(async () => {
        const result = await request(receiver, "incoming-read", name) as
          | PortStateIncoming
          | null;
        return result?.disconnects.length === 2 ? result : false;
      }, "both incoming disconnect callbacks");
      report.incoming.push({ receiver, sender, incoming });
      assertEquals(
        incoming.realm,
        receiver,
        "Incoming callbacks execute in exact requested realm",
      );
      assertEquals(
        incoming.callbacks.length,
        2,
        "Both onConnect callbacks ran",
      );
      assertEquals(
        JSON.stringify(incoming.callbacks.map((callback) => callback.index)),
        "[1,2]",
        "Both distinct incoming listener registrations ran",
      );
      assertEquals(
        JSON.stringify(incoming.disconnects.map((event) => event.index)),
        "[1,2]",
        "Both distinct incoming disconnect listeners ran",
      );
      assertEquals(
        incoming.thenGetterReads,
        0,
        "Port callback return thenables are ignored",
      );
      assert(
        incoming.callbacks[0].startedAt <= sender.localDisconnectAt &&
          sender.localDisconnectAt < incoming.callbacks[0].finishedAt,
        "Actual remote disconnect occurred during first receiver callback",
      );
      for (const callback of incoming.callbacks) {
        assertEquals(
          callback.initialPostThrow,
          null,
          "Incoming callback initially sees open Port",
        );
        assertEquals(
          callback.afterWorkPostThrow,
          null,
          "Incoming callback keeps open Port throughout its task",
        );
      }
      for (const event of incoming.disconnects) {
        assert(
          !event.callbackActive && event.samePort && event.error === null &&
            event.postThrow,
          "Both incoming disconnect callbacks run after original callbacks and observe closed state",
        );
      }
    }
    report.hostEvents = await until(async () => {
      const events = await readEvents(config);
      return events.filter((event) => event.event === "start").length === 4 &&
          events.filter((event) => event.event !== "start").length === 4
        ? events
        : false;
    }, "four allowed owned hosts exit");
    for (const result of report.states) {
      const item = result.cases.find((value) =>
        value.variant === "established-later-task-close"
      );
      assert(
        item && item.hostCloseRequestedAt !== undefined &&
          item.postAt !== undefined,
        "Established host timestamps retained",
      );
      const requested = item.hostCloseRequestedAt, posted = item.postAt;
      assert(
        report.hostEvents.some((event) =>
          event.event === "delayed-exit" && event.at > requested &&
          event.at < posted
        ),
        "Actual host exited before later-task post, proving state ordering",
      );
    }
    assert(
      !await IOUtils.exists(config.forbiddenLaunchPath),
      "Forbidden origin never starts owned executable",
    );
    report.passed = true;
  } catch (error) {
    report.error = String(error);
    failure = error instanceof Error ? error : Error(String(error));
  } finally {
    const errors: string[] = [];
    try {
      if (tab?.isConnected) window.gBrowser.removeTab(tab);
    } catch (error) {
      errors.push(String(error));
    }
    try {
      if (addon) await addon.uninstall();
    } catch (error) {
      errors.push(String(error));
    }
    try {
      if (packagePath) {
        await IOUtils.remove(packagePath, { ignoreAbsent: true });
      }
    } catch (error) {
      errors.push(String(error));
    }
    if (errors.length) {
      report.passed = false;
      report.error = errors.join("; ");
      failure ??= Error(report.error);
    }
    await IOUtils.writeJSON(config.outputPath, report);
    console.info("NATIVE_PORT_STATE_REPORT " + config.outputPath);
  }
  if (failure) throw failure;
}
export function runAllTests(): Promise<void> {
  return runTests("NativePortStateCandidate.test.mts", [{
    name:
      "genuine Chrome Port local state across initial and later tasks, no listeners, incoming callbacks and native host exit",
    fn: exercise,
  }]);
}
