// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertRejects } from "@std/assert";
import { dirname, join } from "@std/path";
import { prepareRuntime, RUNTIME_SOURCE_FILES } from "./prepare-runtime.ts";

const PATCH_TARGET = "widget/cocoa/moz.build";
const PATCH_RELATIVE = "native/macos-app-shim/runtime/integration.patch";
const PATCH = `diff --git a/widget/cocoa/moz.build b/widget/cocoa/moz.build
--- a/widget/cocoa/moz.build
+++ b/widget/cocoa/moz.build
@@ -1 +1,2 @@
 SOURCES = []
+SOURCES += ["appshim/MacWebAppService.mm"]
`;

async function git(root: string, ...args: string[]): Promise<string> {
  const output = await new Deno.Command("git", {
    cwd: root,
    args,
    stdout: "piped",
    stderr: "piped",
  }).output();
  if (!output.success) throw new Error(new TextDecoder().decode(output.stderr));
  return new TextDecoder().decode(output.stdout).trim();
}

async function write(root: string, path: string, value: string): Promise<void> {
  const target = join(root, path);
  await Deno.mkdir(dirname(target), { recursive: true });
  await Deno.writeTextFile(target, value);
}

async function fixture() {
  const temporary = await Deno.makeTempDir({ prefix: "app-shim-runtime-" });
  const floorpRoot = join(temporary, "floorp");
  const runtimeRoot = join(temporary, "runtime");
  await Deno.mkdir(floorpRoot);
  await Deno.mkdir(runtimeRoot);
  await git(runtimeRoot, "init", "--quiet");
  await git(runtimeRoot, "config", "commit.gpgsign", "false");
  await git(runtimeRoot, "config", "core.hooksPath", "/dev/null");
  await write(runtimeRoot, PATCH_TARGET, "SOURCES = []\n");
  await write(runtimeRoot, "unrelated.txt", "untouched\n");
  await git(runtimeRoot, "add", ".");
  await git(
    runtimeRoot,
    "-c",
    "user.name=App Shim Test",
    "-c",
    "user.email=fixture@example.invalid",
    "commit",
    "--quiet",
    "-m",
    "fixture",
  );
  const commit = await git(runtimeRoot, "rev-parse", "HEAD");
  await write(
    floorpRoot,
    "floorp-runtime.lock.json",
    JSON.stringify({ source: { commit } }),
  );
  await write(floorpRoot, PATCH_RELATIVE, PATCH);
  for (const { source } of RUNTIME_SOURCE_FILES) {
    await write(floorpRoot, source, `// fixture ${source}\n`);
  }
  return {
    floorpRoot,
    runtimeRoot,
    temporary,
    check: () => prepareRuntime({ floorpRoot, runtimeRoot, mode: "check" }),
    apply: () => prepareRuntime({ floorpRoot, runtimeRoot, mode: "apply" }),
    cleanup: () => Deno.remove(temporary, { recursive: true }),
  };
}

Deno.test("check validates without changing either checkout", async () => {
  const f = await fixture();
  try {
    const result = await f.check();
    assertEquals(result.state, "ready");
    assertEquals(await git(f.runtimeRoot, "status", "--porcelain"), "");
    await assertRejects(
      () => Deno.stat(join(f.floorpRoot, "_dist")),
      Deno.errors.NotFound,
    );
  } finally {
    await f.cleanup();
  }
});

Deno.test("apply stages exact sources without touching the index and repeats safely", async () => {
  const f = await fixture();
  try {
    const result = await f.apply();
    assertEquals(result.state, "applied");
    for (const { source, destination } of RUNTIME_SOURCE_FILES) {
      assertEquals(
        await Deno.readTextFile(join(f.runtimeRoot, destination)),
        await Deno.readTextFile(join(f.floorpRoot, source)),
      );
    }
    assertEquals(
      await Deno.readTextFile(join(f.runtimeRoot, PATCH_TARGET)),
      'SOURCES = []\nSOURCES += ["appshim/MacWebAppService.mm"]\n',
    );
    assertEquals(
      await git(f.runtimeRoot, "diff", "--cached", "--name-only"),
      "",
    );
    assertEquals((await f.check()).state, "already-applied");
    assertEquals((await f.apply()).state, "already-applied");
  } finally {
    await f.cleanup();
  }
});

