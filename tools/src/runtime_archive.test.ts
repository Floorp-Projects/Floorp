// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertRejects, assertThrows } from "@std/assert";
import * as path from "@std/path";
import { strToU8, zipSync } from "fflate";
import {
  assertSafeFilesystemTree,
  copyDirectoryTreeSafely,
  extractZipSafely,
  findSingleTopLevelAppDirectory,
  normalizeArchivePath,
  validateTarArchiveListings,
} from "./runtime_archive.ts";

function makeZip(
  entries: ReadonlyArray<readonly [string, string]>,
): Uint8Array {
  const input: Record<string, Uint8Array> = Object.create(null);
  for (const [name, value] of entries) input[name] = strToU8(value);
  return zipSync(input);
}

interface FirstZipLayout {
  readonly eocdOffset: number;
  readonly centralOffset: number;
  readonly localOffset: number;
  readonly centralExtraOffset: number;
  readonly localExtraOffset: number;
  readonly dataOffset: number;
  readonly compressedSize: number;
}

function firstZipLayout(archive: Uint8Array): FirstZipLayout {
  const view = new DataView(
    archive.buffer,
    archive.byteOffset,
    archive.byteLength,
  );
  let eocdOffset = -1;
  for (let offset = archive.length - 22; offset >= 0; offset -= 1) {
    if (
      view.getUint32(offset, true) === 0x06054b50 &&
      offset + 22 + view.getUint16(offset + 20, true) === archive.length
    ) {
      eocdOffset = offset;
      break;
    }
  }
  if (eocdOffset < 0) throw new Error("ZIP fixture has no EOCD record.");
  const centralOffset = view.getUint32(eocdOffset + 16, true);
  const centralNameLength = view.getUint16(centralOffset + 28, true);
  const localOffset = view.getUint32(centralOffset + 42, true);
  const localNameLength = view.getUint16(localOffset + 26, true);
  const localExtraLength = view.getUint16(localOffset + 28, true);
  return {
    eocdOffset,
    centralOffset,
    localOffset,
    centralExtraOffset: centralOffset + 46 + centralNameLength,
    localExtraOffset: localOffset + 30 + localNameLength,
    dataOffset: localOffset + 30 + localNameLength + localExtraLength,
    compressedSize: view.getUint32(centralOffset + 20, true),
  };
}

function mutateZip(
  archive: Uint8Array,
  mutate: (result: Uint8Array, view: DataView, layout: FirstZipLayout) => void,
): Uint8Array {
  const result = archive.slice();
  mutate(
    result,
    new DataView(result.buffer, result.byteOffset, result.byteLength),
    firstZipLayout(result),
  );
  return result;
}

function setFirstUnixType(archive: Uint8Array, mode: number): Uint8Array {
  return mutateZip(archive, (_result, view, layout) => {
    view.setUint16(layout.centralOffset + 4, 0x0314, true);
    view.setUint32(layout.centralOffset + 38, (mode << 16) >>> 0, true);
  });
}

function makeZipWithExtra(): Uint8Array {
  return zipSync({
    "floorp/extra.txt": [
      strToU8("extra"),
      { extra: { 0x1234: new Uint8Array([1, 2]) } },
    ],
  });
}

function insertUnlistedLocalRecord(archive: Uint8Array): Uint8Array {
  const layout = firstZipLayout(archive);
  const orphanArchive = makeZip([["floorp/orphan.txt", "orphan"]]);
  const orphanLayout = firstZipLayout(orphanArchive);
  const orphanRecord = orphanArchive.subarray(0, orphanLayout.centralOffset);
  const result = new Uint8Array(archive.length + orphanRecord.length);
  result.set(archive.subarray(0, layout.centralOffset), 0);
  result.set(orphanRecord, layout.centralOffset);
  result.set(
    archive.subarray(layout.centralOffset),
    layout.centralOffset + orphanRecord.length,
  );
  new DataView(result.buffer).setUint32(
    layout.eocdOffset + orphanRecord.length + 16,
    layout.centralOffset + orphanRecord.length,
    true,
  );
  return result;
}

function appendBytes(archive: Uint8Array, suffix: Uint8Array): Uint8Array {
  const result = new Uint8Array(archive.length + suffix.length);
  result.set(archive);
  result.set(suffix, archive.length);
  return result;
}

async function assertZipRejectedWithoutOutsideWrite(
  archive: Uint8Array,
  expectedRoot?: string,
): Promise<void> {
  const root = await Deno.makeTempDir();
  try {
    const archivePath = path.join(root, "fixture.zip");
    const destination = path.join(root, "stage");
    const sentinel = path.join(root, "outside.txt");
    await Deno.writeFile(archivePath, archive);
    await Deno.mkdir(destination);
    await Deno.writeTextFile(sentinel, "sentinel");
    await assertRejects(
      () => extractZipSafely(archivePath, destination, expectedRoot),
      Error,
    );
    assertEquals(await Deno.readTextFile(sentinel), "sentinel");
  } finally {
    await Deno.remove(root, { recursive: true });
  }
}

