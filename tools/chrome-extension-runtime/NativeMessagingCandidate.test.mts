// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
// Public extension APIs in actual document and native Worker contexts.
import { zipSync } from "fflate/browser";
import { nativePortErrorsProbeSource } from "./native-port-errors-probe.mjs";
import { assertNativePortErrors } from "./native-port-errors-assertions.mts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../browser-features/chrome/test/utils/test_harness.ts";
import type {
  MessagingCandidateAddon,
  MessagingCandidateAddonManager,
  MessagingCandidateConfig,
  MessagingCandidateDenial,
  MessagingCandidateEvidence,
  MessagingCandidateHostEvent,
  MessagingCandidateManifest,
  MessagingCandidateParent,
  MessagingCandidateResponse,
  MessagingCandidateSuccess,
  MessagingCandidateTab,
  MessagingCandidateWindow,
} from "./native-messaging-candidate-types.ts";

const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
const allowedId = "abcdefghijklmnopabcdefghijklmnop";
const deniedId = "ponmlkjihgfedcbaponmlkjihgfedcba";
const delay = (ms: number) => new Promise((resolve) => setTimeout(resolve, ms));
async function until<Value>(
  read: () => Value | false | undefined | Promise<Value | false | undefined>,
  description: string,
): Promise<Value> {
  const end = Date.now() + 20000;
  while (Date.now() < end) {
    const value = await read();
    if (value) return value;
    await delay(30);
  }
  throw Error("Timed out waiting for " + description);
}
const extensionSource = nativePortErrorsProbeSource + `
async function nativeMessagingProbe(request){
 const {hostName,token,mode,deny}=request,context=typeof document==='undefined'?'worker':'document';
 const payload=step=>({token,context,step,unicode:'日本語 🌐',nested:{values:[1,true,null,'framed JSON']}});
 if(deny){
  let oneShotError='';
  try{await chrome.runtime.sendNativeMessage(hostName,payload('denied-one-shot'));}catch(error){oneShotError=String(error.message||error);}
  const disconnects=[];
  for(const throwFirst of [false,true])disconnects.push(await probeNativePortErrors({...request,throwFirst}));
  const first=disconnects[0].records[0];
  const connectError=(request.ordinary?first.portError?.message:first.lastError?.message)||'';
  return {oneShotError,connectError,disconnects};
 }
 const normalPorts=[];
 for(const throwFirst of [false,true])normalPorts.push(await probeNativePortErrors({...request,normal:true,throwFirst}));
 const oneShot=await chrome.runtime.sendNativeMessage(hostName,payload('one-shot'));
 const connected=await new Promise((resolve,reject)=>{
  const values=[],port=chrome.runtime.connectNative(hostName);
  const timer=setTimeout(()=>{port.disconnect();reject(Error('Native framed exchange timed out'));},8000);
  port.onDisconnect.addListener(()=>{if(values.length<2){clearTimeout(timer);reject(Error(chrome.runtime.lastError?.message||'Native port disconnected before second frame'));}});
  port.onMessage.addListener(value=>{values.push(value);if(values.length===1)port.postMessage(payload('port-second'));else{clearTimeout(timer);port.disconnect();resolve(values);}});
  port.postMessage(payload('port-first'));
 });
 return {context,isWorker:typeof document==='undefined',runtimeId:chrome.runtime.id,oneShot,connected,normalPorts};
}
`;
async function hostEvents(
  config: MessagingCandidateConfig,
): Promise<MessagingCandidateHostEvent[]> {
  if (!await IOUtils.exists(config.trafficPath)) return [];
  const source = await IOUtils.readUTF8(config.trafficPath);
  return source.trim()
    ? source.trim().split("\n").map((line) =>
      JSON.parse(line) as MessagingCandidateHostEvent
    )
    : [];
}
async function exercise(): Promise<void> {
  const configPath = Services.env.get(
    "FLOORP_NATIVE_MESSAGING_CANDIDATE_CONFIG",
  );
  assert(configPath, "Owned native messaging candidate configuration exists");
  const config = await IOUtils.readJSON(configPath) as MessagingCandidateConfig;
  const { AppConstants } = ChromeUtils.importESModule(
    "resource://gre/modules/AppConstants.sys.mjs",
  );
  assert(
    AppConstants.MOZ_WEBEXT_WEBIDL_ENABLED,
    "Actual native Worker bindings are compiled",
  );
  const observedPlatform = AppConstants.platform === "win"
    ? "windows"
    : AppConstants.platform === "macosx"
    ? "mac"
    : AppConstants.platform;
  assertEquals(
    observedPlatform,
    config.platform,
    "Real operating system matches host registration",
  );
  if (config.platform !== "windows") {
    const home = Services.dirsvc.get("Home", Ci.nsIFile).path;
    const directory = config.platform === "mac"
      ? PathUtils.join(
        home,
        "Library",
        "Application Support",
        "Google",
        "Chrome",
        "NativeMessagingHosts",
      )
      : PathUtils.join(
        Services.env.get("XDG_CONFIG_HOME"),
        "google-chrome",
        "NativeMessagingHosts",
      );
    assertEquals(
      config.manifestPath,
      PathUtils.join(directory, config.hostName + ".json"),
      "Manifest uses the runtime's actual native host search location",
    );
  }
  const manifest = await IOUtils.readJSON(
    config.manifestPath,
  ) as MessagingCandidateManifest;
  assertEquals(
    manifest.name,
    config.hostName,
    "Real OS registration names this owned host",
  );
  assertEquals(
    manifest.path,
    config.hostPath,
    "Registered executable belongs to this test",
  );
  assertEquals(
    JSON.stringify(manifest.allowed_origins),
    JSON.stringify(["chrome-extension://" + allowedId + "/"]),
    "Host explicitly authorizes only the genuine Chrome ID",
  );
  const evidence: MessagingCandidateEvidence = {
    stage: "authored-actual-native-messaging-api-gate",
    passed: false,
    token: config.token,
    runId: Services.env.get("FLOORP_TEST_RUN_ID"),
    platform: config.platform,
    manifestPath: config.manifestPath,
    hostPath: config.hostPath,
    allowedOriginalId: allowedId,
    allowedOrigin: "chrome-extension://" + allowedId + "/",
    successes: [],
    denials: [],
    hostEvents: [],
  };
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
  const addons: MessagingCandidateAddon[] = [],
    tabs: MessagingCandidateTab[] = [],
    packages: string[] = [];
  let failure: Error | undefined;
  try {
    for (const caller of ["allowed", "denied", "ordinary"] as const) {
      const converted = caller !== "ordinary",
        original = caller === "denied" ? deniedId : allowedId;
      const id = converted
        ? original + "@chromium-extension"
        : "ordinary-native-messaging-candidate@example.test";
      const sourceManifest = {
        manifest_version: 3,
        name: "Actual Native Messaging " + caller,
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
        "manifest.json": JSON.stringify(sourceManifest),
        "control.html":
          '<!doctype html><meta charset="utf-8"><script src="control.js"></script>',
        "worker.js": extensionSource +
          `chrome.runtime.onMessage.addListener(request=>request.target==='native-messaging-candidate'?nativeMessagingProbe(request):undefined);`,
        "control.js": extensionSource +
          `window.addEventListener('message',async event=>{if(event.data?.target!=='native-messaging-candidate')return;const requestId=event.data.requestId;try{const result=event.data.mode==='worker'?await chrome.runtime.sendMessage(event.data):await nativeMessagingProbe(event.data);document.title=JSON.stringify({requestId,result});}catch(error){document.title=JSON.stringify({requestId,error:String(error)});}});document.title='native-messaging-candidate-ready';`,
      };
      const packagePath = PathUtils.join(
        PathUtils.tempDir,
        "native-messaging-candidate-" + Services.uuid.generateUUID() + ".xpi",
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
        "actual owned extension policy",
      );
      if (converted) {
        await until(
          () =>
            extension.backgroundState === "running" &&
            extension.backgroundContext?.viewType === "background_worker",
          "actual native Worker context for " + caller,
        );
      }
      const tab = window.gBrowser.addTab(
        extension.baseURI.resolve("control.html"),
        { triggeringPrincipal: extension.principal },
      );
      tabs.push(tab);
      await until(
        () =>
          tab.linkedBrowser.contentTitle === "native-messaging-candidate-ready",
        "extension control document",
      );
      for (
        const context of caller === "ordinary"
          ? ["document"] as const
          : ["document", "worker"] as const
      ) {
        const requestId = Services.uuid.generateUUID().toString();
        const request = {
          target: "native-messaging-candidate",
          requestId,
          hostName: config.hostName,
          token: config.token,
          mode: context,
          deny: caller !== "allowed",
          ordinary: caller === "ordinary",
        };
        const before = (await hostEvents(config)).filter((event) =>
          event.event === "start"
        ).length;
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
            ) as MessagingCandidateResponse;
            return value.requestId === requestId ? value : false;
          } catch {
            return false;
          }
        }, caller + " " + context + " public Native Messaging reply");
        assert(
          !response.error,
          "Public Native Messaging operation completed: " + response.error,
        );
        assert(
          response.result,
          "Public Native Messaging returned concrete results",
        );
        if (caller === "allowed") {
          const result = response.result as MessagingCandidateSuccess;
          assertEquals(
            result.context,
            context,
            "Operation ran in the requested actual context",
          );
          assertEquals(
            result.isWorker,
            context === "worker",
            "Native Worker executes without a document",
          );
          assertEquals(
            result.runtimeId,
            allowedId,
            "Public runtime identity uses the original Chrome ID",
          );
          assertEquals(
            result.connected.length,
            2,
            "Connected host exchanged two separate frames",
          );
          assertEquals(
            result.connected[0].pid,
            result.connected[1].pid,
            "Connected frames come from the same actual process",
          );
          const replies = [result.oneShot, ...result.connected];
          const expectedArgs = [
            evidence.allowedOrigin,
            ...(config.platform === "windows" ? ["--parent-window=0"] : []),
          ];
          for (const [index, reply] of replies.entries()) {
            assert(
              Number.isInteger(reply.pid) && reply.pid > 0,
              "Host reports its actual process ID",
            );
            assertEquals(
              JSON.stringify(reply.argv),
              JSON.stringify(expectedArgs),
              "Actual executable receives Chrome native launch arguments",
            );
            assertEquals(
              reply.serial,
              index === 2 ? 2 : 1,
              "Host decodes each native length-prefixed frame",
            );
            assertEquals(
              JSON.stringify(reply.message),
              JSON.stringify({
                token: config.token,
                context,
                step: ["one-shot", "port-first", "port-second"][index],
                unicode: "日本語 🌐",
                nested: { values: [1, true, null, "framed JSON"] },
              }),
              "UTF-8 nested JSON round trips through the public API and native process",
            );
          }
          assertEquals(
            result.normalPorts.length,
            2,
            "Normal remote disconnect controls ran with and without an intentional throw",
          );
          for (const normal of result.normalPorts) {
            assertNativePortErrors(normal, false);
          }
          evidence.successes.push(result);
        } else {
          const result = response.result as MessagingCandidateDenial;
          assertEquals(
            result.disconnects.length,
            2,
            "Denied native ports ran with and without an intentional first-listener throw",
          );
          for (const disconnect of result.disconnects) {
            assertNativePortErrors(disconnect, caller === "ordinary");
          }
          assert(
            result.oneShotError.length > 0,
            "Unapproved caller cannot sendNativeMessage",
          );
          assert(
            result.connectError.length > 0,
            "Unapproved caller's native port disconnects with an error",
          );
          await delay(100);
          assertEquals(
            (await hostEvents(config)).filter((event) =>
              event.event === "start"
            ).length,
            before,
            "Denied calls never execute the registered host",
          );
          evidence.denials.push({ caller: caller + ":" + context, result });
        }
      }
    }
    evidence.hostEvents = await until(
      async () => {
        const events = await hostEvents(config),
          starts = events.filter((event) => event.event === "start"),
          exits = new Set(
            events.filter((event) =>
              event.event === "exit" && event.clean === true
            ).map((event) => event.pid),
          );
        return starts.length === 4 &&
            starts.every((event) => exits.has(event.pid))
          ? events
          : false;
      },
      "all four owned actual host processes to exit after public API disconnect",
    );
    assertEquals(
      evidence.hostEvents.filter((event) => event.event === "receive").length,
      6,
      "Only the six authorized framed messages reach actual host processes",
    );
    evidence.passed = true;
  } catch (error) {
    evidence.error = String(error);
    failure = error instanceof Error ? error : Error(String(error));
  } finally {
    const cleanupErrors: string[] = [];
    for (const tab of tabs.reverse()) {
      try {
        if (tab.isConnected) window.gBrowser.removeTab(tab);
      } catch (error) {
        cleanupErrors.push(String(error));
      }
    }
    for (const addon of addons.reverse()) {
      try {
        await addon.uninstall();
      } catch (error) {
        cleanupErrors.push(String(error));
      }
    }
    for (const path of packages) {
      try {
        await IOUtils.remove(path, { ignoreAbsent: true });
      } catch (error) {
        cleanupErrors.push(String(error));
      }
    }
    evidence.hostEvents = await hostEvents(config);
    if (cleanupErrors.length) {
      evidence.passed = false;
      evidence.error = cleanupErrors.join("; ");
      failure ??= Error(
        "Owned Native Messaging cleanup failed: " + cleanupErrors.join("; "),
      );
    }
    await IOUtils.writeJSON(config.outputPath, evidence);
  }
  if (failure) throw failure;
}
export async function runAllTests(): Promise<void> {
  await runTests("NativeMessagingCandidate.test.mts", [{
    name:
      "actual cross-platform Chrome Native Messaging public APIs and authorization",
    fn: exercise,
  }]);
}
