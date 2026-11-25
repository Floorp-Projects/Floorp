// SPDX-License-Identifier: MPL-2.0
import * as path from "@std/path";

export function packagedQaEnabled(
  mode: string,
  flag: string | undefined,
  modules: string | undefined,
): boolean {
  if (!flag && !modules) return false;
  if (mode !== "production" || flag !== "1") {
    throw new Error(
      "Packaged QA requires production mode and FLOORP_PACKAGED_QA=1.",
    );
  }
  const parsed: unknown = JSON.parse(modules ?? "null");
  if (
    !Array.isArray(parsed) || parsed.length === 0 ||
    parsed.some((entry) => typeof entry !== "string" || !entry)
  ) {
    throw new Error("Packaged QA requires a nonempty test module array.");
  }
  return true;
}

/** Add generated QA assets to the normal pre-mach JAR manifest, not to a product. */
export function registerPackagedQa(contentDirectory: string): string[] {
  const manifestPath = path.join(contentDirectory, "jar.mn");
  const manifest = Deno.readTextFileSync(manifestPath);
  if (!manifest.includes("% content noraneko %nora-content/")) {
    throw new Error("Expected the generated production content manifest.");
  }
  if (manifest.includes("nora-content/qa/")) {
    throw new Error(
      "QA assets are already registered; rebuild production assets first.",
    );
  }
  const files: string[] = [];
  const visit = (relative: string) => {
    for (
      const entry of Deno.readDirSync(path.join(contentDirectory, relative))
    ) {
      if (entry.isSymlink || !/^[A-Za-z0-9._-]+$/.test(entry.name)) {
        throw new Error(`Unexpected QA asset: ${entry.name}`);
      }
      const child = `${relative}/${entry.name}`;
      if (entry.isDirectory) visit(child);
      else if (entry.isFile) files.push(child);
    }
  };
  visit("qa");
  for (const required of ["qa/packaged-tests.js", "qa/qa-test-manifest.json"]) {
    if (!files.includes(required)) {
      throw new Error(`Missing QA asset: ${required}`);
    }
  }
  files.sort();
  Deno.writeTextFileSync(
    manifestPath,
    `${manifest.trimEnd()}\n${
      files.map((file) => ` nora-content/${file} (${file})`).join("\n")
    }\n`,
  );
  return files;
}
