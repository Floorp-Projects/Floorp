// SPDX-License-Identifier: MPL-2.0
/** @import {ContentChromeIdentityModule, ContentUserScriptsDeclaration, ContentUserScriptsConsent} from "./content-corpus-test-types.ts" */
/** @import {InstalledFrozenCorpus} from "./native-test-types.ts" */
/** @import {FrozenCorpusExtension} from "./corpus-types.ts" */
/** @import {UserScriptsUIWindow} from "../../browser-features/modules/modules/chrome-web-store/test/helpers/user-scripts-consent-test-types.ts" */

/**
 * Use only the manifest returned by the authenticated production conversion.
 * Mutable unpacked caches and extension display names do not authorize UI.
 * @param {unknown} value
 * @returns {ContentUserScriptsDeclaration|null}
 */
export function userScriptsDeclaration(value) {
  if (!value || typeof value !== "object" || Array.isArray(value)) {
    throw Error("Authenticated corpus manifest is missing");
  }
  const manifest = /** @type {Record<string,unknown>} */ (value);
  if (manifest.manifest_version !== 3) return null;
  for (const name of ["permissions", "optional_permissions"]) {
    const permissions = manifest[name];
    if (
      permissions !== undefined &&
      (!Array.isArray(permissions) ||
        permissions.some((permission) => typeof permission !== "string"))
    ) {
      throw Error("Invalid authenticated corpus manifest permissions");
    }
  }
  if (
    Array.isArray(manifest.permissions) &&
    manifest.permissions.includes("userScripts")
  ) return "required";
  if (
    Array.isArray(manifest.optional_permissions) &&
    manifest.optional_permissions.includes("userScripts")
  ) return "optional";
  return null;
}

/**
 * Only genuine browser UI changes the independent setting. An optional public
 * permission remains ungranted; callers must inspect that separately before
 * attempting a primary operation which needs userScripts.
 * @param {InstalledFrozenCorpus} installed
 * @param {FrozenCorpusExtension} record
 * @param {UserScriptsUIWindow} window
 * @returns {Promise<ContentUserScriptsConsent|null>}
 */
export async function allowFrozenCorpusUserScripts(installed, record, window) {
  const declaration = userScriptsDeclaration(installed.originalManifest);
  if (!declaration) return null;
  const { ExtensionUtils } = /** @type {ContentChromeIdentityModule} */ (
    ChromeUtils.importESModule("resource://gre/modules/ExtensionUtils.sys.mjs")
  );
  const chromeId = ExtensionUtils.getChromeExtensionId(installed.extension);
  const addonId = `${record.id}@chromium-extension`;
  if (chromeId !== record.id || installed.extension.id !== addonId) {
    throw Error(
      "Authenticated corpus identity does not authorize Chrome consent",
    );
  }
  const { setUserScriptsConsentUsingUI } = await import(
    "../../browser-features/modules/modules/chrome-web-store/test/helpers/user-scripts-consent-ui.mts"
  );
  const ui = await setUserScriptsConsentUsingUI(addonId, true, window);
  if (
    !ui.statusAfter.allowed ||
    ui.statusBefore.permissionGranted !== ui.statusAfter.permissionGranted
  ) {
    throw Error("Native consent must preserve the actual public permission");
  }
  return { declaration, originalManifestVersion: 3, chromeId, addonId, ui };
}