async function writeZip(
  filePath: string,
  entries: ReadonlyArray<readonly [string, string]>,
): Promise<void> {
  await Deno.writeFile(filePath, makeZip(entries));
}

Deno.test("archive paths reject platform escapes and unsafe Windows names", () => {
  for (
    const unsafe of [
      "../outside",
      "/absolute",
      "//server/share",
      "C:/drive/path",
      "\\\\server\\share",
      "floorp/..",
      "floorp/bad\\name",
      "floorp/control\u0001name",
      "floorp/CON",
      "floorp/AUX.txt",
      "floorp/trailing.",
      "floorp/trailing ",
      "floorp/stream:name",
    ]
  ) {
    assertThrows(() => normalizeArchivePath(unsafe, "file"), Error, "Archive");
  }
  assertEquals(
    normalizeArchivePath("./floorp/application.ini", "file"),
    "floorp/application.ini",
  );
});

Deno.test("strict ZIP32 extractor accepts stored and deflated files under the expected root", async () => {
  const root = await Deno.makeTempDir();
  try {
    const archive = path.join(root, "valid.zip");
    const destination = path.join(root, "stage");
    await Deno.mkdir(destination);
    await Deno.writeFile(
      archive,
      zipSync({
        "floorp/stored.txt": [strToU8("stored"), { level: 0 }],
        "floorp/deflated.txt": [
          strToU8("deflated payload ".repeat(128)),
          { level: 6 },
        ],
      }),
    );

    await extractZipSafely(archive, destination, "floorp");
    assertEquals(
      await Deno.readTextFile(path.join(destination, "floorp", "stored.txt")),
      "stored",
    );
    assertEquals(
      await Deno.readTextFile(
        path.join(destination, "floorp", "deflated.txt"),
      ),
      "deflated payload ".repeat(128),
    );
    await assertSafeFilesystemTree(destination);
  } finally {
    await Deno.remove(root, { recursive: true });
  }
});

Deno.test("confined ZIP extractor rejects escapes and case collisions without touching outside sentinels", async () => {
  const fixtures: ReadonlyArray<{
    name: string;
    entries: ReadonlyArray<readonly [string, string]>;
  }> = [
    { name: "traversal", entries: [["../outside.txt", "owned"]] },
    { name: "absolute", entries: [["/outside.txt", "owned"]] },
    { name: "drive", entries: [["C:/outside.txt", "owned"]] },
    {
      name: "case-collision",
      entries: [["floorp/Case.txt", "one"], ["floorp/case.txt", "two"]],
    },
  ];

  for (const fixture of fixtures) {
    const root = await Deno.makeTempDir();
    try {
      const archive = path.join(root, `${fixture.name}.zip`);
      const destination = path.join(root, "stage");
      const sentinel = path.join(root, "outside.txt");
      await Deno.mkdir(destination);
      await Deno.writeTextFile(sentinel, "sentinel");
      await writeZip(archive, fixture.entries);

      await assertRejects(
        () => extractZipSafely(archive, destination),
        Error,
      );
      assertEquals(await Deno.readTextFile(sentinel), "sentinel");
    } finally {
      await Deno.remove(root, { recursive: true });
    }
  }
});

Deno.test("strict ZIP32 rejects symlink and special Unix metadata", async () => {
  const base = makeZip([["floorp/unsafe", "payload"]]);
  for (const mode of [0o120777, 0o010644, 0o020644, 0o060644, 0o140644]) {
    await assertZipRejectedWithoutOutsideWrite(
      setFirstUnixType(base, mode),
      "floorp",
    );
  }
});

