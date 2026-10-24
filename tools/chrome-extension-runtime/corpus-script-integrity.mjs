// SPDX-License-Identifier: MPL-2.0
import { createHash } from "node:crypto";
import { readdirSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { unzipSync } from "fflate";

/** @param {string | Uint8Array} bytes */
const digest = (bytes) => createHash("sha256").update(bytes).digest("hex");
/** @param {string} name */
const script = (name) => /\.(?:mjs|cjs|js)$/i.test(name);

// The frozen digest refers to the signature-verified CRX in corpus-lock.json.
// Compare every loaded script directly with that archive on each operation run.
/**
 * @param {string} root
 * @param {import("./corpus-types.ts").FrozenCorpusExtension} record
 * @param {string} [directory]
 */
export function verifyCorpusScripts(
  root,
  record,
  directory = join(
    root,
    "_dist/chrome-extension-corpus/chrome-unpacked",
    record.id,
  ),
) {
  const bytes = readFileSync(
    join(root, "_dist/chrome-extension-corpus/packages", record.id + ".crx"),
  );
  if (
    bytes.length > 100 * 1024 * 1024 || digest(bytes) !== record.packageSha256
  ) {
    throw Error("Frozen signed CRX digest mismatch");
  }
  if (bytes.subarray(0, 4).toString() !== "Cr24") {
    throw Error("CRX header missing");
  }
  const version = bytes.readUInt32LE(4);
  const offset = version === 3
    ? 12 + bytes.readUInt32LE(8)
    : version === 2
    ? 16 + bytes.readUInt32LE(8) + bytes.readUInt32LE(12)
    : -1;
  if (offset < 12 || offset >= bytes.length) {
    throw Error("Invalid CRX ZIP offset");
  }
  let entries = 0, expanded = 0;
  const files = unzipSync(bytes.subarray(offset), {
    filter(entry) {
      entries++;
      expanded += entry.originalSize;
      if (
        entries > 10000 || entry.originalSize > 50 * 1024 * 1024 ||
        expanded > 500 * 1024 * 1024
      ) throw Error("Frozen archive expansion limit");
      if (
        !entry.name || entry.name.includes("\\") || entry.name.includes("\0") ||
        entry.name.startsWith("/") || /^[A-Za-z]:/.test(entry.name) ||
        entry.name.split("/").some((part) => part === "." || part === "..")
      ) {
        throw Error("Unsafe frozen archive path");
      }
      return script(entry.name);
    },
  });
  const expected = Object.keys(files).sort();
  /** @type {string[]} */
  const loaded = [];
  /** @param {string} path @param {string} [prefix] */
  function walk(path, prefix = "") {
    for (const entry of readdirSync(path, { withFileTypes: true })) {
      if (entry.isSymbolicLink()) throw Error("Symlink in prepared extension");
      const name = prefix + entry.name;
      if (entry.isDirectory()) walk(join(path, entry.name), name + "/");
      else if (script(name)) loaded.push(name);
    }
  }
  walk(directory);
  if (JSON.stringify(loaded.sort()) !== JSON.stringify(expected)) {
    throw Error("Prepared extension script inventory differs from signed CRX");
  }
  const hashes = expected.map((name) => {
    const hash = digest(files[name]);
    if (digest(readFileSync(join(directory, name))) !== hash) {
      throw Error("Original signed script changed: " + name);
    }
    return name + ":" + hash;
  });
  return {
    unchangedScriptCount: hashes.length,
    unchangedScriptsSha256: digest(hashes.join("\n")),
  };
}
