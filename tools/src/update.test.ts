// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals, assertThrows } from "@std/assert";
import { generateUpdateXml, readBuildid2, writeBuildid2 } from "./update.ts";
import * as path from "@std/path";

const alphaMarUrl =
  "https://github.com/Floorp-Projects/Floorp/releases/download/alpha/floorp-windows-x86_64-full.mar";
const signedWindowsMetadata = {
  schema_version: 2,
  version: "148.0",
  version_display: "12.1.0@148.0",
  buildid: "20260401120000",
  noraneko_version: "12.1.0",
  channel: "release",
  platform: "WINNT",
  arch: "x86_64",
  mar: {
    url:
      "https://github.com/Floorp-Projects/Floorp/releases/download/v12.1.0/floorp-windows-x86_64-full.mar",
    name: "floorp-windows-x86_64-full.mar",
    size: 12345678,
    sha512: "0123456789abcdef".repeat(8),
  },
};

Deno.test("Alpha generates update XML from signed v2 win-meta.json", () => {
  const tmpDir = Deno.makeTempDirSync();
  try {
    const metaPath = path.join(tmpDir, "win-meta.json");
    const outputPath = path.join(tmpDir, "WINNT_x86_64-msvc-x64.update.xml");
    Deno.writeTextFileSync(metaPath, JSON.stringify(signedWindowsMetadata));
    generateUpdateXml(metaPath, outputPath, alphaMarUrl);

    const xml = Deno.readTextFileSync(outputPath);
    assert(
      xml.includes(
        `<patch type="complete" URL="${alphaMarUrl}" size="12345678" hashFunction="sha512" hashValue="${signedWindowsMetadata.mar.sha512}"/>`,
      ),
      xml,
    );
    assert(xml.includes('appVersion2="12.1.0"'));
  } finally {
    Deno.removeSync(tmpDir, { recursive: true });
  }
});

Deno.test("Alpha rejects invalid signed v2 MAR metadata before writing XML", () => {
  const tmpDir = Deno.makeTempDirSync();
  try {
    const metaPath = path.join(tmpDir, "win-meta.json");
    const outputPath = path.join(tmpDir, "WINNT_x86_64-msvc-x64.update.xml");
    for (
      const [mar, invalidField] of [
        [undefined, "mar.size"],
        [{ ...signedWindowsMetadata.mar, size: 0 }, "mar.size"],
        [{ ...signedWindowsMetadata.mar, size: "12345678" }, "mar.size"],
        [
          { ...signedWindowsMetadata.mar, sha512: "a".repeat(127) },
          "mar.sha512",
        ],
        [
          { ...signedWindowsMetadata.mar, sha512: "z".repeat(128) },
          "mar.sha512",
        ],
      ] as const
    ) {
      Deno.writeTextFileSync(
        metaPath,
        JSON.stringify({
          ...signedWindowsMetadata,
          mar,
          mar_size: "12345678",
          mar_shasum: signedWindowsMetadata.mar.sha512,
        }),
      );
      assertThrows(
        () => generateUpdateXml(metaPath, outputPath, alphaMarUrl),
        Error,
        invalidField,
      );
      assertThrows(() => Deno.statSync(outputPath), Deno.errors.NotFound);
    }
  } finally {
    Deno.removeSync(tmpDir, { recursive: true });
  }
});

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
    assert(
      xml.includes(
        'URL="http://github.com/nyanrus/noraneko/releases/download/alpha/noraneko-win-x86_64-full.mar"',
      ),
      "should point to the published MAR",
    );
    assert(
      xml.includes('displayVersion="11.0.0-beta.1"'),
      "should contain displayVersion",
    );
    assert(xml.includes('appVersion="11.0.0"'), "should contain appVersion");
    assert(xml.includes('buildID="20260401120000"'), "should contain buildID");
    assert(xml.includes('appVersion2="2.0.0"'), "should contain appVersion2");
    assert(xml.includes('size="12345678"'), "should contain size");
    assert(
      xml.includes('hashValue="abcdef1234567890"'),
      "should contain hashValue",
    );
    assert(
      xml.includes('hashFunction="sha512"'),
      "should contain hashFunction",
    );
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
