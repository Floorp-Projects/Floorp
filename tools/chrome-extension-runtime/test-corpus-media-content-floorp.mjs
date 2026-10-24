// SPDX-License-Identifier: MPL-2.0
/** @import {Truthy, CorpusLock, CorpusReport, CorpusOperation, MediaSnapshot, NativeWindowHandle, NativeTabHandle, ActorClickData, MediaActorResponses, ResourceProtocolHandler, MediaFixture} from "./assessment-test-types.ts" */
import { runSponsorFloorp } from "./corpus-sponsor-floorp.mjs";
import { installFrozenCorpus } from "./corpus-install-fixture.mjs";
const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
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
    nativeQualified: false,
    qualificationMode:
      Services.env.get("FLOORP_TEST_REQUIRE_NATIVE_CORPUS") === "1"
        ? "candidate"
        : "historical",
    stage: "actual-package-primary-operation",
    runtime: {
      version: Services.appinfo.version,
      buildId: Services.appinfo.appBuildID,
      nativeWorkerPolicy: WebExtensionPolicy.backgroundServiceWorkerEnabled,
      workerWebIDLCompiled: ChromeUtils.importESModule(
        "resource://gre/modules/AppConstants.sys.mjs",
      ).AppConstants.MOZ_WEBEXT_WEBIDL_ENABLED === true,
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
        nativeQualified: false,
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
        const installed = await installFrozenCorpus(root, record, output);
        addon = installed.addon;
        const extension = installed.extension;
        Object.assign(item, {
          unchangedScriptCount: installed.unchangedScriptCount,
          unchangedScriptsSha256: installed.unchangedScriptsSha256,
          sourceIntegrity: installed.sourceIntegrity,
          nativeEvidence: installed.nativeEvidence,
          nativeCandidateIdentity: installed.nativeEvidence.candidateIdentity ??
            null,
          conversion: installed.conversion,
          warnings: installed.warnings,
          background: installed.background,
        });
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
        item.nativeQualified = item.primaryOperationResult === "passed" &&
          item.nativeEvidence?.candidateIdentityVerified === true &&
          item.nativeEvidence.nativeBackgroundVerified === true;
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
  report.nativeQualified = report.extensions.length > 0 &&
    report.extensions.every((item) => item.nativeQualified === true);
  await save();
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
