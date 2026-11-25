// SPDX-License-Identifier: MPL-2.0

/**
 * Local FDH-prepared-runtime control receipt helpers.
 *
 * The receipt records only an explicit registry selection and the runtime
 * identity copied from its Debug marker. It is deliberately not a compiler,
 * native, signature or release attestation. The Debug initializer treats a
 * present receipt as fail-closed: it either reuses a fully matching tree
 * without FTP index/network/download, or refuses the FTP fallback.
 */

import * as path from "@std/path";
import { BRANDING } from "./defines.ts";
import { exists } from "./utils.ts";
import type {
  AppConstantsIdentity,
  ApplicationIniIdentity,
  PreparedRuntimeArchitecture,
  PreparedRuntimeIdentity,
  PreparedRuntimeMarkerIdentity,
  PreparedRuntimePlatform,
  PreparedRuntimeReceipt,
  PreparedRuntimeReceiptState,
  PreparedRuntimeTarget,
  PreparedRuntimeTreeIdentity,
} from "./prepared_runtime_types.ts";

export const DEBUG_RUNTIME_MARKER_FILE = ".floorp-runtime-debug.json";
export const PREPARED_RUNTIME_RECEIPT_FILE =
  ".floorp-prepared-runtime-receipt.json";

const RECEIPT_KIND = "fdh-prepared-runtime-control" as const;
const SHA256_PATTERN = /^[0-9a-f]{64}$/;

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function requireString(value: Record<string, unknown>, key: string): string {
  const field = value[key];
  if (typeof field !== "string" || field.length === 0) {
    throw new Error(
      `Prepared Runtime receipt field '${key}' must be a non-empty string.`,
    );
  }
  return field;
}

function requireSize(value: Record<string, unknown>, key: string): number {
  const field = value[key];
  if (typeof field !== "number" || !Number.isSafeInteger(field) || field <= 0) {
    throw new Error(
      `Prepared Runtime receipt field '${key}' must be a positive integer.`,
    );
  }
  return field;
}

function requireSha256(value: Record<string, unknown>, key: string): string {
  const field = requireString(value, key).toLowerCase();
  if (!SHA256_PATTERN.test(field)) {
    throw new Error(
      `Prepared Runtime receipt field '${key}' must be a 64-hex SHA-256.`,
    );
  }
  return field;
}

function parsePlatform(value: string): PreparedRuntimePlatform {
  if (value === "windows" || value === "linux" || value === "macos") {
    return value;
  }
  throw new Error(`Unknown Prepared Runtime platform: ${value}.`);
}

function parseArchitecture(value: string): PreparedRuntimeArchitecture {
  if (value === "x86_64" || value === "aarch64" || value === "universal") {
    return value;
  }
  throw new Error(`Unknown Prepared Runtime architecture: ${value}.`);
}

/**
 * Normalize the source intent so the same registry path compares equal across
 * Windows path casing. This is only a source-path identity, not a canonical
 * filesystem realpath (the tree may contain devdir links).
 */
export function normalizePreparedRuntimeSourcePath(sourcePath: string): string {
  const resolved = path.resolve(sourcePath);
  return Deno.build.os === "windows" ? resolved.toLowerCase() : resolved;
}

function canonicalRuntimePath(value: string): string {
  let existing = path.resolve(value);
  const suffix: string[] = [];
  for (;;) {
    try {
      const resolved = path.join(Deno.realPathSync(existing), ...suffix);
      return Deno.build.os === "windows" ? resolved.toLowerCase() : resolved;
    } catch (error) {
      if (!(error instanceof Deno.errors.NotFound)) throw error;
      const parent = path.dirname(existing);
      if (parent === existing) throw error;
      suffix.unshift(path.basename(existing));
      existing = parent;
    }
  }
}

/** True when both paths resolve to the same canonical location. */
export function runtimePathsEqual(left: string, right: string): boolean {
  return canonicalRuntimePath(left) === canonicalRuntimePath(right);
}

/**
 * True when one path is the other, an ancestor or a descendant after canonical
 * resolution (so a junction/symlink alias also overlaps).
 */
export function runtimePathsOverlap(
  sourcePath: string,
  destinationRoot: string,
): boolean {
  const source = canonicalRuntimePath(sourcePath);
  const destination = canonicalRuntimePath(destinationRoot);
  const separator = Deno.build.os === "windows" ? "\\" : "/";
  return [
    path.relative(source, destination),
    path.relative(destination, source),
  ]
    .some((relative) =>
      relative === "" ||
      (!path.isAbsolute(relative) && relative !== ".." &&
        !relative.startsWith(`..${separator}`))
    );
}

