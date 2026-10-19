// SPDX-License-Identifier: MPL-2.0
/** @import {Truthy, NativeBinaryInput, CorpusLock, CorpusReport, CorpusOperation, MediaSnapshot, NativeWindowHandle, NativeTabHandle, ActorClickData, MediaActorResponses, AssessmentExtensionParentModule, ResourceProtocolHandler, MediaFixture} from "./assessment-test-types.ts" */
import { unzipSync } from "fflate/browser";
import { runSponsorFloorp } from "./corpus-sponsor-floorp.mjs";
import { CRXConverterClass } from "../../browser-features/modules/modules/chrome-web-store/CRXConverter.sys.mts";
const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
const { AddonManager } = ChromeUtils.importESModule(
  "resource://gre/modules/AddonManager.sys.mjs",
);
const { ExtensionParent } =
  /** @type {AssessmentExtensionParentModule} */ (ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  ));
const { FileUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/FileUtils.sys.mjs",
);
/** @param {number} ms @returns {Promise<void>} */
const pause = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
/** @template T @param {()=>T|Promise<T>} read @param {string} label @returns {Promise<Truthy<T>>} */
async function until(read, label) {
  const end = Date.now() + 30000;
  while (Date.now() < end) {
    const value = await read();
    if (value) return /** @type {Truthy<T>} */ (value);
    await pause(100);
  }
  throw Error("Timed out waiting for " + label);
}
/** @param {NativeBinaryInput} bytes */
const digest = async (bytes) =>
  [
    ...new Uint8Array(
      await crypto.subtle.digest("SHA-256", new Uint8Array(bytes)),
    ),
  ].map(
    (byte) => byte.toString(16).padStart(2, "0"),
  ).join("");