Deno.test("wrong revision is rejected before any Runtime mutation", async () => {
  const f = await fixture();
  try {
    await write(
      f.floorpRoot,
      "floorp-runtime.lock.json",
      JSON.stringify({ source: { commit: "0".repeat(40) } }),
    );
    await assertRejects(f.apply, Error, "does not match locked commit");
    assertEquals(await git(f.runtimeRoot, "status", "--porcelain"), "");
  } finally {
    await f.cleanup();
  }
});

Deno.test("unstaged and staged tracked changes are preserved and rejected", async () => {
  const f = await fixture();
  try {
    await write(f.runtimeRoot, PATCH_TARGET, "user change\n");
    await assertRejects(f.apply, Error, "Unowned changes");
    await git(f.runtimeRoot, "add", PATCH_TARGET);
    await assertRejects(f.apply, Error, "Staged changes");
    assertEquals(
      await Deno.readTextFile(join(f.runtimeRoot, PATCH_TARGET)),
      "user change\n",
    );
  } finally {
    await f.cleanup();
  }
});

Deno.test("existing untracked sources are never adopted even if identical", async () => {
  const f = await fixture();
  try {
    const { source, destination } = RUNTIME_SOURCE_FILES[0];
    await write(
      f.runtimeRoot,
      destination,
      await Deno.readTextFile(join(f.floorpRoot, source)),
    );
    await assertRejects(f.apply, Error, "Refusing to overwrite");
    assertEquals(
      await Deno.readTextFile(join(f.runtimeRoot, PATCH_TARGET)),
      "SOURCES = []\n",
    );
  } finally {
    await f.cleanup();
  }
});

Deno.test("prepared output modifications are detected and never overwritten", async () => {
  const f = await fixture();
  try {
    await f.apply();
    const { destination } = RUNTIME_SOURCE_FILES[0];
    await write(f.runtimeRoot, destination, "manual fix\n");
    await assertRejects(f.check, Error, "were modified");
    await assertRejects(f.apply, Error, "were modified");
    assertEquals(
      await Deno.readTextFile(join(f.runtimeRoot, destination)),
      "manual fix\n",
    );
  } finally {
    await f.cleanup();
  }
});

Deno.test("a reused Runtime path without preparation traces restages from scratch", async () => {
  const f = await fixture();
  try {
    await f.apply();
    const { source, destination } = RUNTIME_SOURCE_FILES[0];
    // Simulate re-cloning the same absolute path: every prepared file is gone
    // and the integration patch is no longer applied.
    await git(
      f.runtimeRoot,
      "apply",
      "--reverse",
      join(f.floorpRoot, PATCH_RELATIVE),
    );
    for (const entry of RUNTIME_SOURCE_FILES) {
      await Deno.remove(join(f.runtimeRoot, entry.destination));
    }
    assertEquals((await f.check()).state, "ready");
    assertEquals((await f.apply()).state, "applied");
    const staged = await Deno.readTextFile(join(f.runtimeRoot, destination));
    assertEquals(staged, `// fixture ${source}\n`);
    assertEquals(
      await Deno.readTextFile(join(f.runtimeRoot, PATCH_TARGET)),
      'SOURCES = []\nSOURCES += ["appshim/MacWebAppService.mm"]\n',
    );
  } finally {
    await f.cleanup();
  }
});

