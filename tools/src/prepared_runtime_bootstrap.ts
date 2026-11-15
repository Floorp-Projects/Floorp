// SPDX-License-Identifier: MPL-2.0

import * as path from "@std/path";
import type {
  PreparedRuntimeBootstrapOptions,
  PreparedRuntimeBootstrapResult,
} from "./prepared_runtime_bootstrap_types.ts";

const TARGET = "moz-src/browser/components/BrowserGlue.sys.mjs";
const PATCH = path.fromFileUrl(
  new URL("../patches/browser-modules-BrowserGlue.sys.patch", import.meta.url),
);
const COMMAND_ROOT = path.fromFileUrl(new URL("../../", import.meta.url));
const LIMIT = 2 * 1024 * 1024;
const START = "/*@ NORA INJECT START @*/";
const END = "/*@ NORA INJECT END @*/";
const MODULE = "resource://noraneko/modules/NoranekoStartup.sys.mjs";
const BLOCK = [
  START,
  "try {",
  '  ChromeUtils.importESModule("' + MODULE + '");',
  "} catch (e) {",
  '  console.error("Failed to import noraneko BrowserGlue.sys.mjs", e);',
  "}",
  END,
  "",
  "",
].join("\n");

function plainPath(file: string): void {
  let current = path.resolve(file);
  while (true) {
    const info = Deno.lstatSync(current);
    if (info.isSymlink) {
      throw new Error("Runtime bootstrap rejects aliased paths");
    }
    const parent = path.dirname(current);
    if (parent === current) break;
    current = parent;
  }
}

function boundedText(file: string): string {
  plainPath(file);
  const opened = Deno.openSync(file, { read: true });
  try {
    const info = opened.statSync();
    if (
      !info.isFile || info.size > LIMIT ||
      (info.nlink !== null && info.nlink !== 1)
    ) {
      throw new Error(
        "Runtime bootstrap requires a bounded regular single-link file",
      );
    }
    const bytes = new Uint8Array(LIMIT + 1);
    let used = 0;
    while (used < bytes.length) {
      const count = opened.readSync(bytes.subarray(used));
      if (count === null) break;
      if (count === 0) {
        throw new Error("Runtime bootstrap read made no progress");
      }
      used += count;
    }
    if (used > LIMIT) {
      throw new Error("Runtime bootstrap input grew beyond its limit");
    }
    return new TextDecoder("utf-8", { fatal: true, ignoreBOM: true }).decode(
      bytes.subarray(0, used),
    );
  } finally {
    opened.close();
  }
}

function count(text: string, part: string): number {
  return text.split(part).length - 1;
}
function validHook(text: string): boolean {
  return count(text, BLOCK) === 1 && count(text, START) === 1 &&
    count(text, END) === 1 && count(text, MODULE) === 1;
}
async function digest(text: string): Promise<string> {
  return Array.from(
    new Uint8Array(
      await crypto.subtle.digest("SHA-256", new TextEncoder().encode(text)),
    ),
    (byte) => byte.toString(16).padStart(2, "0"),
  ).join("");
}

function gitPatch(runtimeDir: string, checkOnly: boolean): boolean {
  const args = [
    "-c",
    "core.autocrlf=false",
    "-c",
    "core.eol=lf",
    "apply",
    ...(checkOnly ? ["--check"] : []),
    "--unsafe-paths",
    "--directory",
    path.resolve(runtimeDir).replaceAll(String.fromCharCode(92), "/"),
    PATCH,
  ];
  return new Deno.Command("git", {
    args,
    cwd: COMMAND_ROOT,
    stdin: "null",
    stdout: "null",
    stderr: "null",
  }).outputSync().success;
}

/** Apply only the existing startup patch; the unrelated full patcher guard stays on. */
export async function prepareRuntimeBootstrap(
  runtimeDir: string,
  options: PreparedRuntimeBootstrapOptions = {},
): Promise<PreparedRuntimeBootstrapResult> {
  const runPatch = options.runPatch ?? gitPatch;
  const target = path.join(runtimeDir, TARGET);
  const original = boundedText(target);
  const patch = boundedText(PATCH);
  let status: PreparedRuntimeBootstrapResult["status"] = "already";
  if (!validHook(original)) {
    if ([START, END, MODULE].some((marker) => original.includes(marker))) {
      throw new Error("Runtime bootstrap hook is conflicting or incomplete");
    }
    const checked = runPatch(runtimeDir, true);
    if (!checked) {
      throw new Error(
        "Existing Runtime bootstrap patch does not apply cleanly",
      );
    }
    if (boundedText(target) !== original || boundedText(PATCH) !== patch) {
      throw new Error(
        "Runtime bootstrap inputs changed during patch validation",
      );
    }
    const applied = runPatch(runtimeDir, false);
    if (!applied) {
      throw new Error("Runtime bootstrap patch was not applied");
    }
    status = "applied";
  }
  const after = boundedText(target);
  if (
    !validHook(after) ||
    (status === "applied" && after.replace(BLOCK, "") !== original)
  ) {
    throw new Error("Runtime bootstrap patch changed unexpected content");
  }
  return {
    status,
    targetRelativePath: TARGET,
    patchSha256: await digest(patch),
    targetSha256: await digest(after),
  };
}
