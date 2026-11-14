// SPDX-License-Identifier: MPL-2.0
import { inflateRawSync } from "node:zlib";
import type {
  QaArchiveFailureCode,
  QaArchiveRequest,
  QaArchiveResult,
  QaArchiveScope,
  QaInflateInfo,
  QaZipEntry,
} from "./fdh_final_package_qa_archive_types.ts";

const MIB = 1024 * 1024;
export const QA_ARCHIVE_LIMITS = Object.freeze({
  archiveBytes: 8 * MIB,
  expandedBytes: 16 * MIB,
  extraBytes: 8 * MIB,
  jsonBytes: MIB,
  entries: 64,
  pathBytes: 240,
  jsonDepth: 64,
});
const SCOPE: QaArchiveScope = {
  scope: "archive-byte-integrity-only",
  publicationAuthorized: false,
  apiAuthenticityVerified: false,
  nativeQaVerified: false,
  liveApprovalVerified: false,
};
const REQUIRED = ["input.json", "source-after-qa.json"];
// Read the native element kind, not a spoofable prototype/toStringTag property.
const TYPED_ARRAY_KIND = Object.getOwnPropertyDescriptor(
  Object.getPrototypeOf(Uint8Array.prototype),
  Symbol.toStringTag,
)?.get;
class ArchiveError extends Error {
  constructor(readonly code: QaArchiveFailureCode) {
    super(code);
  }
}
function fail(code: QaArchiveFailureCode): never {
  throw new ArchiveError(code);
}
const CRC_TABLE = Uint32Array.from({ length: 256 }, (_, i) => {
  let n = i;
  for (let j = 0; j < 8; j++) n = (n >>> 1) ^ ((n & 1) ? 0xedb88320 : 0);
  return n >>> 0;
});
function crc32(bytes: Uint8Array): number {
  let crc = 0xffffffff;
  for (const byte of bytes) crc = CRC_TABLE[(crc ^ byte) & 255] ^ (crc >>> 8);
  return (crc ^ 0xffffffff) >>> 0;
}
function validIdentity(id: number, name: string): boolean {
  return Number.isSafeInteger(id) && id > 0 && typeof name === "string" &&
    name.length > 0 && name.length <= 240 && /^[a-zA-Z0-9_.-]+$/.test(name);
}
function entryLimit(name: string): number {
  return REQUIRED.includes(name)
    ? QA_ARCHIVE_LIMITS.jsonBytes
    : QA_ARCHIVE_LIMITS.extraBytes;
}

/**
 * Strict ZIP32 subset: contiguous local records, central directory, empty-comment
 * EOCD; STORE/DEFLATE, flags UTF8 and/or data descriptor only. Extra fields and
 * comments are intentionally unsupported (including ZIP64/Unicode overrides).
 * Directories, special files and all non-ASCII/ambiguous paths are refused.
 */
