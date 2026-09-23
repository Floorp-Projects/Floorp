// SPDX-License-Identifier: MPL-2.0

export interface PrepareRuntimeOptions {
  floorpRoot: string;
  runtimeRoot: string;
  mode: "check" | "apply";
}

export interface RuntimeSourceFile {
  source: string;
  destination: string;
}

export interface PrepareRuntimeResult {
  state: "ready" | "applied" | "already-applied";
  runtimeRoot: string;
  runtimeCommit: string;
  manifestPath: string;
  files: readonly string[];
}

export interface RuntimePreparationManifest {
  schemaVersion: 1;
  runtimeRoot: string;
  runtimeCommit: string;
  patchSha256: string;
  sourceHashes: Record<string, string>;
  outputHashes: Record<string, string>;
}
