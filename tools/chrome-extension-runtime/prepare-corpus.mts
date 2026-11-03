// SPDX-License-Identifier: MPL-2.0
// Authenticate frozen packages again before extracting an unchanged-script Chrome fixture.
import { unzipSync } from "fflate/browser";
import { encodeBase64 } from "@std/encoding/base64";
import { verifyCRX } from "../../browser-features/modules/modules/chrome-web-store/CRXVerifier.sys.mts";
import {
  MAX_CRX_ENTRIES,
  MAX_EXPANDED_CRX_SIZE,
  MAX_FILE_SIZE,
} from "../../browser-features/modules/modules/chrome-web-store/Constants.sys.mts";
import type { FrozenCorpus } from "./corpus-types.ts";

const root = new URL("../../", import.meta.url);
const directory = new URL("_dist/chrome-extension-corpus/", root);
const lock = JSON.parse(
  await Deno.readTextFile(new URL("corpus-lock.json", import.meta.url)),
) as FrozenCorpus;
const hash = async (bytes: Uint8Array<ArrayBuffer>) =>
  [...new Uint8Array(await crypto.subtle.digest("SHA-256", bytes))].map(
    (value) => value.toString(16).padStart(2, "0"),
  ).join("");

// Only used after production proof verification. Select the exact authenticated
// developer key by its frozen SHA256, preserving the Store ID for loadUnpacked.
function fields(bytes: Uint8Array): { number: number; bytes: Uint8Array }[] {
  let offset = 0;
  const integer = () => {
    let value = 0, multiplier = 1;
    for (let count = 0; count < 8 && offset < bytes.length; count++) {
      const byte = bytes[offset++];
      value += (byte & 127) * multiplier;
      if (!(byte & 128)) {
        if (!Number.isSafeInteger(value)) {
          throw new Error("Oversized protobuf integer");
        }
        return value;
      }
      multiplier *= 128;
    }
    throw new Error("Invalid protobuf integer");
  };
  const result = [];
  while (offset < bytes.length) {
    const tag = integer(), wire = tag % 8, number = Math.floor(tag / 8);
    if (wire === 0) {
      integer();
      continue;
    }
    if (wire === 1 || wire === 5) {
      offset += wire === 1 ? 8 : 4;
      if (offset > bytes.length) throw new Error("Truncated protobuf field");
      continue;
    }
    if (wire !== 2) throw new Error("Unsupported protobuf wire type");
    const length = integer(), end = offset + length;
    if (end > bytes.length) throw new Error("Truncated protobuf bytes");
    result.push({ number, bytes: bytes.subarray(offset, end) });
    offset = end;
  }
  return result;
}
async function developerKey(bytes: Uint8Array<ArrayBuffer>, digest: string) {
  const view = new DataView(bytes.buffer);
  const keys = view.getUint32(4, true) === 2
    ? [bytes.subarray(16, 16 + view.getUint32(8, true))]
    : fields(bytes.subarray(12, 12 + view.getUint32(8, true)))
      .filter((field) => field.number === 2 || field.number === 3)
      .flatMap((field) =>
        fields(field.bytes).filter((key) => key.number === 1).map((key) =>
          key.bytes
        )
      );
  for (const key of keys) {
    const owned = new Uint8Array(key);
    if (await hash(owned) === digest) return encodeBase64(owned);
  }
  throw new Error("Authenticated developer key missing");
}

for (const record of lock.extensions) {
  if (record.status !== "frozen") continue;
  const bytes = await Deno.readFile(
    new URL(`packages/${record.id}.crx`, directory),
  );
  if (await hash(bytes) !== record.packageSha256) {
    throw new Error(`${record.name}: frozen package hash mismatch`);
  }
  const verified = await verifyCRX(bytes.buffer, record.id);
  if (verified.publicKeySha256 !== record.signature?.publicKeySha256) {
    throw new Error("Frozen signing identity mismatch");
  }
  let expanded = 0, count = 0;
  const files = unzipSync(new Uint8Array(verified.zipData), {
    filter(entry) {
      count++;
      expanded += entry.originalSize;
      if (
        count > MAX_CRX_ENTRIES || entry.originalSize > MAX_FILE_SIZE ||
        expanded > MAX_EXPANDED_CRX_SIZE
      ) {
        throw new Error("Frozen archive exceeds expansion bounds");
      }
      const parts = entry.name.split("/");
      if (
        !entry.name || entry.name.includes("\\") || entry.name.includes("\0") ||
        entry.name.startsWith("/") ||
        parts.some((part) => part === "." || part === "..") ||
        /^[A-Za-z]:/.test(entry.name)
      ) throw new Error("Unsafe frozen archive path");
      return !entry.name.endsWith("/");
    },
  });
  const manifest = JSON.parse(new TextDecoder().decode(files["manifest.json"]));
  if (manifest.version !== record.version) {
    throw new Error("Frozen extension version mismatch");
  }
  manifest.key = await developerKey(bytes, verified.publicKeySha256);
  // Content and background scripts remain the exact signed bytes. Chrome's
  // loadUnpacked needs only the original public key added to manifest.json.
  files["manifest.json"] = new Uint8Array(new TextEncoder().encode(
    JSON.stringify(manifest, null, 2),
  ));
  const target = new URL(`chrome-unpacked/${record.id}/`, directory);
  for (const [name, contents] of Object.entries(files)) {
    const file = new URL(
      name.split("/").map(encodeURIComponent).join("/"),
      target,
    );
    await Deno.mkdir(new URL(".", file), { recursive: true });
    await Deno.writeFile(file, contents, { mode: 0o600 });
  }
  console.log(
    `PREPARED ${record.name} ${record.version} ${count} entries ${expanded} bytes`,
  );
}
