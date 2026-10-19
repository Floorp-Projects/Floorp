// SPDX-License-Identifier: MPL-2.0
// Supplemental real vendor-host qualification; never part of the frozen50 score.
const { NativeManifests } = ChromeUtils.importESModule("resource://gre/modules/NativeManifests.sys.mjs");
export async function runAllTests() {
  const output = Services.env.get("FLOORP_COAPP_OUTPUT");
  if (!output) throw Error("Private vendor-host evidence directory required");
  await IOUtils.makeDirectory(output);
  const id = "lmjnegcaeklhafolokijcfjliaokphfk";
  const context = { extension: { id: `${id}@chromium-extension`, manifest: { browser_specific_settings: { gecko: { floorp_chrome_extension: { schema_version: 1, original_id: id } } } } }, manifestVersion: 3, envType: "addon_parent", url: null, jsonStringify: JSON.stringify, cloneScope: globalThis, logError() {}, preprocessors: {} };
  const report = { stage: "supplemental-official-vendor-native-host", frozenCorpusScoreContribution: 0, caller: "authored explicit Chrome-mode context; not frozen VDH v10", runtime: { version: Services.appinfo.version, buildId: Services.appinfo.appBuildID }, passed: false };
  try {
    const host = await NativeManifests.lookupManifest("stdio", "net.downloadhelper.coapp", context);
    report.lookup = host && { path: host.path, chromeOrigin: host.chromeOrigin, executable: host.manifest.path };
    if (host?.chromeOrigin !== `chrome-extension://${id}/`) throw Error("Official vendor Chrome registration was blocked by the unrelated Firefox allowlist");
    report.passed = true;
  } catch (error) { report.error = String(error); throw error; }
  finally { await IOUtils.writeJSON(PathUtils.join(output, "report.json"), report); console.log("REPORT " + PathUtils.join(output, "report.json")); }
}
