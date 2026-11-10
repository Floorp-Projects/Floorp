// SPDX-License-Identifier: MPL-2.0
//
// Vendored upstream Lepton (github.com/black7375/Firefox-UI-Fix).
//
// browser-features/skin/lepton/{LEPTON,css/,icons/,userjs/} are copies of
// one upstream release. UPSTREAM.json records that release and the SHA-256
// of every vendored file; the files must stay byte-identical to it. Floorp
// changes go to browser-features/skin/lepton/overlay/ instead (see README.md
// next to UPSTREAM.json).
//
// Used by:
// - lepton_vendor.test.ts (deno task test:host): the vendored files match
//   UPSTREAM.json, so an accidental edit fails CI.
// - .github/workflows/update_lepton.yml: `verify-assets` checks downloaded
//   release zips against the digests GitHub publishes for the release,
//   `apply` replaces the vendored files and rewrites UPSTREAM.json and the
//   lepton entries of browser-features/skin/jar.mn.
//
// CLI (paths are relative to the current directory):
//   deno run --allow-read --allow-write tools/src/lepton_vendor.ts verify
//   deno run --allow-read tools/src/lepton_vendor.ts verify-assets \
//     --release-json <gh api releases/tags/TAG output> --zips-dir <dir>
//   deno run --allow-read --allow-write tools/src/lepton_vendor.ts apply \
//     --release-json <file> --commit <sha> --zips-dir <dir> \
//     --extracted-dir <dir with one unzipped folder per asset> \
//     [--summary <markdown out>]

import * as path from "@std/path";

export const UPSTREAM_REPOSITORY = "black7375/Firefox-UI-Fix";
export const SKIN_DIR = "browser-features/skin";
export const LEPTON_DIR = `${SKIN_DIR}/lepton`;
export const RECORD_FILE = "UPSTREAM.json";
export const JAR_MANIFEST = `${SKIN_DIR}/jar.mn`;

/** Floorp-owned sheets loaded right after the Lepton sheets (not vendored). */
export const OVERLAY_DIR = "overlay";

/** Release assets Floorp vendors from. */
export const RELEASE_ASSETS = [
  "Lepton.zip",
  "Lepton-Photon-Style.zip",
  "Lepton-Proton-Style.zip",
] as const;
export type ReleaseAsset = typeof RELEASE_ASSETS[number];

/** Upstream branch named in each asset's chrome/LEPTON file. */
const ASSET_BRANCH: Record<ReleaseAsset, string> = {
  "Lepton.zip": "master",
  "Lepton-Photon-Style.zip": "photon-style",
  "Lepton-Proton-Style.zip": "proton-style",
};

/** Directories under the lepton dir that are vendored as a whole. */
export const VENDORED_DIRS = ["css", "icons", "userjs"] as const;

/**
 * Vendored files that are not inside a directory copied as a whole:
 * vendored path -> [asset, path inside the zip].
 */
const SINGLE_FILES: Record<string, [ReleaseAsset, string]> = {
  "LEPTON": ["Lepton.zip", "chrome/LEPTON"],
  "userjs/lepton.js": ["Lepton.zip", "user.js"],
  "userjs/photon.js": ["Lepton-Photon-Style.zip", "user.js"],
  "userjs/protonfix.js": ["Lepton-Proton-Style.zip", "user.js"],
};

/**
 * Directories copied as a whole from Lepton.zip. Floorp loads the same
 * leptonChrome.css / leptonContent.css / icons for the lepton, photon and
 * protonfix designs, so `planRelease` requires these to be identical in all
 * three zips.
 */
const SHARED_DIRS: Record<string, string> = {
  css: "chrome/css",
  icons: "chrome/icons",
};

export const RECORD_RULE = [
  "Vendored upstream Lepton. Do not edit the files listed in `files`:",
  "LEPTON, css/, icons/ and userjs/ must stay byte-identical to the release",
  "below (tools/src/lepton_vendor.test.ts checks the SHA-256 of each file).",
  "Floorp changes to Lepton CSS go to overlay/; see README.md.",
  "Updates come only from .github/workflows/update_lepton.yml (bot PR).",
];

