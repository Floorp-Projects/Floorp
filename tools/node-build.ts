// SPDX-License-Identifier: MPL-2.0

import { spawnSync } from "node:child_process";
import { randomBytes } from "node:crypto";
import {
  mkdirSync,
  readdirSync,
  readFileSync,
  rmSync,
  symlinkSync,
  writeFileSync,
} from "node:fs";
import { createRequire } from "node:module";
import * as path from "node:path";
import process from "node:process";
import { fileURLToPath } from "node:url";
import {
  createNodeBuildPlan,
  PACKAGED_ASSET_MOUNTS,
} from "./src/node_build_plan.ts";
import { packagedQaEnabled } from "./src/packaged_qa_contract.ts";
import { SOURCE_LINKS } from "./src/source_link_plan.ts";
import type {
  NodeBuildCliOptions,
  NodeBuildCommand,
} from "./src/node_build_types.ts";

const PROJECT_ROOT = path.resolve(
  path.dirname(fileURLToPath(import.meta.url)),
  "..",
);

function usage(): string {
  return [
    "Usage: node tools/node-build.ts [--mode dev|test|stage|production] [--buildid2 <uuid-v7>] [--version <version>]",
    "",
    "Builds browser feature assets with Node.js only. This is the Phase 1",
    "migration entry point; tools/feles-build.ts remains the compatibility entry.",
  ].join("\n");
}

function parseCliArgs(args: readonly string[]): NodeBuildCliOptions {
  const options: NodeBuildCliOptions = { mode: "dev", help: false };
  for (let index = 0; index < args.length; index++) {
    const arg = args[index];
    if (arg === "--help" || arg === "-h") {
      options.help = true;
      continue;
    }
    if (arg !== "--mode" && arg !== "--buildid2" && arg !== "--version") {
      throw new Error(`Unknown argument: ${arg}\n${usage()}`);
    }
    const value = args[index + 1];
    if (!value || value.startsWith("--")) {
      throw new Error(`${arg} requires a value\n${usage()}`);
    }
    index++;
    if (arg === "--mode") options.mode = value;
    else if (arg === "--buildid2") options.buildid2 = value;
    else options.version = value;
  }
  return options;
}

function generateUuidV7(now = Date.now()): string {
  const bytes = randomBytes(16);
  const timestamp = BigInt(now);
  bytes[0] = Number((timestamp >> 40n) & 0xffn);
  bytes[1] = Number((timestamp >> 32n) & 0xffn);
  bytes[2] = Number((timestamp >> 24n) & 0xffn);
  bytes[3] = Number((timestamp >> 16n) & 0xffn);
  bytes[4] = Number((timestamp >> 8n) & 0xffn);
  bytes[5] = Number(timestamp & 0xffn);
  bytes[6] = (bytes[6] & 0x0f) | 0x70;
  bytes[8] = (bytes[8] & 0x3f) | 0x80;
  const hex = [...bytes].map((byte) => byte.toString(16).padStart(2, "0"));
  return `${hex.slice(0, 4).join("")}-${hex.slice(4, 6).join("")}-${
    hex.slice(6, 8).join("")
  }-${hex.slice(8, 10).join("")}-${hex.slice(10, 16).join("")}`;
}

