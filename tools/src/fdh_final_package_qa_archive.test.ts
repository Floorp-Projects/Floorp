// SPDX-License-Identifier: MPL-2.0
import { assert, assertEquals } from "@std/assert";
import { deflateSync, zipSync } from "fflate";
import {
  QA_ARCHIVE_LIMITS,
  verifyFinalPackageQaArchive,
} from "./fdh_final_package_qa_archive.ts";
import type {
  QaArchiveFailureCode,
  QaArchiveRequest,
} from "./fdh_final_package_qa_archive_types.ts";

// Synthetic bytes only. No credential, filesystem, network or browser fixtures.
const encoder = new TextEncoder();
const json = (value: unknown) => encoder.encode(JSON.stringify(value, null, 2));
const pre = () => json({ target: "windows/x86_64", synthetic: true });
const post = () =>
  json({ sourceEligibility: { eligible: true }, checkedAt: "synthetic" });
function crc(bytes: Uint8Array): number {
  let n = 0xffffffff;
  for (const b of bytes) {
    n ^= b;
    for (let j = 0; j < 8; j++) n = (n >>> 1) ^ ((n & 1) ? 0xedb88320 : 0);
  }
  return (n ^ 0xffffffff) >>> 0;
}
function join(...parts: Uint8Array[]): Uint8Array<ArrayBuffer> {
  const out = new Uint8Array(parts.reduce((n, p) => n + p.length, 0));
  let at = 0;
  for (const part of parts) {
    out.set(part, at);
    at += part.length;
  }
  return out;
}
function fixtureEntry(
  name: string,
  data: Uint8Array = pre(),
  method = 8,
  descriptor = 16,
) {
  return {
    name,
    data,
    method,
    descriptor,
    raw: method === 0 ? data : deflateSync(data),
    declared: data.length,
    checksum: crc(data),
  };
}
function makeZip(
  entries = [
    fixtureEntry("input.json"),
    fixtureEntry("source-after-qa.json", post()),
  ],
) {
  const locals: Uint8Array[] = [], centrals: Uint8Array[] = [];
  let offset = 0;
  for (const e of entries) {
    const name = encoder.encode(e.name);
    const flags = e.descriptor ? 8 : 0;
    const local = new Uint8Array(30 + name.length);
    const v = new DataView(local.buffer);
    v.setUint32(0, 0x04034b50, true);
    v.setUint16(4, 20, true);
    v.setUint16(6, flags, true);
    v.setUint16(8, e.method, true);
    if (!e.descriptor) {
      v.setUint32(14, e.checksum, true);
      v.setUint32(18, e.raw.length, true);
      v.setUint32(22, e.declared, true);
    }
    v.setUint16(26, name.length, true);
    local.set(name, 30);
    const d = new Uint8Array(e.descriptor);
    if (e.descriptor) {
      const dv = new DataView(d.buffer), at = e.descriptor === 16 ? 4 : 0;
      if (at) dv.setUint32(0, 0x08074b50, true);
      dv.setUint32(at, e.checksum, true);
      dv.setUint32(at + 4, e.raw.length, true);
      dv.setUint32(at + 8, e.declared, true);
    }
    const c = new Uint8Array(46 + name.length), cv = new DataView(c.buffer);
    cv.setUint32(0, 0x02014b50, true);
    cv.setUint16(4, 20, true);
    cv.setUint16(6, 20, true);
    cv.setUint16(8, flags, true);
    cv.setUint16(10, e.method, true);
    cv.setUint32(16, e.checksum, true);
    cv.setUint32(20, e.raw.length, true);
    cv.setUint32(24, e.declared, true);
    cv.setUint16(28, name.length, true);
    cv.setUint32(42, offset, true);
    c.set(name, 46);
    centrals.push(c);
    locals.push(local, e.raw, d);
    offset += local.length + e.raw.length + d.length;
  }
  const directory = join(...centrals),
    end = new Uint8Array(22),
    ev = new DataView(end.buffer);
  ev.setUint32(0, 0x06054b50, true);
  ev.setUint16(8, entries.length, true);
  ev.setUint16(10, entries.length, true);
  ev.setUint32(12, directory.length, true);
  ev.setUint32(16, offset, true);
  return join(...locals, directory, end);
}
function central(bytes: Uint8Array): number {
  return new DataView(bytes.buffer).getUint32(bytes.length - 6, true);
}
function set16(bytes: Uint8Array, at: number, value: number) {
  new DataView(bytes.buffer).setUint16(at, value, true);
}
function set32(bytes: Uint8Array, at: number, value: number) {
  new DataView(bytes.buffer).setUint32(at, value, true);
}
async function request(
  bytes: Uint8Array = makeZip(),
): Promise<QaArchiveRequest> {
  const digest = new Uint8Array(
    await crypto.subtle.digest("SHA-256", new Uint8Array(bytes)),
  );
  return {
    bytes,
    artifact: { id: 123, name: "synthetic-qa-evidence" },
    expected: {
      id: 123,
      name: "synthetic-qa-evidence",
      digest: "sha256:" +
        Array.from(digest, (b) => b.toString(16).padStart(2, "0")).join(""),
    },
  };
}
async function rejected(bytes: Uint8Array, code?: QaArchiveFailureCode) {
  const result = await verifyFinalPackageQaArchive(await request(bytes));
  assert(!result.archiveIntegrityMatched);
  assertEquals(result.publicationAuthorized, false);
  assertEquals("input" in result, false);
  if (code) assertEquals(result.code, code);
}