export type LeptonRecord = {
  $comment: string[];
  upstream: {
    repository: string;
    tag: string;
    commit: string;
    /** Release publication time from the GitHub release (ISO 8601). */
    published_at: string;
    release_url: string;
  };
  /** SHA-256 (hex) of each release zip the files were taken from. */
  assets: Record<string, string>;
  /** Vendored path (relative to the lepton dir) -> SHA-256 (hex). */
  files: Record<string, string>;
};

/** The subset of `gh api repos/{repo}/releases/tags/{tag}` that is used. */
export type ReleaseJson = {
  tag_name: string;
  published_at: string;
  html_url: string;
  draft?: boolean;
  prerelease?: boolean;
  assets: { name: string; digest?: string | null }[];
};

export async function sha256Hex(bytes: Uint8Array): Promise<string> {
  const digest = await crypto.subtle.digest(
    "SHA-256",
    bytes as Uint8Array<ArrayBuffer>,
  );
  return Array.from(new Uint8Array(digest))
    .map((b) => b.toString(16).padStart(2, "0"))
    .join("");
}

async function sha256File(file: string): Promise<string> {
  return await sha256Hex(await Deno.readFile(file));
}

/** Code-point order, so the result does not depend on locale or platform. */
function compareCodePoints(a: string, b: string): number {
  return a < b ? -1 : a > b ? 1 : 0;
}

/** Every file under `dir`, as `/`-separated paths relative to `dir`. */
async function listFiles(dir: string, prefix = ""): Promise<string[]> {
  const out: string[] = [];
  let entries: Deno.DirEntry[];
  try {
    entries = await Array.fromAsync(Deno.readDir(dir));
  } catch (error) {
    if (error instanceof Deno.errors.NotFound) return out;
    throw error;
  }
  for (const entry of entries) {
    const rel = prefix ? `${prefix}/${entry.name}` : entry.name;
    if (entry.isDirectory) {
      out.push(...await listFiles(path.join(dir, entry.name), rel));
    } else if (entry.isFile) {
      out.push(rel);
    } else {
      throw new Error(`unexpected non-file entry in vendored Lepton: ${rel}`);
    }
  }
  return out.sort(compareCodePoints);
}

/** Vendored files currently on disk, relative to the lepton dir. */
export async function listVendoredFiles(leptonDir: string): Promise<string[]> {
  const files: string[] = [];
  for (const dir of VENDORED_DIRS) {
    for (const rel of await listFiles(path.join(leptonDir, dir))) {
      files.push(`${dir}/${rel}`);
    }
  }
  for (const single of Object.keys(SINGLE_FILES)) {
    if (single.includes("/")) continue; // inside a vendored dir
    try {
      if ((await Deno.stat(path.join(leptonDir, single))).isFile) {
        files.push(single);
      }
    } catch (error) {
      if (!(error instanceof Deno.errors.NotFound)) throw error;
    }
  }
  return files.sort(compareCodePoints);
}

export async function hashFiles(
  root: string,
  files: string[],
): Promise<Record<string, string>> {
  const out: Record<string, string> = {};
  for (const rel of [...files].sort(compareCodePoints)) {
    out[rel] = await sha256File(path.join(root, ...rel.split("/")));
  }
  return out;
}

export async function readRecord(leptonDir: string): Promise<LeptonRecord> {
  const text = await Deno.readTextFile(path.join(leptonDir, RECORD_FILE));
  return JSON.parse(text) as LeptonRecord;
}

export function formatRecord(record: LeptonRecord): string {
  return `${JSON.stringify(record, null, 2)}\n`;
}

/**
 * Compare the vendored files on disk with UPSTREAM.json. Returns one message
 * per problem (empty when everything matches).
 */