export async function runAllTests() {
  const output = Services.env.get("FLOORP_MEDIA_CORPUS_OUTPUT"),
    root = Services.env.get("FLOORP_CORPUS_ROOT"),
    selected = Services.env.get("FLOORP_CORPUS_IDS").split(",");
  if (!output || !root) {
    throw Error("Private media corpus output/root required");
  }
  const fixture = /** @type {MediaFixture} */ (await IOUtils.readJSON(
      PathUtils.join(output, "fixture.json"),
    )),
    corpus = /** @type {CorpusLock} */ (await IOUtils.readJSON(
      PathUtils.join(
        root,
        "tools",
        "chrome-extension-runtime",
        "corpus-lock.json",
      ),
    ));
  /** @type {CorpusReport<MediaSnapshot>} */
  const report = {
    stage: "actual-package-primary-operation",
    runtime: {
      version: Services.appinfo.version,
      buildId: Services.appinfo.appBuildID,
      nativeWorkerPolicy: WebExtensionPolicy.backgroundServiceWorkerEnabled,
    },
    fixture,
    extensions: [],
    output,
    primaryOperationPassRate: null,
  };
  const save = () =>
    IOUtils.writeJSON(PathUtils.join(output, "report.json"), report);
  const resource =
    /** @type {ResourceProtocolHandler} */ (Services.io.getProtocolHandler(
      "resource",
    )).QueryInterface(
      Ci.nsIResProtocolHandler,
    );
  resource.setSubstitution(
    "floorp-media-corpus",
    Services.io.newFileURI(
      new FileUtils.File(Services.env.get("FLOORP_TEST_ACTOR_DIRECTORY")),
    ),
  );
  ChromeUtils.registerWindowActor("CorpusMedia", {
    child: {
      esModuleURI: "resource://floorp-media-corpus/CorpusMediaChild.sys.mjs",
    },
    safeForUntrustedWebProcess: true,
    allFrames: false,
    matches: [
      "https://www.youtube.com/*",
      "https://www.youtube-nocookie.com/*",
    ],
  });
  const cert = await IOUtils.readUTF8(PathUtils.join(output, "tls", "ca.pem"));
  Cc["@mozilla.org/security/x509certdb;1"].getService(Ci.nsIX509CertDB)
    .addCertFromBase64(
      cert.replace(/-----[^-]+-----|\s/g, ""),
      "C,,",
    );
  const originalDns = Services.prefs.getStringPref(
      "network.dns.localDomains",
      "",
    ),
    originalProxy = Services.prefs.getIntPref("network.proxy.type", 0);
  Services.prefs.setStringPref(
    "network.dns.localDomains",
    [originalDns, ...fixture.hosts].filter(Boolean).join(","),
  );
  Services.prefs.setIntPref("network.proxy.type", 0);
  const win =
      /** @type {NativeWindowHandle} */ (/** @type {unknown} */ (Services.wm
        .getMostRecentWindow("navigator:browser"))),
    principal = Services.scriptSecurityManager.getSystemPrincipal();
  /** @template {keyof MediaActorResponses} Name @param {NativeTabHandle} tab @param {Name} name @param {ActorClickData} [data] @returns {Promise<MediaActorResponses[Name]>} */
  async function query(tab, name, data) {
    const actor = tab.linkedBrowser.browsingContext.currentWindowGlobal
      ?.getActor("CorpusMedia");
    if (!actor) throw Error("Private media actor is unavailable");
    let timer;
    try {
      return /** @type {MediaActorResponses[Name]} */ (await Promise.race([
        actor.sendQuery(name, data),
        new Promise((_, reject) => {
          timer = setTimeout(
            () => reject(Error("Private media actor timed out " + name)),
            5000,
          );
        }),
      ]));
    } finally {
      clearTimeout(timer);
    }
  }
  /** @param {NativeTabHandle} tab @param {string} [url] */
  function navigate(tab, url) {
    const old = tab.linkedBrowser.browsingContext.currentWindowGlobal
      ?.innerWindowId;
    if (url) {
      tab.linkedBrowser.loadURI(Services.io.newURI(url), {
        triggeringPrincipal: principal,
      });
    } else tab.linkedBrowser.reload();
    return until(async () => {
      const value = await query(tab, "Snapshot").catch(() => null);
      return tab.linkedBrowser.browsingContext.currentWindowGlobal
            ?.innerWindowId !== old &&
        value?.ready === "complete" && (!url || value.url === url) && value;
    }, "new authored native document");
  }
  /** @param {NativeTabHandle} tab */
  async function readyAndPlay(tab) {
    win.gBrowser.selectedTab = tab;
    await until(async () => {
      const value = await query(tab, "Snapshot").catch(() => null);
      return (value?.video?.readyState ?? 0) >= 2 && value;
    }, "real native decoded media");
    await query(tab, "Click", { selector: "#play" });
    return until(async () => {
      const value = await query(tab, "Snapshot");
      return value.video && !value.video.paused &&
        value.video.currentTime > 0.1 && value;
    }, "real native media playback");
  }
  try {
    for (
      const record of corpus.extensions.filter((item) =>
        selected.includes(item.id)
      )
    ) {
      /** @type {CorpusOperation<MediaSnapshot>} */
      /** @type {CorpusOperation<MediaSnapshot>} */
      const item = {
        id: record.id,
        name: record.name,
        version: record.version,
        packageSha256: record.packageSha256,
        primaryOperation: record.primaryOperation,
        primaryOperationResult: "not-run",
        checks: [],
      };
      report.extensions.push(item);
      const originalTabs = new Set(win.gBrowser.tabs);
      let addon, verifyRemoval;
      try {
        const url = record.name === "Enhancer for YouTube"
          ? "https://www.youtube.com/embed/FloorpTest1"
          : "https://www.youtube.com/watch?v=FloorpTest1";
        const tab = win.gBrowser.addTab(url, {
          triggeringPrincipal: principal,
        });
        win.gBrowser.selectedTab = tab;
        try {
          item.baseline = await until(async () => {
            const value = await query(tab, "Snapshot").catch(() => null);
            return value?.url === url && value.ready === "complete" &&
              value.dislike && value;
          }, "native authored TLS fixture");
        } catch (error) {
          item.primaryOperationResult = "harness-fixture-unavailable";
          throw error;
        }
        const bytes = await IOUtils.read(
          PathUtils.join(
            root,
            "_dist",
            "chrome-extension-corpus",
            "packages",
            record.id + ".crx",
          ),
        );
        if (await digest(bytes) !== record.packageSha256) {
          throw Error("Frozen signed package digest mismatch");
        }
        const conversion = await new CRXConverterClass().convert(
          bytes.slice().buffer,
          record.id,
          { id: record.id, name: record.name, version: record.version },
        );
        item.conversion = conversion.report;
        item.warnings = conversion.warnings;
        if (!conversion.success || !conversion.xpiData) {
          throw Error(
            conversion.error?.message || "Production signed conversion failed",
          );
        }
        const scripts = [];
        for (
          const [name, data] of Object.entries(
            unzipSync(new Uint8Array(conversion.xpiData)),
          ).filter(([name]) => /\.(?:js|mjs|cjs)$/i.test(name))
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
          const hash = await digest(data);
          if (hash !== await digest(original)) {
            throw Error("Signed script changed " + name);
          }
          scripts.push(name + ":" + hash);
        }
        item.unchangedScriptCount = scripts.length;
        item.unchangedScriptsSha256 = await digest(
          new TextEncoder().encode(scripts.sort().join("\n")),
        );
        const xpi = PathUtils.join(output, record.id + ".xpi");
        await IOUtils.write(xpi, new Uint8Array(conversion.xpiData));
        addon = await AddonManager.installTemporaryAddon(
          new FileUtils.File(xpi),
        );
        const extension = ExtensionParent.GlobalManager.extensionMap.get(
          record.id + "@chromium-extension",
        );
        if (!extension) throw Error("Actual installed extension is missing");
        item.background = extension.manifest.background;
        if (["Enhancer for YouTube", "SponsorBlock"].includes(record.name)) {
          await pause(500);
          item.onboardingTabs = [...win.gBrowser.tabs].filter((candidate) =>
            candidate !== tab && !originalTabs.has(candidate)
          ).map((candidate) => ({
            url: candidate.linkedBrowser.currentURI.spec,
          }));
          for (const candidate of [...win.gBrowser.tabs]) {
            if (candidate !== tab && !originalTabs.has(candidate)) {
              win.gBrowser.removeTab(candidate, { skipPermitUnload: true });
            }
          }
        }
        win.gBrowser.selectedTab = tab;
        await navigate(tab);
        if (record.name === "Enhancer for YouTube") {
          item.playing = await readyAndPlay(tab);
          await query(tab, "ControlSpeed");
          item.transformed = await until(async () => {
            const value = await query(tab, "Snapshot");
            item.lastSnapshot = value;
            return value.video &&
              Math.abs(value.video.playbackRate - 1.02) < 0.00001 &&
              value.bezel === "1.02x" && value;
          }, "actual native playback speed changed by signed extension");
          item.checks.push(
            "Native Ctrl-wheel invokes unchanged MAIN and ISOLATED extension code and changes actual video playback rate with visible feedback",
          );
          await navigate(tab, url + "?second");
          await readyAndPlay(tab);
          await query(tab, "ControlSpeed");
          item.afterNavigation = await until(async () => {
            const value = await query(tab, "Snapshot");
            return value.video &&
              Math.abs(value.video.playbackRate - 1.02) < 0.00001 &&
              value.bezel === "1.02x" && value;
          }, "actual native control after new-document navigation");
          item.checks.push(
            "New-document navigation retains functioning native player controls",
          );
        } else if (record.name === "Return YouTube Dislike") {
          item.transformed = await until(async () => {
            const value = await query(tab, "Snapshot");
            item.lastSnapshot = value;
            return value.dislike?.includes("123") && value;
          }, "real metadata rendered by unchanged signed extension");
          item.checks.push(
            "Native extension TLS transport renders authored dislike metadata into actual document controls",
          );
          await navigate(tab, url + "&second");
          item.afterNavigation = await until(async () => {
            const value = await query(tab, "Snapshot");
            return value.dislike?.includes("123") && value;
          }, "native metadata after new-document navigation");
          item.checks.push(
            "Actual navigation retains functioning metadata rendering",
          );
        } else if (record.name === "SponsorBlock") {
          verifyRemoval = await runSponsorFloorp({
            tab,
            item,
            url,
            window: win,
            extensionBaseURL: extension.baseURI.spec,
            query: (name, data) => query(tab, name, data),
            until,
            navigate: (next) => navigate(tab, next),
            pause,
          });
        } else throw Error("Primary recipe not implemented for " + record.name);
        await addon.uninstall();
        addon = null;
        await navigate(tab);
        if (record.name === "Enhancer for YouTube") {
          await readyAndPlay(tab);
          await query(tab, "ControlSpeed");
          await pause(200);
        }
        if (verifyRemoval) await verifyRemoval();
        item.afterRemoval = await query(tab, "Snapshot");
        if (
          record.name === "Enhancer for YouTube"
            ? item.afterRemoval.bezel !== undefined ||
              item.afterRemoval.video?.playbackRate !== 1
            : item.afterRemoval.dislike !== item.baseline.dislike
        ) {
          throw Error("Uninstall did not restore authored native controls");
        }
        item.checks.push("Actual uninstall restores authored native controls");
        item.primaryOperationResult = "passed";
      } catch (error) {
        if (item.primaryOperationResult !== "harness-fixture-unavailable") {
          item.primaryOperationResult = "failed";
        }
        item.error = String(error);
        console.error("[Media corpus]", record.name, error);
      } finally {
        await addon?.uninstall();
        for (const tab of [...win.gBrowser.tabs]) {
          if (!originalTabs.has(tab)) {
            win.gBrowser.removeTab(tab, { skipPermitUnload: true });
          }
        }
        await save();
      }
    }
  } finally {
    Services.prefs.setStringPref("network.dns.localDomains", originalDns);
    Services.prefs.setIntPref("network.proxy.type", originalProxy);
    ChromeUtils.unregisterWindowActor("CorpusMedia");
    resource.setSubstitution("floorp-media-corpus", null);
    await save();
  }
  if (
    !report.extensions.length ||
    report.extensions.some((item) => item.primaryOperationResult !== "passed")
  ) {
    throw Error(
      "Media corpus primary failed; see " +
        PathUtils.join(output, "report.json"),
    );
  }
}
