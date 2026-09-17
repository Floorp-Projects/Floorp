import { assertEquals, assertNotEquals } from "@std/assert";
import * as path from "@std/path";

const ROOT = path.fromFileUrl(new URL("../../", import.meta.url));

const PATCH_PAIRS = [
  {
    name: "browser icon picker",
    sourcePatch: "custom-app-icons-browser-icon.patch",
    runtimePatch: "custom-app-icons-browser-icon.windows.patch",
    sourcePath: "browser/components/preferences/config/browser-icon.mjs",
    runtimePath:
      "browser/chrome/browser/content/browser/preferences/config/browser-icon.mjs",
  },
  {
    name: "localization",
    sourcePatch: "custom-app-icons-l10n.patch",
    runtimePatch: "custom-app-icons-l10n.windows.patch",
    sourcePath: "browser/locales/en-US/browser/preferences/browserIcon.ftl",
    runtimePath:
      "browser/localization/en-US/browser/preferences/browserIcon.ftl",
  },
  {
    name: "icon manager",
    sourcePatch: "custom-app-icons-manager.patch",
    runtimePatch: "custom-app-icons-manager.windows.patch",
    sourcePath: "browser/components/shell/CustomIconManager.sys.mjs",
    runtimePath: "moz-src/browser/components/shell/CustomIconManager.sys.mjs",
  },
] as const;

function normalizeNewlines(value: string): string {
  return value.replaceAll("\r\n", "\n").trimEnd();
}

function patchedPath(patch: string, marker: "---" | "+++"): string {
  const line = normalizeNewlines(patch).split("\n").find((candidate) =>
    candidate.startsWith(`${marker} `)
  );
  assertNotEquals(line, undefined, `${marker} patch header is present`);
  return line!.slice(marker.length + 1).replace(/^[ab]\//, "");
}

function patchHunks(patch: string): string {
  const lines = normalizeNewlines(patch).split("\n");
  const firstHunk = lines.findIndex((line) => line.startsWith("@@ "));
  assertNotEquals(firstHunk, -1, "patch contains at least one hunk");
  return lines.slice(firstHunk).join("\n");
}

for (const pair of PATCH_PAIRS) {
  Deno.test(`${pair.name} source and runtime patches stay in sync`, async () => {
    const sourcePatch = await Deno.readTextFile(
      path.join(
        ROOT,
        ".github",
        "patches",
        "floorp-runtime",
        pair.sourcePatch,
      ),
    );
    const runtimePatch = await Deno.readTextFile(
      path.join(ROOT, "tools", "patches", pair.runtimePatch),
    );

    assertEquals(patchedPath(sourcePatch, "---"), pair.sourcePath);
    assertEquals(patchedPath(sourcePatch, "+++"), pair.sourcePath);
    assertEquals(patchedPath(runtimePatch, "---"), pair.runtimePath);
    assertEquals(patchedPath(runtimePatch, "+++"), pair.runtimePath);
    assertEquals(patchHunks(runtimePatch), patchHunks(sourcePatch));
  });
}
