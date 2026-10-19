// SPDX-License-Identifier: MPL-2.0
import { invokeNativeCorpusAction } from "./corpus-native-action.mjs";
import { unzipSync } from "fflate/browser";
import { CRXConverterClass } from "../../browser-features/modules/modules/chrome-web-store/CRXConverter.sys.mts";
const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs",
);
const { AddonManager } = ChromeUtils.importESModule(
  "resource://gre/modules/AddonManager.sys.mjs",
);
const { ExtensionParent } =
  /** @type {{ExtensionParent:import("./native-test-types.ts").NativeCorpusExtensionParent}} */ (ChromeUtils
    .importESModule(
      "resource://gre/modules/ExtensionParent.sys.mjs",
    ));

const { FileUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/FileUtils.sys.mjs",
);
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
/** @param {number} ms */
const pause = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
/** @param {Uint8Array} bytes */
const digest = async (bytes) =>
  [
    ...new Uint8Array(
      await crypto.subtle.digest("SHA-256", new Uint8Array(bytes)),
    ),
  ].map((b) => b.toString(16).padStart(2, "0")).join("");
export async function runAllTests() {
  const root = Services.env.get("FLOORP_CORPUS_ROOT");
  const output = Services.env.get("FLOORP_BITWARDEN_OUTPUT");
  const authoredURL = Services.env.get("FLOORP_BITWARDEN_AUTHORED_URL");
  const vault =
    /** @type {import("./sidepanel-test-types.ts").VaultInfo} */ (await IOUtils
      .readJSON(PathUtils.join(output, "server.json")));
  const credentials =
    /** @type {import("./sidepanel-test-types.ts").VaultCredentials} */ (await IOUtils
      .readJSON(
        PathUtils.join(output, "private-credentials.json"),
      ));
  credentials.authoredURL = authoredURL;
  credentials.vaultURL = vault.url;
  const run = PathUtils.join(output, "floorp-primary");
  await IOUtils.makeDirectory(run);
  /** @type {import("./sidepanel-test-types.ts").VaultReport & {fillMatches?:{username:boolean;password:boolean}}} */
  const report = {
    passed: false,
    runtime: {
      version: Services.appinfo.version,
      buildId: Services.appinfo.appBuildID,
      nativeWorkerCompiled: AppConstants.MOZ_WEBEXT_WEBIDL_ENABLED,
    },
    authoredURL,
    referenceReport: Services.env.get("FLOORP_BITWARDEN_REFERENCE"),
    authoredSourceSha256: Services.env.get("FLOORP_BITWARDEN_FIXTURE_SHA"),
    server: { image: vault.image, url: vault.url },
    steps: [],
  };
  const save = () =>
    IOUtils.writeJSON(PathUtils.join(run, "report.json"), report);
  const window =
    /** @type {import("./native-test-types.ts").NativeCorpusWindow|null} */ (Services
      .wm.getMostRecentWindow("navigator:browser"));
  if (!window) throw Error("Actual native browser window unavailable");
  const tabsBefore = new Set(window.gBrowser.tabs);
  const handler = Services.io.getProtocolHandler("resource");
  if (!handler?.QueryInterface) {
    throw Error("Native resource protocol unavailable");
  }
  const resource = handler.QueryInterface(Ci.nsIResProtocolHandler);
  const directory = new FileUtils.File(
    Services.env.get("FLOORP_TEST_ACTOR_DIRECTORY"),
  );
  resource.setSubstitution(
    "floorp-bitwarden-tests",
    Services.io.newFileURI(directory),
  );
  ChromeUtils.registerWindowActor("CorpusBitwarden", {
    child: {
      esModuleURI:
        "resource://floorp-bitwarden-tests/CorpusBitwardenChild.sys.mjs",
    },
    safeForUntrustedWebProcess: true,
    allFrames: false,
  });
  const cert = await IOUtils.readUTF8(
    PathUtils.join(output, "data", "tls", "ca.pem"),
  );
  Cc["@mozilla.org/security/x509certdb;1"].getService(Ci.nsIX509CertDB)
    .addCertFromBase64(
      cert.replace(/-----[^-]+-----|\s/g, ""),
      "C,,",
    );
  const originalDns = Services.prefs.getStringPref(
    "network.dns.localDomains",
    "",
  );
  Services.prefs.setStringPref(
    "network.dns.localDomains",
    [originalDns, "corpus.floorp.test"].filter(Boolean).join(","),
  );
  const originalProxy = Services.prefs.getIntPref("network.proxy.type", 0);
  Services.prefs.setIntPref("network.proxy.type", 0);
  /** @type {import("./native-test-types.ts").NativeCorpusAddon|undefined} */
  let addon;
  /** @type {import("./native-test-types.ts").NativeCorpusExtension|undefined} */
  let extension;
  /** @type {import("./native-test-types.ts").NativeCorpusTab|undefined} */
  let author;
  /** @type {import("./native-test-types.ts").NativeCorpusBrowser|undefined} */
  let popup;
  /** @template T @param {import("./native-test-types.ts").NativeCorpusBrowser|undefined} browser @param {string} name @param {object} [data] @returns {Promise<T>} */
  const query = (browser, name, data = {}) => {
    if (!browser?.browsingContext?.currentWindowGlobal) {
      return Promise.reject(Error("Native extension view is closed"));
    }
    return /** @type {Promise<T>} */ (Promise.race([
      browser.browsingContext.currentWindowGlobal.getActor("CorpusBitwarden")
        .sendQuery(name, data),
      pause(15000).then(() => {
        throw Error("Private test actor query timed out: " + name);
      }),
    ]));
  };
  const findPopup = () =>
    [...(extension?.views || [])].find((view) => view.viewType === "popup")
      ?.xulBrowser;
  const action = async () => {
    if (!extension || !author) {
      throw Error("Native Bitwarden action target missing");
    }
    report.nativeAction = await invokeNativeCorpusAction(
      extension,
      window,
      author,
    );
    for (let n = 0; n < 100; n++) {
      popup = findPopup();
      if (popup?.browsingContext?.currentWindowGlobal) break;
      await pause(100);
    }
    if (!popup?.browsingContext?.currentWindowGlobal) {
      throw Error("Actual native Bitwarden action popup unavailable");
    }
  };
  try {
    const id = "nngceckbapebfimnlniiiahkandclblb";
    const corpus =
      /** @type {import("./corpus-types.ts").FrozenCorpus} */ (await IOUtils
        .readJSON(
          PathUtils.join(
            root,
            "tools",
            "chrome-extension-runtime",
            "corpus-lock.json",
          ),
        ));
    const frozen = corpus.extensions.find((record) => record.id === id);
    if (!frozen?.version) throw Error("Frozen Bitwarden package missing");
    const bytes = await IOUtils.read(
      PathUtils.join(
        root,
        "_dist",
        "chrome-extension-corpus",
        "packages",
        `${id}.crx`,
      ),
    );
    if (await digest(bytes) !== frozen.packageSha256) {
      throw Error("Frozen Bitwarden package mismatch");
    }
    const conversion = await new CRXConverterClass().convert(
      Uint8Array.from(bytes).buffer,
      id,
      {
        id,
        name: frozen.name,
        version: frozen.version,
      },
    );
    if (!conversion.success || !conversion.xpiData) {
      throw Error(conversion.error?.message || "Conversion failed");
    }
    report.extension = {
      id,
      version: frozen.version,
      packageSha256: frozen.packageSha256,
      warnings: conversion.warnings,
      background: conversion.report?.background,
    };
    const scripts = [];
    for (
      const [name, bytes] of Object.entries(
        unzipSync(new Uint8Array(conversion.xpiData)),
      )
    ) {
      if (!/\.(?:js|mjs|cjs)$/i.test(name)) continue;
      const hash = await digest(bytes);
      if (
        hash !==
          await digest(
            await IOUtils.read(
              PathUtils.join(
                root,
                "_dist",
                "chrome-extension-corpus",
                "chrome-unpacked",
                id,
                ...name.split("/"),
              ),
            ),
          )
      ) throw Error("Vendor script changed: " + name);
      scripts.push(name + ":" + hash);
    }
    report.unchangedScriptCount = scripts.length;
    report.unchangedScriptsSha256 = await digest(
      new TextEncoder().encode(scripts.sort().join("\n")),
    );
    const xpi = PathUtils.join(run, "bitwarden.xpi");
    await IOUtils.write(xpi, new Uint8Array(conversion.xpiData));
    addon = await AddonManager.installTemporaryAddon(new FileUtils.File(xpi));
    extension = ExtensionParent.GlobalManager.extensionMap.get(
      id + "@chromium-extension",
    );
    if (!extension) throw Error("Native installed Bitwarden missing");
    await pause(6500);
    for (const tab of [...window.gBrowser.tabs]) {
      if (!tabsBefore.has(tab)) {
        window.gBrowser.removeTab(tab, { skipPermitUnload: true });
      }
    }
    author = window.gBrowser.addTab(authoredURL, {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
    window.gBrowser.selectedTab = author;
    for (let n = 0; n < 100; n++) {
      try {
        /** @type {import("./sidepanel-test-types.ts").FilledValues} */
        const values = await query(author.linkedBrowser, "ReadFilled");
        report.authorFixtureProbe = {
          secure: values.secure,
          usernamePresent: typeof values.username === "string",
          passwordPresent: typeof values.password === "string",
          url: author.linkedBrowser.currentURI.spec,
        };
        if (values.secure && values.username === "" && values.password === "") {
          report.initialFieldsEmpty = true;
          break;
        }
      } catch (error) {
        report.authorFixtureProbe = {
          error: String(error),
          url: author.linkedBrowser.currentURI.spec,
        };
      }
      await pause(100);
    }
    if (!report.initialFieldsEmpty) {
      report.authorFixtureUI = await query(author.linkedBrowser, "Snapshot")
        .catch((error) => ({ error: String(error) }));
      throw Error("Authored HTTPS fixture unavailable or not initially empty");
    }
    await action();
    await pause(2500);
    await IOUtils.writeJSON(
      PathUtils.join(run, "ui.json"),
      await query(popup, "Snapshot"),
    );
    report.ready = true;
    await save();
    let handled = 0;
    while (!report.closed) {
      const commands =
        /** @type {import("./sidepanel-test-types.ts").ProbeCommand[]} */ (await IOUtils
          .readJSON(
            PathUtils.join(run, "commands.json"),
          ).catch(() => []));
      while (handled < commands.length) {
        const command = commands[handled++];
        try {
          if (command.op === "close") {
            report.closed = true;
            break;
          }
          if (command.op === "action") await action();
          else if (command.op === "click") await query(popup, "Click", command);
          else if (command.op === "fill") {
            await query(popup, "Type", {
              ...command,
              value: command.valueFrom
                ? credentials[command.valueFrom]
                : command.text,
            });
          } else if (command.op === "verify-fill") {
            /** @type {import("./sidepanel-test-types.ts").FilledValues} */
            const values = await query(author.linkedBrowser, "ReadFilled");
            report.fillMatches = {
              username: values.username === credentials.username,
              password: values.password === credentials.password,
            };
            report.passed = report.fillMatches.username &&
              report.fillMatches.password;
          } else if (command.op !== "snapshot") throw Error("Unknown command");
          await pause(command.waitMs || 250);
          report.steps.push(command);
          report.lastError = null;
        } catch (error) {
          report.lastError = String(error);
        }
        report.handled = handled;
        const ui = await query(findPopup() || author.linkedBrowser, "Snapshot")
          .catch((error) => ({ closed: true, error: String(error) }));
        await IOUtils.writeJSON(PathUtils.join(run, "ui.json"), ui);
        await save();
      }
      await pause(100);
    }
    if (!report.passed) {
      throw Error("Actual Bitwarden primary operation was not completed");
    }
    return report;
  } catch (error) {
    report.error = String(error);
    throw error;
  } finally {
    await save();
    await addon?.uninstall();
    for (const tab of [...window.gBrowser.tabs]) {
      if (!tabsBefore.has(tab)) {
        window.gBrowser.removeTab(tab, { skipPermitUnload: true });
      }
    }
    Services.prefs.setIntPref("network.proxy.type", originalProxy);
    ChromeUtils.unregisterWindowActor("CorpusBitwarden");
    /** @type {{setSubstitution(name:string,uri:nsIURI|null):void}} */ (resource)
      .setSubstitution("floorp-bitwarden-tests", null);
    if (originalDns) {
      Services.prefs.setStringPref("network.dns.localDomains", originalDns);
    } else Services.prefs.clearUserPref("network.dns.localDomains");
  }
}
