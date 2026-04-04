// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assert } from "@std/assert";
import { generateUpdateXml, writeBuildid2, readBuildid2 } from "./update.ts";
import * as path from "@std/path";

Deno.test("generateUpdateXml produces valid XML with correct metadata", () => {
  const tmpDir = Deno.makeTempDirSync();
  try {
    const meta = {
      version: "11.0.0",
      version_display: "11.0.0-beta.1",
      buildid: "20260401120000",
      noraneko_version: "2.0.0",
      mar_size: "12345678",
      mar_shasum: "abcdef1234567890",
    };
    const metaPath = path.join(tmpDir, "meta.json");
    const outputPath = path.join(tmpDir, "update.xml");
    Deno.writeTextFileSync(metaPath, JSON.stringify(meta));

    generateUpdateXml(metaPath, outputPath);

    const xml = Deno.readTextFileSync(outputPath);
    assert(xml.includes('displayVersion="11.0.0-beta.1"'), "should contain displayVersion");
    assert(xml.includes('appVersion="11.0.0"'), "should contain appVersion");
    assert(xml.includes('buildID="20260401120000"'), "should contain buildID");
    assert(xml.includes('appVersion2="2.0.0"'), "should contain appVersion2");
    assert(xml.includes('size="12345678"'), "should contain size");
    assert(xml.includes('hashValue="abcdef1234567890"'), "should contain hashValue");
    assert(xml.includes('hashFunction="sha512"'), "should contain hashFunction");
    assert(xml.includes("<?xml"), "should start with XML declaration");
    assert(xml.includes("<updates>"), "should contain updates element");
  } finally {
    Deno.removeSync(tmpDir, { recursive: true });
  }
});

Deno.test("writeBuildid2 and readBuildid2 round-trip", () => {
  const tmpDir = Deno.makeTempDirSync();
  try {
    const filePath = path.join(tmpDir, "buildid2.txt");
    writeBuildid2(filePath, "20260401120000");
    const result = readBuildid2(filePath);
    assertEquals(result, "20260401120000");
  } finally {
    Deno.removeSync(tmpDir, { recursive: true });
  }
});

Deno.test("readBuildid2 returns null for non-existent file", () => {
  const result = readBuildid2("/tmp/nonexistent_buildid2_file_" + Date.now());
  assertEquals(result, null);
});

Deno.test("writeBuildid2 creates parent directories", () => {
  const tmpDir = Deno.makeTempDirSync();
  try {
    const filePath = path.join(tmpDir, "nested", "dir", "buildid2.txt");
    writeBuildid2(filePath, "test-id");
    const result = readBuildid2(filePath);
    assertEquals(result, "test-id");
  } finally {
    Deno.removeSync(tmpDir, { recursive: true });
  }
});
