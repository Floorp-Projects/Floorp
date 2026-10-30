// SPDX-License-Identifier: MPL-2.0
import { CRXConverterClass } from "../../browser-features/modules/modules/chrome-web-store/CRXConverter.sys.mts";
import {
  sha256,
  verifyFrozenCorpusScripts,
} from "./corpus-source-integrity.mjs";
import { validateNativeCorpusBackground } from "./corpus-native-validation.mjs";
export {
  sha256,
  verifyFrozenCorpusScripts,
} from "./corpus-source-integrity.mjs";
const { AddonManager } = ChromeUtils.importESModule(
  "resource://gre/modules/AddonManager.sys.mjs",
);
const { FileUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/FileUtils.sys.mjs",
);
/** @returns {import("./native-test-types.ts").NativeCorpusCandidateIdentity|undefined} */
function candidateIdentity() {
  const serialized = Services.env.get("FLOORP_TEST_NATIVE_CANDIDATE_IDENTITY");
  if (!serialized) return undefined;
  const value = /** @type {unknown} */ (JSON.parse(serialized));
  if (!value || typeof value !== "object") {
    throw Error("Invalid native candidate identity");
  }
  const identity = /** @type {Record<string,unknown>} */ (value);
  for (const name of ["manifestSha256", "packageSha256"]) {
    if (
      typeof identity[name] !== "string" ||
      !/^[0-9a-f]{64}$/.test(identity[name])
    ) {
      throw Error("Invalid native candidate identity digest");
    }
  }
  for (const name of ["sourceCommit", "floorpCommit"]) {
    if (
      typeof identity[name] !== "string" ||
      !/^[0-9a-f]{40}$/.test(identity[name])
    ) {
      throw Error("Invalid native candidate identity revision");
    }
  }
  if (
    typeof identity.buildId !== "string" || !identity.buildId ||
    typeof identity.platform !== "string" || !identity.platform ||
    !identity.patches || typeof identity.patches !== "object" ||
    !Object.entries(identity.patches).length ||
    Object.values(identity.patches).some((hash) =>
      typeof hash !== "string" || !/^[0-9a-f]{64}$/.test(hash)
    )
  ) {
    throw Error("Invalid native candidate identity metadata");
  }
  return /** @type {import("./native-test-types.ts").NativeCorpusCandidateIdentity} */ (value);
}

/**
 * The Python candidate wrapper authenticates the package and running build.
 * This additionally observes the actual installed extension's native context.
 * Ordinary Gecko's global worker preference deliberately need not be enabled.
 * @param {import("./native-test-types.ts").NativeCorpusExtension} extension
 * @param {import("./native-test-types.ts").NativeCorpusBackgroundInput} input
 * @returns {Promise<import("./native-test-types.ts").NativeCorpusBackgroundEvidence>}
 */
export async function assertNativeCorpusBackground(extension, input) {
  const { AppConstants } = ChromeUtils.importESModule(
    "resource://gre/modules/AppConstants.sys.mjs",
  );
  const { setTimeout } = ChromeUtils.importESModule(
    "resource://gre/modules/Timer.sys.mjs",
  );
  const strictNative =
    Services.env.get("FLOORP_TEST_REQUIRE_NATIVE_CORPUS") === "1";
  const identity = strictNative ? candidateIdentity() : undefined;
  if (
    strictNative &&
    (input.originalServiceWorker || input.convertedServiceWorker ||
      input.background?.selected === "service_worker")
  ) {
    for (
      let attempt = 0;
      attempt < 100 &&
      extension.backgroundContext?.viewType !== "background_worker";
      attempt++
    ) {
      await new Promise((resolve) => setTimeout(resolve, 100));
    }
  }
  const snapshot = {
    ...input,
    runId: Services.env.get("FLOORP_TEST_RUN_ID") || null,
    strictNative,
    version: Services.appinfo.version,
    buildId: Services.appinfo.appBuildID,
    workerWebIDLCompiled: AppConstants.MOZ_WEBEXT_WEBIDL_ENABLED === true,
    backgroundViewType: extension.backgroundContext?.viewType ?? null,
    candidateIdentity: identity,
  };
  validateNativeCorpusBackground(snapshot);
  return {
    ...snapshot,
    nativeQualified: false,
    candidateIdentityVerified: strictNative,
    nativeBackgroundVerified: strictNative,
    buildScope: strictNative
      ? "strict-native-candidate"
      : "historical-nonqualifying",
    nativeWorkerVerified: !!input.originalServiceWorker &&
      snapshot.backgroundViewType === "background_worker",
  };
}
/**
 * @param {string} root
 * @param {import("./corpus-types.ts").FrozenCorpusExtension} record
 * @param {string} output
 * @param {{beforeInstall?:()=>Promise<void>}} [options]
 * @returns {Promise<import("./native-test-types.ts").InstalledFrozenCorpus>}
 */
