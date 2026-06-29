// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertStringIncludes } from "@std/assert";
import * as path from "@std/path";
import { collectFirefoxTests } from "./collect_firefox_tests.ts";

async function runCommand(
  cwd: string,
  command: string,
  args: string[],
): Promise<void> {
  const result = await new Deno.Command(command, {
    args,
    cwd,
    stdout: "piped",
    stderr: "piped",
  }).output();
  if (!result.success) {
    const stderr = new TextDecoder().decode(result.stderr);
    throw new Error(`${command} ${args.join(" ")} failed: ${stderr}`);
  }
}

async function writeFixtureFile(
  root: string,
  relativePath: string,
  text: string,
): Promise<void> {
  const filePath = path.join(root, ...relativePath.split("/"));
  await Deno.mkdir(path.dirname(filePath), { recursive: true });
  await Deno.writeTextFile(filePath, text);
}

async function writeFixtureSymlink(
  root: string,
  relativePath: string,
  target: string,
): Promise<void> {
  const filePath = path.join(root, ...relativePath.split("/"));
  await Deno.mkdir(path.dirname(filePath), { recursive: true });
  await Deno.symlink(target, filePath);
}

async function createRuntimeFixture(): Promise<string> {
  const dir = await Deno.makeTempDir();
  await runCommand(dir, "git", ["init", "-q"]);
  await writeFixtureFile(
    dir,
    "browser/base/content/test/general/browser.toml",
    "[DEFAULT]\n",
  );
  await writeFixtureFile(
    dir,
    "browser/base/content/test/general/browser_bug537474.js",
    "add_task(async function test_bug() {});\n",
  );
  await writeFixtureFile(
    dir,
    "browser/base/content/test/general/head.js",
    "const HEAD = true;\n",
  );
  await writeFixtureFile(
    dir,
    "browser/base/content/test/general/support.html",
    "<!doctype html>\n",
  );
  await writeFixtureSymlink(
    dir,
    "browser/base/content/test/general/external-link.txt",
    "/etc/passwd",
  );
  await writeFixtureFile(
    dir,
    "browser/base/content/test/chrome/chrome.toml",
    "[DEFAULT]\n",
  );
  await writeFixtureFile(
    dir,
    "browser/base/content/test/chrome/chrome_window.js",
    "add_task(async function test_chrome() {});\n",
  );
  await writeFixtureFile(
    dir,
    "browser/components/customizableui/test/browser-common.toml",
    "[DEFAULT]\n",
  );
  await writeFixtureFile(
    dir,
    "browser/components/customizableui/test/browser_toolbar.js",
    "add_task(async function test_toolbar() {});\n",
  );
  await writeFixtureFile(
    dir,
    "dom/base/crashtests/crashtests.list",
    "load 123.html\n",
  );
  await writeFixtureFile(
    dir,
    "dom/base/crashtests/123.html",
    "<!doctype html>\n",
  );
  await writeFixtureFile(
    dir,
    "dom/base/test/mochitest.toml",
    "[DEFAULT]\n",
  );
  await writeFixtureFile(
    dir,
    "dom/base/test/test_parent.html",
    "<!doctype html>\n",
  );
  await writeFixtureFile(
    dir,
    "dom/base/test/reftest/reftest.list",
    "== reftest_child.html reftest_child.html\n",
  );
  await writeFixtureFile(
    dir,
    "dom/base/test/reftest/reftest_child.html",
    "<!doctype html>\n",
  );
  await writeFixtureFile(
    dir,
    "testing/web-platform/tests/url/url-origin.any.js",
    "test(() => {}, 'origin');\n",
  );
  await writeFixtureFile(
    dir,
    "testing/web-platform/meta/url/url-origin.any.js.ini",
    "[url-origin.any.html]\n",
  );
  await writeFixtureFile(
    dir,
    "browser/components/search/test/marionette/manifest.toml",
    "[DEFAULT]\n",
  );
  await writeFixtureFile(
    dir,
    "browser/components/search/test/marionette/test_search.py",
    "def test_search():\n    pass\n",
  );
  await writeFixtureFile(
    dir,
    "testing/firefox-ui/tests/functional/manifest.toml",
    "[DEFAULT]\n",
  );
  await writeFixtureFile(
    dir,
    "testing/firefox-ui/tests/functional/test_security.py",
    "def test_security():\n    pass\n",
  );
  await writeFixtureFile(
    dir,
    "browser/base/content/test/perftest.toml",
    "[DEFAULT]\n",
  );
  await writeFixtureFile(
    dir,
    "browser/base/content/test/browser_startup.js",
    "add_task(async function test_startup() {});\n",
  );
  await writeFixtureFile(
    dir,
    "toolkit/components/ml/tests/browser_eval/eval.toml",
    "[DEFAULT]\n",
  );
  await writeFixtureFile(
    dir,
    "toolkit/components/ml/tests/browser_eval/browser_eval.js",
    "add_task(async function test_eval() {});\n",
  );
  await writeFixtureFile(
    dir,
    "testing/mozbase/mozlog/tests/manifest.toml",
    "[DEFAULT]\n",
  );
  await writeFixtureFile(
    dir,
    "testing/mozbase/mozlog/tests/test_logger.py",
    "def test_logger():\n    pass\n",
  );
  await writeFixtureFile(
    dir,
    "python/mozbuild/mozbuild/test/python.toml",
    "[DEFAULT]\n",
  );
  await writeFixtureFile(
    dir,
    "python/mozbuild/mozbuild/test/test_backend.py",
    "def test_backend():\n    pass\n",
  );
  await writeFixtureFile(
    dir,
    "js/src/tests/jstests.list",
    "script non262/test.js\n",
  );
  await writeFixtureFile(
    dir,
    "js/src/tests/non262/test.js",
    "assertEq(1, 1);\n",
  );
  await writeFixtureFile(
    dir,
    "testing/web-platform/wptrunner.ini",
    "[wptrunner]\n",
  );
  await writeFixtureFile(
    dir,
    "testing/web-platform/docs/index.rst",
    "not a collected test root\n",
  );
  await writeFixtureFile(
    dir,
    "toolkit/components/example/tests/xpcshell/xpcshell.toml",
    "[DEFAULT]\n",
  );
  await writeFixtureFile(
    dir,
    "toolkit/components/example/tests/xpcshell/test_example.js",
    "function run_test() {}\n",
  );
  await writeFixtureFile(
    dir,
    "browser/base/content/not-a-test/browser_outside.js",
    "not collected\n",
  );
  await runCommand(dir, "git", ["add", "."]);
  await runCommand(dir, "git", [
    "-c",
    "user.name=Floorp Test",
    "-c",
    "user.email=floorp-test@example.invalid",
    "commit",
    "-q",
    "-m",
    "fixture",
  ]);
  return dir;
}

