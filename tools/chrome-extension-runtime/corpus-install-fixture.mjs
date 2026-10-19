// SPDX-License-Identifier: MPL-2.0
import { unzipSync } from "fflate/browser";
import { CRXConverterClass } from "../../browser-features/modules/modules/chrome-web-store/CRXConverter.sys.mts";
const { AddonManager } = ChromeUtils.importESModule("resource://gre/modules/AddonManager.sys.mjs");
const { FileUtils } = ChromeUtils.importESModule("resource://gre/modules/FileUtils.sys.mjs");
export const sha256 = async (bytes) => [...new Uint8Array(await crypto.subtle.digest("SHA-256", bytes))].map((byte) => byte.toString(16).padStart(2, "0")).join("");
export async function installFrozenCorpus(root, record, output) {
  const bytes = await IOUtils.read(PathUtils.join(root, "_dist", "chrome-extension-corpus", "packages", record.id + ".crx"));
  if (await sha256(bytes) !== record.packageSha256) throw Error("Frozen signed CRX hash mismatch");
  const result = await new CRXConverterClass().convert(bytes.buffer, record.id, { id: record.id, name: record.name, version: record.version });
  if (!result.success || !result.xpiData) throw Error(result.error?.message || "Production conversion failed");
  const hashes = [];
  for (const [name, bytes] of Object.entries(unzipSync(new Uint8Array(result.xpiData)))) {
    if (!/\.(?:mjs|cjs|js)$/i.test(name)) continue;
    const original = await IOUtils.read(PathUtils.join(root, "_dist", "chrome-extension-corpus", "chrome-unpacked", record.id, ...name.split("/")));
    const hash = await sha256(bytes);
    if (hash !== await sha256(original)) throw Error("Signed script changed during conversion: " + name);
    hashes.push(name + ":" + hash);
  }
  const path = PathUtils.join(output, record.id + ".xpi");
  await IOUtils.write(path, new Uint8Array(result.xpiData));
  const addon = await AddonManager.installTemporaryAddon(new FileUtils.File(path));
  return { addon, unchangedScriptCount: hashes.length, unchangedScriptsSha256: await sha256(new TextEncoder().encode(hashes.sort().join("\n"))), warnings: result.warnings };
}
export function registerCorpusOperations(root) {
  Services.io.getProtocolHandler("resource").QueryInterface(Ci.nsIResProtocolHandler).setSubstitution(
    "floorp-corpus-operations", Services.io.newFileURI(new FileUtils.File(PathUtils.join(root, "tools", "chrome-extension-runtime"))),
  );
  ChromeUtils.registerWindowActor("CorpusOperations", {
    child: { esModuleURI: "resource://floorp-corpus-operations/CorpusOperationsChild.sys.mjs" },
    allFrames: false,
    safeForUntrustedWebProcess: true,
  });
}
