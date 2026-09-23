// SPDX-License-Identifier: MPL-2.0

import { parseArgs } from "@std/cli/parse-args";
import { dirname, fromFileUrl, join, resolve } from "@std/path";
import type {
  PrepareRuntimeOptions,
  PrepareRuntimeResult,
  RuntimePreparationManifest,
  RuntimeSourceFile,
} from "./prepare-runtime-types.ts";

const NATIVE_ROOT = "native/macos-app-shim";
const PATCH_PATH = `${NATIVE_ROOT}/runtime/integration.patch`;
const DESTINATION = "widget/cocoa/appshim";
const MANIFEST_DIRECTORY = "_dist/app-shim-runtime";

/** Explicit host and standalone Shim staging; no tests or arbitrary tree copying. */
export const RUNTIME_SOURCE_FILES: readonly RuntimeSourceFile[] = [
  ...[
    "Protocol",
    "MachTransport",
    "PeerIdentity",
    "Session",
  ].flatMap((
    name,
  ) =>
    ["h", "mm"].map((extension) => ({
      source: `${NATIVE_ROOT}/${name}.${extension}`,
      destination: `${DESTINATION}/${name}.${extension}`,
    }))
  ),
  ...[
    "BundleTransaction",
    "MacWebAppService",
    "MacWebAppWidget",
    "MacWebAppPresentation",
    "MacWebAppInput",
  ].flatMap(
    (name) =>
      ["h", "mm"].map((extension) => ({
        source: `${NATIVE_ROOT}/runtime/${name}.${extension}`,
        destination: `${DESTINATION}/${name}.${extension}`,
      })),
  ),
  {
    source: `${NATIVE_ROOT}/runtime/nsIMacWebAppService.idl`,
    destination: "widget/nsIMacWebAppService.idl",
  },
  ...[
    "Protocol",
    "MachTransport",
    "PeerIdentity",
    "Session",
    "ColdLaunch",
    "ShimApplication",
    "ShimView",
  ].flatMap((name) =>
    ["h", "mm"].map((extension) => ({
      source: `${NATIVE_ROOT}/${name}.${extension}`,
      destination: `${DESTINATION}/shim/${name}.${extension}`,
    }))
  ),
  {
    source: `${NATIVE_ROOT}/main.mm`,
    destination: `${DESTINATION}/shim/main.mm`,
  },
  {
    source: `${NATIVE_ROOT}/runtime/shim.moz.build`,
    destination: `${DESTINATION}/shim/moz.build`,
  },
];

const PATCH_TARGETS = new Set([
  "accessible/mac/AccessibleWrap.mm",
  "widget/moz.build",
  "widget/cocoa/moz.build",
  "widget/cocoa/components.conf",
  "widget/cocoa/nsCocoaWindow.mm",
  "widget/cocoa/NativeMenuSupport.mm",
  "widget/cocoa/nsMenuUtilsX.mm",
  "widget/nsIWidget.h",
  "widget/nsIWidget.cpp",
  "xpfe/appshell/AppWindow.cpp",
  "toolkit/content/globalOverlay.js",
  "browser/installer/package-manifest.in",
  "browser/app/macbuild/Contents/MacOS-files.in",
  "gfx/layers/NativeLayerCA.h",
  "gfx/layers/NativeLayerCA.mm",
  "gfx/webrender_bindings/RenderCompositorNative.cpp",
]);

const decoder = new TextDecoder();

async function sha256(bytes: Uint8Array): Promise<string> {
  const digest = await crypto.subtle.digest("SHA-256", new Uint8Array(bytes));
  return Array.from(
    new Uint8Array(digest),
    (byte) => byte.toString(16).padStart(2, "0"),
  ).join("");
}

async function info(path: string): Promise<Deno.FileInfo | null> {
  try {
    return await Deno.lstat(path);
  } catch (error) {
    if (error instanceof Deno.errors.NotFound) return null;
    throw error;
  }
}