Deno.test("archive CRC oracle known vector", () =>
  assertEquals(crc(encoder.encode("123456789")), 0xcbf43926));
for (const method of [0, 8]) {
  for (const descriptor of [0, 12, 16]) {
    Deno.test(
      "archive accepts method " + method + " descriptor " + descriptor,
      async () => {
        const result = await verifyFinalPackageQaArchive(
          await request(
            makeZip([
              fixtureEntry("input.json", pre(), method, descriptor),
              fixtureEntry("source-after-qa.json", post(), method, descriptor),
            ]),
          ),
        );
        assert(result.archiveIntegrityMatched);
        assertEquals(result.input, JSON.parse(new TextDecoder().decode(pre())));
        assertEquals(
          result.sourceAfterQa,
          JSON.parse(new TextDecoder().decode(post())),
        );
        assertEquals(result.entryCount, 2);
        assertEquals(result.expandedBytes, pre().length + post().length);
        assertEquals(result.scope, "archive-byte-integrity-only");
        for (
          const field of [
            result.publicationAuthorized,
            result.apiAuthenticityVerified,
            result.nativeQaVerified,
            result.liveApprovalVerified,
          ]
        ) assertEquals(field, false);
      },
    );
  }
}
Deno.test("archive fflate independently produced ZIP compatible", async () => {
  const result = await verifyFinalPackageQaArchive(
    await request(
      zipSync({
        "input.json": pre(),
        "source-after-qa.json": post(),
        "logs/synthetic.txt": encoder.encode("extra"),
      }),
    ),
  );
  assert(result.archiveIntegrityMatched);
  assertEquals(result.entryCount, 3);
  assertEquals("extras" in result, false);
});
Deno.test("archive actual layout shaped 8-entry fixture, extras discarded", async () => {
  const entries = [
    fixtureEntry("input.json"),
    fixtureEntry("source-after-qa.json", post()),
    ...Array.from(
      { length: 6 },
      (_, i) =>
        fixtureEntry("logs/report-" + i + ".txt", new Uint8Array(2000).fill(i)),
    ),
  ];
  const result = await verifyFinalPackageQaArchive(
    await request(makeZip(entries)),
  );
  assert(result.archiveIntegrityMatched);
  assertEquals(result.entryCount, 8);
});
for (const changed of ["id", "name", "digest"] as const) {
  Deno.test("archive rejects wrong trusted " + changed, async () => {
    const r = await request();
    if (changed === "id") {
      r.expected.id++;
    }
    if (changed === "name") r.expected.name += "-other";
    if (changed === "digest") r.expected.digest = "sha256:" + "0".repeat(64);
    const result = await verifyFinalPackageQaArchive(r);
    assert(!result.archiveIntegrityMatched);
    assertEquals(
      result.code,
      changed === "digest" ? "digest-mismatch" : "identity-mismatch",
    );
  });
}
Deno.test("archive hash covers exact supplied view not backing buffer", async () => {
  const zip = makeZip(),
    padded = join(new Uint8Array([1, 2]), zip, new Uint8Array([3]));
  const result = await verifyFinalPackageQaArchive(
    await request(padded.subarray(2, 2 + zip.length)),
  );
  assert(result.archiveIntegrityMatched);
});
Deno.test("archive caller mutation after first await cannot alter snapshot or claims", async () => {
  const r = await request();
  const pending = verifyFinalPackageQaArchive(r);
  r.bytes.fill(0);
  r.expected.digest = "sha256:" + "0".repeat(64);
  r.expected.id = 999;
  r.artifact.name = "changed";
  const result = await pending;
  assert(result.archiveIntegrityMatched);
  assertEquals(result.artifact.id, 123);
});
for (const mode of ["getter", "proxy"] as const) {
  Deno.test(
    "archive captures all metadata primitives exactly once via " + mode,
    async () => {
      const r = await request();
      const original = { ...r.expected };
      const counts: Record<string, number> = {};
      for (const owner of ["artifact", "expected"] as const) {
        const target = r[owner];
        const keys = owner === "artifact"
          ? ["id", "name"]
          : ["id", "name", "digest"];
        const read = (key: string, value: unknown) => {
          const counter = owner + "." + key;
          counts[counter] = (counts[counter] ?? 0) + 1;
          // A second read would either flip a validated claim or throw. Success
          // must retain the first captured primitives, never the flipped value.
          if (counts[counter] > 1) {
            throw new Error("metadata read more than once");
          }
          return value;
        };
        if (mode === "getter") {
          for (const key of keys) {
            const value = Reflect.get(target, key);
            Object.defineProperty(target, key, { get: () => read(key, value) });
          }
        } else {
          const handler = {
            get: (object: object, key: string | symbol) =>
              read(String(key), Reflect.get(object, key)),
          };
          if (owner === "artifact") {
            r.artifact = new Proxy<QaArchiveRequest["artifact"]>(
              r.artifact,
              handler,
            );
          } else {r.expected = new Proxy<QaArchiveRequest["expected"]>(
              r.expected,
              handler,
            );}
        }
      }
      const pending = verifyFinalPackageQaArchive(r);
      r.bytes.fill(0);
      const result = await pending;
      assert(result.archiveIntegrityMatched);
      assertEquals(result.artifact, original);
      assertEquals(counts, {
        "artifact.id": 1,
        "artifact.name": 1,
        "expected.id": 1,
        "expected.name": 1,
        "expected.digest": 1,
      });
    },
  );
}
Deno.test("archive metadata getter cannot flip compared identity before capture", async () => {
  const r = await request();
  let calls = 0;
  Object.defineProperty(r.expected, "id", {
    get: () => ++calls <= 2 ? 123 : 456,
  });
  const result = await verifyFinalPackageQaArchive(r);
  assert(result.archiveIntegrityMatched);
  assertEquals(result.artifact.id, 123);
  assertEquals(calls, 1);
});
for (const owner of ["artifact", "expected"] as const) {
  for (
    const key of owner === "artifact"
      ? ["id", "name"]
      : ["id", "name", "digest"]
  ) {
    Deno.test(
      "archive throwing metadata accessor fails closed " + owner + "." + key,
      async () => {
        const r = await request();
        Object.defineProperty(r[owner], key, {
          get: () => {
            throw new Error("synthetic accessor failure");
          },
        });
        const result = await verifyFinalPackageQaArchive(r);
        assert(!result.archiveIntegrityMatched);
        assertEquals(result.code, "invalid-input");
        assertEquals("input" in result, false);
      },
    );
  }
}
Deno.test("archive rejects shared and resizable input buffers", async () => {
  const r = await request();
  for (
    const buffer of [
      new SharedArrayBuffer(100),
      new ArrayBuffer(100, { maxByteLength: 200 }),
    ]
  ) {
    r.bytes = new Uint8Array(buffer);
    const result = await verifyFinalPackageQaArchive(r);
    assert(!result.archiveIntegrityMatched);
    assertEquals(result.code, "invalid-input");
  }
});
Deno.test("archive accepts valid empty extras for STORE and DEFLATE", async () => {
  for (const method of [0, 8]) {
    const result = await verifyFinalPackageQaArchive(
      await request(
        makeZip([
          fixtureEntry("input.json"),
          fixtureEntry("source-after-qa.json", post()),
          fixtureEntry("logs/empty.txt", new Uint8Array(), method),
        ]),
      ),
    );
    assert(result.archiveIntegrityMatched);
  }
});
Deno.test("archive rejects masked typed array length and buffer properties", async () => {
  for (const key of ["buffer", "length", "byteLength", "byteOffset"]) {
    const r = await request();
    Object.defineProperty(r.bytes, key, {
      value: key === "buffer" ? new ArrayBuffer(0) : 1,
    });
    const result = await verifyFinalPackageQaArchive(r);
    assert(!result.archiveIntegrityMatched);
    assertEquals(result.code, "invalid-input");
  }
});
Deno.test("archive compressed cap checked before hashing", async () => {
  await rejected(
    new Uint8Array(QA_ARCHIVE_LIMITS.archiveBytes + 1),
    "archive-limit",
  );
});
Deno.test("archive rejects false huge expansion before allocation", async () => {
  const e = fixtureEntry("input.json");
  e.declared = 0xfffffff0;
  await rejected(
    makeZip([e, fixtureEntry("source-after-qa.json", post())]),
    "expanded-limit",
  );
});
Deno.test("archive rejects aggregate advertised expansion", async () => {
  const extras = ["a", "b", "c"].map((n) => {
    const e = fixtureEntry(n);
    e.declared = 6 * 1024 * 1024;
    return e;
  });
  await rejected(
    makeZip([
      fixtureEntry("input.json"),
      fixtureEntry("source-after-qa.json", post()),
      ...extras,
    ]),
    "expanded-limit",
  );
});
for (const declared of [0, 1, 64]) {
  Deno.test(
    "archive false small declared size rejects bomb " + declared,
    async () => {
      const e = fixtureEntry(
        "input.json",
        new Uint8Array(4 * 1024 * 1024).fill(65),
      );
      e.declared = declared;
      await rejected(
        makeZip([e, fixtureEntry("source-after-qa.json", post())]),
        "compressed-data",
      );
    },
  );
}
Deno.test("archive checks CRC of required JSON and discarded extras", async () => {
  for (const name of ["input.json", "logs/extra.txt"]) {
    const entries = [
      fixtureEntry("input.json"),
      fixtureEntry("source-after-qa.json", post()),
    ];
    if (name !== "input.json") entries.push(fixtureEntry(name));
    entries.find((e) => e.name === name)!.checksum ^= 1;
    await rejected(makeZip(entries), "crc-mismatch");
  }
});
for (
  const bad of [
    new Uint8Array(),
    new Uint8Array([7]),
    new Uint8Array([1, 1, 0, 0, 0, 65]),
    new Uint8Array([3]),
  ]
) {
  Deno.test("archive rejects corrupt compressed " + [...bad], async () => {
    const e = fixtureEntry("input.json");
    e.raw = bad;
    await rejected(
      makeZip([e, fixtureEntry("source-after-qa.json", post())]),
      "compressed-data",
    );
  });
}
for (const suffix of [new Uint8Array([0]), deflateSync(pre())]) {
  Deno.test(
    "archive rejects trailing/concatenated raw stream " + suffix.length,
    async () => {
      const e = fixtureEntry("input.json");
      e.raw = join(e.raw, suffix);
      await rejected(
        makeZip([e, fixtureEntry("source-after-qa.json", post())]),
        "compressed-data",
      );
    },
  );
}
Deno.test("archive rejects truncated compressed stream even with coherent ZIP sizes", async () => {
  const e = fixtureEntry("input.json");
  e.raw = e.raw.subarray(0, e.raw.length - 1);
  await rejected(
    makeZip([e, fixtureEntry("source-after-qa.json", post())]),
    "compressed-data",
  );
});
for (
  const name of [
    "../input.json",
    "/input.json",
    "a//b",
    "a/./b",
    "a/../b",
    "a\\b",
    "C:foo",
    "foo/",
    "CON.txt",
    "logs/nul",
    "a.",
    "a b",
    "a%2fb",
    "é.json",
    "a\0b",
    "a".repeat(241),
    "INPUT.JSON",
    "logs",
    "input.json/extra",
  ]
) {
  Deno.test(
    "archive unsafe or ambiguous path " + JSON.stringify(name),
    async () => {
      const entries = [
        fixtureEntry("input.json"),
        fixtureEntry("source-after-qa.json", post()),
        fixtureEntry("logs/extra.txt"),
        fixtureEntry(name),
      ];
      await rejected(makeZip(entries), "unsafe-path");
    },
  );
}
Deno.test("archive duplicate exact root path", async () =>
  await rejected(
    makeZip([
      fixtureEntry("input.json"),
      fixtureEntry("input.json"),
      fixtureEntry("source-after-qa.json", post()),
    ]),
    "unsafe-path",
  ));
