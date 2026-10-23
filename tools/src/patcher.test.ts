import { assertEquals, assertThrows } from "@std/assert";
import * as path from "@std/path";
import {
  patchAppliesToCurrentPlatform,
  reverseAppliedPatches,
} from "./patcher.ts";

Deno.test("patchAppliesToCurrentPlatform accepts generic patch files", () => {
  assertEquals(
    patchAppliesToCurrentPlatform("browser-modules-BrowserGlue.sys.patch"),
    true,
  );
});

Deno.test("patchAppliesToCurrentPlatform accepts unified patch files on all platforms", () => {
  assertEquals(
    patchAppliesToCurrentPlatform(
      "browser-chrome-browser-content-browser-browser-init.patch",
    ),
    true,
  );
  assertEquals(
    patchAppliesToCurrentPlatform(
      "browser-chrome-browser-content-browser-aboutDialog.patch",
    ),
    true,
  );
});

Deno.test("patchAppliesToCurrentPlatform filters platform-specific patch files", () => {
  const platform = Deno.build.os === "darwin"
    ? "darwin"
    : Deno.build.os === "windows"
    ? "windows"
    : "linux";

  assertEquals(
    patchAppliesToCurrentPlatform(
      "browser-chrome-browser-content-browser-browser-init.windows.patch",
    ),
    platform === "windows",
  );
});

function fixturePatch(file: string, before: string, after: string): string {
  return `diff --git a/${file} b/${file}
--- a/${file}
+++ b/${file}
@@ -1 +1 @@
-${before}
+${after}
`;
}

const canRunGit =
  (await Deno.permissions.query({ name: "run", command: "git" })).state ===
    "granted" &&
  (await Deno.permissions.query({ name: "env" })).state === "granted";

Deno.test("inverse plan follows application dependencies and forbids partial rejection", () => {
  const applied = Deno.makeTempDirSync({ prefix: "floorp-patch-plan-" });
  try {
    for (const name of ["b-dependent.patch", "a-base.patch"]) {
      Deno.writeTextFileSync(path.join(applied, name), "");
    }
    const reversed: string[] = [];
    reverseAppliedPatches("binary", applied, (command, args = []) => {
      assertEquals(command, "git");
      assertEquals(args.includes("-R"), true);
      assertEquals(args.includes("--reject"), false);
      reversed.push(path.basename(args.at(-1)!));
      return { success: true, code: 0, stdout: "", stderr: "" };
    });
    assertEquals(reversed, ["b-dependent.patch", "a-base.patch"]);
  } finally {
    Deno.removeSync(applied, { recursive: true });
  }
});

Deno.test({
  name: "reversing dependent applied patches restores their original base",
  ignore: !canRunGit,
  fn() {
    const fixture = Deno.makeTempDirSync({ prefix: "floorp-patch-order-" });
    try {
      const binary = path.join(fixture, "binary");
      const applied = path.join(fixture, "applied");
      Deno.mkdirSync(binary);
      Deno.mkdirSync(applied);
      Deno.writeTextFileSync(path.join(binary, "input.txt"), "second patch\n");
      // These patches represent A(base -> first), then B(first -> second).
      // Reversing A before B fails because its expected output no longer exists.
      Deno.writeTextFileSync(
        path.join(applied, "a-base.patch"),
        fixturePatch("input.txt", "original", "first patch"),
      );
      Deno.writeTextFileSync(
        path.join(applied, "b-dependent.patch"),
        fixturePatch("input.txt", "first patch", "second patch"),
      );
      reverseAppliedPatches(binary, applied);
      assertEquals(
        Deno.readTextFileSync(path.join(binary, "input.txt")),
        "original\n",
      );
    } finally {
      Deno.removeSync(fixture, { recursive: true });
    }
  },
});

Deno.test({
  name: "a failed inverse does not partially change files or leave rejects",
  ignore: !canRunGit,
  fn() {
    const fixture = Deno.makeTempDirSync({ prefix: "floorp-patch-atomic-" });
    try {
      const binary = path.join(fixture, "binary");
      const applied = path.join(fixture, "applied");
      Deno.mkdirSync(binary);
      Deno.mkdirSync(applied);
      Deno.writeTextFileSync(path.join(binary, "first.txt"), "patched first\n");
      Deno.writeTextFileSync(
        path.join(binary, "second.txt"),
        "unexpected content\n",
      );
      Deno.writeTextFileSync(
        path.join(applied, "a-multiple.patch"),
        fixturePatch("first.txt", "original first", "patched first") +
          fixturePatch("second.txt", "original second", "patched second"),
      );
      assertThrows(
        () => reverseAppliedPatches(binary, applied),
        Error,
        "Failed to reverse patch atomically",
      );
      assertEquals(
        Deno.readTextFileSync(path.join(binary, "first.txt")),
        "patched first\n",
      );
      assertEquals(
        Deno.readTextFileSync(path.join(binary, "second.txt")),
        "unexpected content\n",
      );
      assertEquals(
        [...Deno.readDirSync(binary)].map((entry) => entry.name).sort(),
        ["first.txt", "second.txt"],
      );
    } finally {
      Deno.removeSync(fixture, { recursive: true });
    }
  },
});
