// SPDX-License-Identifier: MPL-2.0
/** Synthetic filesystem-only Git effect; host tests retain NO run/env permission. */
import * as path from "@std/path";
import { prepareRuntimeBootstrap } from "./prepared_runtime_bootstrap.ts";
import type { PreparedRuntimeBootstrapRunner } from "./prepared_runtime_bootstrap_types.ts";
const nl = String.fromCharCode(10);
const patchFile = path.fromFileUrl(
  new URL("../patches/browser-modules-BrowserGlue.sys.patch", import.meta.url),
);
const runPatch: PreparedRuntimeBootstrapRunner = (root, checkOnly) => {
  const target = path.join(
    root,
    "moz-src/browser/components/BrowserGlue.sys.mjs",
  );
  const original = Deno.readTextFileSync(target);
  const context = [
    'import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";',
    'import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";',
    "",
    "const lazy = {};",
    "",
    "ChromeUtils.defineESModuleGetters(lazy, {",
  ].join(nl);
  if (!original.includes(context)) return false;
  if (!checkOnly) {
    const added = Deno.readTextFileSync(patchFile).split(nl).filter((line) =>
      line.startsWith("+") && !line.startsWith("+++")
    ).map((line) => line.slice(1)).join(nl);
    Deno.writeTextFileSync(
      target,
      original.replace("const lazy = {};", added + nl + "const lazy = {};"),
    );
  }
  return true;
};
export function prepareSyntheticRuntimeBootstrap(runtimeDir: string) {
  return prepareRuntimeBootstrap(runtimeDir, { runPatch });
}
