// SPDX-License-Identifier: MPL-2.0
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
const { AddonManager } = ChromeUtils.importESModule(
  "resource://gre/modules/AddonManager.sys.mjs",
);
const { ExtensionParent } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionParent.sys.mjs",
);
const { FileUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/FileUtils.sys.mjs",
);
export async function runAllTests() {
  const root = Services.env.get("FLOORP_HEADERS_ROOT");
  const output = Services.env.get("FLOORP_HEADERS_OUTPUT");
  const origin = Services.env.get("FLOORP_HEADERS_ORIGIN");
  const fixture = PathUtils.join(output, "fixture");
  await IOUtils.makeDirectory(fixture);
  const id = "d".repeat(32);
  await IOUtils.writeJSON(PathUtils.join(fixture, "manifest.json"), {
    manifest_version: 3,
    name: "Authored native response-header fixture",
    version: "1",
    permissions: ["declarativeNetRequest", "declarativeNetRequestFeedback"],
    host_permissions: ["<all_urls>"],
    browser_specific_settings: {
      gecko: {
        id: id + "@chromium-extension",
        floorp_chrome_extension: { schema_version: 1, original_id: id },
      },
    },
  });
  await IOUtils.writeUTF8(
    PathUtils.join(fixture, "probe.html"),
    '<!doctype html><title>waiting</title><script type="module" src="probe.mjs"></script>',
  );
  await IOUtils.copy(
    PathUtils.join(
      root,
      "tools",
      "chrome-extension-runtime",
      "ResponseHeadersVectors.mjs",
    ),
    PathUtils.join(fixture, "vectors.mjs"),
  );
  await IOUtils.writeUTF8(
    PathUtils.join(fixture, "probe.mjs"),
    `import {runResponseHeaderVectors} from './vectors.mjs';try{document.title=JSON.stringify({passed:true,...await runResponseHeaderVectors(chrome,${JSON.stringify(origin)})});}catch(error){document.title=JSON.stringify({passed:false,error:String(error),checks:error.checks,observations:error.observations});}`,
  );
  let addon, tab;
  const window =
    /** @type {import("./network-test-types.ts").NativeBrowserWindow | null} */ (
      Services.wm.getMostRecentWindow("navigator:browser")
    );
  if (!window) throw Error("Native browser window missing");
  /** @type {import("./network-test-types.ts").HeaderReport} */
  const report = {
    passed: false,
    origin,
    runtime: {
      version: Services.appinfo.version,
      buildId: Services.appinfo.appBuildID,
    },
  };
  try {
    addon = await AddonManager.installTemporaryAddon(
      new FileUtils.File(fixture),
    );
    const extension = ExtensionParent.GlobalManager.getExtension(addon.id);
    tab = window.gBrowser.addTab(extension.baseURI.resolve("probe.html"), {
      triggeringPrincipal: extension.principal,
    });
    window.gBrowser.selectedTab = tab;
    const deadline = Date.now() + 60000;
    while (Date.now() < deadline) {
      try {
        Object.assign(report, JSON.parse(tab.linkedBrowser.contentTitle));
        break;
      } catch {
        /* The owned extension has not published its JSON title yet. */
      }
      await new Promise((resolve) => setTimeout(resolve, 100));
    }
    if (!report.passed)
      throw Error(report.error || "Native header fixture timed out");
    window.gBrowser.removeTab(tab, { skipPermitUnload: true });
    tab = null;
    await addon.uninstall();
    addon = null;
    const ordinary = PathUtils.join(output, "ordinary");
    await IOUtils.makeDirectory(ordinary);
    await IOUtils.writeJSON(PathUtils.join(ordinary, "manifest.json"), {
      manifest_version: 3,
      name: "Ordinary Firefox response header isolation",
      version: "1",
      permissions: ["declarativeNetRequest"],
      host_permissions: ["<all_urls>"],
      browser_specific_settings: {
        gecko: { id: "ordinary-response-headers@floorp.test" },
      },
    });
    await IOUtils.writeUTF8(
      PathUtils.join(ordinary, "probe.html"),
      '<!doctype html><title>waiting</title><script src="probe.js"></script>',
    );
    await IOUtils.writeUTF8(
      PathUtils.join(ordinary, "probe.js"),
      `(async()=>{let rejected=false;try{await browser.declarativeNetRequest.updateDynamicRules({addRules:[{id:1,condition:{responseHeaders:[{header:'x-probe'}]},action:{type:'block'}}]});}catch(error){rejected=true;}document.title=JSON.stringify({rejected,stored:(await browser.declarativeNetRequest.getDynamicRules()).length});})()`,
    );
    addon = await AddonManager.installTemporaryAddon(
      new FileUtils.File(ordinary),
    );
    const ordinaryExtension = ExtensionParent.GlobalManager.getExtension(
      addon.id,
    );
    tab = window.gBrowser.addTab(
      ordinaryExtension.baseURI.resolve("probe.html"),
      { triggeringPrincipal: ordinaryExtension.principal },
    );
    const ordinaryDeadline = Date.now() + 15000;
    while (Date.now() < ordinaryDeadline) {
      try {
        report.ordinary = JSON.parse(tab.linkedBrowser.contentTitle);
        break;
      } catch {
        /* The owned extension has not published its JSON title yet. */
      }
      await new Promise((resolve) => setTimeout(resolve, 100));
    }
    if (!report.ordinary?.rejected || report.ordinary.stored !== 0)
      throw Error("Ordinary Firefox response header gate failed");
    (report.checks ??= []).push(
      "Ordinary Firefox extension rejects Chrome response-header conditions without installing partial rules",
    );
  } catch (error) {
    report.passed = false;
    report.error = String(error);
    throw error;
  } finally {
    if (tab) window.gBrowser.removeTab(tab, { skipPermitUnload: true });
    await addon?.uninstall();
    await IOUtils.writeJSON(PathUtils.join(output, "report.json"), report);
    console.log(`[Response header compatibility] ${output}/report.json`);
  }
}
