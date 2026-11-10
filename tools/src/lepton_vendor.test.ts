// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals, assertRejects, assertThrows } from "@std/assert";
import * as path from "@std/path";
import {
  applyRelease,
  assertSafeName,
  formatSummary,
  hashFiles,
  isUtcTimestamp,
  JAR_MANIFEST,
  LEPTON_DIR,
  listVendoredFiles,
  readRecord,
  RELEASE_ASSETS,
  type ReleaseAsset,
  type ReleaseJson,
  sha256Hex,
  updateJarManifest,
  UPSTREAM_REPOSITORY,
  validateRelease,
  verifyAssets,
  verifyVendoredFiles,
  WORKFLOW_FILE,
} from "./lepton_vendor.ts";
import { resolveFromRoot } from "./utils.ts";

const leptonDir = resolveFromRoot(LEPTON_DIR);

Deno.test("vendored Lepton files are byte-identical to UPSTREAM.json", async () => {
  const problems = await verifyVendoredFiles(leptonDir);
  assertEquals(
    problems,
    [],
    "browser-features/skin/lepton/{LEPTON,css,icons,userjs} must not be " +
      "edited; put Floorp changes in overlay/ (see README.md). Updates " +
      `come only from ${WORKFLOW_FILE}`,
  );
});

Deno.test("UPSTREAM.json names one upstream release", async () => {
  const record = await readRecord(leptonDir);
  assertEquals(record.upstream.repository, UPSTREAM_REPOSITORY);
  assert(/^v\d+\.\d+\.\d+$/.test(record.upstream.tag), record.upstream.tag);
  assert(/^[0-9a-f]{40}$/.test(record.upstream.commit), "full commit SHA");
  assert(
    isUtcTimestamp(record.upstream.published_at),
    `published_at: ${record.upstream.published_at}`,
  );
  assertEquals(
    record.upstream.release_url,
    `https://github.com/${UPSTREAM_REPOSITORY}/releases/tag/${record.upstream.tag}`,
  );
  assertEquals(Object.keys(record.assets), [...RELEASE_ASSETS]);
  for (const hash of Object.values(record.assets)) {
    assert(/^[0-9a-f]{64}$/.test(hash), hash);
  }
  const info = await Deno.readTextFile(path.join(leptonDir, "LEPTON"));
  assert(
    info.includes(`Ver=${record.upstream.tag}`),
    "LEPTON and UPSTREAM.json must name the same release",
  );
  assert(record.files["css/leptonChrome.css"], "leptonChrome.css recorded");
  assert(record.files["css/leptonContent.css"], "leptonContent.css recorded");
});

Deno.test(".gitattributes keeps vendored Lepton bytes unconverted", async () => {
  const lines = (await Deno.readTextFile(resolveFromRoot(".gitattributes")))
    .split(/\r?\n/).map((line) => line.trim());
  for (const entry of ["LEPTON", "css/**", "icons/**", "userjs/**"]) {
    assert(
      lines.includes(`${LEPTON_DIR}/${entry} -text`),
      `.gitattributes must mark ${LEPTON_DIR}/${entry} -text`,
    );
  }
});

