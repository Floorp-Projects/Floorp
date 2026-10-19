// SPDX-License-Identifier: MPL-2.0
import { unzipSync } from "fflate/browser";
import { CRXConverterClass } from "../../browser-features/modules/modules/chrome-web-store/CRXConverter.sys.mts";
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
const { AddonManager } = ChromeUtils.importESModule(
  "resource://gre/modules/AddonManager.sys.mjs",
);
const { ExtensionParent } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionParent.sys.mjs",
);
const { ExtensionCommon } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionCommon.sys.mjs",
);
const { FileUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/FileUtils.sys.mjs",
);
const pause = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
export async function runAllTests() {
  const root = Services.env.get("FLOORP_CORPUS_ROOT");
  const fixture = Services.env.get("FLOORP_CORPUS_FIXTURE");
  if (!root || !fixture) {
    throw Error("FLOORP_CORPUS_ROOT and FLOORP_CORPUS_FIXTURE are required");
  }
  const ids = Services.env.get("FLOORP_CORPUS_IDS").split(",").filter(Boolean);
  const corpus = await IOUtils.readJSON(
    PathUtils.join(
      root,
      "tools",
      "chrome-extension-runtime",
      "corpus-lock.json",
    ),
  );
  const recipes = await IOUtils.readJSON(
    PathUtils.join(
      root,
      "tools",
      "chrome-extension-runtime",
      "corpus-sidepanel-recipes.json",
    ),
  );
  const output = PathUtils.join(
    root,
    "_dist",
    "chrome-runtime-tests",
    `corpus-sidepanel-floorp-${Date.now()}`,
  );
  await IOUtils.makeDirectory(output);
  const report = {
    stage: "actual-sidepanel-primary-operation",
    fixture,
    authoredFixture: JSON.parse(
      Services.env.get("FLOORP_CORPUS_FIXTURE_METADATA") || "{}",
    ),
    runtime: {
      version: Services.appinfo.version,
      buildId: Services.appinfo.appBuildID,
    },
    extensions: [],
    output,
  };
  const save = () =>
    IOUtils.writeJSON(PathUtils.join(output, "report.json"), report);
  const resource = Services.io.getProtocolHandler("resource").QueryInterface(
    Ci.nsIResProtocolHandler,
  );
  resource.setSubstitution(
    "floorp-sidepanel-corpus",
    Services.io.newFileURI(
      new FileUtils.File(
        PathUtils.join(root, "tools", "chrome-extension-runtime"),
      ),
    ),
  );
  ChromeUtils.registerWindowActor("CorpusSidePanel", {
    child: {
      esModuleURI:
        "resource://floorp-sidepanel-corpus/CorpusSidePanelChild.sys.mjs",
    },
    allFrames: false,
    remoteTypes:["web","extension","webIsolated=http://floorp.test"],
  });
  const window = Services.wm.getMostRecentWindow("navigator:browser");
  const converter = new CRXConverterClass();
  const sentinel =
    "Floorp authored compatibility passage 73195: Arctic foxes cross the quiet bridge.";
  const originalProxy = Services.prefs.getIntPref("network.proxy.type", 0);
  Services.prefs.setIntPref("network.proxy.type", 0);
  const originalDns = Services.prefs.getStringPref(
    "network.dns.localDomains",
    "",
  );
  Services.prefs.setStringPref(
    "network.dns.localDomains",
    [originalDns, "corpus.floorp.test"].filter(Boolean).join(","),
  );
  const actorFor = (tab) =>
    tab.linkedBrowser.browsingContext.currentWindowGlobal?.getActor(
      "CorpusSidePanel",
    );
  try {
    for (
      const record of corpus.extensions.filter((e) =>
        e.area === "sidePanel" && (!ids.length || ids.includes(e.id))
      )
    ) {
      const item = {
        id: record.id,
        name: record.name,
        version: record.version,
        packageSha256: record.packageSha256,
        result: "failed",
        checks: [],
      };
      report.extensions.push(item);
      let addon;
      const originalTabs = new Set(window.gBrowser.tabs);
      try {
        const bytes = await IOUtils.read(
          PathUtils.join(
            root,
            "_dist",
            "chrome-extension-corpus",
            "packages",
            `${record.id}.crx`,
          ),
        );
        const digest = [
          ...new Uint8Array(await crypto.subtle.digest("SHA-256", bytes)),
        ].map((b) => b.toString(16).padStart(2, "0")).join("");
        if (digest !== record.packageSha256) {
          throw Error("Frozen signed CRX digest mismatch");
        }
        const conversion = await converter.convert(bytes.buffer, record.id, {
          id: record.id,
          name: record.name,
          version: record.version,
        });
        item.warnings = conversion.warnings;
        if (!conversion.success || !conversion.xpiData) {
          throw Error(
            conversion.error?.message || "Production conversion failed",
          );
        }
        const files = unzipSync(new Uint8Array(conversion.xpiData));
        const unchanged = [];
        for (
          const [name, contents] of Object.entries(files).filter(([name]) =>
            /\.(?:js|mjs|cjs)$/i.test(name)
          )
        ) {
          const original = await IOUtils.read(
            PathUtils.join(
              root,
              "_dist",
              "chrome-extension-corpus",
              "chrome-unpacked",
              record.id,
              ...name.split("/"),
            ),
          );
          const digest = async (bytes) =>
            [...new Uint8Array(await crypto.subtle.digest("SHA-256", bytes))]
              .map((byte) => byte.toString(16).padStart(2, "0")).join("");
          const actual = await digest(contents);
          if (actual !== await digest(original)) {
            throw Error(
              "Original signed script changed during conversion: " + name,
            );
          }
          unchanged.push(name + ":" + actual);
        }
        item.unchangedScriptCount = unchanged.length;
        item.unchangedScriptsSha256 = [
          ...new Uint8Array(
            await crypto.subtle.digest(
              "SHA-256",
              new TextEncoder().encode(unchanged.sort().join("\n")),
            ),
          ),
        ].map((byte) => byte.toString(16).padStart(2, "0")).join("");
        const path = PathUtils.join(output, `${record.id}.xpi`);
        await IOUtils.write(path, new Uint8Array(conversion.xpiData));
        addon = await AddonManager.installTemporaryAddon(
          new FileUtils.File(path),
        );
        const extension = ExtensionParent.GlobalManager.extensionMap.get(
          `${record.id}@chromium-extension`,
        );
        if (!extension) throw Error("Installed native extension missing");
        await pause(1500);
        const firstURL = fixture + record.id + "/";
        item.authoredURL = firstURL;
        item.authoredSourceSha256 = report.authoredFixture.pages?.find((page) =>
          page.id === record.id
        )?.sha256;
        const first = window.gBrowser.addTab(firstURL, {
          triggeringPrincipal: Services.scriptSecurityManager
            .getSystemPrincipal(),
        });
        const second = window.gBrowser.addTab(firstURL + "second", {
          triggeringPrincipal: Services.scriptSecurityManager
            .getSystemPrincipal(),
        });
        window.gBrowser.selectedTab = first;
        let authoredReady = false;
        for (let n = 0; n < 40; n++) {
          try {
            const state = await actorFor(first)?.sendQuery("Snapshot");
            item.authoredDocument = {
              url: state?.url,
              text: state?.text?.slice(0, 500),
              uri: first.linkedBrowser.currentURI.spec,
              remoteType: first.linkedBrowser.remoteType,
            };
            if (state?.text?.includes(sentinel)) {
              authoredReady = true;
              break;
            }
          } catch (error) {
            item.authoredDocument = {
              error: String(error),
              uri: first.linkedBrowser.currentURI.spec,
              remoteType: first.linkedBrowser.remoteType,
            };
          }
          await pause(100);
        }
        if (!authoredReady) {
          item.result = "harness-fixture-unavailable";
          throw Error("Controlled authored content did not load");
        }
        item.checks.push("controlled authored text verified in content tab");
        const actionModule = await extension.apiManager.asyncGetAPI(
          "browserAction",
          extension,
          "addon_parent",
        );
        const action = actionModule;
        ExtensionCommon.withHandlingUserInput(
          window,
          () => action.triggerAction(window),
        );
        item.checks.push("real native toolbar action invoked on authored page");
        let view;
        for (let n = 0; n < 45; n++) {
          view = [...extension.views].find((v) =>
            v.viewType === "sidebar" || v.viewType === "popup"
          );
          if (view) break;
          await pause(200);
        }
        const getActor = () =>
          view?.xulBrowser?.browsingContext?.currentWindowGlobal?.getActor(
            "CorpusSidePanel",
          ) || actorFor(first);
        item.injectedPagePanel = !view;
        let ui;
        for (let n = 0; n < 40; n++) {
          try {
            ui = await getActor()?.sendQuery("Snapshot");
            if (ui?.ready === "complete" && ui.text?.length > 10) break;
          } catch {}
          await pause(200);
        }
        if (!ui) throw Error("Actual action produced no inspectable UI");
        item.initialUI = ui;
        const recipe = recipes.find((r) => r.id === record.id) || {};
        if (recipe.readSelector) {
          item.readClicked = await getActor().sendQuery("Click", {
            selector: recipe.readSelector,
          });
          await pause(1000);
        }
        item.afterReadUI = await getActor().sendQuery("Snapshot");
        item.readEvidence = {
          domContainsAuthoredText: !item.injectedPagePanel &&
              item.afterReadUI.text?.includes(sentinel) || false,
        };
        const selector = recipe.draftSelector ||
          "textarea:not([readonly]):not([disabled]),[contenteditable=true]";
        const draft = `Floorp retained draft ${record.id.slice(0, 8)} 84219`;
        const typed = await getActor().sendQuery("TypeDraft", {
          selector,
          text: draft,
        });
        item.draftBefore = typed.value;
        if (typed.value?.includes(draft)) {
          window.gBrowser.selectedTab = second;
          await pause(250);
          window.gBrowser.selectedTab = first;
          await pause(400);
          item.draftAfter =
            (await getActor().sendQuery("ReadDraft", { selector })).value;
          item.draftRetained = item.draftAfter === item.draftBefore;
          if (recipe.submitForRead) {
            await getActor().sendQuery("PressEnter", { selector });
            await pause(1500);
            item.readAttemptUI = await getActor().sendQuery("Snapshot");
          }
        }
        item.finalUI = await getActor().sendQuery("Snapshot");
        if (item.readEvidence.domContainsAuthoredText && item.draftRetained) {
          item.result = "passed";
        } else if (
          /login|log in|sign in|sign up|account|premium/i.test(
            item.readAttemptUI?.text || item.finalUI.text || "",
          )
        ) {
          item.result = "blocked-account";
          item.blocker =
            "Unchanged extension UI requires an account before authored-page read can be verified";
        } else {item.blocker =
            "Authored-page read and retained draft were not both demonstrated through the actual extension UI";}
      } catch (error) {
        item.error = String(error);
      } finally {
        await addon?.uninstall();
        for (const tab of [...window.gBrowser.tabs]) {
          if (!originalTabs.has(tab)) {
            window.gBrowser.removeTab(tab, { skipPermitUnload: true });
          }
        }
        await save();
        console.log(
          `[Sidepanel corpus] ${item.name}: ${item.result} ${
            item.blocker || item.error || ""
          }`,
        );
      }
    }
  } finally {
    ChromeUtils.unregisterWindowActor("CorpusSidePanel");
    Services.prefs.setIntPref("network.proxy.type", originalProxy);
    if (originalDns) {
      Services.prefs.setStringPref("network.dns.localDomains", originalDns);
    } else Services.prefs.clearUserPref("network.dns.localDomains");
    await save();
    console.log(`[Sidepanel corpus] report ${output}/report.json`);
  }
}
