// SPDX-License-Identifier: MPL-2.0
import { unzipSync } from "fflate/browser";
import { extractZipFromCRX } from "../../browser-features/modules/modules/chrome-web-store/CRXParser.sys.mts";
import {
  MAX_CRX_ENTRIES,
  MAX_CRX_SIZE,
  MAX_EXPANDED_CRX_SIZE,
  MAX_FILE_SIZE,
} from "../../browser-features/modules/modules/chrome-web-store/Constants.sys.mts";

/** @param {Uint8Array} bytes */
export const sha256 = async (bytes) =>
  [
    ...new Uint8Array(
      await crypto.subtle.digest("SHA-256", new Uint8Array(bytes)),
    ),
  ]
    .map((byte) => byte.toString(16).padStart(2, "0")).join("");

/** @param {Uint8Array} bytes */
function archiveScripts(bytes) {
  let count = 0, expanded = 0;
  const names = new Set();
  return unzipSync(bytes, {
    filter(entry) {
      count++;
      expanded += entry.originalSize;
      const name = entry.name;
      if (
        count > MAX_CRX_ENTRIES || entry.originalSize > MAX_FILE_SIZE ||
        expanded > MAX_EXPANDED_CRX_SIZE
      ) {
        throw Error("Frozen corpus archive exceeds expansion limits");
      }
      if (
        !name || name.includes("\\") || name.includes("\0") ||
        name.startsWith("/") || /^[A-Za-z]:/.test(name) ||
        name.split("/").some((part) => part === "." || part === "..") ||
        names.has(name)
      ) {
        throw Error("Unsafe or duplicate frozen corpus archive path: " + name);
      }
      names.add(name);
      return name === "manifest.json" || /\.(?:mjs|cjs|js)$/i.test(name);
    },
  });
}

/** @param {Uint8Array|undefined} bytes @returns {string|null} */
function serviceWorker(bytes) {
  if (!bytes) throw Error("Frozen corpus manifest missing");
  const manifest =
    /** @type {unknown} */ (JSON.parse(new TextDecoder().decode(bytes)));
  if (!manifest || typeof manifest !== "object") {
    throw Error("Invalid frozen corpus manifest");
  }
  const background = /** @type {{background?:unknown}} */ (manifest).background;
  const worker = background && typeof background === "object"
    ? /** @type {{service_worker?:unknown}} */ (background).service_worker
    : undefined;
  if (worker !== undefined && (typeof worker !== "string" || !worker)) {
    throw Error("Invalid frozen corpus service worker path");
  }
  return typeof worker === "string" ? worker : null;
}

/**
 * Conversion separately authenticates the CRX signature. This check binds the
 * exact original archive's complete script inventory to the installed output;
 * no prepared extraction directory participates in the comparison.
 * @param {Uint8Array} crxBytes
 * @param {ArrayBuffer} xpiData
 * @param {Pick<import("./corpus-types.ts").FrozenCorpusExtension,"packageSha256">} record
 * @returns {Promise<import("./native-test-types.ts").FrozenCorpusScriptEvidence>}
 */
export async function verifyFrozenCorpusScripts(crxBytes, xpiData, record) {
  if (
    !record.packageSha256 || crxBytes.byteLength > MAX_CRX_SIZE ||
    await sha256(crxBytes) !== record.packageSha256
  ) {
    throw Error("Frozen signed CRX hash mismatch");
  }
  const zip = extractZipFromCRX(new Uint8Array(crxBytes).buffer);
  if (!zip) throw Error("Frozen CRX ZIP extraction failed");
  const original = archiveScripts(new Uint8Array(zip));
  const converted = archiveScripts(new Uint8Array(xpiData));
  const scriptNames = (/** @type {Record<string,Uint8Array>} */ files) =>
    Object.keys(files).filter((name) => /\.(?:mjs|cjs|js)$/i.test(name)).sort();
  const expected = scriptNames(original), actual = scriptNames(converted);
  if (JSON.stringify(expected) !== JSON.stringify(actual)) {
    throw Error("Converted script inventory differs from frozen signed CRX");
  }
  const hashes = [];
  for (const name of expected) {
    const before = original[name], after = converted[name];
    if (
      before.length !== after.length ||
      before.some((byte, index) => byte !== after[index])
    ) {
      throw Error("Original signed script changed during conversion: " + name);
    }
    hashes.push(name + ":" + await sha256(before));
  }
  return {
    unchangedScriptCount: hashes.length,
    unchangedScriptsSha256: await sha256(
      new TextEncoder().encode(hashes.join("\n")),
    ),
    originalServiceWorker: serviceWorker(original["manifest.json"]),
    convertedServiceWorker: serviceWorker(converted["manifest.json"]),
  };
}