export async function verifyVendoredFiles(
  leptonDir: string,
): Promise<string[]> {
  const record = await readRecord(leptonDir);
  const problems: string[] = [];
  const onDisk = await listVendoredFiles(leptonDir);
  const recorded = Object.keys(record.files);
  for (const rel of recorded) {
    if (!onDisk.includes(rel)) problems.push(`missing: ${rel}`);
  }
  for (const rel of onDisk) {
    if (!(rel in record.files)) {
      problems.push(
        `not in ${RECORD_FILE}: ${rel} (Floorp files go to ${OVERLAY_DIR}/)`,
      );
    }
  }
  for (const rel of recorded.filter((r) => onDisk.includes(r))) {
    const file = path.join(leptonDir, ...rel.split("/"));
    const bytes = await Deno.readFile(file);
    const actual = await sha256Hex(bytes);
    if (actual === record.files[rel]) continue;
    let hint = "";
    if (bytes.includes(0x0d)) {
      const lf = new TextEncoder().encode(
        new TextDecoder().decode(bytes).replaceAll("\r\n", "\n"),
      );
      if (await sha256Hex(lf) === record.files[rel]) {
        hint = " (only line endings differ: this checkout converted LF to " +
          "CRLF before .gitattributes marked vendored Lepton -text; " +
          `delete ${LEPTON_DIR}/{LEPTON,css,icons,userjs} and run ` +
          `\`git checkout -- ${LEPTON_DIR}\`)`;
      }
    }
    problems.push(
      `modified: ${rel} sha256 ${actual}, ${record.upstream.tag} has ` +
        `${record.files[rel]}${hint}`,
    );
  }
  return problems;
}

/** Check the release zips against the digests GitHub publishes for them. */
export async function verifyAssets(
  release: ReleaseJson,
  zipsDir: string,
): Promise<Record<ReleaseAsset, string>> {
  if (release.draft || release.prerelease) {
    throw new Error(`${release.tag_name} is a draft or prerelease`);
  }
  const hashes = {} as Record<ReleaseAsset, string>;
  for (const name of RELEASE_ASSETS) {
    const asset = release.assets.find((a) => a.name === name);
    if (!asset) throw new Error(`${release.tag_name} has no asset ${name}`);
    const expected = asset.digest?.match(/^sha256:([0-9a-f]{64})$/)?.[1];
    if (!expected) {
      throw new Error(
        `${release.tag_name} asset ${name} has no sha256 digest: ${asset.digest}`,
      );
    }
    const actual = await sha256File(path.join(zipsDir, name));
    if (actual !== expected) {
      throw new Error(
        `${name}: downloaded sha256 ${actual} != release digest ${expected}`,
      );
    }
    hashes[name] = actual;
  }
  return hashes;
}

function assetDir(extractedDir: string, asset: ReleaseAsset): string {
  return path.join(extractedDir, asset.replace(/\.zip$/, ""));
}

/**
 * Map every vendored path to its source file in the extracted release and
 * check the release looks like the one Floorp expects.
 */
export async function planRelease(
  extractedDir: string,
  tag: string,
): Promise<Map<string, string>> {
  const plan = new Map<string, string>();
  for (const asset of RELEASE_ASSETS) {
    const info = await Deno.readTextFile(
      path.join(assetDir(extractedDir, asset), "chrome", "LEPTON"),
    );
    const ver = info.match(/^Ver=(.*)$/m)?.[1]?.trim();
    const branch = info.match(/^Branch=(.*)$/m)?.[1]?.trim();
    if (ver !== tag) {
      throw new Error(`${asset}: chrome/LEPTON says Ver=${ver}, not ${tag}`);
    }
    if (branch !== ASSET_BRANCH[asset]) {
      throw new Error(
        `${asset}: chrome/LEPTON says Branch=${branch}, not ${
          ASSET_BRANCH[asset]
        }`,
      );
    }
  }
  const main = assetDir(extractedDir, "Lepton.zip");
  for (const [dest, src] of Object.entries(SHARED_DIRS)) {
    const files = await listFiles(path.join(main, ...src.split("/")));
    if (files.length === 0) throw new Error(`Lepton.zip has no ${src}/`);
    const reference = await hashFiles(
      path.join(main, ...src.split("/")),
      files,
    );
    for (const other of RELEASE_ASSETS.slice(1)) {
      const otherRoot = path.join(
        assetDir(extractedDir, other),
        ...src.split("/"),
      );
      const otherHashes = await hashFiles(
        otherRoot,
        await listFiles(otherRoot),
      );
      if (JSON.stringify(otherHashes) !== JSON.stringify(reference)) {
        throw new Error(
          `${other} ${src}/ differs from Lepton.zip; Floorp loads one copy ` +
            "for lepton, photon and protonfix, so this release needs a manual update",
        );
      }
    }
    for (const rel of files) {
      plan.set(
        `${dest}/${rel}`,
        path.join(main, ...src.split("/"), ...rel.split("/")),
      );
    }
  }
  for (const [dest, [asset, src]] of Object.entries(SINGLE_FILES)) {
    const file = path.join(assetDir(extractedDir, asset), ...src.split("/"));
    if (!(await Deno.stat(file)).isFile) throw new Error(`${asset}: no ${src}`);
    plan.set(dest, file);
  }
  return new Map([...plan].sort(([a], [b]) => compareCodePoints(a, b)));
}