function parseZip(bytes: Uint8Array): QaZipEntry[] {
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const range = (at: number, size: number) => {
    if (!Number.isSafeInteger(at) || at < 0 || at + size > bytes.length) {
      fail("zip-structure");
    }
  };
  const u16 = (at: number) => {
    range(at, 2);
    return view.getUint16(at, true);
  };
  const u32 = (at: number) => {
    range(at, 4);
    return view.getUint32(at, true);
  };
  const end = bytes.length - 22;
  if (end < 0 || u32(end) !== 0x06054b50) fail("zip-structure");
  if (u16(end + 4) || u16(end + 6) || u16(end + 20)) fail("unsupported-zip");
  const count = u16(end + 10);
  const central = u32(end + 16);
  const centralSize = u32(end + 12);
  if (
    count === 0xffff || central === 0xffffffff || centralSize === 0xffffffff
  ) fail("unsupported-zip");
  if (count < 2 || count > QA_ARCHIVE_LIMITS.entries) fail("archive-limit");
  if (u16(end + 8) !== count || central + centralSize !== end) {
    fail("zip-structure");
  }
  let pos = central;
  let expanded = 0;
  const entries: QaZipEntry[] = [];
  const names = new Set<string>();
  for (let i = 0; i < count; i++) {
    range(pos, 46);
    if (u32(pos) !== 0x02014b50 || pos + 46 > end) fail("zip-structure");
    const version = u16(pos + 6), flags = u16(pos + 8), method = u16(pos + 10);
    const compressed = u32(pos + 20),
      size = u32(pos + 24),
      local = u32(pos + 42);
    const nameLength = u16(pos + 28),
      extra = u16(pos + 30),
      comment = u16(pos + 32);
    if (
      version > 20 || (flags & ~0x0808) || (method !== 0 && method !== 8) ||
      extra || comment || u16(pos + 34) || compressed === 0xffffffff ||
      size === 0xffffffff || local === 0xffffffff
    ) fail("unsupported-zip");
    const attrs = u32(pos + 38), mode = (attrs >>> 16) & 0xf000;
    if ((attrs & 0x10) || (mode !== 0 && mode !== 0x8000)) {
      fail("unsupported-zip");
    }
    if (
      !nameLength || nameLength > QA_ARCHIVE_LIMITS.pathBytes ||
      pos + 46 + nameLength > end
    ) fail("unsafe-path");
    const rawName = bytes.subarray(pos + 46, pos + 46 + nameLength);
    // A deliberately narrow portable spelling; no Unicode, colon, whitespace,
    // backslash, percent encoding, trailing dots, device aliases or dot segments.
    if (rawName.some((b) => b < 33 || b > 126)) fail("unsafe-path");
    const name = String.fromCharCode(...rawName);
    const parts = name.split("/");
    if (
      parts.some((part) =>
        !/^[a-zA-Z0-9_][a-zA-Z0-9_.-]*$/.test(part) ||
        part.endsWith(".") ||
        /^(con|prn|aux|nul|com[1-9]|lpt[1-9])(?:\.|$)/i.test(part)
      )
    ) fail("unsafe-path");
    const folded = name.toLowerCase();
    if (
      names.has(folded) ||
      [...names].some((other) =>
        other.startsWith(folded + "/") || folded.startsWith(other + "/")
      )
    ) fail("unsafe-path");
    names.add(folded);
    if (
      size > entryLimit(name) ||
      expanded + size > QA_ARCHIVE_LIMITS.expandedBytes
    ) fail("expanded-limit");
    expanded += size;
    if (method === 0 && compressed !== size) fail("zip-structure");
    entries.push({
      name,
      version,
      flags,
      method,
      time: u16(pos + 12),
      date: u16(pos + 14),
      crc: u32(pos + 16),
      compressed,
      expanded: size,
      local,
      data: 0,
    });
    pos += 46 + nameLength;
  }
  if (
    pos !== end ||
    !REQUIRED.every((name) => entries.some((e) => e.name === name))
  ) fail("zip-structure");
  entries.sort((a, b) => a.local - b.local);
  let cursor = 0;
  for (let i = 0; i < entries.length; i++) {
    const e = entries[i];
    if (
      e.local !== cursor || e.local + 30 > central ||
      u32(e.local) !== 0x04034b50
    ) fail("zip-structure");
    const at = e.local;
    const n = u16(at + 26), extra = u16(at + 28);
    if (extra) fail("unsupported-zip");
    if (
      u16(at + 4) !== e.version || u16(at + 6) !== e.flags ||
      u16(at + 8) !== e.method ||
      u16(at + 10) !== e.time || u16(at + 12) !== e.date || n !== e.name.length
    ) fail("zip-structure");
    range(at + 30, n);
    if (
      String.fromCharCode(...bytes.subarray(at + 30, at + 30 + n)) !== e.name
    ) fail("zip-structure");
    const localCrc = u32(at + 14),
      localCompressed = u32(at + 18),
      localExpanded = u32(at + 22);
    const exact = localCrc === e.crc && localCompressed === e.compressed &&
      localExpanded === e.expanded;
    const zero = localCrc === 0 && localCompressed === 0 && localExpanded === 0;
    if (!(exact || ((e.flags & 8) && zero))) fail("zip-structure");
    e.data = at + 30 + n;
    cursor = e.data + e.compressed;
    const next = i + 1 < entries.length ? entries[i + 1].local : central;
    if (cursor > next || next > central) fail("zip-structure");
    if (e.flags & 8) {
      const length = next - cursor;
      if (length !== 12 && length !== 16) fail("zip-structure");
      if (length === 16) {
        if (u32(cursor) !== 0x08074b50) fail("zip-structure");
        cursor += 4;
      }
      if (
        u32(cursor) !== e.crc || u32(cursor + 4) !== e.compressed ||
        u32(cursor + 8) !== e.expanded
      ) fail("zip-structure");
      cursor += 12;
    }
    if (cursor !== next) fail("zip-structure");
  }
  return entries;
}