/** Parse a marker record into the receipt's identity fields. */
export function markerIdentityFromRecord(
  marker: unknown,
): PreparedRuntimeMarkerIdentity {
  if (!isRecord(marker)) {
    throw new Error("Debug Runtime marker is not an object.");
  }
  if (marker.schemaVersion !== 1) {
    throw new Error("Debug Runtime marker schemaVersion must be 1.");
  }
  if (marker.distribution !== "debug") {
    throw new Error("Debug Runtime marker distribution must be debug.");
  }
  return {
    schemaVersion: 1,
    distribution: "debug",
    platform: parsePlatform(requireString(marker, "platform")),
    architecture: parseArchitecture(requireString(marker, "architecture")),
    artifactName: requireString(marker, "artifactName"),
    artifactSize: requireSize(marker, "artifactSize"),
    artifactSha256: requireSha256(marker, "artifactSha256"),
    version: requireString(marker, "version"),
    buildId: requireString(marker, "buildId"),
  };
}

/** Read the raw Debug marker at a runtime root; null when absent/unreadable. */
export function readPreparedRuntimeMarker(
  binRootDir: string,
): PreparedRuntimeMarkerIdentity | null {
  const markerPath = path.join(binRootDir, DEBUG_RUNTIME_MARKER_FILE);
  if (!exists(markerPath)) return null;
  try {
    return markerIdentityFromRecord(
      JSON.parse(Deno.readTextFileSync(markerPath)) as unknown,
    );
  } catch {
    return null;
  }
}

/** Build a receipt from a validated marker identity and explicit source path. */
export function buildPreparedRuntimeReceipt(
  marker: PreparedRuntimeMarkerIdentity,
  sourcePath: string,
  preparedAt: string = new Date().toISOString(),
): PreparedRuntimeReceipt {
  return {
    schemaVersion: 1,
    kind: RECEIPT_KIND,
    distribution: "debug",
    platform: marker.platform,
    architecture: marker.architecture,
    sourcePath: normalizePreparedRuntimeSourcePath(sourcePath),
    preparedAt,
    identity: {
      artifactName: marker.artifactName,
      artifactSize: marker.artifactSize,
      artifactSha256: marker.artifactSha256,
      version: marker.version,
      buildId: marker.buildId,
    },
  };
}

/** Strict receipt read; absence is distinct from malformed content. */
export function readPreparedRuntimeReceipt(
  binRootDir: string,
): PreparedRuntimeReceiptState {
  const receiptPath = path.join(binRootDir, PREPARED_RUNTIME_RECEIPT_FILE);
  if (!exists(receiptPath)) return { status: "absent" };
  try {
    const raw: unknown = JSON.parse(Deno.readTextFileSync(receiptPath));
    if (!isRecord(raw)) {
      return { status: "invalid", reason: "receipt is not an object" };
    }
    if (raw.schemaVersion !== 1) {
      return { status: "invalid", reason: "schemaVersion is not 1" };
    }
    if (raw.kind !== RECEIPT_KIND) {
      return { status: "invalid", reason: "kind is not the control receipt" };
    }
    if (raw.distribution !== "debug") {
      return { status: "invalid", reason: "distribution is not debug" };
    }
    const platform = parsePlatform(requireString(raw, "platform"));
    const architecture = parseArchitecture(
      requireString(raw, "architecture"),
    );
    const sourcePath = normalizePreparedRuntimeSourcePath(
      requireString(raw, "sourcePath"),
    );
    const preparedAt = requireString(raw, "preparedAt");
    const identityRaw = raw.identity;
    if (!isRecord(identityRaw)) {
      return { status: "invalid", reason: "identity is not an object" };
    }
    const identity: PreparedRuntimeIdentity = {
      artifactName: requireString(identityRaw, "artifactName"),
      artifactSize: requireSize(identityRaw, "artifactSize"),
      artifactSha256: requireSha256(identityRaw, "artifactSha256"),
      version: requireString(identityRaw, "version"),
      buildId: requireString(identityRaw, "buildId"),
    };
    return {
      status: "present",
      receipt: {
        schemaVersion: 1,
        kind: RECEIPT_KIND,
        distribution: "debug",
        platform,
        architecture,
        sourcePath,
        preparedAt,
        identity,
      },
    };
  } catch (error) {
    return {
      status: "invalid",
      reason: error instanceof Error ? error.message : String(error),
    };
  }
}

/** Write the receipt only after the explicit source has been copied+relinked. */
export function writePreparedRuntimeReceipt(
  binRootDir: string,
  receipt: PreparedRuntimeReceipt,
): void {
  Deno.mkdirSync(binRootDir, { recursive: true });
  Deno.writeTextFileSync(
    path.join(binRootDir, PREPARED_RUNTIME_RECEIPT_FILE),
    `${JSON.stringify(receipt, null, 2)}\n`,
  );
}

