// SPDX-License-Identifier: MPL-2.0

import {
  FEATURE_PACKAGE_MAX_BYTES,
  FEATURE_PACKAGE_METADATA_MAX_BYTES,
  FEATURE_PACKAGE_PKCS8_MAX_BYTES,
  FEATURE_PACKAGE_POLICY_MAX_BYTES,
  FEATURE_PACKAGE_SOURCE_MAX_BYTES,
  FeaturePackageError,
  parseFeaturePackagePolicyJson,
  signFeaturePackage,
  verifySignedFeaturePackage,
} from "../browser-features/modules/modules/feature-host/package/FeaturePackageVerifier.ts";
import type {
  FeaturePackageAuthoringMetadata,
  FeaturePackagePolicy,
} from "../browser-features/modules/modules/feature-host/package/types.ts";
import type {
  FeaturePackageCliErrorCode,
  FeaturePackageCliFlags,
  FeaturePackageCliResult,
} from "./feature-package-types.ts";

const HELP = `Usage:
  deno run -A tools/feature-package.ts pack --metadata <json> --source <utf8-file> --key-id <id> --key-pkcs8 <binary-file> --out <new-json-file>
  deno run -A tools/feature-package.ts verify --package <json-file> --policy <json-file>
`;

const PACK_FLAGS = [
  "metadata",
  "source",
  "key-id",
  "key-pkcs8",
  "out",
] as const;
const VERIFY_FLAGS = ["package", "policy"] as const;

function arrayBuffer(bytes: Uint8Array): ArrayBuffer {
  const buffer = new ArrayBuffer(bytes.byteLength);
  new Uint8Array(buffer).set(bytes);
  return buffer;
}

const CLI_MESSAGES: Record<FeaturePackageCliErrorCode, string> = {
  "invalid-arguments": "Invalid command arguments.",
  "invalid-metadata": "Package metadata is invalid.",
  "invalid-policy": "Trusted policy is invalid.",
  "invalid-source": "Package source is invalid.",
  "invalid-key": "Signing key input is invalid.",
  "input-too-large": "An input file exceeds its size limit.",
  "output-exists": "Output file already exists.",
  "file-operation-failed": "A package file operation failed.",
  "package-rejected": "Package verification failed.",
};

class CliError extends Error {
  constructor(readonly code: FeaturePackageCliErrorCode) {
    super(CLI_MESSAGES[code]);
    this.name = "CliError";
  }
}

function fail(code: FeaturePackageCliErrorCode): never {
  throw new CliError(code);
}

function parsePairs(
  args: readonly string[],
  allowed: readonly string[],
): FeaturePackageCliFlags {
  const flags: Record<string, string> = Object.create(null);
  if (args.length % 2 !== 0) fail("invalid-arguments");
  for (let index = 0; index < args.length; index += 2) {
    const flag = args[index];
    const value = args[index + 1];
    if (
      !flag.startsWith("--") || value.length === 0 || value.startsWith("--")
    ) {
      fail("invalid-arguments");
    }
    const name = flag.slice(2);
    if (!allowed.includes(name) || Object.hasOwn(flags, name)) {
      fail("invalid-arguments");
    }
    flags[name] = value;
  }
  if (allowed.some((name) => !Object.hasOwn(flags, name))) {
    fail("invalid-arguments");
  }
  return Object.freeze(flags);
}

async function readBounded(path: string, limit: number): Promise<Uint8Array> {
  let file: Deno.FsFile;
  try {
    file = await Deno.open(path, { read: true });
  } catch {
    fail("file-operation-failed");
  }
  try {
    const bytes = new Uint8Array(limit + 1);
    let length = 0;
    while (length < bytes.length) {
      const read = await file.read(bytes.subarray(length));
      if (read === null) break;
      if (read === 0) fail("file-operation-failed");
      length += read;
    }
    if (length > limit) fail("input-too-large");
    return bytes.slice(0, length);
  } catch (error: unknown) {
    if (error instanceof CliError) throw error;
    fail("file-operation-failed");
  } finally {
    file.close();
  }
}