Deno.test("archive missing required root JSON cannot use nested basename", async () =>
  await rejected(
    makeZip([
      fixtureEntry("logs/input.json"),
      fixtureEntry("source-after-qa.json", post()),
    ]),
    "zip-structure",
  ));
for (
  const [label, mutate] of [
    ["encryption", (b: Uint8Array) => set16(b, central(b) + 8, 1)],
    ["reserved flags", (b: Uint8Array) => set16(b, central(b) + 8, 16)],
    ["multidisk", (b: Uint8Array) => set16(b, b.length - 18, 1)],
    ["entry disk", (b: Uint8Array) => set16(b, central(b) + 34, 1)],
    [
      "ZIP64 sentinel",
      (b: Uint8Array) => set32(b, central(b) + 24, 0xffffffff),
    ],
    ["ZIP64 version", (b: Uint8Array) => set16(b, central(b) + 6, 45)],
    ["method", (b: Uint8Array) => set16(b, central(b) + 10, 9)],
    ["symlink", (b: Uint8Array) => set32(b, central(b) + 38, 0xa1ff0000)],
    ["directory attrs", (b: Uint8Array) => set32(b, central(b) + 38, 16)],
    ["central extra", (b: Uint8Array) => set16(b, central(b) + 30, 4)],
    ["local extra", (b: Uint8Array) => set16(b, 28, 4)],
    ["entry comment", (b: Uint8Array) => set16(b, central(b) + 32, 1)],
    ["EOCD comment", (b: Uint8Array) => set16(b, b.length - 2, 1)],
    ["central/local flags", (b: Uint8Array) => set16(b, 6, 0)],
    ["central/local name", (b: Uint8Array) => {
      b[30] = 88;
    }],
    ["central/local method", (b: Uint8Array) => set16(b, 8, 0)],
    ["central/local time", (b: Uint8Array) => set16(b, 10, 1)],
    ["central/local sizes", (b: Uint8Array) => set32(b, 22, 1)],
    ["central/local CRC", (b: Uint8Array) => set32(b, 14, 1)],
    ["local overlap", (b: Uint8Array) => set32(b, central(b) + 42, 1)],
    ["EOCD count", (b: Uint8Array) => set16(b, b.length - 14, 3)],
    ["EOCD size", (b: Uint8Array) => set32(b, b.length - 10, 1)],
    ["descriptor CRC", (b: Uint8Array) => {
      const v = new DataView(b.buffer), c = central(b);
      const dataEnd = 30 + 10 + v.getUint32(c + 20, true);
      set32(b, dataEnd + 4, 1);
    }],
    ["descriptor compressed size", (b: Uint8Array) => {
      const v = new DataView(b.buffer), c = central(b);
      const dataEnd = 40 + v.getUint32(c + 20, true);
      set32(b, dataEnd + 8, 1);
    }],
    ["descriptor expanded size", (b: Uint8Array) => {
      const v = new DataView(b.buffer), c = central(b);
      const dataEnd = 40 + v.getUint32(c + 20, true);
      set32(b, dataEnd + 12, 1);
    }],
  ] as const
) {
  Deno.test("archive rejects metadata conflict " + label, async () => {
    const b = makeZip();
    mutate(b);
    await rejected(b);
  });
}
Deno.test("archive rejects ZIP truncation, prefix and suffix", async () => {
  const b = makeZip();
  for (
    const altered of [
      b.subarray(0, b.length - 1),
      join(b, new Uint8Array([0])),
      join(new Uint8Array([0]), b),
    ]
  ) await rejected(altered);
});
Deno.test("archive rejects excessive entry count", async () =>
  await rejected(
    makeZip([
      fixtureEntry("input.json"),
      fixtureEntry("source-after-qa.json", post()),
      ...Array.from({ length: 63 }, (_, i) => fixtureEntry("extra-" + i)),
    ]),
    "archive-limit",
  ));
