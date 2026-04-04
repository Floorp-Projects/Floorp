// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assert, assertThrows } from "@std/assert";
import {
  filterRuntimeEntries,
  scoreRuntimeEntry,
  pickRuntimeEntry,
} from "./initializer.ts";

// --- filterRuntimeEntries ---

Deno.test("filterRuntimeEntries returns empty array for null/undefined data", () => {
  assertEquals(filterRuntimeEntries({}), []);
  assertEquals(filterRuntimeEntries({ data: undefined }), []);
  assertEquals(filterRuntimeEntries({ data: null as unknown as undefined }), []);
});

Deno.test("filterRuntimeEntries filters out non-file entries", () => {
  const index = {
    data: [
      { type: "file", name: "artifact.zip" },
      { type: "directory", name: "somedir" },
      { type: "file", name: "" },
      { type: "file", name: "another.tar.xz" },
    ],
  };
  const result = filterRuntimeEntries(index);
  assertEquals(result.length, 2);
  assertEquals(result[0].name, "artifact.zip");
  assertEquals(result[1].name, "another.tar.xz");
});

Deno.test("filterRuntimeEntries handles entries with missing name", () => {
  const index = {
    data: [
      { type: "file" } as unknown as { type: string; name: string },
      { type: "file", name: "valid.zip" },
    ],
  };
  const result = filterRuntimeEntries(index);
  assertEquals(result.length, 1);
  assertEquals(result[0].name, "valid.zip");
});

// --- scoreRuntimeEntry ---

Deno.test("scoreRuntimeEntry gives platform points for windows keyword", () => {
  const score = scoreRuntimeEntry("floorp-windows-x86_64-artifact.zip", {
    platform: "windows",
    architecture: "x86_64",
    filename: "test.zip",
    format: "zip",
  });
  // "windows" matches both "windows" and "win" keywords (50+50=100),
  // "x86_64" matches "x86_64" keyword (40), artifact bonus (5), .zip bonus (5)
  assertEquals(score, 150);
});

Deno.test("scoreRuntimeEntry gives platform points for darwin keyword", () => {
  const score = scoreRuntimeEntry("floorp-macos-universal.dmg", {
    platform: "darwin",
    architecture: "universal",
    filename: "test.dmg",
    format: "dmg",
  });
  // "macos" matches both "macos" and "mac" keywords (50+50=100),
  // "universal" matches "universal" keyword (40) = 140
  assertEquals(score, 140);
});

Deno.test("scoreRuntimeEntry gives zero for unmatched entry", () => {
  const score = scoreRuntimeEntry("readme.txt", {
    platform: "linux",
    architecture: "x86_64",
    filename: "test.tar.xz",
    format: "tar.xz",
  });
  assertEquals(score, 0);
});

Deno.test("scoreRuntimeEntry adds bonus for .zip extension", () => {
  const withZip = scoreRuntimeEntry("linux-x86_64.zip", {
    platform: "linux",
    architecture: "x86_64",
    filename: "test.zip",
    format: "zip",
  });
  const withoutZip = scoreRuntimeEntry("linux-x86_64.tar", {
    platform: "linux",
    architecture: "x86_64",
    filename: "test.zip",
    format: "zip",
  });
  assert(withZip > withoutZip, "zip extension should add bonus points");
});

Deno.test("scoreRuntimeEntry adds bonus for artifact keyword", () => {
  const withArtifact = scoreRuntimeEntry("linux-artifact.tar", {
    platform: "linux",
    architecture: "x86_64",
    filename: "test.tar.xz",
    format: "tar.xz",
  });
  const withoutArtifact = scoreRuntimeEntry("linux-build.tar", {
    platform: "linux",
    architecture: "x86_64",
    filename: "test.tar.xz",
    format: "tar.xz",
  });
  assert(withArtifact > withoutArtifact, "artifact keyword should add bonus");
});

// --- pickRuntimeEntry ---

Deno.test("pickRuntimeEntry picks the highest-scored entry", () => {
  const entries = [
    { type: "file", name: "readme.txt" },
    { type: "file", name: "floorp-windows-x86_64-artifact.zip" },
    { type: "file", name: "floorp-linux-x86_64.tar.xz" },
  ];
  const picked = pickRuntimeEntry(entries, {
    platform: "windows",
    architecture: "x86_64",
    filename: "test.zip",
    format: "zip",
  });
  assertEquals(picked.name, "floorp-windows-x86_64-artifact.zip");
});

Deno.test("pickRuntimeEntry throws when entries array is empty", () => {
  assertThrows(
    () =>
      pickRuntimeEntry([], {
        platform: "windows",
        architecture: "x86_64",
        filename: "test.zip",
        format: "zip",
      }),
    Error,
    "No runtime artifacts available",
  );
});

Deno.test("pickRuntimeEntry returns first entry when no strong match", () => {
  const entries = [
    { type: "file", name: "unknown-file.dat" },
    { type: "file", name: "other-file.dat" },
  ];
  const picked = pickRuntimeEntry(entries, {
    platform: "linux",
    architecture: "aarch64",
    filename: "test.tar.xz",
    format: "tar.xz",
  });
  assertEquals(picked.name, "unknown-file.dat");
});