function preparedRuntimeAppRoot(
  binRootDir: string,
  platform: PreparedRuntimePlatform,
): string {
  const base = path.join(binRootDir, BRANDING.base_name);
  if (platform === "macos") {
    return path.join(
      base,
      `${BRANDING.display_name}.app`,
      "Contents",
      "Resources",
    );
  }
  return base;
}

function preparedRuntimeExecutablePath(
  binRootDir: string,
  platform: PreparedRuntimePlatform,
): string {
  const base = path.join(binRootDir, BRANDING.base_name);
  if (platform === "macos") {
    return path.join(
      base,
      `${BRANDING.display_name}.app`,
      "Contents",
      "MacOS",
      BRANDING.base_name,
    );
  }
  return platform === "windows"
    ? path.join(base, `${BRANDING.base_name}.exe`)
    : path.join(base, `${BRANDING.base_name}-bin`);
}

function parseApplicationIniIdentity(text: string): ApplicationIniIdentity {
  let name = "";
  let version = "";
  let buildId = "";
  for (const line of text.split(/\r?\n/)) {
    const match = /^([A-Za-z]+)=(.*)$/.exec(line.trim());
    if (!match) continue;
    if (match[1] === "Name") name = match[2];
    if (match[1] === "Version") version = match[2];
    if (match[1] === "BuildID") buildId = match[2];
  }
  if (!name || !version || !buildId) {
    throw new Error(
      "Debug Runtime application.ini is missing Name/Version/BuildID.",
    );
  }
  return { name, version, buildId };
}

/** Keep canonical source lines while removing comments and multiline decoys. */
function canonicalAppConstantsSource(text: string): string {
  let code = "";
  let quote = "";
  let blockComment = false;
  let lineComment = false;
  for (let i = 0; i < text.length; i++) {
    const char = text[i];
    const next = text[i + 1];
    if (lineComment) {
      if (char === "\n" || char === "\r") {
        lineComment = false;
        code += char;
      } else code += " ";
    } else if (blockComment) {
      if (char === "*" && next === "/") {
        blockComment = false;
        code += "  ";
        i++;
      } else code += char === "\n" || char === "\r" ? char : " ";
    } else if (quote) {
      // Strings cannot inject canonical property lines or object delimiters.
      code += /[\r\n{}]/.test(char) ? " " : char;
      if (char === "\\") {
        if (next === undefined) {
          throw new Error("Unterminated AppConstants string.");
        }
        code += /[\r\n{}]/.test(next) ? " " : next;
        i++;
      } else if (char === quote) quote = "";
    } else if (char === "/" && next === "/") {
      lineComment = true;
      code += "  ";
      i++;
    } else if (char === "/" && next === "*") {
      blockComment = true;
      code += "  ";
      i++;
    } else if (char === '"' || char === "'") {
      quote = char;
      code += char;
    } else if (char.charCodeAt(0) === 96) {
      // Generated AppConstants uses literal fields, never template evaluation.
      throw new Error(
        "Unsupported template literal in canonical AppConstants.",
      );
    } else code += char;
  }
  if (quote || blockComment) {
    throw new Error("Unterminated AppConstants source.");
  }
  return code;
}

/** Parse only top-level literal properties of the canonical generated object. */
function parseAppConstantsIdentity(text: string): AppConstantsIdentity {
  const code = canonicalAppConstantsSource(text);
  const header =
    /^\s*export\s+(?:var|const|let)\s+AppConstants\s*=\s*Object\.freeze\(\{\s*$/m
      .exec(code);
  if (!header) throw new Error("Canonical AppConstants object is missing.");
  let depth = 1;
  let debugCount = 0;
  let debug = false;
  let appName = "";
  let version = "";
  let buildId = "";
  for (
    const line of code.slice(header.index + header[0].length).split(/\r?\n/)
  ) {
    const trimmed = line.trim();
    if (depth === 1) {
      const debugProperty = /^DEBUG:\s*(true|false)\s*,?$/.exec(trimmed);
      if (debugProperty) {
        debugCount++;
        debug = debugProperty[1] === "true";
      }
      if (appName === "") {
        appName = /^MOZ_APP_NAME:\s*"([^"]+)"\s*,?$/.exec(trimmed)?.[1] ?? "";
      }
      if (version === "") {
        version = /^MOZ_APP_VERSION:\s*"([^"]+)"\s*,?$/.exec(trimmed)?.[1] ??
          "";
      }
      if (buildId === "") {
        buildId = /^MOZ_BUILDID:\s*"([^"]+)"\s*,?$/.exec(trimmed)?.[1] ?? "";
      }
    }
    for (const char of line) {
      if (char === "{") depth++;
      else if (char === "}") depth--;
    }
    if (depth === 0) break;
    if (depth < 0) throw new Error("Malformed canonical AppConstants object.");
  }
  if (depth !== 0 || debugCount !== 1 || !appName || !version || !buildId) {
    throw new Error(
      "Debug Runtime AppConstants is missing unambiguous canonical identity fields.",
    );
  }
  return { debug, appName, version, buildId };
}

