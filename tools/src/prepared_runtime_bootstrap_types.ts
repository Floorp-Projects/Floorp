// SPDX-License-Identifier: MPL-2.0

export type PreparedRuntimeBootstrapRunner = (
  runtimeDir: string,
  checkOnly: boolean,
) => boolean;

export interface PreparedRuntimeBootstrapOptions {
  /** Test-only dependency injection; never exposed as a CLI override. */
  readonly runPatch?: PreparedRuntimeBootstrapRunner;
}

/** Local JS hook evidence only; not native compiler/signature attestation. */
export interface PreparedRuntimeBootstrapResult {
  readonly status: "applied" | "already";
  readonly targetRelativePath: "moz-src/browser/components/BrowserGlue.sys.mjs";
  readonly patchSha256: string;
  readonly targetSha256: string;
}
