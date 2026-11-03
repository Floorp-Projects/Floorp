// SPDX-License-Identifier: MPL-2.0
// Build genuine authored framework apps from the project's exact cached packages.
/** @import {BinaryInput,FrameworkMetadata,FrameworkPackage,ProjectNpmLock,PackageManifest} from "./assessment-test-types.ts" */
import { createHash } from "node:crypto";
import { execFileSync } from "node:child_process";
import {
  copyFileSync,
  globSync,
  mkdirSync,
  readdirSync,
  readFileSync,
  writeFileSync,
} from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
const root = resolve(dirname(fileURLToPath(import.meta.url)), "../..");
const tools = join(root, "tools/chrome-extension-runtime");
/** @param {BinaryInput} bytes */
const digest = (bytes) => createHash("sha256").update(bytes).digest("hex");
/** @param {string} output @returns {FrameworkMetadata} */
export function buildDevtoolsFixture(output) {
  mkdirSync(output, { recursive: true });
  const lock = /** @type {ProjectNpmLock} */ (JSON.parse(
    readFileSync(join(root, "deno.lock"), "utf8"),
  )).npm;
  /** @type {Record<string,FrameworkPackage>} */
  const packages = {};
  for (
    const [name, version] of Object.entries({
      react: "19.2.6",
      "react-dom": "19.2.6",
      redux: "5.0.1",
      scheduler: "0.27.0",
    })
  ) {
    const directory = join(
      root,
      "node_modules/.deno",
      name + "@" + version,
      "node_modules",
      name,
    );
    const manifest = /** @type {PackageManifest} */ (JSON.parse(
      readFileSync(join(directory, "package.json"), "utf8"),
    ));
    if (manifest.name !== name || manifest.version !== version) {
      throw Error("Unexpected cached framework version");
    }
    const [key, record] = Object.entries(lock).find(([key]) =>
      key === name + "@" + version || key.startsWith(name + "@" + version + "_")
    ) || [];
    if (!key || !record?.integrity) {
      throw Error("Framework absent from existing project lock");
    }
    /** @type {string[]} */
    const entries = [];
    /** @param {string} path @param {string} [prefix] */
    const visit = (path, prefix = "") => {
      for (
        const entry of readdirSync(path, { withFileTypes: true }).sort((a, b) =>
          a.name.localeCompare(b.name)
        )
      ) {
        if (entry.name === "node_modules") {
          continue;
        }
        const relative = prefix + entry.name;
        if (entry.isDirectory()) {
          visit(join(path, entry.name), relative + "/");
        } else if (entry.isFile()) {
          entries.push(
            relative + ":" + digest(readFileSync(join(path, entry.name))),
          );
        } else throw Error("Unexpected non-file framework cache entry");
      }
    };
    visit(directory);
    packages[name] = {
      version,
      projectLockKey: key,
      projectLockIntegrity: record.integrity,
      cachedFilesSha256: digest(entries.join("\n")),
      cachedFileCount: entries.length,
    };
  }
  const esbuild = [...globSync(
    join(
      root,
      "node_modules/.deno/esbuild@*/node_modules/esbuild/bin/esbuild",
    ),
  )][0];
  if (!esbuild) throw Error("Existing project esbuild is unavailable");
  execFileSync("node", [
    esbuild,
    join(tools, "fixtures/devtools/app.mjs"),
    "--bundle",
    "--format=iife",
    '--define:process.env.NODE_ENV="development"',
    ...["react", "react-dom", "redux"].map((name) =>
      "--alias:" + name + "=" +
      join(
        root,
        "node_modules/.deno",
        name + "@" + packages[name].version,
        "node_modules",
        name,
      )
    ),
    "--outfile=" + join(output, "app.js"),
  ]);
  copyFileSync(
    join(tools, "fixtures/devtools/index.html"),
    join(output, "index.html"),
  );
  const metadata = {
    source:
      "Existing project npm dependency cache, pinned by deno.lock; vendor archives have not been independently downloaded for this run",
    packages,
    htmlSha256: digest(readFileSync(join(output, "index.html"))),
    appSha256: digest(readFileSync(join(output, "app.js"))),
  };
  writeFileSync(
    join(output, "frameworks.json"),
    JSON.stringify(metadata, null, 2),
  );
  return metadata;
}
if (
  process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)
) {
  if (!process.argv[2]) throw Error("Output directory is required");
  console.log(JSON.stringify(buildDevtoolsFixture(resolve(process.argv[2]))));
}