Deno.test("collectFirefoxTests collects browser chrome roots and candidates", async () => {
  const runtimeDir = await createRuntimeFixture();
  const outputDir = await Deno.makeTempDir();
  try {
    const manifest = await collectFirefoxTests({
      runtimeDir,
      outputDir,
      scope: "browser-chrome",
      sourceRepo: "Floorp-Projects/Floorp-Runtime",
      sourceRef: "fixture-ref",
    });

    assertEquals(manifest.scope, "browser-chrome");
    assertEquals(manifest.source.ref, "fixture-ref");
    assertEquals(manifest.counts.roots, 3);
    assertEquals(manifest.counts.candidates, 2);
    assertEquals(
      manifest.files.some((file) =>
        file.path ===
          "browser/base/content/test/general/browser_bug537474.js" &&
        file.roles.includes("candidate")
      ),
      true,
    );
    assertEquals(
      manifest.files.some((file) =>
        file.path ===
          "browser/components/customizableui/test/browser_toolbar.js" &&
        file.roles.includes("candidate")
      ),
      true,
    );
    assertEquals(
      manifest.files.some((file) =>
        file.path ===
          "toolkit/components/example/tests/xpcshell/test_example.js"
      ),
      false,
    );

    const copied = await Deno.readTextFile(
      path.join(
        outputDir,
        "files",
        "browser",
        "base",
        "content",
        "test",
        "general",
        "browser_bug537474.js",
      ),
    );
    assertStringIncludes(copied, "add_task");

    const candidates = await Deno.readTextFile(
      path.join(outputDir, "browser-chrome-candidates.json"),
    );
    assertStringIncludes(candidates, "browser_bug537474.js");

    const summary = await Deno.readTextFile(path.join(outputDir, "SUMMARY.md"));
    assertStringIncludes(summary, "Browser chrome candidates: 2");

    const copiedSymlinkBlob = await Deno.readTextFile(
      path.join(
        outputDir,
        "files",
        "browser",
        "base",
        "content",
        "test",
        "general",
        "external-link.txt",
      ),
    );
    assertEquals(copiedSymlinkBlob, "/etc/passwd");
  } finally {
    await Deno.remove(runtimeDir, { recursive: true });
    await Deno.remove(outputDir, { recursive: true });
  }
});