/**
 * Regenerate the `nora-skin/lepton/...` entries of jar.mn from the files on
 * disk (vendored css/icons/userjs plus the overlay sheets), keeping every
 * other line. LEPTON, README.md and UPSTREAM.json are not packaged.
 */
export async function updateJarManifest(
  jarManifest: string,
  leptonDir: string,
): Promise<boolean> {
  const before = await Deno.readTextFile(jarManifest);
  const eol = before.includes("\r\n") ? "\r\n" : "\n";
  const lines = before.split(/\r?\n/);
  const isLepton = (line: string) => line.startsWith("  nora-skin/lepton/");
  const first = lines.findIndex(isLepton);
  if (first < 0) throw new Error(`${jarManifest} has no lepton entries`);
  const packaged: string[] = [];
  for (const dir of [...VENDORED_DIRS, OVERLAY_DIR]) {
    for (const rel of await listFiles(path.join(leptonDir, dir))) {
      packaged.push(`${dir}/${rel}`);
    }
  }
  const entries = packaged.sort(compareCodePoints).map((rel) =>
    `  nora-skin/lepton/${rel} (lepton/${rel})`
  );
  const kept = lines.filter((line) => !isLepton(line));
  kept.splice(first, 0, ...entries);
  const after = kept.join(eol);
  if (after === before) return false;
  await Deno.writeTextFile(jarManifest, after);
  return true;
}

export type ApplyResult = {
  changed: boolean;
  previousTag: string | null;
  added: string[];
  removed: string[];
  modified: string[];
  record: LeptonRecord;
};

/**
 * Replace the vendored files with the release in `extractedDir` and rewrite
 * UPSTREAM.json. Nothing is written when the release is the recorded one and
 * the files on disk already match it.
 */
export async function applyRelease(options: {
  leptonDir: string;
  extractedDir: string;
  release: ReleaseJson;
  commit: string;
  assetHashes: Record<ReleaseAsset, string>;
}): Promise<ApplyResult> {
  const { leptonDir, extractedDir, release, commit, assetHashes } = options;
  if (!/^[0-9a-f]{40}$/.test(commit)) {
    throw new Error(`commit must be a full SHA-1: ${commit}`);
  }
  const plan = await planRelease(extractedDir, release.tag_name);
  const files: Record<string, string> = {};
  for (const [rel, src] of plan) files[rel] = await sha256File(src);

  let previous: LeptonRecord | null = null;
  try {
    previous = await readRecord(leptonDir);
  } catch (error) {
    if (!(error instanceof Deno.errors.NotFound)) throw error;
  }
  const record: LeptonRecord = {
    $comment: RECORD_RULE,
    upstream: {
      repository: UPSTREAM_REPOSITORY,
      tag: release.tag_name,
      commit,
      published_at: release.published_at,
      release_url: release.html_url,
    },
    assets: Object.fromEntries(
      RELEASE_ASSETS.map((name) => [name, assetHashes[name]]),
    ),
    files,
  };

  const onDisk = await hashFiles(leptonDir, await listVendoredFiles(leptonDir));
  const added = Object.keys(files).filter((rel) => !(rel in onDisk));
  const removed = Object.keys(onDisk).filter((rel) => !(rel in files));
  const modified = Object.keys(files).filter((rel) =>
    rel in onDisk && onDisk[rel] !== files[rel]
  );
  const recordUnchanged = previous !== null &&
    formatRecord(previous) === formatRecord(record);
  const changed = !recordUnchanged || added.length > 0 ||
    removed.length > 0 || modified.length > 0;

  if (changed) {
    for (const rel of removed) {
      await Deno.remove(path.join(leptonDir, ...rel.split("/")));
    }
    for (const rel of [...added, ...modified]) {
      const dest = path.join(leptonDir, ...rel.split("/"));
      await Deno.mkdir(path.dirname(dest), { recursive: true });
      await Deno.copyFile(plan.get(rel) as string, dest);
    }
    await Deno.writeTextFile(
      path.join(leptonDir, RECORD_FILE),
      formatRecord(record),
    );
  }
  return {
    changed,
    previousTag: previous?.upstream.tag ?? null,
    added,
    removed,
    modified,
    record,
  };
}