function isInflateInfo(value: unknown): value is QaInflateInfo {
  if (
    !value || typeof value !== "object" || !("buffer" in value) ||
    !("engine" in value)
  ) return false;
  const engine = value.engine;
  return value.buffer instanceof Uint8Array && !!engine &&
    typeof engine === "object" &&
    "bytesWritten" in engine && typeof engine.bytesWritten === "number" &&
    Number.isSafeInteger(engine.bytesWritten) && engine.bytesWritten > 0;
}
function expand(bytes: Uint8Array, e: QaZipEntry): Uint8Array {
  const compressed = bytes.subarray(e.data, e.data + e.compressed);
  if (e.method === 0) return compressed;
  // Deno 2.9.4 processChunkSync bounds nread BEFORE another 1024-byte output
  // chunk/concat. Native start_write supplies zlib only the caller output slice.
  // Do not replace with unzipSync (attacker originalSize allocation) or an
  // Inflate callback cap (allocation occurs before callback). Fixed hard caps
  // bound output, not the entire JS/native heap: concat can transiently double it.
  try {
    const result: unknown = inflateRawSync(compressed, {
      info: true,
      chunkSize: 1024,
      maxOutputLength: Math.min(entryLimit(e.name), e.expanded) + 1,
    });
    if (
      !isInflateInfo(result) ||
      result.engine.bytesWritten !== compressed.length ||
      result.buffer.length !== e.expanded
    ) fail("compressed-data");
    return result.buffer;
  } catch {
    fail("compressed-data");
  }
}

/** Narrow workflow format: canonical JSON.stringify(value,null,2), optional LF. */
function decodeJson(bytes: Uint8Array): Record<string, unknown> {
  try {
    if (
      bytes.length >= 3 && bytes[0] === 0xef && bytes[1] === 0xbb &&
      bytes[2] === 0xbf
    ) fail("json-format");
    const text = new TextDecoder("utf-8", { fatal: true }).decode(bytes);
    let depth = 0, quoted = false, escaped = false;
    for (const char of text) {
      if (quoted) {
        if (escaped) escaped = false;
        else if (char === "\\") escaped = true;
        else if (char === '"') quoted = false;
      } else if (char === '"') quoted = true;
      else if (char === "{" || char === "[") {
        if (++depth > QA_ARCHIVE_LIMITS.jsonDepth) fail("json-format");
      } else if (char === "}" || char === "]") {
        if (--depth < 0) fail("json-format");
      }
    }
    if (depth || quoted) fail("json-format");
    const value: unknown = JSON.parse(text);
    if (!value || typeof value !== "object" || Array.isArray(value)) {
      fail("json-format");
    }
    // Also rejects duplicate/escaped-conflicting keys, alternate number spellings
    // and insignificant whitespace without inventing a security JSON parser.
    const canonical = JSON.stringify(value, null, 2);
    if (text !== canonical && text !== canonical + "\n") fail("json-format");
    return value as Record<string, unknown>;
  } catch {
    fail("json-format");
  }
}

