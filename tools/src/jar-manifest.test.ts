// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals } from "@std/assert";
import { resolveFromRoot } from "./utils.ts";

const JAR_MN_PATH = resolveFromRoot("browser-features/skin/jar.mn");

Deno.test("noraneko-skin jar.mn uses nora-skin prefix (not broken %/)", async () => {
  const content = await Deno.readTextFile(JAR_MN_PATH);
  const lines = content.split(/\r?\n/);

  const header = lines.find((line) => line.includes("noraneko-skin"));
  assert(header, "jar.mn must declare noraneko-skin package");
  assertEquals(
    header,
    "% content noraneko-skin %nora-skin/ contentaccessible=yes",
    "header must match gen_jarmn pattern to avoid noraneko// manifest",
  );
  assert(
    !content.includes("% content noraneko-skin %/"),
    "bare %/ produces invalid chrome manifest (noraneko//)",
  );

  const entries = lines.filter((line) =>
    line.startsWith("  ") && line.includes("(")
  );
  assert(entries.length > 0, "jar.mn must contain file entries");

  for (const entry of entries) {
    const leftPath = entry.trim().split(" ")[0];
    assert(
      leftPath.startsWith("nora-skin/"),
      `entry left path must be under nora-skin/: ${entry}`,
    );
  }
});