export async function installFrozenCorpus(root, record, output, options = {}) {
  if (!record.version || !record.packageSha256) {
    throw Error("Frozen package metadata is incomplete");
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
  if (await sha256(bytes) !== record.packageSha256) {
    throw Error("Frozen signed CRX hash mismatch");
  }
  const result = await new CRXConverterClass().convert(
    new Uint8Array(bytes).buffer,
    record.id,
    { id: record.id, name: record.name, version: record.version },
  );
  if (!result.success || !result.xpiData || !result.originalManifest) {
    throw Error(result.error?.message || "Production conversion failed");
  }
  const sourceIntegrity = await verifyFrozenCorpusScripts(
    bytes,
    result.xpiData,
    record,
  );
  const path = PathUtils.join(output, record.id + ".xpi");
  await IOUtils.write(path, new Uint8Array(result.xpiData));
  await options.beforeInstall?.();
  const addon = await AddonManager.installTemporaryAddon(
    new FileUtils.File(path),
  );
  try {
    const { ExtensionParent } =
      /** @type {{ExtensionParent:import("./native-test-types.ts").NativeCorpusExtensionParent}} */ (
        ChromeUtils.importESModule(
          "resource://gre/modules/ExtensionParent.sys.mjs",
        )
      );
    const extension = ExtensionParent.GlobalManager.extensionMap.get(
      `${record.id}@chromium-extension`,
    );
    if (!extension) throw Error("Installed native corpus extension missing");
    const nativeEvidence = await assertNativeCorpusBackground(extension, {
      background: result.report?.background,
      originalServiceWorker: sourceIntegrity.originalServiceWorker,
      convertedServiceWorker: sourceIntegrity.convertedServiceWorker,
    });
    return {
      addon,
      unchangedScriptCount: sourceIntegrity.unchangedScriptCount,
      unchangedScriptsSha256: sourceIntegrity.unchangedScriptsSha256,
      warnings: result.warnings,
      background: result.report?.background,
      conversion: result.report,
      originalManifest: result.originalManifest,
      sourceIntegrity,
      nativeEvidence,
      extension,
    };
  } catch (error) {
    await addon.uninstall();
    throw error;
  }
}
/** @param {string} root */
export function registerCorpusOperations(root) {
  const handler = Services.io.getProtocolHandler("resource");
  if (!handler.QueryInterface) {
    throw Error("Native resource protocol interface unavailable");
  }
  handler.QueryInterface(Ci.nsIResProtocolHandler).setSubstitution(
    "floorp-corpus-operations",
    Services.io.newFileURI(
      new FileUtils.File(
        Services.env.get("FLOORP_TEST_ACTOR_DIRECTORY") ||
          PathUtils.join(root, "tools", "chrome-extension-runtime"),
      ),
    ),
  );
  ChromeUtils.registerWindowActor("CorpusOperations", {
    child: {
      esModuleURI:
        "resource://floorp-corpus-operations/CorpusOperationsChild.sys.mjs",
    },
    allFrames: false,
    safeForUntrustedWebProcess: true,
  });
}