/**
 * Pure, nonauthorizing byte-integrity boundary. Trusted metadata must be supplied
 * separately; matching id/name claims do NOT authenticate an API or a download.
 * No filesystem/network/native QA/approval/publisher operation is performed.
 */
export async function verifyFinalPackageQaArchive(
  request: QaArchiveRequest,
): Promise<QaArchiveResult> {
  try {
    const { bytes, artifact, expected } = request;
    // Read each metadata primitive once: accessors/Proxies must not substitute
    // a different claim between validation, comparison and result construction.
    const artifactId = artifact.id, artifactName = artifact.name;
    const expectedId = expected.id,
      expectedName = expected.name,
      expectedDigest = expected.digest;
    if (
      // Internal-slot check rejects even Proxies that forge prototype/properties.
      !ArrayBuffer.isView(bytes) || !TYPED_ARRAY_KIND ||
      Reflect.apply(TYPED_ARRAY_KIND, bytes, []) !== "Uint8Array" ||
      !(bytes instanceof Uint8Array) ||
      Object.getPrototypeOf(bytes) !== Uint8Array.prototype ||
      ["buffer", "length", "byteLength", "byteOffset"].some((key) =>
        Object.hasOwn(bytes, key)
      ) ||
      !(bytes.buffer instanceof ArrayBuffer) ||
      Object.getPrototypeOf(bytes.buffer) !== ArrayBuffer.prototype ||
      Object.hasOwn(bytes.buffer, "resizable") || bytes.buffer.resizable ||
      !validIdentity(artifactId, artifactName) ||
      !validIdentity(expectedId, expectedName) ||
      typeof expectedDigest !== "string" ||
      !/^sha256:[0-9a-f]{64}$/.test(expectedDigest)
    ) fail("invalid-input");
    if (bytes.length > QA_ARCHIVE_LIMITS.archiveBytes || bytes.length < 22) {
      fail("archive-limit");
    }
    if (artifactId !== expectedId || artifactName !== expectedName) {
      fail("identity-mismatch");
    }
    // Capture all claims and a private ordinary-buffer copy before the first await.
    // Shared/resizable buffers are unsupported; later caller mutation cannot alter
    // the bytes parsed after WebCrypto yields, nor the expected digest comparison.
    const identity = {
      id: expectedId,
      name: expectedName,
      digest: expectedDigest,
    };
    const snapshot = new Uint8Array(bytes);
    const hash = new Uint8Array(
      await crypto.subtle.digest("SHA-256", snapshot),
    );
    const digest = "sha256:" +
      Array.from(hash, (b) => b.toString(16).padStart(2, "0")).join("");
    if (digest !== identity.digest) fail("digest-mismatch");
    const entries = parseZip(snapshot);
    let expandedBytes = 0;
    let input: Record<string, unknown> | undefined;
    let sourceAfterQa: Record<string, unknown> | undefined;
    for (const e of entries) {
      const output = expand(snapshot, e);
      if (output.length !== e.expanded) fail("compressed-data");
      if (crc32(output) !== e.crc) fail("crc-mismatch");
      expandedBytes += output.length;
      if (e.name === "input.json") input = decodeJson(output);
      if (e.name === "source-after-qa.json") sourceAfterQa = decodeJson(output);
      // Extras are verified sequentially and never retained in the result.
    }
    if (!input || !sourceAfterQa) fail("zip-structure");
    return {
      ...SCOPE,
      archiveIntegrityMatched: true,
      artifact: identity,
      input,
      sourceAfterQa,
      entryCount: entries.length,
      expandedBytes,
    };
  } catch (error) {
    return {
      ...SCOPE,
      archiveIntegrityMatched: false,
      code: error instanceof ArchiveError ? error.code : "invalid-input",
    };
  }
}