Deno.test("jar.mn lists every packaged Lepton file", async () => {
  const dir = await Deno.makeTempDir({ prefix: "lepton-jar-" });
  try {
    const copy = path.join(dir, "jar.mn");
    await Deno.copyFile(resolveFromRoot(JAR_MANIFEST), copy);
    assertEquals(
      await updateJarManifest(copy, leptonDir),
      false,
      "jar.mn lepton entries are stale; run the update in lepton_vendor.ts",
    );
    const text = await Deno.readTextFile(copy);
    for (
      const sheet of [
        "overlay/leptonChromeOverlay.css",
        "overlay/leptonContentOverlay.css",
      ]
    ) {
      assert(
        text.includes(`  nora-skin/lepton/${sheet} (lepton/${sheet})`),
        sheet,
      );
    }
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

// ---------------------------------------------------------------------------
// verify / apply against temporary copies
// ---------------------------------------------------------------------------

const encoder = new TextEncoder();

async function write(file: string, text: string): Promise<void> {
  await Deno.mkdir(path.dirname(file), { recursive: true });
  await Deno.writeFile(file, encoder.encode(text));
}

const BRANCH: Record<ReleaseAsset, string> = {
  "Lepton.zip": "master",
  "Lepton-Photon-Style.zip": "photon-style",
  "Lepton-Proton-Style.zip": "proton-style",
};

/** A fake extracted release with one folder per asset. */
async function fakeRelease(
  root: string,
  tag: string,
  css: Record<string, string>,
  icons: Record<string, string>,
): Promise<void> {
  for (const asset of RELEASE_ASSETS) {
    const dir = path.join(root, asset.replace(/\.zip$/, ""));
    await write(
      path.join(dir, "chrome", "LEPTON"),
      `[Info]\nVer=${tag}\nBranch=${BRANCH[asset]}\n`,
    );
    for (const [name, text] of Object.entries(css)) {
      await write(path.join(dir, "chrome", "css", name), text);
    }
    for (const [name, text] of Object.entries(icons)) {
      await write(path.join(dir, "chrome", "icons", name), text);
    }
    await write(path.join(dir, "user.js"), `// ${asset} ${tag}\r\n`);
    await write(path.join(dir, "chrome", "userChrome.css"), "/* unused */");
  }
}

function releaseJson(tag: string): ReleaseJson {
  return {
    tag_name: tag,
    published_at: "2026-01-01T00:00:00Z",
    html_url: `https://github.com/${UPSTREAM_REPOSITORY}/releases/tag/${tag}`,
    assets: [],
  };
}

const FAKE_COMMIT = "0123456789abcdef0123456789abcdef01234567";
const FAKE_ASSETS = Object.fromEntries(
  RELEASE_ASSETS.map((name) => [name, "0".repeat(64)]),
) as Record<ReleaseAsset, string>;

Deno.test("apply replaces vendored files and is a no-op when repeated", async () => {
  const tmp = await Deno.makeTempDir({ prefix: "lepton-apply-" });
  try {
    const lepton = path.join(tmp, "lepton");
    const v1 = path.join(tmp, "v1");
    const v2 = path.join(tmp, "v2");
    await fakeRelease(v1, "v1.0.0", { "leptonChrome.css": "a{}\n" }, {
      "old.svg": "<svg/>",
      "keep.svg": "<svg>k</svg>\r\n",
    });
    await fakeRelease(v2, "v1.0.1", { "leptonChrome.css": "a{b:c}\n" }, {
      "keep.svg": "<svg>k</svg>\r\n",
      "new.svg": "<svg>n</svg>",
    });
    await write(path.join(lepton, "overlay", "x.css"), "/* floorp */\n");

    const first = await applyRelease({
      leptonDir: lepton,
      extractedDir: v1,
      release: releaseJson("v1.0.0"),
      commit: FAKE_COMMIT,
      assetHashes: FAKE_ASSETS,
    });
    assert(first.changed);
    assertEquals(first.previousTag, null);
    assertEquals(await verifyVendoredFiles(lepton), []);

    const again = await applyRelease({
      leptonDir: lepton,
      extractedDir: v1,
      release: releaseJson("v1.0.0"),
      commit: FAKE_COMMIT,
      assetHashes: FAKE_ASSETS,
    });
    assertEquals(again.changed, false, "same release must not rewrite");

    const second = await applyRelease({
      leptonDir: lepton,
      extractedDir: v2,
      release: releaseJson("v1.0.1"),
      commit: FAKE_COMMIT,
      assetHashes: FAKE_ASSETS,
    });
    assert(second.changed);
    assertEquals(second.previousTag, "v1.0.0");
    assertEquals(second.added, ["icons/new.svg"]);
    assertEquals(second.removed, ["icons/old.svg"]);
    assertEquals(second.modified, [
      "LEPTON",
      "css/leptonChrome.css",
      "userjs/lepton.js",
      "userjs/photon.js",
      "userjs/protonfix.js",
    ]);
    assertEquals(await verifyVendoredFiles(lepton), []);
    assertEquals((await readRecord(lepton)).upstream.tag, "v1.0.1");
    // Bytes are copied exactly, CRLF included; overlay/ is left alone.
    assertEquals(
      await Deno.readTextFile(path.join(lepton, "icons", "keep.svg")),
      "<svg>k</svg>\r\n",
    );
    assertEquals(
      await Deno.readTextFile(path.join(lepton, "overlay", "x.css")),
      "/* floorp */\n",
    );
    assertEquals(
      await listVendoredFiles(lepton),
      Object.keys(await hashFiles(lepton, await listVendoredFiles(lepton))),
    );
  } finally {
    await Deno.remove(tmp, { recursive: true });
  }
});

Deno.test("verify reports edits, extra files and converted line endings", async () => {
  const tmp = await Deno.makeTempDir({ prefix: "lepton-verify-" });
  try {
    const lepton = path.join(tmp, "lepton");
    const release = path.join(tmp, "release");
    await fakeRelease(release, "v1.0.0", { "leptonChrome.css": "a{}\nb{}\n" }, {
      "i.svg": "<svg/>",
    });
    await applyRelease({
      leptonDir: lepton,
      extractedDir: release,
      release: releaseJson("v1.0.0"),
      commit: FAKE_COMMIT,
      assetHashes: FAKE_ASSETS,
    });
    await write(path.join(lepton, "css", "leptonChrome.css"), "a{}\r\nb{}\r\n");
    await write(path.join(lepton, "icons", "i.svg"), "<svg x/>");
    await write(path.join(lepton, "css", "floorp.css"), "c{}");
    const problems = await verifyVendoredFiles(lepton);
    assertEquals(problems.length, 3, problems.join("\n"));
    assert(
      problems.some((p) =>
        p.startsWith("not in UPSTREAM.json: css/floorp.css")
      ),
    );
    assert(
      problems.some((p) =>
        p.startsWith("modified: css/leptonChrome.css") &&
        p.includes("only line endings differ")
      ),
    );
    assert(
      problems.some((p) =>
        p.startsWith("modified: icons/i.svg") && !p.includes("line endings")
      ),
    );
    await Deno.remove(path.join(lepton, "icons", "i.svg"));
    assert(
      (await verifyVendoredFiles(lepton)).includes("missing: icons/i.svg"),
    );
    // Only README.md, UPSTREAM.json and overlay/ may sit next to the
    // vendored entries.
    await write(path.join(lepton, "overlay", "ok.css"), "/* floorp */");
    await write(path.join(lepton, "README.md"), "# ok");
    await write(path.join(lepton, "floorp.css"), "d{}");
    const root = await verifyVendoredFiles(lepton);
    assertEquals(
      root.filter((p) => p.startsWith("unexpected entry")).length,
      1,
      root.join(" | "),
    );
    assert(root.some((p) => p.startsWith("unexpected entry: floorp.css")));
  } finally {
    await Deno.remove(tmp, { recursive: true });
  }
});

Deno.test("apply refuses a release that does not match", async () => {
  const tmp = await Deno.makeTempDir({ prefix: "lepton-refuse-" });
  try {
    const lepton = path.join(tmp, "lepton");
    const release = path.join(tmp, "release");
    await fakeRelease(release, "v1.0.0", { "leptonChrome.css": "a{}" }, {
      "i.svg": "<svg/>",
    });
    const apply = (tag: string, commit = FAKE_COMMIT) =>
      applyRelease({
        leptonDir: lepton,
        extractedDir: release,
        release: releaseJson(tag),
        commit,
        assetHashes: FAKE_ASSETS,
      });
    await assertRejects(() => apply("v9.9.9"), Error, 'Ver="v1.0.0"');
    await assertRejects(() => apply("v1.0.0", "abc"), Error, "full SHA-1");
    // Photon/Proton zips must ship the same css/ Floorp loads for all designs.
    await write(
      path.join(
        release,
        "Lepton-Photon-Style",
        "chrome",
        "css",
        "leptonChrome.css",
      ),
      "a{color:red}",
    );
    await assertRejects(
      () => apply("v1.0.0"),
      Error,
      "differs from Lepton.zip",
    );
    assertEquals(
      await listVendoredFiles(lepton),
      [],
      "a refused release must not write anything",
    );
  } finally {
    await Deno.remove(tmp, { recursive: true });
  }
});

Deno.test("verifyAssets checks the release digests", async () => {
  const tmp = await Deno.makeTempDir({ prefix: "lepton-assets-" });
  try {
    const assets: ReleaseJson["assets"] = [];
    for (const name of RELEASE_ASSETS) {
      const bytes = encoder.encode(`zip ${name}`);
      await Deno.writeFile(path.join(tmp, name), bytes);
      assets.push({ name, digest: `sha256:${await sha256Hex(bytes)}` });
    }
    const release = { ...releaseJson("v1.0.0"), assets };
    const hashes = await verifyAssets(release, tmp);
    assertEquals(Object.keys(hashes), [...RELEASE_ASSETS]);

    await Deno.writeFile(path.join(tmp, "Lepton.zip"), encoder.encode("evil"));
    await assertRejects(
      () => verifyAssets(release, tmp),
      Error,
      "!= release digest",
    );
    await assertRejects(
      () =>
        verifyAssets({
          ...release,
          assets: assets.map((a) => ({ ...a, digest: null })),
        }, tmp),
      Error,
      "no sha256 digest",
    );
    await assertRejects(
      () => verifyAssets({ ...release, prerelease: true }, tmp),
      Error,
      "prerelease",
    );
  } finally {
    await Deno.remove(tmp, { recursive: true });
  }
});

/**
 * Create a symlink, or return false where this OS/user may not (Windows
 * without Developer Mode). CI runs on Linux, where this always works.
 */
async function trySymlink(
  target: string,
  link: string,
  type: "file" | "dir",
): Promise<boolean> {
  await Deno.mkdir(path.dirname(link), { recursive: true });
  try {
    await Deno.symlink(target, link, { type });
    return true;
  } catch (error) {
    if (Deno.build.os === "windows") {
      console.warn(`symlink not permitted here, skipping: ${error}`);
      return false;
    }
    throw error;
  }
}

Deno.test("apply refuses symlinks in the extracted release", async () => {
  const tmp = await Deno.makeTempDir({ prefix: "lepton-symlink-" });
  try {
    const secret = path.join(tmp, "secret.txt");
    await write(secret, "GH_TOKEN=do-not-copy");
    const cases: [string, (release: string) => Promise<boolean>][] = [
      ["user.js -> secret", async (release) => {
        const userJs = path.join(release, "Lepton", "user.js");
        await Deno.remove(userJs);
        return await trySymlink(secret, userJs, "file");
      }],
      ["LEPTON -> secret", async (release) => {
        const info = path.join(
          release,
          "Lepton-Proton-Style",
          "chrome",
          "LEPTON",
        );
        await Deno.remove(info);
        return await trySymlink(secret, info, "file");
      }],
      ["icon -> secret", async (release) => {
        return await trySymlink(
          secret,
          path.join(release, "Lepton", "chrome", "icons", "x.svg"),
          "file",
        );
      }],
      ["chrome/css -> dir", async (release) => {
        const css = path.join(release, "Lepton", "chrome", "css");
        await Deno.remove(css, { recursive: true });
        const elsewhere = path.join(tmp, "elsewhere");
        await write(path.join(elsewhere, "leptonChrome.css"), "a{}");
        return await trySymlink(elsewhere, css, "dir");
      }],
    ];
    for (const [i, [name, plant]] of cases.entries()) {
      const lepton = path.join(tmp, `lepton-${i}`);
      const release = path.join(tmp, "release");
      await Deno.remove(release, { recursive: true }).catch(() => {});
      await fakeRelease(release, "v1.0.0", { "leptonChrome.css": "a{}" }, {
        "i.svg": "<svg/>",
      });
      if (!(await plant(release))) return;
      await assertRejects(
        () =>
          applyRelease({
            leptonDir: lepton,
            extractedDir: release,
            release: releaseJson("v1.0.0"),
            commit: FAKE_COMMIT,
            assetHashes: FAKE_ASSETS,
          }),
        Error,
        "symlink?",
        name,
      );
      assertEquals(
        await listVendoredFiles(lepton),
        [],
        `${name}: nothing may be written`,
      );
    }
  } finally {
    await Deno.remove(tmp, { recursive: true });
  }
});

Deno.test("verify refuses symlinks among the vendored files", async () => {
  const tmp = await Deno.makeTempDir({ prefix: "lepton-vsymlink-" });
  try {
    const lepton = path.join(tmp, "lepton");
    const release = path.join(tmp, "release");
    await fakeRelease(release, "v1.0.0", { "leptonChrome.css": "a{}" }, {
      "i.svg": "<svg/>",
    });
    await applyRelease({
      leptonDir: lepton,
      extractedDir: release,
      release: releaseJson("v1.0.0"),
      commit: FAKE_COMMIT,
      assetHashes: FAKE_ASSETS,
    });
    const icon = path.join(lepton, "icons", "i.svg");
    const copy = path.join(tmp, "i.svg");
    await Deno.rename(icon, copy);
    if (!(await trySymlink(copy, icon, "file"))) return;
    await assertRejects(
      () => verifyVendoredFiles(lepton),
      Error,
      "symlink?",
    );
  } finally {
    await Deno.remove(tmp, { recursive: true });
  }
});

// ---------------------------------------------------------------------------
// Untrusted names: git metadata, dotfiles, newlines, markup
// ---------------------------------------------------------------------------

Deno.test("assertSafeName allows Lepton names and refuses unsafe ones", () => {
  for (
    const ok of [
      "leptonChrome.css",
      "tab-bottom-corner-left-chromeLegacy-clipped.svg",
      "c++.svg",
      "LEPTON",
      "user.js",
    ]
  ) {
    assertSafeName(ok, "test");
  }
  for (
    const bad of [
      ".git",
      ".gitattributes",
      ".hidden.svg",
      "..",
      "a\nb.svg",
      "a\rb.svg",
      "a\u0000b",
      "a`b.svg",
      "@here.svg",
      "a\b.svg",
      "a b.svg",
      "<x>.svg",
      "",
    ]
  ) {
    assertThrows(
      () => assertSafeName(bad, "test"),
      Error,
      "unsafe file name",
      JSON.stringify(bad),
    );
  }
});

const NEWLINE_OK = Deno.build.os !== "windows"; // NTFS has no "\n" in names

Deno.test("apply refuses planted git metadata, dotfiles and odd names", async () => {
  const tmp = await Deno.makeTempDir({ prefix: "lepton-names-" });
  try {
    const cases: [string, string, string][] = [
      [".git in icons", "Lepton/chrome/icons/x/.git/HEAD", "unsafe file name"],
      [
        ".git config",
        "Lepton/chrome/icons/x/.git/config",
        "unsafe file name",
      ],
      ["dotfile", "Lepton-Photon-Style/chrome/css/.hidden", "unsafe file name"],
      ["backtick", "Lepton/chrome/icons/a`b.svg", "unsafe file name"],
      ["nested icons dir", "Lepton/chrome/icons/x/y.svg", "unexpected entries"],
      ["unknown top level", "Lepton/evil.sh", "unexpected entries"],
      ["unknown chrome entry", "Lepton/chrome/extra.css", "unexpected entries"],
      ["unknown asset dir", "Other/chrome/LEPTON", "unexpected entries"],
    ];
    if (NEWLINE_OK) {
      cases.push([
        "newline",
        "Lepton/chrome/icons/a\nb.svg",
        "unsafe file name",
      ]);
    }
    for (const [i, [name, planted, message]] of cases.entries()) {
      const lepton = path.join(tmp, `lepton-${i}`);
      const release = path.join(tmp, `release-${i}`);
      await fakeRelease(release, "v1.0.0", { "leptonChrome.css": "a{}" }, {
        "i.svg": "<svg/>",
      });
      await write(path.join(release, ...planted.split("/")), "[core]\n");
      await assertRejects(
        () =>
          applyRelease({
            leptonDir: lepton,
            extractedDir: release,
            release: releaseJson("v1.0.0"),
            commit: FAKE_COMMIT,
            assetHashes: FAKE_ASSETS,
          }),
        Error,
        message,
        name,
      );
      assertEquals(
        await listVendoredFiles(lepton),
        [],
        `${name}: nothing may be written`,
      );
    }
  } finally {
    await Deno.remove(tmp, { recursive: true });
  }
});

Deno.test("verify refuses unsafe names among the vendored files", async () => {
  const tmp = await Deno.makeTempDir({ prefix: "lepton-vnames-" });
  try {
    const lepton = path.join(tmp, "lepton");
    const release = path.join(tmp, "release");
    await fakeRelease(release, "v1.0.0", { "leptonChrome.css": "a{}" }, {
      "i.svg": "<svg/>",
    });
    await applyRelease({
      leptonDir: lepton,
      extractedDir: release,
      release: releaseJson("v1.0.0"),
      commit: FAKE_COMMIT,
      assetHashes: FAKE_ASSETS,
    });
    await write(path.join(lepton, "icons", "x", ".git", "config"), "[core]");
    await assertRejects(
      () => verifyVendoredFiles(lepton),
      Error,
      "unsafe file name",
    );
  } finally {
    await Deno.remove(tmp, { recursive: true });
  }
});

Deno.test("jar.mn is not written when a packaged name is unsafe", async () => {
  const tmp = await Deno.makeTempDir({ prefix: "lepton-jarname-" });
  try {
    const bad = [
      "a b.css",
      ...(NEWLINE_OK ? ["a\n  nora-skin/x (y).css"] : []),
    ];
    for (const name of bad) {
      const lepton = path.join(tmp, "lepton");
      await Deno.remove(lepton, { recursive: true }).catch(() => {});
      await write(path.join(lepton, "css", "leptonChrome.css"), "a{}");
      await write(path.join(lepton, "overlay", name), "/* x */");
      const jar = path.join(tmp, "jar.mn");
      const before =
        "noraneko.jar:\n  nora-skin/lepton/css/old.css (lepton/css/old.css)\n";
      await Deno.writeTextFile(jar, before);
      await assertRejects(
        () => updateJarManifest(jar, lepton),
        Error,
        "unsafe file name",
        JSON.stringify(name),
      );
      assertEquals(await Deno.readTextFile(jar), before, "jar.mn unchanged");
    }
  } finally {
    await Deno.remove(tmp, { recursive: true });
  }
});

Deno.test("the PR summary lists file names literally in a code block", async () => {
  const tmp = await Deno.makeTempDir({ prefix: "lepton-summary-" });
  try {
    const lepton = path.join(tmp, "lepton");
    const release = path.join(tmp, "release");
    await fakeRelease(release, "v1.0.0", { "leptonChrome.css": "a{}" }, {
      "new.svg": "<svg/>",
    });
    const result = await applyRelease({
      leptonDir: lepton,
      extractedDir: release,
      release: {
        ...releaseJson("v1.0.0"),
        html_url: "https://example.invalid/@everyone",
      },
      commit: FAKE_COMMIT,
      assetHashes: FAKE_ASSETS,
    });
    const summary = formatSummary(result);
    assert(
      summary.includes(
        "```text\nLEPTON\ncss/leptonChrome.css\nicons/new.svg\n",
      ),
      summary,
    );
    assert(!summary.includes("example.invalid"), "release URL is rebuilt");
    assertEquals(
      result.record.upstream.release_url,
      `https://github.com/${UPSTREAM_REPOSITORY}/releases/tag/v1.0.0`,
    );
  } finally {
    await Deno.remove(tmp, { recursive: true });
  }
});

// ---------------------------------------------------------------------------
// Names Windows cannot check out
// ---------------------------------------------------------------------------

const WINDOWS_BAD_NAMES = [
  "CON",
  "con",
  "Prn.css",
  "aux.css",
  "nul.svg",
  "NUL.tar.gz",
  "com1",
  "COM9.svg",
  "com0.svg",
  "lpt1",
  "Lpt9.css",
  "icon.",
  "icon.svg.",
  "x..",
];

Deno.test("assertSafeName refuses Windows device names and trailing dots", () => {
  for (const bad of WINDOWS_BAD_NAMES) {
    assertThrows(
      () => assertSafeName(bad, "test"),
      Error,
      "unsafe file name",
      JSON.stringify(bad),
    );
  }
  assertThrows(() => assertSafeName("nul.svg", "t"), Error, "device name");
  assertThrows(() => assertSafeName("a.", "t"), Error, "ends in a dot");
  // Names that only start like a device name are fine.
  for (
    const ok of [
      "console.css",
      "connect.svg",
      "auxiliary.svg",
      "null.svg",
      "nul-x.svg",
      "com10.svg",
      "lpt.svg",
      "prn_icon.svg",
      "a.b.c",
    ]
  ) {
    assertSafeName(ok, "test");
  }
});

/** True when `dir` keeps `a` and `A` apart (Linux; not Windows or macOS). */
async function isCaseSensitive(dir: string): Promise<boolean> {
  const probe = path.join(dir, "case-probe");
  await write(path.join(probe, "a"), "");
  await write(path.join(probe, "A"), "");
  let count = 0;
  for await (const _ of Deno.readDir(probe)) count++;
  await Deno.remove(probe, { recursive: true });
  return count === 2;
}

Deno.test("apply refuses Windows device names, trailing dots and case-only duplicates", async () => {
  const tmp = await Deno.makeTempDir({ prefix: "lepton-winnames-" });
  try {
    const cases: [string, string, string][] = [];
    // NTFS cannot hold these names (or drops the trailing dot), so the
    // planted files only exist on Linux and macOS; CI runs on Linux.
    if (Deno.build.os !== "windows") {
      cases.push(
        ["device name", "Lepton/chrome/icons/nul.svg", "device name"],
        ["device name dir", "Lepton/chrome/CON/x", "device name"],
        ["upper device", "Lepton-Proton-Style/chrome/css/AUX.css", "device"],
        ["trailing dot", "Lepton/chrome/icons/i.svg.", "ends in a dot"],
      );
    }
    if (await isCaseSensitive(tmp)) {
      cases.push(["case-only", "Lepton/chrome/icons/I.svg", "only in case"]);
    }
    if (cases.length === 0) {
      console.warn("this file system cannot hold the planted names; skipping");
    }
    for (const [i, [name, planted, message]] of cases.entries()) {
      const lepton = path.join(tmp, `lepton-${i}`);
      const release = path.join(tmp, `release-${i}`);
      await fakeRelease(release, "v1.0.0", { "leptonChrome.css": "a{}" }, {
        "i.svg": "<svg/>",
      });
      await write(path.join(release, ...planted.split("/")), "x");
      await assertRejects(
        () =>
          applyRelease({
            leptonDir: lepton,
            extractedDir: release,
            release: releaseJson("v1.0.0"),
            commit: FAKE_COMMIT,
            assetHashes: FAKE_ASSETS,
          }),
        Error,
        message,
        name,
      );
      assertEquals(
        await listVendoredFiles(lepton),
        [],
        `${name}: nothing may be written`,
      );
    }
  } finally {
    await Deno.remove(tmp, { recursive: true });
  }
});

// ---------------------------------------------------------------------------
// Release metadata written to UPSTREAM.json
// ---------------------------------------------------------------------------

Deno.test("isUtcTimestamp accepts only real ISO-8601 UTC times", () => {
  for (
    const ok of [
      "2026-01-01T00:00:00Z",
      "2025-08-31T23:59:59Z",
      "2024-02-29T12:00:00Z",
    ]
  ) {
    assert(isUtcTimestamp(ok), ok);
  }
  for (
    const bad of [
      "",
      "2026-01-01",
      "2026-01-01T00:00:00",
      "2026-01-01T00:00:00.000Z",
      "2026-01-01T00:00:00+09:00",
      "2026-01-01 00:00:00Z",
      "2026-13-01T00:00:00Z",
      "2026-02-30T00:00:00Z",
      "2025-02-29T00:00:00Z",
      "2026-01-01T24:00:00Z",
      "2026-01-01T00:60:00Z",
      "2026-01-01T00:00:00Z\n::error::x",
      " 2026-01-01T00:00:00Z",
      "yesterday",
      null,
      1767225600,
    ]
  ) {
    assert(!isUtcTimestamp(bad), JSON.stringify(bad));
  }
});

Deno.test("apply and verifyAssets refuse malformed release metadata", async () => {
  const tmp = await Deno.makeTempDir({ prefix: "lepton-meta-" });
  try {
    const lepton = path.join(tmp, "lepton");
    const release = path.join(tmp, "release");
    await fakeRelease(release, "v1.0.0", { "leptonChrome.css": "a{}" }, {
      "i.svg": "<svg/>",
    });
    const missing = { ...releaseJson("v1.0.0") } as Partial<ReleaseJson>;
    delete missing.published_at;
    const bad: [string, ReleaseJson, string][] = [
      [
        "published_at not UTC",
        { ...releaseJson("v1.0.0"), published_at: "2026-01-01T09:00:00+09:00" },
        "published_at",
      ],
      [
        "published_at with markup",
        { ...releaseJson("v1.0.0"), published_at: "<b>soon</b>" },
        "published_at",
      ],
      ["published_at missing", missing as ReleaseJson, "published_at"],
      [
        "tag_name",
        { ...releaseJson("v1.0.0"), tag_name: "v1.0.0\n::error::x" },
        "tag_name",
      ],
    ];
    for (const [name, json, message] of bad) {
      assertThrows(() => validateRelease(json), Error, message, name);
      await assertRejects(
        () => verifyAssets(json, tmp),
        Error,
        message,
        `verifyAssets: ${name}`,
      );
      await assertRejects(
        () =>
          applyRelease({
            leptonDir: lepton,
            extractedDir: release,
            release: json,
            commit: FAKE_COMMIT,
            assetHashes: FAKE_ASSETS,
          }),
        Error,
        message,
        `apply: ${name}`,
      );
    }
    await assertRejects(
      () =>
        applyRelease({
          leptonDir: lepton,
          extractedDir: release,
          release: releaseJson("v1.0.0"),
          commit: FAKE_COMMIT,
          assetHashes: { ...FAKE_ASSETS, "Lepton.zip": "not-a-hash" },
        }),
      Error,
      "SHA-256",
    );
    assertEquals(await listVendoredFiles(lepton), [], "nothing written");
    await assertRejects(
      () => readRecord(lepton),
      Deno.errors.NotFound,
      undefined,
      "no UPSTREAM.json written",
    );

    const ok = await applyRelease({
      leptonDir: lepton,
      extractedDir: release,
      release: releaseJson("v1.0.0"),
      commit: FAKE_COMMIT,
      assetHashes: FAKE_ASSETS,
    });
    assertEquals(ok.record.upstream.published_at, "2026-01-01T00:00:00Z");
    assertEquals(
      (await readRecord(lepton)).upstream.published_at,
      "2026-01-01T00:00:00Z",
    );
  } finally {
    await Deno.remove(tmp, { recursive: true });
  }
});

Deno.test("error messages that need a maintainer name update_lepton.yml", async () => {
  const tmp = await Deno.makeTempDir({ prefix: "lepton-hints-" });
  try {
    const lepton = path.join(tmp, "lepton");
    const release = path.join(tmp, "release");
    const photonCss = path.join(
      release,
      "Lepton-Photon-Style",
      "chrome",
      "css",
      "leptonChrome.css",
    );
    await fakeRelease(release, "v1.0.0", { "leptonChrome.css": "a{}" }, {
      "i.svg": "<svg/>",
    });
    const sub = path.join(release, "Lepton", "chrome", "icons", "sub");
    await write(path.join(sub, "x.svg"), "x");
    const apply = () =>
      applyRelease({
        leptonDir: lepton,
        extractedDir: release,
        release: releaseJson("v1.0.0"),
        commit: FAKE_COMMIT,
        assetHashes: FAKE_ASSETS,
      });
    const nested = await assertRejects(apply, Error, "unexpected entries");
    assert(nested.message.includes(WORKFLOW_FILE), nested.message);
    assert(nested.message.includes("SHARED_DIRS"), nested.message);

    await Deno.remove(sub, { recursive: true });
    await write(photonCss, "a{color:red}");
    const differs = await assertRejects(
      apply,
      Error,
      "differs from Lepton.zip",
    );
    assert(differs.message.includes(WORKFLOW_FILE), differs.message);

    await write(photonCss, "a{}");
    await apply();
    await write(path.join(lepton, "css", "floorp.css"), "c{}");
    const problems = await verifyVendoredFiles(lepton);
    assert(
      problems.some((p) =>
        p.startsWith("not in UPSTREAM.json: css/floorp.css") &&
        p.includes(WORKFLOW_FILE)
      ),
      problems.join("\n"),
    );
  } finally {
    await Deno.remove(tmp, { recursive: true });
  }
});

// ---------------------------------------------------------------------------
// update_lepton.yml: Deno permissions and the shell name checks
// ---------------------------------------------------------------------------

const workflowText = () => Deno.readTextFile(resolveFromRoot(WORKFLOW_FILE));

/** Every `deno ...` command in the workflow, keyed by lepton_vendor command. */
async function workflowDenoRuns(): Promise<Map<string, string[]>> {
  const joined = (await workflowText()).replace(/\\\r?\n\s*/g, " ");
  const runs = new Map<string, string[]>();
  for (const line of joined.split(/\r?\n/)) {
    if (!/^\s*deno\s/.test(line)) continue;
    const words = line.trim().split(/\s+/);
    assertEquals(words[1], "run", `only deno run is expected: ${line}`);
    const script = words.indexOf("tools/src/lepton_vendor.ts");
    assert(script > 0, `unexpected deno command in the workflow: ${line}`);
    const command = words[script + 1];
    assert(!runs.has(command), `${command} runs twice`);
    runs.set(command, words);
  }
  return runs;
}

/** Values of `--allow-<kind>=a,b` (quotes stripped); null when absent. */
function allowList(words: string[], kind: string): string[] | null {
  const flag = words.find((w) =>
    w === `--allow-${kind}` || w.startsWith(`--allow-${kind}=`)
  );
  if (flag === undefined) return null;
  assert(flag.includes("="), `${flag} must name the paths it allows`);
  return flag.slice(flag.indexOf("=") + 1).replaceAll('"', "").split(",");
}

function covered(file: string, allowed: string[]): boolean {
  return allowed.some((a) => file === a || file.startsWith(`${a}/`));
}

function runOf(runs: Map<string, string[]>, command: string): string[] {
  const words = runs.get(command);
  assert(words !== undefined, `update_lepton.yml runs ${command}`);
  return words;
}

Deno.test("update_lepton.yml gives Deno only the paths each command needs", async () => {
  const runs = await workflowDenoRuns();
  assertEquals([...runs.keys()].sort(), ["apply", "verify", "verify-assets"]);
  for (const [command, words] of runs) {
    const script = words.indexOf("tools/src/lepton_vendor.ts");
    for (const flag of words.slice(2, script)) {
      assert(
        /^--allow-(read|write)=\S+$/.test(flag),
        `${command}: only scoped --allow-read/--allow-write are allowed ` +
          `(no net, env, run, -A), got ${flag}`,
      );
    }
  }
  const verifyAssetsRun = runOf(runs, "verify-assets");
  assertEquals(allowList(verifyAssetsRun, "read"), ["$WORK"]);
  assertEquals(allowList(verifyAssetsRun, "write"), null);
  const verifyRun = runOf(runs, "verify");
  assertEquals(allowList(verifyRun, "read"), [LEPTON_DIR]);
  assertEquals(allowList(verifyRun, "write"), null);

  const apply = runOf(runs, "apply");
  const read = allowList(apply, "read") ?? [];
  const writable = allowList(apply, "write") ?? [];
  assertEquals(read, ["$WORK", "browser-features/skin"]);
  assertEquals(writable, [LEPTON_DIR, JAR_MANIFEST, "$WORK/pr-body.md"]);
  // Every path argument of apply, and its default lepton dir and jar.mn,
  // is readable; apply writes only the vendored files, jar.mn and the
  // summary.
  const arg = (name: string) => {
    const i = apply.indexOf(`--${name}`);
    assert(i > 0 && i + 1 < apply.length, `apply --${name}`);
    return apply[i + 1].replaceAll('"', "");
  };
  for (const name of ["release-json", "zips-dir", "extracted-dir", "summary"]) {
    assert(covered(arg(name), read), `--${name} ${arg(name)} is readable`);
  }
  assert(covered(arg("summary"), writable), "--summary is writable");
  assert(!apply.includes("--lepton-dir") && !apply.includes("--jar-manifest"));
  for (const file of [LEPTON_DIR, JAR_MANIFEST]) {
    assert(covered(file, read) && covered(file, writable), file);
  }
});

Deno.test("update_lepton.yml name checks match assertSafeName", async () => {
  const text = await workflowText();
  const segs = [...text.matchAll(/^\s*seg='([^']*)'\r?$/gm)].map((m) => m[1]);
  const winbads = [...text.matchAll(/^\s*winbad='([^']*)'\r?$/gm)].map((m) =>
    m[1]
  );
  assertEquals(segs.length, 2, "seg in the extract and commit steps");
  assertEquals(winbads.length, 2, "winbad in the extract and commit steps");
  assertEquals(new Set(segs).size, 1);
  assertEquals(new Set(winbads).size, 1);
  // Both are POSIX ERE that read the same as a JS RegExp here; the
  // workflow matches winbad case-insensitively (grep -i, ${name,,}).
  const seg = new RegExp(`^${segs[0]}$`);
  const winbad = new RegExp(winbads[0], "i");
  const shellRefuses = (name: string) => !seg.test(name) || winbad.test(name);
  const toolRefuses = (name: string) => {
    try {
      assertSafeName(name, "test");
      return false;
    } catch {
      return true;
    }
  };
  for (
    const name of [
      ...WINDOWS_BAD_NAMES,
      "leptonChrome.css",
      "console.css",
      "com10.svg",
      "null.svg",
      ".git",
      "a b",
      "c++.svg",
      "LEPTON",
    ]
  ) {
    assertEquals(shellRefuses(name), toolRefuses(name), JSON.stringify(name));
  }
  // Paths: a bad segment anywhere, and directory entries ending in "/".
  for (
    const bad of ["chrome/icons/nul.svg", "chrome/CON/x", "chrome/x./y", "a/b."]
  ) {
    assert(winbad.test(bad), bad);
  }
  for (const ok of ["chrome/icons/", "chrome/css/leptonChrome.css"]) {
    assert(!winbad.test(ok), ok);
  }
});