Deno.test("strict ZIP32 rejects ambiguous or inconsistent archive structure", async () => {
  const base = makeZip([["floorp/file.txt", "payload payload payload"]]);
  const stored = zipSync({
    "floorp/stored.txt": [strToU8("stored payload"), { level: 0 }],
  });
  const emptyStored = zipSync({
    "floorp/empty.txt": [new Uint8Array(), { level: 0 }],
  });
  const withExtra = makeZipWithExtra();

  const malformedCentralExtra = mutateZip(
    withExtra,
    (_result, view, layout) => {
      view.setUint16(layout.centralExtraOffset + 2, 0xffff, true);
    },
  );
  const malformedLocalExtra = mutateZip(
    withExtra,
    (_result, view, layout) => {
      view.setUint16(layout.localExtraOffset + 2, 0xffff, true);
    },
  );
  const aesExtra = mutateZip(withExtra, (_result, view, layout) => {
    view.setUint16(layout.centralExtraOffset, 0x9901, true);
  });
  const duplicateEocd = (() => {
    const layout = firstZipLayout(base);
    const result = appendBytes(
      base,
      base.subarray(layout.eocdOffset, layout.eocdOffset + 22),
    );
    new DataView(result.buffer).setUint16(layout.eocdOffset + 20, 22, true);
    return result;
  })();

  const cases: ReadonlyArray<readonly [string, Uint8Array]> = [
    [
      "central CRC differs from LFH",
      mutateZip(base, (_result, view, layout) => {
        view.setUint32(
          layout.centralOffset + 16,
          view.getUint32(layout.centralOffset + 16, true) ^ 1,
          true,
        );
      }),
    ],
    [
      "local raw name differs from central",
      mutateZip(base, (result, _view, layout) => {
        result[layout.localOffset + 30] ^= 1;
      }),
    ],
    [
      "local method differs from central",
      mutateZip(base, (_result, view, layout) => {
        view.setUint16(layout.localOffset + 8, 0, true);
      }),
    ],
    [
      "local size differs from central",
      mutateZip(base, (_result, view, layout) => {
        view.setUint32(
          layout.localOffset + 18,
          view.getUint32(layout.localOffset + 18, true) + 1,
          true,
        );
      }),
    ],
    [
      "unsupported method",
      mutateZip(base, (_result, view, layout) => {
        view.setUint16(layout.centralOffset + 10, 12, true);
        view.setUint16(layout.localOffset + 8, 12, true);
      }),
    ],
    [
      "deflate method without a bitstream",
      mutateZip(emptyStored, (_result, view, layout) => {
        view.setUint16(layout.centralOffset + 6, 20, true);
        view.setUint16(layout.centralOffset + 10, 8, true);
        view.setUint16(layout.localOffset + 4, 20, true);
        view.setUint16(layout.localOffset + 8, 8, true);
      }),
    ],
    [
      "encryption flag",
      mutateZip(base, (_result, view, layout) => {
        view.setUint16(
          layout.centralOffset + 8,
          view.getUint16(layout.centralOffset + 8, true) | 1,
          true,
        );
        view.setUint16(
          layout.localOffset + 6,
          view.getUint16(layout.localOffset + 6, true) | 1,
          true,
        );
      }),
    ],
    [
      "data descriptor flag",
      mutateZip(base, (_result, view, layout) => {
        view.setUint16(
          layout.centralOffset + 8,
          view.getUint16(layout.centralOffset + 8, true) | 8,
          true,
        );
        view.setUint16(
          layout.localOffset + 6,
          view.getUint16(layout.localOffset + 6, true) | 8,
          true,
        );
      }),
    ],
    [
      "ZIP64 size sentinel",
      mutateZip(base, (_result, view, layout) => {
        view.setUint32(layout.centralOffset + 20, 0xffff_ffff, true);
      }),
    ],
    [
      "multi-disk EOCD",
      mutateZip(base, (_result, view, layout) => {
        view.setUint16(layout.eocdOffset + 4, 1, true);
      }),
    ],
    [
      "central count mismatch",
      mutateZip(base, (_result, view, layout) => {
        view.setUint16(layout.eocdOffset + 8, 2, true);
        view.setUint16(layout.eocdOffset + 10, 2, true);
      }),
    ],
    [
      "central range mismatch",
      mutateZip(base, (_result, view, layout) => {
        view.setUint32(
          layout.eocdOffset + 12,
          view.getUint32(layout.eocdOffset + 12, true) - 1,
          true,
        );
      }),
    ],
    [
      "claimed preamble before first local record",
      mutateZip(base, (_result, view, layout) => {
        view.setUint32(layout.centralOffset + 42, 1, true);
      }),
    ],
    ["malformed central extra", malformedCentralExtra],
    ["malformed local extra", malformedLocalExtra],
    ["AES extra", aesExtra],
    [
      "missing central directory",
      mutateZip(base, (_result, view, layout) => {
        view.setUint32(layout.centralOffset, 0, true);
      }),
    ],
    ["truncated EOCD", base.subarray(0, base.length - 5).slice()],
    ["bytes after EOCD", appendBytes(base, new Uint8Array([0]))],
    ["unlisted local record", insertUnlistedLocalRecord(base)],
    ["more than one EOF-aligned EOCD", duplicateEocd],
    [
      "stored payload CRC mismatch",
      mutateZip(stored, (result, _view, layout) => {
        result[layout.dataOffset] ^= 1;
      }),
    ],
    [
      "declared CRC does not match payload",
      mutateZip(base, (_result, view, layout) => {
        const crc = view.getUint32(layout.centralOffset + 16, true) ^ 1;
        view.setUint32(layout.centralOffset + 16, crc, true);
        view.setUint32(layout.localOffset + 14, crc, true);
      }),
    ],
  ];

  for (const [name, archive] of cases) {
    try {
      await assertZipRejectedWithoutOutsideWrite(archive, "floorp");
    } catch (error) {
      throw new Error(`Invalid ZIP case was not safely rejected: ${name}.`, {
        cause: error,
      });
    }
  }
});

