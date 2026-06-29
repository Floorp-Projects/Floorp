// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertRejects, assertStringIncludes } from "@std/assert";
import * as path from "@std/path";
import { prepareFirefoxBrowserTests } from "./prepare_firefox_browser_tests.ts";

async function writeJson(filePath: string, value: unknown): Promise<void> {
  await Deno.mkdir(path.dirname(filePath), { recursive: true });
  await Deno.writeTextFile(filePath, `${JSON.stringify(value, null, 2)}\n`);
}

Deno.test("prepareFirefoxBrowserTests generates wrappers for allowlisted raw tests", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const collectionDir = path.join(dir, "collection");
    const allowlistPath = path.join(dir, "allowlist.json");
    const outputDir = path.join(dir, "generated");
    const upstreamPath =
      "browser/base/content/test/general/browser_bug537474.js";
    const outputPath = `files/${upstreamPath}`;
    await Deno.mkdir(path.dirname(path.join(collectionDir, outputPath)), {
      recursive: true,
    });
    await Deno.writeTextFile(
      path.join(collectionDir, outputPath),
      "add_task(async function test() {});\n",
    );
    await writeJson(path.join(collectionDir, "manifest.json"), {
      schemaVersion: 1,
      source: {
        repository: "Floorp-Projects/Floorp-Runtime",
        ref: "fixture-ref",
        commit: "fixture-commit",
      },
      files: [
        {
          path: upstreamPath,
          outputPath,
          roles: ["candidate"],
          harnesses: ["browser-chrome"],
        },
      ],
    });
    await writeJson(allowlistPath, [
      {
        name: "browser-bug-537474",
        path: upstreamPath,
        note: "fixture",
      },
    ]);

    const manifest = await prepareFirefoxBrowserTests({
      collectionDir,
      allowlistPath,
      outputDir,
    });

    assertEquals(manifest.tests.length, 1);
    assertEquals(manifest.tests[0]?.upstreamPath, upstreamPath);
    assertEquals(
      manifest.tests[0]?.importSpecifier,
      `#firefox-tests/${upstreamPath}`,
    );

    const wrapper = await Deno.readTextFile(
      path.join(outputDir, "browser-bug-537474.test.js"),
    );
    assertStringIncludes(wrapper, "@colocated-env browser");
    assertStringIncludes(wrapper, `import "#firefox-tests/${upstreamPath}";`);

    const generatedManifest = await Deno.readTextFile(
      path.join(outputDir, "manifest.json"),
    );
    assertStringIncludes(generatedManifest, "fixture-commit");
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("prepareFirefoxBrowserTests rejects allowlisted files missing from collection", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const collectionDir = path.join(dir, "collection");
    const allowlistPath = path.join(dir, "allowlist.json");
    await writeJson(path.join(collectionDir, "manifest.json"), {
      schemaVersion: 1,
      files: [],
    });
    await writeJson(allowlistPath, [
      "browser/base/content/test/general/browser_missing.js",
    ]);

    await assertRejects(
      () =>
        prepareFirefoxBrowserTests({
          collectionDir,
          allowlistPath,
          outputDir: path.join(dir, "generated"),
        }),
      Error,
      "missing from collection manifest",
    );
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});
