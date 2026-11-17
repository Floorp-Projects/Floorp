// SPDX-License-Identifier: MPL-2.0
// Debug Gecko builds crash (MOZ_CRASH) when a chrome: or about: page uses a
// CSP source outside nsContentSecurityUtils' allowlists. Keep every packaged
// Noraneko page CSP and the Runtime allowlist patch in step, so a new source
// or a new page cannot reach the native debug test cohort unreviewed.
import { strict as assert } from "node:assert";

const root = new URL("../../", import.meta.url);
const PATCH =
  ".github/patches/floorp-runtime/common/noraneko-chrome-csp-allowlist.patch";

// Document URLs each packaged page is loaded under (chrome: directly, or the
// about: alias registered in NoranekoStartup.sys.mts / the aboutDialog
// override in tools/src/injector.ts).
const PAGES: Record<string, string[]> = {
  "browser-features/pages-modal-child/index.html": [
    "chrome://noraneko-modal-child/content/index.html",
  ],
  "browser-features/pages-newtab/index.html": [
    "chrome://noraneko-newtab/content/index.html",
    "about:newtab",
    "about:home",
  ],
  "browser-features/pages-notes/index.html": [
    "chrome://noraneko-notes/content/index.html",
  ],
  "browser-features/pages-profile-manager/index.html": [
    "chrome://noraneko-profile-manager/content/index.html",
  ],
  "browser-features/pages-settings/index.html": [
    "chrome://noraneko-settings/content/index.html",
    "about:hub",
  ],
  "browser-features/pages-welcome/index.html": [
    "chrome://noraneko-welcome/content/index.html",
    "about:welcome",
  ],
  "browser-features/pages-aboutDialog/public/aboutDialog.html": [
    "chrome://browser/content/aboutDialog.xhtml",
  ],
};

// Entries already present upstream (Runtime 741c4537), per allowlist.
const UPSTREAM = {
  inline: ["about:newtab", "about:welcome", "about:home"],
  dataBlob: [
    "about:home",
    "about:newtab",
    "about:welcome",
    "chrome://browser/content/aboutDialog.xhtml",
  ],
  https: ["about:home", "about:newtab", "about:welcome"],
};

// Sources every debug check accepts without an allowlist entry.
const BUILTIN = new Set(["chrome:", "resource:", "moz-src:", "'none'"]);
const CHECKED = new Set([
  "default-src",
  "script-src",
  "style-src",
  "img-src",
  "media-src",
  "object-src",
]);

function csp(path: string): Map<string, string[]> {
  const html = Deno.readTextFileSync(new URL(path, root));
  const value = html.match(
    /http-equiv="Content-Security-Policy"\s+content="([^"]+)"/,
  )?.[1];
  assert.ok(value, `${path} must declare a CSP`);
  return new Map(
    value.split(";").map((d) => d.trim()).filter(Boolean).map((d) => {
      const [name, ...sources] = d.split(/\s+/);
      return [name, sources];
    }),
  );
}

function patchLists() {
  const text = Deno.readTextFileSync(new URL(PATCH, root));
  const hunks = text.split(/^@@ .*@@$/m).slice(1);
  assert.equal(hunks.length, 3, "inline, data/blob and https hunks only");
  const added = (hunk: string) =>
    [...hunk.matchAll(/^\+\s+"([^"]+)"_ns,$/gm)].map((m) => m[1]);
  // Each hunk is identified by the declaration that follows its list.
  const after = (hunk: string, marker: string) =>
    assert.ok(hunk.includes(marker), `hunk must end before ${marker}`);
  after(hunks[0], "// img-src moz-remote-image:");
  after(hunks[1], "// img-src https:");
  after(hunks[2], "// img-src http:");
  assert.ok(
    !/^-/m.test(text.replace(/^--- a\/.*$/m, "")),
    "the patch only adds entries",
  );
  return {
    inline: added(hunks[0]),
    dataBlob: added(hunks[1]),
    https: added(hunks[2]),
  };
}

Deno.test("packaged page CSPs only use checked directives and builtin defaults", () => {
  for (const path of Object.keys(PAGES)) {
    const policy = csp(path);
    for (const name of policy.keys()) {
      assert.ok(CHECKED.has(name), `${path}: unchecked directive ${name}`);
    }
    for (const source of policy.get("default-src") ?? []) {
      assert.ok(BUILTIN.has(source), `${path}: default-src ${source}`);
    }
    assert.deepEqual(policy.get("object-src"), ["'none'"], path);
    assert.ok(
      !(policy.get("style-src") ?? []).includes("data:"),
      `${path}: style-src data: is never allowlisted for Noraneko pages`,
    );
  }
});

Deno.test("every non-builtin page CSP source is allowlisted for each document URL", () => {
  const lists = patchLists();
  const need: Record<keyof typeof lists, Set<string>> = {
    inline: new Set(),
    dataBlob: new Set(),
    https: new Set(),
  };
  for (const [path, urls] of Object.entries(PAGES)) {
    const policy = csp(path);
    const style = policy.get("style-src") ?? [];
    const img = policy.get("img-src") ?? [];
    for (const source of style) {
      if (source === "'unsafe-inline'") urls.forEach((u) => need.inline.add(u));
      else assert.ok(BUILTIN.has(source), `${path}: style-src ${source}`);
    }
    for (const source of img) {
      if (source === "data:" || source === "blob:") {
        urls.forEach((u) => need.dataBlob.add(u));
      } else if (source === "https:") urls.forEach((u) => need.https.add(u));
      else {
        assert.ok(
          BUILTIN.has(source) || source === "moz-icon:",
          `${path}: img-src ${source}`,
        );
      }
    }
  }
  for (const key of Object.keys(need) as (keyof typeof lists)[]) {
    const expected = [...need[key]]
      .filter((u) => !UPSTREAM[key].includes(u))
      .sort();
    assert.deepEqual(
      [...lists[key]].sort(),
      expected,
      `${key} allowlist must match the packaged page CSPs exactly`,
    );
  }
});