Deno.test("strict ZIP expected root rejects extra top-level members", async () => {
  await assertZipRejectedWithoutOutsideWrite(
    makeZip([
      ["floorp/application.ini", "valid"],
      ["other-root/unlisted.txt", "invalid"],
    ]),
    "floorp",
  );
});

Deno.test("tar listing accepts only regular files and directories in the Floorp root", () => {
  const count = validateTarArchiveListings(
    "floorp/\nfloorp/application.ini\nfloorp/floorp-bin\n",
    "drwxr-xr-x root/root 0 2026-01-01 floorp/\n" +
      "-rw-r--r-- root/root 10 2026-01-01 floorp/application.ini\n" +
      "-rwxr-xr-x root/root 10 2026-01-01 floorp/floorp-bin\n",
    "floorp",
  );
  assertEquals(count, 3);
});

Deno.test("tar listing rejects links and special filesystem members", () => {
  for (const type of ["l", "h", "c", "b", "p", "s"]) {
    assertThrows(
      () =>
        validateTarArchiveListings(
          "floorp/unsafe\n",
          `${type}rw-r--r-- root/root 0 2026-01-01 floorp/unsafe\n`,
          "floorp",
        ),
      Error,
      "forbidden type",
    );
  }
});

Deno.test("tar listing rejects unsafe paths and case-fold collisions", () => {
  for (
    const unsafe of [
      "../outside",
      "/absolute",
      "C:/drive",
      "floorp/bad\\name",
      "floorp/CON",
      "other-root/file",
    ]
  ) {
    assertThrows(() =>
      validateTarArchiveListings(
        `${unsafe}\n`,
        `-rw-r--r-- root/root 1 2026-01-01 ${unsafe}\n`,
        "floorp",
      )
    );
  }
  assertThrows(
    () =>
      validateTarArchiveListings(
        "floorp/Case\nfloorp/case\n",
        "-rw-r--r-- root/root 1 2026-01-01 floorp/Case\n" +
          "-rw-r--r-- root/root 1 2026-01-01 floorp/case\n",
        "floorp",
      ),
    Error,
    "case-fold",
  );
});

Deno.test("DMG-like safe copy selects one app and preserves outside sentinel", async () => {
  const root = await Deno.makeTempDir();
  try {
    const mount = path.join(root, "mount");
    const app = path.join(mount, "Floorp.app");
    const executable = path.join(app, "Contents", "MacOS", "floorp");
    const destination = path.join(root, "stage", "floorp", "Floorp.app");
    const sentinel = path.join(root, "outside.txt");
    await Deno.mkdir(path.dirname(executable), { recursive: true });
    await Deno.writeTextFile(executable, "binary");
    await Deno.writeTextFile(sentinel, "sentinel");

    assertEquals(
      await findSingleTopLevelAppDirectory(mount, "Floorp.app"),
      app,
    );
    await Deno.mkdir(path.dirname(destination), { recursive: true });
    await copyDirectoryTreeSafely(app, destination);
    assertEquals(
      await Deno.readTextFile(
        path.join(destination, "Contents", "MacOS", "floorp"),
      ),
      "binary",
    );
    assertEquals(await Deno.readTextFile(sentinel), "sentinel");
    await assertSafeFilesystemTree(path.join(root, "stage"));
  } finally {
    await Deno.remove(root, { recursive: true });
  }
});

Deno.test("DMG-like safe copy rejects source symlinks without touching their targets", async () => {
  const root = await Deno.makeTempDir();
  try {
    const source = path.join(root, "Floorp.app");
    const contents = path.join(source, "Contents");
    const destination = path.join(root, "stage", "Floorp.app");
    const sentinel = path.join(root, "outside.txt");
    await Deno.mkdir(contents, { recursive: true });
    await Deno.mkdir(path.dirname(destination), { recursive: true });
    await Deno.writeTextFile(sentinel, "sentinel");
    await Deno.symlink(sentinel, path.join(contents, "escape"), {
      type: "file",
    });

    await assertRejects(
      () => copyDirectoryTreeSafely(source, destination),
      Error,
      "special filesystem entry",
    );
    assertEquals(await Deno.readTextFile(sentinel), "sentinel");
  } finally {
    await Deno.remove(root, { recursive: true });
  }
});