/** Reject links in every existing component, including the final file. */
async function checkedPath(root: string, relative: string): Promise<string> {
  const components = relative.split("/");
  if (
    components.some((part) =>
      !/^[A-Za-z0-9_.-]+$/.test(part) || part === ".." || part === "."
    )
  ) {
    throw new Error(`Unsafe relative path: ${relative}`);
  }
  let current = root;
  for (const [index, component] of components.entries()) {
    current = join(current, component);
    const entry = await info(current);
    if (entry?.isSymlink) {
      throw new Error(`Symlink traversal refused: ${current}`);
    }
    if (entry && index < components.length - 1 && !entry.isDirectory) {
      throw new Error(`Expected a directory: ${current}`);
    }
  }
  return current;
}

async function checkedRoot(path: string): Promise<string> {
  const absolute = resolve(path);
  const entry = await info(absolute);
  if (!entry?.isDirectory || entry.isSymlink) {
    throw new Error(`Expected a real checkout directory: ${absolute}`);
  }
  return await Deno.realPath(absolute);
}

async function readFile(root: string, relative: string): Promise<Uint8Array> {
  const path = await checkedPath(root, relative);
  if (!(await info(path))?.isFile) {
    throw new Error(`Missing regular file: ${path}`);
  }
  return await Deno.readFile(path);
}

async function git(
  cwd: string,
  args: string[],
  input?: Uint8Array,
): Promise<Deno.CommandOutput> {
  const child = new Deno.Command("git", {
    cwd,
    args: ["--no-optional-locks", ...args],
    stdin: input ? "piped" : "null",
    stdout: "piped",
    stderr: "piped",
  }).spawn();
  const output = child.output();
  if (input) {
    const writer = child.stdin.getWriter();
    await writer.write(input);
    await writer.close();
  }
  return await output;
}

async function gitChecked(
  cwd: string,
  args: string[],
  input?: Uint8Array,
): Promise<string> {
  const result = await git(cwd, args, input);
  if (!result.success) {
    throw new Error(
      `git ${args.join(" ")} failed: ${decoder.decode(result.stderr).trim()}`,
    );
  }
  return decoder.decode(result.stdout);
}