for (
  const text of [
    '{"a":1}',
    '{\n  "a": 1,\n  "a": 2\n}',
    '{\n  "a": 1,\n  "\\u0061": 2\n}',
    "[]",
    "null",
    '"string"',
    '{\n  "a": NaN\n}',
    '{\n  "a": 1e999\n}',
    '{\n  "a": -0\n}',
    '{\n  "a": 1\n}\n\n',
    "{",
    '{\n  "a": 1,\n}',
  ]
) {
  Deno.test(
    "archive rejects noncanonical/invalid JSON " + text,
    async () =>
      await rejected(
        makeZip([
          fixtureEntry("input.json", encoder.encode(text)),
          fixtureEntry("source-after-qa.json", post()),
        ]),
        "json-format",
      ),
  );
}
Deno.test("archive JSON UTF8 and BOM strict", async () => {
  for (
    const data of [
      new Uint8Array([0xc0, 0xaf]),
      join(new Uint8Array([0xef, 0xbb, 0xbf]), pre()),
    ]
  ) {
    await rejected(
      makeZip([
        fixtureEntry("input.json", data),
        fixtureEntry("source-after-qa.json", post()),
      ]),
      "json-format",
    );
  }
});
Deno.test("archive JSON nesting prescan rejects depth65, accepts64 and quoted braces", async () => {
  for (const depth of [64, 65]) {
    let value: unknown = { text: '}{[]"\\' };
    for (let i = 1; i < depth; i++) value = { nested: value };
    const result = await verifyFinalPackageQaArchive(
      await request(
        makeZip([
          fixtureEntry("input.json", json(value)),
          fixtureEntry("source-after-qa.json", post()),
        ]),
      ),
    );
    assertEquals(result.archiveIntegrityMatched, depth === 64);
  }
});
Deno.test("archive rejects forwarding Proxy before any byte property reads", async () => {
  const r = await request(makeZip());
  let reads = 0;
  r.bytes = new Proxy(r.bytes, {
    get(target, key) {
      reads++;
      const value: unknown = Reflect.get(target, key, target);
      return typeof value === "function" ? value.bind(target) : value;
    },
  });
  const result = await verifyFinalPackageQaArchive(r);
  assert(!result.archiveIntegrityMatched);
  assertEquals(result.code, "invalid-input");
  assertEquals(reads, 0);
});
Deno.test("archive rejects Proxy size and iterator spoof before copy", async () => {
  const r = await request(makeZip());
  let iterations = 0;
  const original = r.bytes;
  r.bytes = new Proxy(original, {
    get(target, key) {
      if (key === "length" || key === "byteLength") return 22;
      if (key === Symbol.iterator) {
        return function* () {
          iterations++;
          yield* original;
          yield* original;
        };
      }
      const value: unknown = Reflect.get(target, key, target);
      return typeof value === "function" ? value.bind(target) : value;
    },
  });
  const result = await verifyFinalPackageQaArchive(r);
  assert(!result.archiveIntegrityMatched);
  assertEquals(result.code, "invalid-input");
  assertEquals(iterations, 0);
});
Deno.test("archive rejects nonordinary backing ArrayBuffer subclass", async () => {
  const r = await request(makeZip());
  class SyntheticBuffer extends ArrayBuffer {}
  const view = new Uint8Array(new SyntheticBuffer(r.bytes.length));
  view.set(r.bytes);
  r.bytes = view;
  const result = await verifyFinalPackageQaArchive(r);
  assert(!result.archiveIntegrityMatched);
  assertEquals(result.code, "invalid-input");
});
Deno.test("archive rejects backing prototype before resizable accessor", async () => {
  const r = await request(makeZip());
  let reads = 0;
  const prototype = Object.create(ArrayBuffer.prototype);
  Object.defineProperty(prototype, "resizable", {
    get() {
      reads++;
      return false;
    },
  });
  Object.setPrototypeOf(r.bytes.buffer, prototype);
  const result = await verifyFinalPackageQaArchive(r);
  assert(!result.archiveIntegrityMatched);
  assertEquals(result.code, "invalid-input");
  assertEquals(reads, 0);
});

for (const ElementArray of [Uint16Array, Uint8ClampedArray]) {
  Deno.test(
    "archive rejects forged Uint8 prototype on " + ElementArray.name,
    async () => {
      const r = await request(makeZip());
      const forged = new ElementArray(r.bytes);
      Object.setPrototypeOf(forged, Uint8Array.prototype);
      r.bytes = forged as unknown as Uint8Array;
      const result = await verifyFinalPackageQaArchive(r);
      assert(!result.archiveIntegrityMatched);
      assertEquals(result.code, "invalid-input");
    },
  );
}

Deno.test("archive accepts optional one LF and UTF8 JSON string values", async () => {
  const result = await verifyFinalPackageQaArchive(
    await request(
      makeZip([
        fixtureEntry(
          "input.json",
          join(json({ name: "合成" }), encoder.encode("\n")),
        ),
        fixtureEntry("source-after-qa.json", post()),
      ]),
    ),
  );
  assert(result.archiveIntegrityMatched);
});
