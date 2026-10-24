// SPDX-License-Identifier: MPL-2.0
/** @import {FrameworkReference,VueFrameworkMetadata} from "./vue-fixture-types.ts" */
import { createHash } from "node:crypto";
import { execFileSync } from "node:child_process";
import {
  copyFileSync,
  globSync,
  mkdirSync,
  readFileSync,
  writeFileSync,
} from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
const tools = dirname(fileURLToPath(import.meta.url)),
  root = resolve(tools, "../..");
/** @param {string|Uint8Array} bytes */
const sha = (bytes) => createHash("sha256").update(bytes).digest("hex");
/** @param {string} output @returns {VueFrameworkMetadata} */
export function buildVueFixture(output) {
  const frozen = /** @type {FrameworkReference} */ (JSON.parse(
    readFileSync(join(tools, "framework-reference.json"), "utf8"),
  )).packages.vue;
  if (frozen.version !== "3.5.44") {
    throw Error("Unexpected frozen official Vue version");
  }
  const archive = join(root, "_dist/framework-reference/vue-3.5.44.tgz"),
    bytes = readFileSync(archive);
  if (
    sha(bytes) !== frozen.sha256 ||
    "sha512-" + createHash("sha512").update(bytes).digest("base64") !==
      frozen.integrity
  ) {
    throw Error(
      "Official Vue archive digest differs from frozen registry metadata",
    );
  }
  const vendor = execFileSync("python3", [
    "-c",
    "import sys,tarfile\nwith tarfile.open(sys.argv[1], 'r:gz') as archive:\n member=archive.getmember('package/dist/vue.global.js')\n if not member.isfile() or member.size>2*1024*1024: raise ValueError('Invalid official Vue bundle')\n sys.stdout.buffer.write(archive.extractfile(member).read())",
    archive,
  ], { maxBuffer: 2 * 1024 * 1024 });
  const esbuild = [...globSync(
    join(
      root,
      "node_modules/.deno/esbuild@*/node_modules/esbuild/bin/esbuild",
    ),
  )][0];
  if (!esbuild) throw Error("Existing project esbuild is unavailable");
  mkdirSync(output, { recursive: true });
  const authored = join(tools, "fixtures/devtools/vue-app.mjs"),
    entry = join(output, "authored-vue.js");
  execFileSync("node", [
    esbuild,
    authored,
    "--bundle",
    "--format=iife",
    "--outfile=" + entry,
  ], { cwd: root });
  writeFileSync(
    join(output, "app.js"),
    Buffer.concat([vendor, Buffer.from("\n"), readFileSync(entry)]),
  );
  copyFileSync(
    join(tools, "fixtures/devtools/vue.html"),
    join(output, "index.html"),
  );
  const metadata = {
    source:
      "Official npm Vue3.5.44 archive, independently checked against frozen vendor HTTPS metadata and SHA512",
    packages: { vue: { ...frozen, vendorBundleSha256: sha(vendor) } },
    htmlSha256: sha(readFileSync(join(output, "index.html"))),
    appSha256: sha(readFileSync(join(output, "app.js"))),
    authoredSourceSha256: sha(readFileSync(authored)),
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
  console.log(JSON.stringify(buildVueFixture(resolve(process.argv[2]))));
}
