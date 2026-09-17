// SPDX-License-Identifier: MPL-2.0

import * as path from "@std/path";
import { Inflate } from "fflate";

const MAX_ARCHIVE_ENTRIES = 250_000;
const MAX_ZIP_INPUT_BYTES = 512 * 1024 * 1024;
const MAX_ZIP_OUTPUT_BYTES = 4 * 1024 * 1024 * 1024;
const MAX_TAR_LISTING_BYTES = 32 * 1024 * 1024;
const MAX_COMMAND_STDERR_BYTES = 4 * 1024 * 1024;
const ZIP_READ_CHUNK_BYTES = 1024 * 1024;
const ZIP_LOCAL_FILE_HEADER_SIGNATURE = 0x04034b50;
const ZIP_CENTRAL_DIRECTORY_SIGNATURE = 0x02014b50;
const ZIP_END_OF_CENTRAL_DIRECTORY_SIGNATURE = 0x06054b50;
const ZIP64_EXTRA_FIELD_ID = 0x0001;
const ZIP_AES_EXTRA_FIELD_ID = 0x9901;
const ZIP_UTF8_FLAG = 0x0800;
const ZIP_DATA_DESCRIPTOR_FLAG = 0x0008;
const ZIP_ENCRYPTED_FLAG = 0x0001;
const ZIP32_SENTINEL = 0xffff_ffff;

type SafeEntryKind = "directory" | "file";

interface IndexedPath {
  kind: SafeEntryKind;
  relativePath: string;
  explicit: boolean;
}

class SafePathIndex {
  readonly #entries = new Map<string, IndexedPath>();
  #explicitEntries = 0;

  register(rawPath: string, kind: SafeEntryKind): string {
    const relativePath = normalizeArchivePath(rawPath, kind);
    const segments = relativePath.split("/");

    for (let index = 0; index < segments.length; index += 1) {
      const candidate = segments.slice(0, index + 1).join("/");
      const candidateKind = index === segments.length - 1 ? kind : "directory";
      const explicit = index === segments.length - 1;
      const folded = foldPath(candidate);
      const previous = this.#entries.get(folded);

      if (previous) {
        if (previous.relativePath !== candidate) {
          throw new Error(
            `Archive contains a case-fold path collision: ${previous.relativePath} and ${candidate}.`,
          );
        }
        if (previous.kind !== candidateKind) {
          throw new Error(
            `Archive path changes type between file and directory: ${candidate}.`,
          );
        }
        if (explicit && previous.explicit) {
          throw new Error(`Archive contains a duplicate path: ${candidate}.`);
        }
        if (explicit) previous.explicit = true;
        continue;
      }

      this.#entries.set(folded, {
        kind: candidateKind,
        relativePath: candidate,
        explicit,
      });
    }

    this.#explicitEntries += 1;
    if (this.#explicitEntries > MAX_ARCHIVE_ENTRIES) {
      throw new Error(
        `Archive contains more than ${MAX_ARCHIVE_ENTRIES} entries.`,
      );
    }
    return relativePath;
  }
}

const WINDOWS_RESERVED_NAME =
  /^(?:CON|PRN|AUX|NUL|CLOCK\$|CONIN\$|CONOUT\$|COM[1-9\u00b9\u00b2\u00b3]|LPT[1-9\u00b9\u00b2\u00b3])$/iu;
