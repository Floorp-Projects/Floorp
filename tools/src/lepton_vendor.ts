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
// Release zips are untrusted input. Every path is checked with lstat and
// anything that is not a regular file or a plain directory (a symlink in
// particular) aborts before a byte is read or written, so a planted link
// cannot copy a runner file (token, .git/config) into the repository.
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

export const TAG_PATTERN = /^v\d+\.\d+\.\d+$/;

/**
 * Entries each release zip may contain at its top level and in `chrome/`
 * (Lepton v8.7.5 and v8.7.6). Anything else fails the update so a human
 * looks at it. Only chrome/css/, chrome/icons/, chrome/LEPTON and user.js
 * are copied; css/ and icons/ must be flat.
 */
const ASSET_TOP_LEVEL = [
  "CREDITS",
  "LICENSE",
  "chrome",
  "install.ps1",
  "install.sh",
  "user.js",
];
const CHROME_TOP_LEVEL = [
  "LEPTON",
  "css",
  "icons",
  "userChrome.css",
  "userContent.css",
];

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

/**
 * `lstat` that does not follow a symlink. Returns null when `file` does not
 * exist. Release zips are untrusted: unzip restores symlinks, and following
 * one (e.g. `user.js -> /proc/self/environ` or `-> .git/config`) would copy
 * that file's content into the repository.
 */
async function lstatOrNull(file: string): Promise<Deno.FileInfo | null> {
  try {
    return await Deno.lstat(file);
  } catch (error) {
    if (error instanceof Deno.errors.NotFound) return null;
    throw error;
  }
}

function isRegularFile(info: Deno.FileInfo): boolean {
  return info.isFile && !info.isSymlink;
}

function isPlainDirectory(info: Deno.FileInfo): boolean {
  return info.isDirectory && !info.isSymlink;
}

/**
 * Allowed file and directory names in a release and in the vendored and
 * overlay dirs: ASCII letters, digits, `.`, `_`, `+`, `-`, not starting with
 * a dot. This rejects `.git`, `.gitattributes`, `..`, dotfiles, control
 * characters and newlines, backslashes, spaces, backticks and `@`, so a
 * release cannot plant git metadata or inject markup into jar.mn or the PR
 * body. Every current Lepton file name matches.
 */
export const SAFE_NAME = /^[A-Za-z0-9_+-][A-Za-z0-9._+-]*$/;

export function assertSafeName(name: string, where: string): void {
  if (!SAFE_NAME.test(name)) {
    throw new Error(
      `unsafe file name ${JSON.stringify(name)} in ${where} ` +
        `(allowed: ${SAFE_NAME.source})`,
    );
  }
}

/**
 * Every file under `dir`, as `/`-separated paths relative to `dir`. Throws on
 * a symlink or any other entry that is not a regular file or directory
 * (including `dir` itself), and on a name that does not match SAFE_NAME. A
 * missing `dir` lists as empty.
 */
async function listFiles(dir: string, prefix = ""): Promise<string[]> {
  const out: string[] = [];
  const info = await lstatOrNull(dir);
  if (info === null) return out;
  if (!isPlainDirectory(info)) {
    throw new Error(`not a plain directory (symlink?): ${dir}`);
  }
  for await (const entry of Deno.readDir(dir)) {
    assertSafeName(entry.name, dir);
    const rel = prefix ? `${prefix}/${entry.name}` : entry.name;
    const child = path.join(dir, entry.name);
    const childInfo = await Deno.lstat(child);
    if (isPlainDirectory(childInfo)) {
      out.push(...await listFiles(child, rel));
    } else if (isRegularFile(childInfo)) {
      out.push(rel);
    } else {
      throw new Error(`not a regular file (symlink?): ${child}`);
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
    const file = path.join(leptonDir, single);
    const info = await lstatOrNull(file);
    if (info === null) continue;
    if (!isRegularFile(info)) {
      throw new Error(`not a regular file (symlink?): ${file}`);
    }
    files.push(single);
  }
  return files.sort(compareCodePoints);
}

/**
 * Entries allowed directly in the lepton dir: the vendored ones plus the
 * Floorp-owned README.md, UPSTREAM.json and overlay/.
 */
export const LEPTON_DIR_ENTRIES = [
  ...Object.keys(SINGLE_FILES).filter((single) => !single.includes("/")),
  ...VENDORED_DIRS,
  OVERLAY_DIR,
  RECORD_FILE,
  "README.md",
];

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
  for await (const entry of Deno.readDir(leptonDir)) {
    if (!LEPTON_DIR_ENTRIES.includes(entry.name)) {
      problems.push(
        `unexpected entry: ${entry.name} (allowed: ${
          LEPTON_DIR_ENTRIES.join(", ")
        }; Floorp files go to ${OVERLAY_DIR}/)`,
      );
    }
  }
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
  if (!TAG_PATTERN.test(tag)) throw new Error(`unexpected tag: ${tag}`);
  // Reject symlinks, special files and unsafe names anywhere in the
  // extracted release before reading any of it (listFiles throws on them),
  // and any top-level entry Floorp does not know.
  const all = await listFiles(extractedDir);
  const unexpected = all.filter((rel) => {
    const [asset, top, inner, ...rest] = rel.split("/");
    if (!RELEASE_ASSETS.some((a) => assetDir("", a) === asset)) return true;
    if (!ASSET_TOP_LEVEL.includes(top)) return true;
    if (top !== "chrome") return inner !== undefined;
    if (!CHROME_TOP_LEVEL.includes(inner ?? "")) return true;
    return (inner === "css" || inner === "icons")
      ? rest.length !== 1
      : rest.length > 0;
  });
  if (unexpected.length > 0) {
    throw new Error(
      `unexpected entries in the release (update ASSET_TOP_LEVEL / ` +
        `CHROME_TOP_LEVEL in lepton_vendor.ts after reviewing them): ` +
        unexpected.slice(0, 20).join(", "),
    );
  }
  for (const asset of RELEASE_ASSETS) {
    if (!all.some((rel) => rel.startsWith(`${assetDir("", asset)}/`))) {
      throw new Error(`release has no ${asset} contents`);
    }
  }
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
    const info = await lstatOrNull(file);
    if (info === null || !isRegularFile(info)) {
      throw new Error(`${asset}: no regular file ${src}`);
    }
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
      // Built from validated parts, not taken from the API response.
      release_url:
        `https://github.com/${UPSTREAM_REPOSITORY}/releases/tag/${release.tag_name}`,
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
      // Replace, never write through, whatever is at dest; copy only the
      // bytes (no exec bit from the zip).
      if (await lstatOrNull(dest) !== null) await Deno.remove(dest);
      await Deno.copyFile(plan.get(rel) as string, dest);
      if (Deno.build.os !== "windows") await Deno.chmod(dest, 0o644);
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
  // File names already match SAFE_NAME (no backtick, `@`, `<`, newline);
  // a fenced block keeps them literal anyway (no mentions, no markup).
  const list = (title: string, items: string[]) =>
    items.length === 0 ? [] : [
      `<details><summary>${title} (${items.length})</summary>`,
      "",
      "```text",
      ...items,
      "```",
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