function object(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function hashRecord(value: unknown): value is Record<string, string> {
  return object(value) &&
    Object.values(value).every((hash) =>
      typeof hash === "string" && /^[a-f0-9]{64}$/.test(hash)
    );
}

function parseManifest(bytes: Uint8Array): RuntimePreparationManifest {
  const value: unknown = JSON.parse(decoder.decode(bytes));
  if (
    !object(value) || value.schemaVersion !== 1 ||
    typeof value.runtimeRoot !== "string" ||
    typeof value.runtimeCommit !== "string" ||
    typeof value.patchSha256 !== "string" || !hashRecord(value.sourceHashes) ||
    !hashRecord(value.outputHashes)
  ) {
    throw new Error("Invalid App Shim runtime preparation manifest");
  }
  return value as unknown as RuntimePreparationManifest;
}

function sameHashes(
  first: Record<string, string>,
  second: Record<string, string>,
): boolean {
  const keys = Object.keys(first).sort();
  return keys.length === Object.keys(second).length &&
    keys.every((key) => first[key] === second[key]);
}

async function outputHashes(
  root: string,
  files: readonly string[],
): Promise<Record<string, string>> {
  return Object.fromEntries(
    await Promise.all(
      files.map(async (
        file,
      ) => [file, await sha256(await readFile(root, file))]),
    ),
  );
}

async function patchFiles(root: string, patch: Uint8Array): Promise<string[]> {
  // Only modifications of these existing files are supported; no mode changes,
  // renames, removals, binary files, or new files can bypass the staging list.
  if (
    /^(?:new file mode|deleted file mode|old mode|new mode|rename |copy |GIT binary patch)/m
      .test(decoder.decode(patch))
  ) {
    throw new Error(
      "Runtime integration patch must only modify existing text files",
    );
  }
  const numstat = await gitChecked(
    root,
    ["apply", "--numstat", "-z", "-"],
    patch,
  );
  const files = numstat.split("\0").filter(Boolean).map((line) => {
    const match = /^(\d+)\t(\d+)\t(.+)$/.exec(line);
    if (!match || !PATCH_TARGETS.has(match[3])) {
      throw new Error(`Unexpected runtime integration patch target: ${line}`);
    }
    return match[3];
  });
  if (files.length === 0 || new Set(files).size !== files.length) {
    throw new Error(
      "Runtime integration patch must contain unique file changes",
    );
  }
  return files;
}

export async function prepareRuntime(
  options: PrepareRuntimeOptions,
): Promise<PrepareRuntimeResult> {
  const floorpRoot = await checkedRoot(options.floorpRoot);
  const runtimeRoot = await checkedRoot(options.runtimeRoot);
  if (floorpRoot === runtimeRoot) {
    throw new Error("Use a separate Runtime checkout");
  }
  const topLevel =
    (await gitChecked(runtimeRoot, ["rev-parse", "--show-toplevel"])).trim();
  if (await Deno.realPath(topLevel) !== runtimeRoot) {
    throw new Error("Runtime path must be the root of its own Git checkout");
  }
  const lock: unknown = JSON.parse(
    decoder.decode(await readFile(floorpRoot, "floorp-runtime.lock.json")),
  );
  if (
    !object(lock) || !object(lock.source) ||
    typeof lock.source.commit !== "string" ||
    !/^[a-f0-9]{40}$/.test(lock.source.commit)
  ) {
    throw new Error("Missing exact source.commit in floorp-runtime.lock.json");
  }
  const runtimeCommit = (await gitChecked(runtimeRoot, ["rev-parse", "HEAD"]))
    .trim();
  if (runtimeCommit !== lock.source.commit) {
    throw new Error(
      `Runtime HEAD ${runtimeCommit} does not match locked commit ${lock.source.commit}`,
    );
  }

  const patch = await readFile(floorpRoot, PATCH_PATH);
  const patchSha256 = await sha256(patch);
  const modifiedFiles = await patchFiles(runtimeRoot, patch);
  const sources = new Map<string, Uint8Array>();
  const sourceContents = new Map<string, Uint8Array>();
  for (const { source, destination } of RUNTIME_SOURCE_FILES) {
    let bytes = sourceContents.get(source);
    if (!bytes) {
      bytes = await readFile(floorpRoot, source);
      sourceContents.set(source, bytes);
    }
    sources.set(destination, bytes);
  }
  const sourceHashes = Object.fromEntries(
    await Promise.all(
      [...sources].map(async (
        [destination, bytes],
      ) => [destination, await sha256(bytes)]),
    ),
  );
  const files = [...modifiedFiles, ...sources.keys()].sort();
  for (const file of files) await checkedPath(runtimeRoot, file);
  const manifestKey = await sha256(new TextEncoder().encode(runtimeRoot));
  const manifestRelative = `${MANIFEST_DIRECTORY}/${manifestKey}.json`;
  const manifestPath = await checkedPath(floorpRoot, manifestRelative);
  const existingManifest = await info(manifestPath);
  const staged = await gitChecked(runtimeRoot, [
    "diff",
    "--cached",
    "--name-only",
    "-z",
    "--",
    ...files,
  ]);
  if (staged.length > 0) {
    throw new Error("Staged changes exist in Runtime integration paths");
  }

  const result = { runtimeRoot, runtimeCommit, manifestPath, files };
  if (existingManifest) {
    const previous = parseManifest(
      await readFile(floorpRoot, manifestRelative),
    );
    if (
      previous.runtimeRoot !== runtimeRoot ||
      previous.runtimeCommit !== runtimeCommit ||
      previous.patchSha256 !== patchSha256 ||
      !sameHashes(previous.sourceHashes, sourceHashes)
    ) {
      throw new Error(
        "Preparation inputs changed; use a clean isolated Runtime checkout before restaging",
      );
    }
    if (
      !sameHashes(previous.outputHashes, await outputHashes(runtimeRoot, files))
    ) {
      throw new Error(
        "Previously prepared Runtime files were modified; refusing to overwrite them",
      );
    }
    await gitChecked(
      runtimeRoot,
      ["apply", "--reverse", "--check", "-"],
      patch,
    );
    return { ...result, state: "already-applied" };
  }

  for (const file of modifiedFiles) {
    const tracked =
      (await gitChecked(runtimeRoot, ["ls-files", "--stage", "--", file]))
        .trim();
    if (!/^100644 [a-f0-9]{40} 0\t/.test(tracked)) {
      throw new Error(
        `Integration patch target is not a tracked regular file: ${file}`,
      );
    }
  }
  const changed = await gitChecked(runtimeRoot, [
    "diff",
    "HEAD",
    "--name-only",
    "-z",
    "--",
    ...files,
  ]);
  if (changed.length > 0) {
    throw new Error("Unowned changes exist in Runtime integration paths");
  }
  for (const file of sources.keys()) {
    if (await info(await checkedPath(runtimeRoot, file))) {
      throw new Error(
        `Refusing to overwrite an existing Runtime file: ${file}`,
      );
    }
  }
  await gitChecked(runtimeRoot, ["apply", "--check", "-"], patch);
  if (options.mode === "check") return { ...result, state: "ready" };

  // Everything is checked before the first Runtime mutation. On an ordinary
  // failure remove only files created here, then undo exactly this patch.
  const created: string[] = [];
  let patchApplied = false;
  try {
    await Deno.mkdir(dirname(manifestPath), { recursive: true });
    await checkedPath(floorpRoot, manifestRelative);
    await gitChecked(runtimeRoot, ["apply", "-"], patch);
    patchApplied = true;
    for (const [file, bytes] of sources) {
      const destination = await checkedPath(runtimeRoot, file);
      await Deno.mkdir(dirname(destination), { recursive: true });
      await checkedPath(runtimeRoot, file);
      await Deno.writeFile(destination, bytes, { createNew: true });
      created.push(destination);
    }
    const manifest: RuntimePreparationManifest = {
      schemaVersion: 1,
      runtimeRoot,
      runtimeCommit,
      patchSha256,
      sourceHashes,
      outputHashes: await outputHashes(runtimeRoot, files),
    };
    await gitChecked(
      runtimeRoot,
      ["apply", "--reverse", "--check", "-"],
      patch,
    );
    await Deno.writeTextFile(
      manifestPath,
      JSON.stringify(manifest, null, 2) + "\n",
      { createNew: true },
    );
    return { ...result, state: "applied" };
  } catch (error) {
    for (const file of created.reverse()) await Deno.remove(file);
    if (patchApplied) {
      await gitChecked(
        runtimeRoot,
        ["apply", "--reverse", "--check", "-"],
        patch,
      );
      await gitChecked(runtimeRoot, ["apply", "--reverse", "-"], patch);
    }
    throw error;
  }
}

async function main(): Promise<void> {
  const args = parseArgs(Deno.args, {
    boolean: ["help", "check", "apply"],
    string: ["runtime"],
    unknown(argument) {
      throw new Error(`Unknown argument: ${argument}`);
    },
  });
  if (args.help) {
    console.log(
      "Usage: deno task app-shim:prepare-runtime --runtime <isolated-checkout> (--check | --apply)\n" +
        "Validates the locked Runtime revision and stages native source changes.\n" +
        "Does not create a checkout, build Gecko, or enable App Shim capabilities.",
    );
    return;
  }
  if (!args.runtime || args.check === args.apply) {
    throw new Error("Specify --runtime and exactly one of --check or --apply");
  }
  const result = await prepareRuntime({
    floorpRoot: resolve(dirname(fromFileUrl(import.meta.url)), "../.."),
    runtimeRoot: args.runtime,
    mode: args.apply ? "apply" : "check",
  });
  console.log(JSON.stringify(result, null, 2));
  console.log(
    "Native rebuild required: packaged prebuilt Runtime artifacts do not contain these changes.",
  );
}

if (import.meta.main) await main();