function decodeText(
  bytes: Uint8Array,
  code: "invalid-metadata" | "invalid-policy" | "invalid-source",
): string {
  try {
    return new TextDecoder("utf-8", { fatal: true, ignoreBOM: true }).decode(
      bytes,
    );
  } catch {
    fail(code);
  }
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function parseMetadata(bytes: Uint8Array): FeaturePackageAuthoringMetadata {
  if (bytes.length > FEATURE_PACKAGE_METADATA_MAX_BYTES) {
    fail("input-too-large");
  }
  let value: unknown;
  try {
    value = JSON.parse(decodeText(bytes, "invalid-metadata"));
  } catch (error: unknown) {
    if (error instanceof CliError) throw error;
    fail("invalid-metadata");
  }
  if (
    !isRecord(value) || Object.keys(value).length !== 4 ||
    !["featureId", "version", "releaseSequence", "compatibility"].every((key) =>
      Object.hasOwn(value, key)
    ) ||
    Object.keys(value).some((key) =>
      !["featureId", "version", "releaseSequence", "compatibility"].includes(
        key,
      )
    )
  ) fail("invalid-metadata");
  return value as unknown as FeaturePackageAuthoringMetadata;
}

async function createNewFile(path: string, bytes: Uint8Array): Promise<void> {
  let file: Deno.FsFile;
  try {
    file = await Deno.open(path, { write: true, createNew: true });
  } catch (error: unknown) {
    if (error instanceof Deno.errors.AlreadyExists) fail("output-exists");
    fail("file-operation-failed");
  }
  try {
    let offset = 0;
    while (offset < bytes.length) {
      const written = await file.write(bytes.subarray(offset));
      if (written === 0) fail("file-operation-failed");
      offset += written;
    }
  } catch (error: unknown) {
    if (error instanceof CliError) throw error;
    fail("file-operation-failed");
  } finally {
    file.close();
  }
}

function asPolicy(value: Uint8Array): FeaturePackagePolicy {
  try {
    return parseFeaturePackagePolicyJson(value);
  } catch (error: unknown) {
    if (error instanceof FeaturePackageError && error.code === "size-limit") {
      fail("input-too-large");
    }
    fail("invalid-policy");
  }
}

async function runPack(
  args: readonly string[],
): Promise<FeaturePackageCliResult> {
  const flags = parsePairs(args, PACK_FLAGS);
  const [metadataBytes, sourceBytes, keyBytes] = await Promise.all([
    readBounded(flags.metadata, FEATURE_PACKAGE_METADATA_MAX_BYTES),
    readBounded(flags.source, FEATURE_PACKAGE_SOURCE_MAX_BYTES),
    readBounded(flags["key-pkcs8"], FEATURE_PACKAGE_PKCS8_MAX_BYTES),
  ]);
  if (sourceBytes.length === 0) fail("invalid-source");
  if (keyBytes.length === 0) fail("invalid-key");
  const source = decodeText(sourceBytes, "invalid-source");
  const metadata = parseMetadata(metadataBytes);
  let privateKey: CryptoKey;
  try {
    privateKey = await crypto.subtle.importKey(
      "pkcs8",
      arrayBuffer(keyBytes),
      { name: "Ed25519" },
      false,
      ["sign"],
    );
  } catch {
    fail("invalid-key");
  }
  const packageBytes = await signFeaturePackage(
    metadata,
    source,
    flags["key-id"],
    privateKey,
  );
  if (packageBytes.length > FEATURE_PACKAGE_MAX_BYTES) fail("input-too-large");
  await createNewFile(flags.out, packageBytes);
  return {
    exitCode: 0,
    stdout: JSON.stringify({ written: true, bytes: packageBytes.length }),
    stderr: "",
  };
}

async function runVerify(
  args: readonly string[],
): Promise<FeaturePackageCliResult> {
  const flags = parsePairs(args, VERIFY_FLAGS);
  const [packageBytes, policyBytes] = await Promise.all([
    readBounded(flags.package, FEATURE_PACKAGE_MAX_BYTES),
    readBounded(flags.policy, FEATURE_PACKAGE_POLICY_MAX_BYTES),
  ]);
  const policy = asPolicy(policyBytes);
  let verified;
  try {
    verified = await verifySignedFeaturePackage(packageBytes, policy);
  } catch (error: unknown) {
    if (error instanceof FeaturePackageError) fail("package-rejected");
    fail("package-rejected");
  }
  return {
    exitCode: 0,
    stdout: JSON.stringify({
      valid: true,
      featureId: verified.manifest.featureId,
      version: verified.manifest.version,
      releaseSequence: verified.manifest.releaseSequence,
      keyId: verified.keyId,
      packageSha256: verified.packageSha256,
    }),
    stderr: "",
  };
}

export async function executeFeaturePackageCli(
  args: readonly string[],
): Promise<FeaturePackageCliResult> {
  if (args.length === 0 || (args.length === 1 && args[0] === "--help")) {
    return { exitCode: 0, stdout: HELP.trimEnd(), stderr: "" };
  }
  try {
    const [command, ...rest] = args;
    if (command === "pack") return await runPack(rest);
    if (command === "verify") return await runVerify(rest);
    fail("invalid-arguments");
  } catch (error: unknown) {
    const code = error instanceof CliError
      ? error.code
      : "file-operation-failed";
    if (error instanceof FeaturePackageError) {
      const packageCode: FeaturePackageCliErrorCode =
        error.code === "invalid-source"
          ? "invalid-source"
          : error.code === "invalid-signing-key"
          ? "invalid-key"
          : error.code === "invalid-metadata"
          ? "invalid-metadata"
          : error.code === "size-limit"
          ? "input-too-large"
          : "package-rejected";
      return {
        exitCode: 1,
        stdout: "",
        stderr: JSON.stringify({
          error: packageCode,
          message: CLI_MESSAGES[packageCode],
        }),
      };
    }
    const message = CLI_MESSAGES[code];
    return {
      exitCode: 1,
      stdout: "",
      stderr: JSON.stringify({ error: code, message }),
    };
  }
}

if (import.meta.main) {
  const result = await executeFeaturePackageCli(Deno.args);
  if (result.stdout) console.log(result.stdout);
  if (result.stderr) console.error(result.stderr);
  Deno.exitCode = result.exitCode;
}