/** Markdown body for the bot PR. */
export function formatSummary(result: ApplyResult): string {
  const { record } = result;
  const list = (title: string, items: string[]) =>
    items.length === 0 ? [] : [
      `<details><summary>${title} (${items.length})</summary>`,
      "",
      ...items.map((rel) => `- \`${rel}\``),
      "",
      "</details>",
      "",
    ];
  return [
    `Updates the vendored Lepton from ${result.previousTag ?? "(none)"} to ` +
    `[${record.upstream.tag}](${record.upstream.release_url}) ` +
    `(${record.upstream.repository}@${record.upstream.commit}).`,
    "",
    "Opened by `.github/workflows/update_lepton.yml`. The release zips were " +
    "checked against the SHA-256 digests GitHub publishes for the release:",
    "",
    ...Object.entries(record.assets).map(([name, hash]) =>
      `- \`${name}\` \`${hash}\``
    ),
    "",
    ...list("Modified files", result.modified),
    ...list("Added files", result.added),
    ...list("Removed files", result.removed),
    "Before merging: run the design tests (`deno task test -- --near " +
    "browser-features/chrome/common/designs`) and check the Lepton, Photon " +
    "and ProtonFix designs. Floorp fixes go to " +
    "`browser-features/skin/lepton/overlay/` in a separate PR, never into the " +
    "vendored files. This branch is force-pushed by the workflow.",
    "",
  ].join("\n");
}

function parseArgs(args: string[]): Map<string, string> {
  const out = new Map<string, string>();
  for (let i = 0; i < args.length; i += 2) {
    const key = args[i];
    const value = args[i + 1];
    if (!key.startsWith("--") || value === undefined) {
      throw new Error(`bad arguments near ${key}`);
    }
    out.set(key.slice(2), value);
  }
  return out;
}

function required(args: Map<string, string>, key: string): string {
  const value = args.get(key);
  if (!value) throw new Error(`--${key} is required`);
  return value;
}

async function main(argv: string[]): Promise<number> {
  const [command, ...rest] = argv;
  const args = parseArgs(rest);
  const leptonDir = args.get("lepton-dir") ?? LEPTON_DIR;
  switch (command) {
    case "verify": {
      const problems = await verifyVendoredFiles(leptonDir);
      for (const p of problems) console.error(p);
      if (problems.length === 0) {
        const record = await readRecord(leptonDir);
        console.log(
          `vendored Lepton matches ${record.upstream.tag} ` +
            `(${Object.keys(record.files).length} files)`,
        );
      }
      return problems.length === 0 ? 0 : 1;
    }
    case "verify-assets": {
      const release = JSON.parse(
        await Deno.readTextFile(required(args, "release-json")),
      ) as ReleaseJson;
      await verifyAssets(release, required(args, "zips-dir"));
      console.log(`release assets of ${release.tag_name} match their digests`);
      return 0;
    }
    case "apply": {
      const release = JSON.parse(
        await Deno.readTextFile(required(args, "release-json")),
      ) as ReleaseJson;
      const assetHashes = await verifyAssets(
        release,
        required(args, "zips-dir"),
      );
      const result = await applyRelease({
        leptonDir,
        extractedDir: required(args, "extracted-dir"),
        release,
        commit: required(args, "commit"),
        assetHashes,
      });
      const jarChanged = await updateJarManifest(
        args.get("jar-manifest") ?? JAR_MANIFEST,
        leptonDir,
      );
      const summary = args.get("summary");
      if (summary) await Deno.writeTextFile(summary, formatSummary(result));
      console.log(
        result.changed || jarChanged
          ? `updated vendored Lepton ${result.previousTag} -> ` +
            `${release.tag_name}: ${result.modified.length} modified, ` +
            `${result.added.length} added, ${result.removed.length} removed`
          : `vendored Lepton already matches ${release.tag_name}`,
      );
      return 0;
    }
    default:
      console.error("usage: lepton_vendor.ts verify|verify-assets|apply ...");
      return 2;
  }
}

if (import.meta.main) {
  Deno.exit(await main(Deno.args));
}