/** Return whether a build ID is a lowercase UUIDv7 accepted by release provenance. */
function isUuidV7(value: string): boolean {
  return /^[0-9a-f]{8}-[0-9a-f]{4}-7[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/
    .test(value);
}

function packageVersion(): string {
  const packageJson = JSON.parse(
    readFileSync(path.join(PROJECT_ROOT, "package.json"), "utf8"),
  ) as { version?: unknown };
  if (typeof packageJson.version !== "string" || !packageJson.version) {
    throw new Error("Root package.json does not contain a version string.");
  }
  return packageJson.version;
}

function resolvePackageBin(packageName: string, binName = packageName): string {
  const require = createRequire(path.join(PROJECT_ROOT, "package.json"));
  const packageJsonPath = require.resolve(`${packageName}/package.json`);
  const packageJson = JSON.parse(readFileSync(packageJsonPath, "utf8")) as {
    bin?: string | Record<string, string>;
  };
  const bin = typeof packageJson.bin === "string"
    ? packageJson.bin
    : packageJson.bin?.[binName];
  if (!bin) {
    throw new Error(`Package ${packageName} does not expose bin ${binName}.`);
  }
  return path.join(path.dirname(packageJsonPath), bin);
}

function runBuildCommand(buildCommand: NodeBuildCommand): void {
  console.log(
    `[node-build] ${buildCommand.name}: ${buildCommand.command} ${
      buildCommand.args.join(" ")
    }`,
  );
  const result = spawnSync(buildCommand.command, [...buildCommand.args], {
    cwd: buildCommand.cwd,
    encoding: "utf8",
    env: process.env,
    maxBuffer: 16 * 1024 * 1024,
  });
  if (result.error) {
    throw new Error(
      `${buildCommand.name} failed to start: ${result.error.message}`,
    );
  }
  if (result.status !== 0) {
    throw new Error(
      `${buildCommand.name} failed with exit ${result.status}\nSTDOUT:\n${result.stdout}\nSTDERR:\n${result.stderr}`,
    );
  }
}

function writeBuildid2(buildid2: string): void {
  const output = path.join(PROJECT_ROOT, "_dist", "buildid2");
  mkdirSync(path.dirname(output), { recursive: true });
  writeFileSync(output, buildid2);
}

function removePath(target: string): void {
  rmSync(target, { recursive: true, force: true });
}

function createDirectoryLink(link: string, target: string): void {
  removePath(link);
  try {
    symlinkSync(target, link);
    return;
  } catch (error) {
    if (process.platform === "win32") {
      try {
        symlinkSync(target, link, "junction");
        return;
      } catch (junctionError) {
        throw new Error(
          `Failed to create directory junction ${link} -> ${target}: ${
            junctionError instanceof Error
              ? junctionError.message
              : String(junctionError)
          }`,
        );
      }
    }
    throw new Error(
      `Failed to create symlink ${link} -> ${target}: ${
        error instanceof Error ? error.message : String(error)
      }`,
    );
  }
}

/** Recreate bridge source links required before any feature compilation starts. */
function createSourceLinks(): void {
  for (const { link, target } of SOURCE_LINKS) {
    createDirectoryLink(
      path.join(PROJECT_ROOT, ...link),
      path.join(PROJECT_ROOT, ...target),
    );
  }
}

function createPackagedAssetLinks(): void {
  const outputDirectory = path.join(PROJECT_ROOT, "_dist", "noraneko");
  removePath(outputDirectory);
  mkdirSync(outputDirectory, { recursive: true });

  for (const [name, target] of PACKAGED_ASSET_MOUNTS) {
    createDirectoryLink(
      path.join(outputDirectory, name),
      path.resolve(PROJECT_ROOT, target),
    );
  }
}

function registerPackagedQa(contentDirectory: string): string[] {
  const manifestPath = path.join(contentDirectory, "jar.mn");
  const manifest = readFileSync(manifestPath, "utf8");
  if (!manifest.includes("% content noraneko %nora-content/")) {
    throw new Error("Expected the generated production content manifest.");
  }
  if (manifest.includes("nora-content/qa/")) {
    throw new Error(
      "QA assets are already registered; rebuild production assets first.",
    );
  }

  const files: string[] = [];
  const visit = (relative: string): void => {
    for (
      const entry of readdirSync(path.join(contentDirectory, relative), {
        withFileTypes: true,
      })
    ) {
      if (entry.isSymbolicLink() || !/^[A-Za-z0-9._-]+$/.test(entry.name)) {
        throw new Error(`Unexpected QA asset: ${entry.name}`);
      }
      const child = `${relative}/${entry.name}`;
      if (entry.isDirectory()) visit(child);
      else if (entry.isFile()) files.push(child);
    }
  };
  visit("qa");

  for (const required of ["qa/packaged-tests.js", "qa/qa-test-manifest.json"]) {
    if (!files.includes(required)) {
      throw new Error(`Missing QA asset: ${required}`);
    }
  }
  files.sort();
  writeFileSync(
    manifestPath,
    `${manifest.trimEnd()}\n${
      files.map((file) => ` nora-content/${file} (${file})`).join("\n")
    }\n`,
  );
  return files;
}

function main(): void {
  const options = parseCliArgs(process.argv.slice(2));
  if (options.help) {
    console.log(usage());
    return;
  }

  const packagedQa = packagedQaEnabled(
    options.mode.trim().toLowerCase(),
    process.env.FLOORP_PACKAGED_QA,
    process.env.FLOORP_QA_TEST_MODULES,
  );
  const buildid2 = options.buildid2 ?? generateUuidV7();
  if (!isUuidV7(buildid2)) {
    throw new Error("--buildid2 must be a UUIDv7 value.");
  }
  const version = options.version ?? packageVersion();
  const tsdown = resolvePackageBin("tsdown");
  const plan = createNodeBuildPlan({
    mode: options.mode,
    buildid2,
    version,
    projectRoot: PROJECT_ROOT,
    nodeExecutable: process.execPath,
    tsdownExecutable: tsdown,
    packagedQa,
  });

  console.log(`[node-build] Building features with mode=${plan.mode}`);
  createSourceLinks();
  writeBuildid2(buildid2);
  for (const buildCommand of plan.commands) {
    runBuildCommand(buildCommand);
  }

  if (plan.packagedQa) {
    const files = registerPackagedQa(
      path.join(PROJECT_ROOT, "bridge/loader-features/_dist"),
    );
    console.log(`[node-build] Registered ${files.length} packaged QA assets.`);
  }

  if (plan.usesPackagedAssets) {
    createPackagedAssetLinks();
  }
  console.log("[node-build] Build complete.");
}

try {
  main();
} catch (error) {
  console.error(
    `[node-build] ERROR: ${
      error instanceof Error ? error.message : String(error)
    }`,
  );
  process.exit(1);
}