Deno.test("changed native inputs cannot silently replace a prepared revision", async () => {
  const f = await fixture();
  try {
    await f.apply();
    const { source, destination } = RUNTIME_SOURCE_FILES[0];
    const previous = await Deno.readTextFile(join(f.runtimeRoot, destination));
    await write(f.floorpRoot, source, "new implementation\n");
    await assertRejects(f.apply, Error, "Preparation inputs changed");
    assertEquals(
      await Deno.readTextFile(join(f.runtimeRoot, destination)),
      previous,
    );
  } finally {
    await f.cleanup();
  }
});

Deno.test("patch applied without our manifest is treated as unowned", async () => {
  const f = await fixture();
  try {
    await git(f.runtimeRoot, "apply", join(f.floorpRoot, PATCH_RELATIVE));
    await assertRejects(f.apply, Error, "Unowned changes");
  } finally {
    await f.cleanup();
  }
});

Deno.test("unrelated user changes remain allowed and untouched", async () => {
  const f = await fixture();
  try {
    await write(f.runtimeRoot, "unrelated.txt", "my edit\n");
    await f.apply();
    assertEquals(
      await Deno.readTextFile(join(f.runtimeRoot, "unrelated.txt")),
      "my edit\n",
    );
  } finally {
    await f.cleanup();
  }
});

Deno.test("symlink destination directories cannot redirect staging outside Runtime", async () => {
  const f = await fixture();
  try {
    const outside = join(f.temporary, "outside");
    await Deno.mkdir(outside);
    await Deno.symlink(outside, join(f.runtimeRoot, "widget/cocoa/appshim"));
    await assertRejects(f.apply, Error, "Symlink traversal refused");
    assertEquals(Array.from(Deno.readDirSync(outside)).length, 0);
    assertEquals(
      await Deno.readTextFile(join(f.runtimeRoot, PATCH_TARGET)),
      "SOURCES = []\n",
    );
  } finally {
    await f.cleanup();
  }
});

Deno.test("symlink native source files cannot be read into the Runtime", async () => {
  const f = await fixture();
  try {
    const source = join(f.floorpRoot, RUNTIME_SOURCE_FILES[0].source);
    await Deno.remove(source);
    await Deno.symlink(join(f.runtimeRoot, "unrelated.txt"), source);
    await assertRejects(f.apply, Error, "Symlink traversal refused");
    assertEquals(await git(f.runtimeRoot, "status", "--porcelain"), "");
  } finally {
    await f.cleanup();
  }
});

Deno.test("symlink manifest directories cannot redirect writes", async () => {
  const f = await fixture();
  try {
    const outside = join(f.temporary, "outside");
    await Deno.mkdir(outside);
    await Deno.symlink(outside, join(f.floorpRoot, "_dist"));
    await assertRejects(f.apply, Error, "Symlink traversal refused");
    assertEquals(Array.from(Deno.readDirSync(outside)).length, 0);
    assertEquals(await git(f.runtimeRoot, "status", "--porcelain"), "");
  } finally {
    await f.cleanup();
  }
});

Deno.test("integration patches cannot modify files outside the explicit allowlist", async () => {
  const f = await fixture();
  try {
    await write(
      f.floorpRoot,
      PATCH_RELATIVE,
      PATCH.replaceAll(PATCH_TARGET, "unrelated.txt").replace(
        " SOURCES = []",
        " untouched",
      ),
    );
    await assertRejects(
      f.apply,
      Error,
      "Unexpected runtime integration patch target",
    );
    assertEquals(await git(f.runtimeRoot, "status", "--porcelain"), "");
  } finally {
    await f.cleanup();
  }
});

Deno.test("invalid patch fails preflight without copying native files", async () => {
  const f = await fixture();
  try {
    await write(
      f.floorpRoot,
      PATCH_RELATIVE,
      PATCH.replace(" SOURCES = []", " WRONG CONTENT"),
    );
    await assertRejects(f.apply, Error, "git apply --check");
    assertEquals(await git(f.runtimeRoot, "status", "--porcelain"), "");
  } finally {
    await f.cleanup();
  }
});