Deno.test("collectFirefoxTests all scope includes non browser chrome harnesses", async () => {
  const runtimeDir = await createRuntimeFixture();
  const outputDir = await Deno.makeTempDir();
  try {
    const manifest = await collectFirefoxTests({
      runtimeDir,
      outputDir,
      scope: "all",
      sourceRepo: "Floorp-Projects/Floorp-Runtime",
    });

    assertEquals(manifest.counts.roots, 16);
    assertEquals(manifest.harnessCounts.xpcshell, 2);
    assertEquals(manifest.harnessCounts.crashtest, 2);
    assertEquals(manifest.harnessCounts.eval, 2);
    assertEquals(manifest.harnessCounts["firefox-ui"], 2);
    assertEquals(manifest.harnessCounts.generic, 2);
    assertEquals(manifest.harnessCounts.js, 2);
    assertEquals(manifest.harnessCounts.marionette, 2);
    assertEquals(manifest.harnessCounts.mochitest, 2);
    assertEquals(manifest.harnessCounts.performance, 2);
    assertEquals(manifest.harnessCounts.python, 2);
    assertEquals(manifest.harnessCounts.reftest, 2);
    assertEquals(manifest.harnessCounts["web-platform"], 2);
    assertEquals(
      manifest.files.some((file) =>
        file.path ===
          "toolkit/components/example/tests/xpcshell/test_example.js" &&
        file.harnesses.includes("xpcshell") &&
        file.roles.includes("test")
      ),
      true,
    );
    assertEquals(
      manifest.files.some((file) =>
        file.path === "testing/web-platform/tests/url/url-origin.any.js" &&
        file.harnesses.includes("web-platform") &&
        file.roles.includes("test")
      ),
      true,
    );
    const nestedReftest = manifest.files.find((file) =>
      file.path === "dom/base/test/reftest/reftest_child.html"
    );
    assertEquals(nestedReftest?.harnesses, ["reftest"]);
    assertEquals(nestedReftest?.roles.includes("test"), true);
    assertEquals(
      manifest.files.some((file) =>
        file.path ===
          "browser/components/search/test/marionette/test_search.py" &&
        file.harnesses.includes("marionette") &&
        file.roles.includes("test")
      ),
      true,
    );
    assertEquals(
      manifest.files.some((file) =>
        file.path === "testing/web-platform/docs/index.rst"
      ),
      false,
    );
  } finally {
    await Deno.remove(runtimeDir, { recursive: true });
    await Deno.remove(outputDir, { recursive: true });
  }
});

Deno.test("collectFirefoxTests path prefix limits roots and copied files", async () => {
  const runtimeDir = await createRuntimeFixture();
  const outputDir = await Deno.makeTempDir();
  try {
    const manifest = await collectFirefoxTests({
      runtimeDir,
      outputDir,
      scope: "all",
      sourceRepo: "Floorp-Projects/Floorp-Runtime",
      pathPrefix: "browser/base/content/test/general/",
    });

    assertEquals(
      manifest.filters.pathPrefix,
      "browser/base/content/test/general",
    );
    assertEquals(manifest.counts.roots, 1);
    assertEquals(manifest.counts.candidates, 1);
    assertEquals(
      manifest.files.every((file) =>
        file.path.startsWith("browser/base/content/test/general/")
      ),
      true,
    );
    assertEquals(
      manifest.files.some((file) =>
        file.path === "browser/base/content/test/chrome/chrome_window.js"
      ),
      false,
    );
  } finally {
    await Deno.remove(runtimeDir, { recursive: true });
    await Deno.remove(outputDir, { recursive: true });
  }
});