function readAppConstants(
  binRootDir: string,
  platform: PreparedRuntimePlatform,
): AppConstantsIdentity {
  const file = path.join(
    preparedRuntimeAppRoot(binRootDir, platform),
    "modules",
    "AppConstants.sys.mjs",
  );
  if (!exists(file)) {
    throw new Error(`Debug Runtime AppConstants is missing: ${file}.`);
  }
  return parseAppConstantsIdentity(Deno.readTextFileSync(file));
}

/**
 * Validate a receipt against the actual live tree: matching Debug marker,
 * native target, real Debug AppConstants (DEBUG true), app constants,
 * application.ini version/buildId, and a non-empty executable.
 *
 * No broad safe-filesystem-tree assertion is made here: a relinked registry
 * tree legitimately carries devdir symlinks.
 */
export function assertPreparedRuntimeReceiptMatchesTree(
  binRootDir: string,
  receipt: PreparedRuntimeReceipt,
  target: PreparedRuntimeTarget,
): PreparedRuntimeTreeIdentity {
  if (receipt.platform !== target.platform) {
    throw new Error(
      `Prepared Runtime receipt platform ${receipt.platform} does not match ${target.platform}.`,
    );
  }
  if (receipt.architecture !== target.architecture) {
    throw new Error(
      `Prepared Runtime receipt architecture ${receipt.architecture} does not match ${target.architecture}.`,
    );
  }

  const marker = markerIdentityFromRecord(
    JSON.parse(
      Deno.readTextFileSync(
        path.join(binRootDir, DEBUG_RUNTIME_MARKER_FILE),
      ),
    ) as unknown,
  );
  for (
    const [key, expected] of Object.entries({
      schemaVersion: 1,
      distribution: "debug",
      platform: receipt.platform,
      architecture: receipt.architecture,
      artifactName: receipt.identity.artifactName,
      artifactSize: receipt.identity.artifactSize,
      artifactSha256: receipt.identity.artifactSha256,
      version: receipt.identity.version,
      buildId: receipt.identity.buildId,
    })
  ) {
    const actual = (marker as unknown as Record<string, unknown>)[key];
    if (actual !== expected) {
      throw new Error(
        `Prepared Runtime marker '${key}' differs from the control receipt.`,
      );
    }
  }

  const constants = readAppConstants(binRootDir, receipt.platform);
  if (!constants.debug) {
    throw new Error("Prepared Runtime AppConstants is not a real Debug build.");
  }
  if (constants.appName !== BRANDING.base_name) {
    throw new Error(
      `Prepared Runtime AppConstants app name ${constants.appName} does not match ${BRANDING.base_name}.`,
    );
  }
  if (constants.version !== receipt.identity.version) {
    throw new Error(
      `Prepared Runtime AppConstants version ${constants.version} differs from the receipt.`,
    );
  }
  if (constants.buildId !== receipt.identity.buildId) {
    throw new Error(
      `Prepared Runtime AppConstants buildId ${constants.buildId} differs from the receipt.`,
    );
  }

  const appRoot = preparedRuntimeAppRoot(binRootDir, receipt.platform);
  const appIniPath = path.join(appRoot, "application.ini");
  if (!exists(appIniPath)) {
    throw new Error(
      `Prepared Runtime application.ini is missing: ${appIniPath}.`,
    );
  }
  const appIni = parseApplicationIniIdentity(Deno.readTextFileSync(appIniPath));
  if (appIni.name !== BRANDING.display_name) {
    throw new Error(
      `Prepared Runtime application name ${appIni.name} does not match ${BRANDING.display_name}.`,
    );
  }
  if (appIni.version !== receipt.identity.version) {
    throw new Error(
      `Prepared Runtime application.ini version ${appIni.version} differs from the receipt.`,
    );
  }
  if (appIni.buildId !== receipt.identity.buildId) {
    throw new Error(
      `Prepared Runtime application.ini buildId ${appIni.buildId} differs from the receipt.`,
    );
  }

  const executablePath = preparedRuntimeExecutablePath(
    binRootDir,
    receipt.platform,
  );
  let executable: Deno.FileInfo;
  try {
    executable = Deno.lstatSync(executablePath);
  } catch {
    throw new Error(
      `Prepared Runtime executable is missing or empty: ${executablePath}.`,
    );
  }
  if (!executable.isFile || executable.size <= 0) {
    throw new Error(
      `Prepared Runtime executable is missing or empty: ${executablePath}.`,
    );
  }
  return { version: appIni.version, buildId: appIni.buildId };
}
