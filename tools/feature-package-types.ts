// SPDX-License-Identifier: MPL-2.0

export interface FeaturePackageCliResult {
  readonly exitCode: number;
  readonly stdout: string;
  readonly stderr: string;
}

export type FeaturePackageCliErrorCode =
  | "invalid-arguments"
  | "invalid-metadata"
  | "invalid-policy"
  | "invalid-source"
  | "invalid-key"
  | "input-too-large"
  | "output-exists"
  | "file-operation-failed"
  | "package-rejected";

export interface FeaturePackageCliFlags {
  readonly [name: string]: string;
}
