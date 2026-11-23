// SPDX-License-Identifier: MPL-2.0

/**
 * Types for the local FDH-prepared-runtime control receipt.
 *
 * This is a selection/intent record, not a compiler, native, signature or
 * release attestation. It exists only when an explicit registry source was
 * validated, copied and relinked by `prepareRuntime`.
 */

export type PreparedRuntimePlatform = "windows" | "linux" | "macos";
export type PreparedRuntimeArchitecture = "x86_64" | "aarch64" | "universal";

export interface PreparedRuntimeMarkerIdentity {
  schemaVersion: 1;
  distribution: "debug";
  platform: PreparedRuntimePlatform;
  architecture: PreparedRuntimeArchitecture;
  artifactName: string;
  artifactSize: number;
  artifactSha256: string;
  version: string;
  buildId: string;
}

export interface PreparedRuntimeIdentity {
  artifactName: string;
  artifactSize: number;
  artifactSha256: string;
  version: string;
  buildId: string;
}

export interface PreparedRuntimeReceipt {
  schemaVersion: 1;
  kind: "fdh-prepared-runtime-control";
  distribution: "debug";
  platform: PreparedRuntimePlatform;
  architecture: PreparedRuntimeArchitecture;
  /** Normalized absolute path of the explicit registry source. */
  sourcePath: string;
  preparedAt: string;
  identity: PreparedRuntimeIdentity;
}

export type PreparedRuntimeReceiptState =
  | { status: "absent" }
  | { status: "present"; receipt: PreparedRuntimeReceipt }
  | { status: "invalid"; reason: string };

export interface PreparedRuntimeTreeIdentity {
  version: string;
  buildId: string;
}

export interface PreparedRuntimeTarget {
  platform: PreparedRuntimePlatform;
  architecture: PreparedRuntimeArchitecture;
}

export interface ApplicationIniIdentity {
  name: string;
  version: string;
  buildId: string;
}

export interface AppConstantsIdentity {
  debug: boolean;
  appName: string;
  version: string;
  buildId: string;
}