const WINDOWS_UNSAFE_CHARACTER = /[<>:"|?*]/u;

function containsControlCharacter(value: string): boolean {
  for (let index = 0; index < value.length; index += 1) {
    const code = value.charCodeAt(index);
    if (code <= 0x1f || code === 0x7f) return true;
  }
  return false;
}

function foldPath(value: string): string {
  return value.normalize("NFC").toLocaleLowerCase("en-US");
}

function assertSafeWindowsSegment(segment: string): void {
  if (
    WINDOWS_UNSAFE_CHARACTER.test(segment) ||
    segment.endsWith(".") ||
    segment.endsWith(" ")
  ) {
    throw new Error(`Archive path has an unsafe Windows segment: ${segment}.`);
  }
  const deviceBase = segment.split(".", 1)[0];
  if (WINDOWS_RESERVED_NAME.test(deviceBase)) {
    throw new Error(`Archive path uses a reserved Windows name: ${segment}.`);
  }
}

export function normalizeArchivePath(
  rawPath: string,
  kind: SafeEntryKind,
): string {
  if (!rawPath || containsControlCharacter(rawPath)) {
    throw new Error("Archive path is empty or contains control characters.");
  }
  if (
    rawPath.includes("\\") ||
    rawPath.startsWith("/") ||
    rawPath.startsWith("//") ||
    /^[A-Za-z]:/u.test(rawPath)
  ) {
    throw new Error(
      `Archive path is absolute or platform-ambiguous: ${rawPath}.`,
    );
  }

  let candidate = rawPath;
  if (kind === "directory" && candidate.endsWith("/")) {
    candidate = candidate.slice(0, -1);
  } else if (kind === "file" && candidate.endsWith("/")) {
    throw new Error(`Archive file path ends with a separator: ${rawPath}.`);
  }
  while (candidate.startsWith("./")) candidate = candidate.slice(2);
  if (!candidate || candidate.includes("//")) {
    throw new Error(`Archive path is empty or non-canonical: ${rawPath}.`);
  }

  const segments = candidate.split("/");
  for (const segment of segments) {
    if (!segment || segment === "." || segment === "..") {
      throw new Error(`Archive path contains an unsafe segment: ${rawPath}.`);
    }
    if (containsControlCharacter(segment)) {
      throw new Error(`Archive path contains control characters: ${rawPath}.`);
    }
    assertSafeWindowsSegment(segment);
  }
  return segments.join("/");
}

function confinedDestination(root: string, relativePath: string): string {
  const destination = path.resolve(root, ...relativePath.split("/"));
  const relative = path.relative(path.resolve(root), destination);
  if (
    !relative ||
    relative === ".." ||
    relative.startsWith(`..${path.SEPARATOR}`) ||
    path.isAbsolute(relative)
  ) {
    throw new Error(
      `Archive destination escapes its extraction root: ${relativePath}.`,
    );
  }
  return destination;
}

async function assertEmptyDirectory(root: string): Promise<void> {
  const info = await Deno.lstat(root);
  if (!info.isDirectory || info.isSymlink) {
    throw new Error(`Extraction root is not a real directory: ${root}.`);
  }
  for await (const entry of Deno.readDir(root)) {
    throw new Error(
      `Extraction root must be fresh and empty; found ${entry.name} in ${root}.`,
    );
  }
}

function writeAllSync(file: Deno.FsFile, data: Uint8Array): void {
  let offset = 0;
  while (offset < data.length) {
    const written = file.writeSync(data.subarray(offset));
    if (written <= 0) {
      throw new Error("Failed to make progress writing ZIP data.");
    }
    offset += written;
  }
}

interface ValidatedZipEntry {
  readonly name: string;
  readonly relativePath: string;
  readonly kind: SafeEntryKind;
  readonly rawName: Uint8Array;
  readonly versionNeeded: number;
  readonly flags: number;
  readonly method: 0 | 8;
  readonly modifiedTime: number;
  readonly modifiedDate: number;
  readonly crc32: number;
  readonly compressedSize: number;
  readonly uncompressedSize: number;
  readonly localHeaderOffset: number;
  readonly mode: number | null;
  dataOffset: number;
}

function assertByteRange(
  byteLength: number,
  offset: number,
  length: number,
  label: string,
): void {
  if (
    !Number.isSafeInteger(offset) ||
    !Number.isSafeInteger(length) ||
    offset < 0 ||
    length < 0 ||
    offset > byteLength ||
    length > byteLength - offset
  ) {
    throw new Error(`ZIP ${label} is truncated or outside the archive.`);
  }
}

function equalBytes(left: Uint8Array, right: Uint8Array): boolean {
  if (left.length !== right.length) return false;
  for (let index = 0; index < left.length; index += 1) {
    if (left[index] !== right[index]) return false;
  }
  return true;
}

function validateZipExtraFields(
  archive: Uint8Array,
  view: DataView,
  offset: number,
  length: number,
  label: string,
): void {
  assertByteRange(archive.length, offset, length, `${label} extra fields`);
  const end = offset + length;
  const seen = new Set<number>();
  let cursor = offset;
  while (cursor < end) {
    if (end - cursor < 4) {
      throw new Error(`ZIP ${label} has a malformed extra-field header.`);
    }
    const identifier = view.getUint16(cursor, true);
    const dataLength = view.getUint16(cursor + 2, true);
    cursor += 4;
    if (dataLength > end - cursor) {
      throw new Error(`ZIP ${label} has a malformed extra-field payload.`);
    }
    if (seen.has(identifier)) {
      throw new Error(
        `ZIP ${label} repeats extra field 0x${identifier.toString(16)}.`,
      );
    }
    seen.add(identifier);
    if (identifier === ZIP64_EXTRA_FIELD_ID) {
      throw new Error(`ZIP64 extra fields are not supported (${label}).`);
    }
    if (identifier === ZIP_AES_EXTRA_FIELD_ID) {
      throw new Error(`ZIP AES extra fields are not supported (${label}).`);
    }
    cursor += dataLength;
  }
  if (cursor !== end) {
    throw new Error(`ZIP ${label} has malformed extra fields.`);
  }
}

function decodeZipName(rawName: Uint8Array, flags: number): string {
  if (rawName.length === 0) throw new Error("ZIP member name is empty.");
  if ((flags & ZIP_UTF8_FLAG) !== 0) {
    try {
      return new TextDecoder("utf-8", { fatal: true }).decode(rawName);
    } catch (error) {
      throw new Error("ZIP member name is not valid UTF-8.", {
        cause: error,
      });
    }
  }
  for (const value of rawName) {
    if (value > 0x7f) {
      throw new Error(
        "ZIP legacy-encoded member names must be ASCII for deterministic extraction.",
      );
    }
  }
  return new TextDecoder("utf-8", { fatal: true }).decode(rawName);
}

function validateZipFlags(flags: number, method: number, label: string): void {
  if ((flags & ZIP_ENCRYPTED_FLAG) !== 0) {
    throw new Error(`ZIP encryption is not supported (${label}).`);
  }
  if ((flags & ZIP_DATA_DESCRIPTOR_FLAG) !== 0) {
    throw new Error(`ZIP data descriptors are not supported (${label}).`);
  }
  const allowedFlags = method === 8 ? ZIP_UTF8_FLAG | 0x0006 : ZIP_UTF8_FLAG;
  if ((flags & ~allowedFlags) !== 0) {
    throw new Error(
      `ZIP ${label} uses unsupported general-purpose flags 0x${
        flags.toString(16)
      }.`,
    );
  }
}

function classifyZipEntry(
  name: string,
  versionMadeBy: number,
  externalAttributes: number,
): { kind: SafeEntryKind; mode: number | null } {
  const hostSystem = versionMadeBy >>> 8;
  const hasDirectorySuffix = name.endsWith("/");
  const dosDirectory = (externalAttributes & 0x10) !== 0;

  if (hostSystem === 0) {
    if ((externalAttributes & 0xffff_ff00) !== 0) {
      throw new Error(
        `ZIP member ${name} has unsupported FAT external attributes.`,
      );
    }
    if (hasDirectorySuffix !== dosDirectory) {
      throw new Error(
        `ZIP member ${name} has inconsistent directory metadata.`,
      );
    }
    return {
      kind: hasDirectorySuffix ? "directory" : "file",
      mode: null,
    };
  }

  if (hostSystem !== 3) {
    throw new Error(
      `ZIP member ${name} uses unsupported creator system ${hostSystem}.`,
    );
  }
  const mode = externalAttributes >>> 16;
  const fileType = mode & 0o170000;
  let kind: SafeEntryKind;
  if (fileType === 0o100000) {
    kind = "file";
  } else if (fileType === 0o040000) {
    kind = "directory";
  } else if (fileType === 0) {
    kind = hasDirectorySuffix ? "directory" : "file";
  } else {
    throw new Error(
      `ZIP member ${name} has forbidden Unix file type 0o${
        fileType.toString(8)
      }.`,
    );
  }
  if (
    hasDirectorySuffix !== (kind === "directory") ||
    (dosDirectory && kind !== "directory")
  ) {
    throw new Error(
      `ZIP member ${name} has inconsistent directory metadata.`,
    );
  }
  return { kind, mode };
}

function findUniqueZipEocd(archive: Uint8Array, view: DataView): number {
  if (archive.length < 22) throw new Error("ZIP archive has no EOCD record.");
  const candidates: number[] = [];
  const firstPossible = Math.max(0, archive.length - 22 - 0xffff);
  for (let offset = firstPossible; offset <= archive.length - 22; offset += 1) {
    if (
      view.getUint32(offset, true) !== ZIP_END_OF_CENTRAL_DIRECTORY_SIGNATURE
    ) {
      continue;
    }
    const commentLength = view.getUint16(offset + 20, true);
    if (offset + 22 + commentLength === archive.length) candidates.push(offset);
  }
  if (candidates.length !== 1) {
    throw new Error(
      `ZIP must have exactly one EOCD record ending at physical EOF; found ${candidates.length}.`,
    );
  }
  return candidates[0];
}

function validateExpectedZipRoot(
  relativePath: string,
  kind: SafeEntryKind,
  expectedRoot: string | undefined,
): void {
  if (expectedRoot === undefined) return;
  const normalizedRoot = normalizeArchivePath(expectedRoot, "directory");
  if (
    relativePath !== normalizedRoot &&
    !relativePath.startsWith(`${normalizedRoot}/`)
  ) {
    throw new Error(
      `ZIP member is outside the expected ${normalizedRoot} root: ${relativePath}.`,
    );
  }
  if (relativePath === normalizedRoot && kind !== "directory") {
    throw new Error(`ZIP expected root ${normalizedRoot} is not a directory.`);
  }
}

function parseClassicZip32(
  archive: Uint8Array,
  expectedRoot: string | undefined,
): ValidatedZipEntry[] {
  const view = new DataView(
    archive.buffer,
    archive.byteOffset,
    archive.byteLength,
  );
  const eocdOffset = findUniqueZipEocd(archive, view);
  const diskNumber = view.getUint16(eocdOffset + 4, true);
  const centralDirectoryDisk = view.getUint16(eocdOffset + 6, true);
  const entriesOnDisk = view.getUint16(eocdOffset + 8, true);
  const entryCount = view.getUint16(eocdOffset + 10, true);
  const centralDirectorySize = view.getUint32(eocdOffset + 12, true);
  const centralDirectoryOffset = view.getUint32(eocdOffset + 16, true);
  if (
    diskNumber !== 0 ||
    centralDirectoryDisk !== 0 ||
    entriesOnDisk !== entryCount
  ) {
    throw new Error("Multi-disk ZIP archives are not supported.");
  }
  if (
    entriesOnDisk === 0xffff ||
    entryCount === 0xffff ||
    centralDirectorySize === ZIP32_SENTINEL ||
    centralDirectoryOffset === ZIP32_SENTINEL
  ) {
    throw new Error("ZIP64 archives are not supported.");
  }
  if (entryCount === 0) throw new Error("ZIP archive is empty.");
  if (entryCount > MAX_ARCHIVE_ENTRIES) {
    throw new Error(
      `Archive contains more than ${MAX_ARCHIVE_ENTRIES} entries.`,
    );
  }
  if (
    centralDirectoryOffset > eocdOffset ||
    centralDirectorySize !== eocdOffset - centralDirectoryOffset
  ) {
    throw new Error(
      "ZIP central-directory range does not end exactly at the EOCD record.",
    );
  }

  const pathIndex = new SafePathIndex();
  const entries: ValidatedZipEntry[] = [];
  let cursor = centralDirectoryOffset;
  let declaredOutputBytes = 0;
  for (let entryIndex = 0; entryIndex < entryCount; entryIndex += 1) {
    assertByteRange(eocdOffset, cursor, 46, "central-directory header");
    if (view.getUint32(cursor, true) !== ZIP_CENTRAL_DIRECTORY_SIGNATURE) {
      throw new Error(
        `ZIP central-directory entry ${entryIndex} has an invalid signature.`,
      );
    }
    const versionMadeBy = view.getUint16(cursor + 4, true);
    const versionNeeded = view.getUint16(cursor + 6, true);
    const flags = view.getUint16(cursor + 8, true);
    const method = view.getUint16(cursor + 10, true);
    const modifiedTime = view.getUint16(cursor + 12, true);
    const modifiedDate = view.getUint16(cursor + 14, true);
    const crc32 = view.getUint32(cursor + 16, true);
    const compressedSize = view.getUint32(cursor + 20, true);
    const uncompressedSize = view.getUint32(cursor + 24, true);
    const nameLength = view.getUint16(cursor + 28, true);
    const extraLength = view.getUint16(cursor + 30, true);
    const commentLength = view.getUint16(cursor + 32, true);
    const diskStart = view.getUint16(cursor + 34, true);
    const externalAttributes = view.getUint32(cursor + 38, true);
    const localHeaderOffset = view.getUint32(cursor + 42, true);
    const recordLength = 46 + nameLength + extraLength + commentLength;
    assertByteRange(
      eocdOffset,
      cursor,
      recordLength,
      "central-directory record",
    );

    if (
      versionNeeded < 10 ||
      versionNeeded > 20 ||
      (method === 8 && versionNeeded < 20)
    ) {
      throw new Error(
        `ZIP central entry ${entryIndex} needs unsupported version ${versionNeeded}.`,
      );
    }
    if (method !== 0 && method !== 8) {
      throw new Error(
        `ZIP central entry ${entryIndex} uses unsupported method ${method}.`,
      );
    }
    validateZipFlags(flags, method, `central entry ${entryIndex}`);
    if (
      compressedSize === ZIP32_SENTINEL ||
      uncompressedSize === ZIP32_SENTINEL ||
      localHeaderOffset === ZIP32_SENTINEL ||
      diskStart === 0xffff
    ) {
      throw new Error("ZIP64 entry fields are not supported.");
    }
    if (diskStart !== 0) {
      throw new Error("Multi-disk ZIP entry fields are not supported.");
    }
    if (method === 0 && compressedSize !== uncompressedSize) {
      throw new Error(
        "Stored ZIP entry has different compressed and raw sizes.",
      );
    }
    if (method === 8 && compressedSize === 0) {
      throw new Error("Deflated ZIP entry has no compressed bitstream.");
    }

    const rawName = archive.subarray(cursor + 46, cursor + 46 + nameLength);
    const name = decodeZipName(rawName, flags);
    const metadata = classifyZipEntry(
      name,
      versionMadeBy,
      externalAttributes,
    );
    const relativePath = pathIndex.register(name, metadata.kind);
    const canonicalName = metadata.kind === "directory"
      ? `${relativePath}/`
      : relativePath;
    if (name !== canonicalName) {
      throw new Error(`ZIP member path is not canonical: ${name}.`);
    }
    validateExpectedZipRoot(relativePath, metadata.kind, expectedRoot);
    if (
      metadata.kind === "directory" &&
      (compressedSize !== 0 || uncompressedSize !== 0 || crc32 !== 0)
    ) {
      throw new Error(`ZIP directory ${name} contains data.`);
    }
    validateZipExtraFields(
      archive,
      view,
      cursor + 46 + nameLength,
      extraLength,
      `central entry ${name}`,
    );

    declaredOutputBytes += uncompressedSize;
    if (declaredOutputBytes > MAX_ZIP_OUTPUT_BYTES) {
      throw new Error(`ZIP expands beyond ${MAX_ZIP_OUTPUT_BYTES} bytes.`);
    }
    entries.push({
      name,
      relativePath,
      kind: metadata.kind,
      rawName,
      versionNeeded,
      flags,
      method,
      modifiedTime,
      modifiedDate,
      crc32,
      compressedSize,
      uncompressedSize,
      localHeaderOffset,
      mode: metadata.mode,
      dataOffset: -1,
    });
    cursor += recordLength;
  }
  if (cursor !== eocdOffset) {
    throw new Error(
      "ZIP central-directory size/count leaves unparsed or overlapping bytes.",
    );
  }

  entries.sort((left, right) =>
    left.localHeaderOffset - right.localHeaderOffset
  );
  cursor = 0;
  for (const entry of entries) {
    if (entry.localHeaderOffset !== cursor) {
      throw new Error(
        `ZIP local records are not contiguous at ${entry.name}: expected ${cursor}, found ${entry.localHeaderOffset}.`,
      );
    }
    assertByteRange(
      centralDirectoryOffset,
      cursor,
      30,
      `local header for ${entry.name}`,
    );
    if (view.getUint32(cursor, true) !== ZIP_LOCAL_FILE_HEADER_SIGNATURE) {
      throw new Error(
        `ZIP local header for ${entry.name} has an invalid signature.`,
      );
    }
    const versionNeeded = view.getUint16(cursor + 4, true);
    const flags = view.getUint16(cursor + 6, true);
    const method = view.getUint16(cursor + 8, true);
    const modifiedTime = view.getUint16(cursor + 10, true);
    const modifiedDate = view.getUint16(cursor + 12, true);
    const crc32 = view.getUint32(cursor + 14, true);
    const compressedSize = view.getUint32(cursor + 18, true);
    const uncompressedSize = view.getUint32(cursor + 22, true);
    const nameLength = view.getUint16(cursor + 26, true);
    const extraLength = view.getUint16(cursor + 28, true);
    const headerLength = 30 + nameLength + extraLength;
    assertByteRange(
      centralDirectoryOffset,
      cursor,
      headerLength,
      `local record for ${entry.name}`,
    );
    validateZipFlags(flags, method, `local entry ${entry.name}`);
    const localName = archive.subarray(cursor + 30, cursor + 30 + nameLength);
    if (
      versionNeeded !== entry.versionNeeded ||
      flags !== entry.flags ||
      method !== entry.method ||
      modifiedTime !== entry.modifiedTime ||
      modifiedDate !== entry.modifiedDate ||
      crc32 !== entry.crc32 ||
      compressedSize !== entry.compressedSize ||
      uncompressedSize !== entry.uncompressedSize ||
      !equalBytes(localName, entry.rawName)
    ) {
      throw new Error(
        `ZIP local header does not match its central entry: ${entry.name}.`,
      );
    }
    validateZipExtraFields(
      archive,
      view,
      cursor + 30 + nameLength,
      extraLength,
      `local entry ${entry.name}`,
    );
    entry.dataOffset = cursor + headerLength;
    assertByteRange(
      centralDirectoryOffset,
      entry.dataOffset,
      entry.compressedSize,
      `compressed data for ${entry.name}`,
    );
    cursor = entry.dataOffset + entry.compressedSize;
  }
  if (cursor !== centralDirectoryOffset) {
    throw new Error(
      "ZIP local records do not end exactly at the central directory.",
    );
  }
  return entries;
}

const CRC32_TABLE = (() => {
  const table = new Uint32Array(256);
  for (let index = 0; index < table.length; index += 1) {
    let value = index;
    for (let bit = 0; bit < 8; bit += 1) {
      value = (value & 1) !== 0 ? 0xedb8_8320 ^ (value >>> 1) : value >>> 1;
    }
    table[index] = value >>> 0;
  }
  return table;
})();

function updateCrc32(crc: number, data: Uint8Array): number {
  let value = crc;
  for (const byte of data) {
    value = CRC32_TABLE[(value ^ byte) & 0xff] ^ (value >>> 8);
  }
  return value >>> 0;
}

function extractValidatedZipFile(
  archive: Uint8Array,
  entry: ValidatedZipEntry,
  output: Deno.FsFile,
): void {
  let writtenBytes = 0;
  let runningCrc = 0xffff_ffff;
  let finalCallbacks = 0;
  const consume = (data: Uint8Array, final: boolean): void => {
    if (final) finalCallbacks += 1;
    if (finalCallbacks > 1) {
      throw new Error(
        `ZIP inflater finalized more than once for ${entry.name}.`,
      );
    }
    writtenBytes += data.length;
    if (writtenBytes > entry.uncompressedSize) {
      throw new Error(
        `ZIP output exceeds its declared size for ${entry.name}.`,
      );
    }
    runningCrc = updateCrc32(runningCrc, data);
    if (data.length > 0) writeAllSync(output, data);
  };

  if (entry.method === 0) {
    let cursor = entry.dataOffset;
    const end = entry.dataOffset + entry.compressedSize;
    while (cursor < end) {
      const next = Math.min(end, cursor + ZIP_READ_CHUNK_BYTES);
      consume(archive.subarray(cursor, next), false);
      cursor = next;
    }
    consume(new Uint8Array(), true);
  } else {
    const inflater = new Inflate(consume);
    let cursor = entry.dataOffset;
    const end = entry.dataOffset + entry.compressedSize;
    if (cursor === end) {
      inflater.push(new Uint8Array(), true);
    } else {
      while (cursor < end) {
        const next = Math.min(end, cursor + ZIP_READ_CHUNK_BYTES);
        inflater.push(archive.subarray(cursor, next), next === end);
        cursor = next;
      }
    }
  }

  const crc32 = (runningCrc ^ 0xffff_ffff) >>> 0;
  if (finalCallbacks !== 1) {
    throw new Error(
      `ZIP decompressor did not finalize exactly once for ${entry.name}.`,
    );
  }
  if (writtenBytes !== entry.uncompressedSize) {
    throw new Error(
      `ZIP size mismatch for ${entry.name}: ${writtenBytes} !== ${entry.uncompressedSize}.`,
    );
  }
  if (crc32 !== entry.crc32) {
    throw new Error(
      `ZIP CRC32 mismatch for ${entry.name}: 0x${crc32.toString(16)} !== 0x${
        entry.crc32.toString(16)
      }.`,
    );
  }
}

/**
 * Validate a classic, single-disk ZIP32 archive completely before extracting
 * its members into an already-created empty staging directory.
 */
export async function extractZipSafely(
  archivePath: string,
  destinationRoot: string,
  expectedRoot?: string,
): Promise<void> {
  await assertEmptyDirectory(destinationRoot);
  const archiveInfo = await Deno.lstat(archivePath);
  if (!archiveInfo.isFile || archiveInfo.isSymlink) {
    throw new Error(`ZIP input is not a real regular file: ${archivePath}.`);
  }
  if (archiveInfo.size > MAX_ZIP_INPUT_BYTES) {
    throw new Error(`ZIP input exceeds ${MAX_ZIP_INPUT_BYTES} bytes.`);
  }
  const archive = await Deno.readFile(archivePath);
  if (archive.length > MAX_ZIP_INPUT_BYTES) {
    throw new Error(`ZIP input exceeds ${MAX_ZIP_INPUT_BYTES} bytes.`);
  }
  const entries = parseClassicZip32(archive, expectedRoot);
  const directoryModes: Array<{ destination: string; mode: number | null }> =
    [];

  for (const entry of entries) {
    const destination = confinedDestination(
      destinationRoot,
      entry.relativePath,
    );
    if (entry.kind === "directory") {
      await Deno.mkdir(destination, { recursive: true });
      directoryModes.push({ destination, mode: entry.mode });
      continue;
    }
    await Deno.mkdir(path.dirname(destination), { recursive: true });
    const output = await Deno.open(destination, {
      createNew: true,
      write: true,
    });
    try {
      extractValidatedZipFile(archive, entry, output);
    } finally {
      output.close();
    }
    await applyPortableMode(destination, entry.mode);
  }
  for (const directory of directoryModes.reverse()) {
    await applyPortableMode(directory.destination, directory.mode);
  }
  await assertSafeFilesystemTree(destinationRoot);
}

function splitCommandLines(value: string, label: string): string[] {
  if (value.includes("\0")) {
    throw new Error(`${label} contains a NUL byte.`);
  }
  const normalized = value.replaceAll("\r\n", "\n");
  if (normalized.includes("\r")) {
    throw new Error(`${label} contains an unexpected carriage return.`);
  }
  const lines = normalized.split("\n");
  if (lines.at(-1) === "") lines.pop();
  if (lines.length === 0 || lines.some((line) => line.length === 0)) {
    throw new Error(`${label} is empty or contains an empty record.`);
  }
  return lines;
}

export function validateTarArchiveListings(
  namesOutput: string,
  verboseOutput: string,
  expectedRoot: string,
): number {
  const names = splitCommandLines(namesOutput, "tar member listing");
  const verbose = splitCommandLines(verboseOutput, "tar verbose listing");
  if (names.length !== verbose.length) {
    throw new Error(
      `tar listing count mismatch: ${names.length} names and ${verbose.length} type records.`,
    );
  }

  const index = new SafePathIndex();
  for (let entryIndex = 0; entryIndex < names.length; entryIndex += 1) {
    const type = verbose[entryIndex][0];
    if (type !== "-" && type !== "d") {
      throw new Error(
        `tar member ${names[entryIndex]} has forbidden type ${
          JSON.stringify(type)
        }.`,
      );
    }
    const relativePath = index.register(
      names[entryIndex],
      type === "d" ? "directory" : "file",
    );
    if (
      relativePath !== expectedRoot &&
      !relativePath.startsWith(`${expectedRoot}/`)
    ) {
      throw new Error(
        `tar member is outside the expected ${expectedRoot} root: ${relativePath}.`,
      );
    }
  }
  return names.length;
}

async function readBounded(
  stream: ReadableStream<Uint8Array>,
  maximumBytes: number,
  label: string,
): Promise<Uint8Array> {
  const reader = stream.getReader();
  const chunks: Uint8Array[] = [];
  let total = 0;
  try {
    while (true) {
      const result = await reader.read();
      if (result.done) break;
      total += result.value.length;
      if (total > maximumBytes) {
        throw new Error(`${label} exceeds ${maximumBytes} bytes.`);
      }
      chunks.push(result.value);
    }
  } finally {
    reader.releaseLock();
  }
  const output = new Uint8Array(total);
  let offset = 0;
  for (const chunk of chunks) {
    output.set(chunk, offset);
    offset += chunk.length;
  }
  return output;
}

async function runTar(
  args: string[],
  stdoutLimit = MAX_TAR_LISTING_BYTES,
): Promise<string> {
  const child = new Deno.Command("tar", {
    args,
    stdout: "piped",
    stderr: "piped",
  }).spawn();
  try {
    const [stdout, stderr, status] = await Promise.all([
      readBounded(child.stdout, stdoutLimit, "tar stdout"),
      readBounded(child.stderr, MAX_COMMAND_STDERR_BYTES, "tar stderr"),
      child.status,
    ]);
    const decode = (value: Uint8Array): string =>
      new TextDecoder("utf-8", { fatal: true }).decode(value);
    const stderrText = decode(stderr);
    if (!status.success) {
      throw new Error(
        `tar ${
          args[0] ?? "command"
        } failed with exit ${status.code}: ${stderrText.trim()}`,
      );
    }
    return decode(stdout);
  } catch (error) {
    try {
      child.kill("SIGKILL");
    } catch {
      // The process already exited.
    }
    throw error;
  }
}

export async function extractTarXzSafely(
  archivePath: string,
  destinationRoot: string,
  expectedRoot: string,
): Promise<void> {
  await assertEmptyDirectory(destinationRoot);
  const listingArgs = [
    "--list",
    "--xz",
    "--file",
    archivePath,
    "--quoting-style=escape",
  ];
  const names = await runTar(listingArgs);
  const verbose = await runTar([
    ...listingArgs,
    "--verbose",
    "--numeric-owner",
  ]);
  validateTarArchiveListings(names, verbose, expectedRoot);
  await runTar([
    "--extract",
    "--xz",
    "--file",
    archivePath,
    "--directory",
    destinationRoot,
    "--no-same-owner",
    "--no-same-permissions",
    "--delay-directory-restore",
  ], 1024 * 1024);
  await assertSafeFilesystemTree(destinationRoot);
}

export async function findSingleTopLevelAppDirectory(
  mountRoot: string,
  expectedAppName: string,
): Promise<string> {
  const candidates: string[] = [];
  for await (const entry of Deno.readDir(mountRoot)) {
    if (!entry.name.toLocaleLowerCase("en-US").endsWith(".app")) continue;
    const candidate = path.join(mountRoot, entry.name);
    const info = await Deno.lstat(candidate);
    if (!info.isDirectory || info.isSymlink) {
      throw new Error(`DMG .app entry is not a real directory: ${entry.name}.`);
    }
    candidates.push(entry.name);
  }
  if (candidates.length !== 1 || candidates[0] !== expectedAppName) {
    throw new Error(
      `Expected exactly one top-level ${expectedAppName} directory in DMG; found ${
        JSON.stringify(candidates)
      }.`,
    );
  }
  return path.join(mountRoot, candidates[0]);
}

async function applyPortableMode(
  filePath: string,
  mode: number | null,
): Promise<void> {
  if (Deno.build.os === "windows" || mode === null) return;
  await Deno.chmod(filePath, mode & 0o777);
}

export async function copyDirectoryTreeSafely(
  sourceRoot: string,
  destinationRoot: string,
): Promise<void> {
  const sourceInfo = await Deno.lstat(sourceRoot);
  if (!sourceInfo.isDirectory || sourceInfo.isSymlink) {
    throw new Error(`Safe-copy source is not a real directory: ${sourceRoot}.`);
  }
  try {
    await Deno.lstat(destinationRoot);
    throw new Error(
      `Safe-copy destination already exists: ${destinationRoot}.`,
    );
  } catch (error) {
    if (!(error instanceof Deno.errors.NotFound)) throw error;
  }

  const index = new SafePathIndex();
  await Deno.mkdir(destinationRoot, { recursive: false });

  const copyDirectory = async (
    sourceDirectory: string,
    relativeDirectory: string,
  ): Promise<void> => {
    const entries: Deno.DirEntry[] = [];
    for await (const entry of Deno.readDir(sourceDirectory)) {
      entries.push(entry);
    }
    entries.sort((left, right) => left.name.localeCompare(right.name));

    for (const entry of entries) {
      const rawRelative = relativeDirectory
        ? `${relativeDirectory}/${entry.name}`
        : entry.name;
      const source = path.join(sourceDirectory, entry.name);
      const info = await Deno.lstat(source);
      const kind: SafeEntryKind = info.isDirectory ? "directory" : "file";
      if (
        info.isSymlink ||
        (!info.isDirectory && !info.isFile) ||
        info.isBlockDevice === true ||
        info.isCharDevice === true ||
        info.isFifo === true ||
        info.isSocket === true
      ) {
        throw new Error(
          `Safe-copy rejects special filesystem entry: ${rawRelative}.`,
        );
      }
      const relativePath = index.register(rawRelative, kind);
      const destination = confinedDestination(destinationRoot, relativePath);
      if (kind === "directory") {
        await Deno.mkdir(destination);
        await copyDirectory(source, relativePath);
        await applyPortableMode(destination, info.mode);
      } else {
        await Deno.copyFile(source, destination);
        await applyPortableMode(destination, info.mode);
      }
    }
  };

  await copyDirectory(sourceRoot, "");
  await applyPortableMode(destinationRoot, sourceInfo.mode);
  await assertSafeFilesystemTree(destinationRoot);
}

export async function assertSafeFilesystemTree(root: string): Promise<void> {
  const rootInfo = await Deno.lstat(root);
  if (!rootInfo.isDirectory || rootInfo.isSymlink) {
    throw new Error(`Runtime staging root is not a real directory: ${root}.`);
  }
  const index = new SafePathIndex();
  const pending: Array<{ directory: string; relative: string }> = [{
    directory: root,
    relative: "",
  }];

  while (pending.length > 0) {
    const current = pending.pop();
    if (!current) break;
    for await (const entry of Deno.readDir(current.directory)) {
      const relative = current.relative
        ? `${current.relative}/${entry.name}`
        : entry.name;
      const entryPath = path.join(current.directory, entry.name);
      const info = await Deno.lstat(entryPath);
      if (info.isSymlink) {
        throw new Error(
          `Runtime staging tree contains a symlink: ${relative}.`,
        );
      }
      if (info.isDirectory) {
        const canonical = index.register(relative, "directory");
        pending.push({ directory: entryPath, relative: canonical });
        continue;
      }
      if (
        !info.isFile ||
        info.isBlockDevice === true ||
        info.isCharDevice === true ||
        info.isFifo === true ||
        info.isSocket === true
      ) {
        throw new Error(
          `Runtime staging tree contains a special entry: ${relative}.`,
        );
      }
      if (info.nlink !== null && info.nlink > 1) {
        throw new Error(
          `Runtime staging tree contains a hard-linked file: ${relative}.`,
        );
      }
      index.register(relative, "file");
    }
  }
}
