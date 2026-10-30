// SPDX-License-Identifier: MPL-2.0
import { assertEquals, assertRejects, assertThrows } from "@std/assert";
import { zipSync } from "fflate";
import {
  sha256,
  verifyFrozenCorpusScripts,
} from "../chrome-extension-runtime/corpus-source-integrity.mjs";
import { validateNativeCorpusBackground } from "../chrome-extension-runtime/corpus-native-validation.mjs";
import type { NativeCorpusBackgroundSnapshot } from "../chrome-extension-runtime/native-test-types.ts";

const encode = (value: string) => new TextEncoder().encode(value);
const manifest = encode(
  JSON.stringify({
    manifest_version: 3,
    background: { service_worker: "worker.js" },
  }),
);
const original = {
  "manifest.json": manifest,
  "worker.js": encode("self.original = 73195;"),
  "nested/view.MJS": encode("export const value = 42;"),
};

// Header-only test archives exercise the inventory boundary. They are never
// installed; production installFrozenCorpus also verifies the actual signature.
function crx(files: Record<string, Uint8Array>): Uint8Array {
  const zip = zipSync(files), bytes = new Uint8Array(16 + zip.length);
  bytes.set(encode("Cr24"));
  new DataView(bytes.buffer).setUint32(4, 2, true);
  bytes.set(zip, 16);
  return bytes;
}
async function verify(
  files: Record<string, Uint8Array> = original,
  converted: Record<string, Uint8Array> = original,
) {
  const bytes = crx(files);
  return await verifyFrozenCorpusScripts(
    bytes,
    new Uint8Array(zipSync(converted)).buffer,
    { packageSha256: await sha256(bytes) },
  );
}

Deno.test("frozen inventory compares every script directly with original CRX bytes", async () => {
  const result = await verify();
  assertEquals(result.unchangedScriptCount, 2);
  assertEquals(result.originalServiceWorker, "worker.js");
  assertEquals(result.convertedServiceWorker, "worker.js");
  assertEquals(result.unchangedScriptsSha256.length, 64);
});
Deno.test("frozen inventory rejects omitted, added and rewritten original scripts", async () => {
  await assertRejects(
    () =>
      verify(original, {
        "manifest.json": manifest,
        "worker.js": original["worker.js"],
      }),
    Error,
    "inventory differs",
  );
  await assertRejects(
    () => verify(original, { ...original, "extra.js": encode("injected()") }),
    Error,
    "inventory differs",
  );
  await assertRejects(
    () =>
      verify(original, {
        ...original,
        "worker.js": encode("self.original = 0;"),
      }),
    Error,
    "script changed",
  );
});
Deno.test("frozen inventory permits manifest conversion and records genuine worker fallback", async () => {
  const result = await verify(original, {
    ...original,
    "manifest.json": encode(
      JSON.stringify({
        manifest_version: 3,
        background: { scripts: ["worker.js"] },
      }),
    ),
  });
  assertEquals(result.originalServiceWorker, "worker.js");
  assertEquals(result.convertedServiceWorker, null);
});
Deno.test("frozen inventory requires pinned package digest and CRX header", async () => {
  const bytes = crx(original), xpi = new Uint8Array(zipSync(original)).buffer;
  await assertRejects(
    () =>
      verifyFrozenCorpusScripts(bytes, xpi, { packageSha256: "0".repeat(64) }),
    Error,
    "CRX hash mismatch",
  );
  bytes[0] = 0;
  await assertRejects(
    async () =>
      verifyFrozenCorpusScripts(bytes, xpi, {
        packageSha256: await sha256(bytes),
      }),
    Error,
    "ZIP extraction failed",
  );
});
Deno.test("frozen archive rejects unsafe original and converted paths", async () => {
  for (
    const name of [
      "../escape.js",
      "/absolute.js",
      "C:/drive.js",
      "a\\b.js",
      "a/./b.js",
      "a/../b.js",
      "nul\0.js",
    ]
  ) {
    await assertRejects(
      () => verify({ ...original, [name]: encode("unsafe") }),
      Error,
      "Unsafe",
    );
    await assertRejects(
      () => verify(original, { ...original, [name]: encode("unsafe") }),
      Error,
      "Unsafe",
    );
  }
});
Deno.test("frozen archive bounds declared file size before decompression", async () => {
  const bytes = crx(original), xpi = zipSync(original);
  const view = new DataView(xpi.buffer, xpi.byteOffset, xpi.byteLength);
  for (let offset = 0; offset + 28 < xpi.length; offset++) {
    if (view.getUint32(offset, true) === 0x02014b50) {
      view.setUint32(offset + 24, 50 * 1024 * 1024 + 1, true);
      break;
    }
  }
  await assertRejects(
    async () =>
      verifyFrozenCorpusScripts(bytes, new Uint8Array(xpi).buffer, {
        packageSha256: await sha256(bytes),
      }),
    Error,
    "expansion limits",
  );
});
Deno.test("frozen archive rejects duplicate ZIP entry names", async () => {
  const bytes = crx(original),
    xpi = zipSync({
      ...original,
      "other0.js": encode("a"),
      "other1.js": encode("b"),
    });
  const search = encode("other1.js"), replace = encode("other0.js");
  for (let offset = 0; offset <= xpi.length - search.length; offset++) {
    if (search.every((byte, index) => xpi[offset + index] === byte)) {
      xpi.set(replace, offset);
    }
  }
  await assertRejects(
    async () =>
      verifyFrozenCorpusScripts(bytes, new Uint8Array(xpi).buffer, {
        packageSha256: await sha256(bytes),
      }),
    Error,
    "duplicate",
  );
});

function native(): NativeCorpusBackgroundSnapshot {
  return {
    runId: "f".repeat(32),
    strictNative: true,
    version: "158.0.1",
    buildId: "20261019010000",
    workerWebIDLCompiled: true,
    background: {
      nativeWorkerCompiled: true,
      selected: "service_worker",
      serviceWorkerFallback: false,
    },
    originalServiceWorker: "worker.js",
    convertedServiceWorker: "worker.js",
    backgroundViewType: "background_worker",
    candidateIdentity: {
      manifestSha256: "a".repeat(64),
      packageSha256: "b".repeat(64),
      sourceCommit: "c".repeat(40),
      floorpCommit: "d".repeat(40),
      patches: { "worker.patch": "e".repeat(64) },
      buildId: "20261019010000",
      platform: "linux",
    },
  };
}
Deno.test("strict native guard accepts matching compiled candidate and actual Worker", () => {
  assertEquals(validateNativeCorpusBackground(native()), undefined);
});
Deno.test("strict native guard rejects missing or malformed invocation identity", () => {
  for (
    const runId of [
      null,
      "",
      "a".repeat(31),
      "A".repeat(32),
      "z".repeat(32),
      "a".repeat(33),
    ]
  ) {
    assertThrows(
      () => validateNativeCorpusBackground({ ...native(), runId }),
      Error,
      "actual host invocation run ID",
    );
  }
});
Deno.test("strict native guard rejects absent candidate identity and mismatched running build", () => {
  assertThrows(
    () =>
      validateNativeCorpusBackground({
        ...native(),
        candidateIdentity: undefined,
      }),
    Error,
    "authenticated candidate",
  );
  assertThrows(
    () =>
      validateNativeCorpusBackground({
        ...native(),
        buildId: "20200101000000",
      }),
    Error,
    "authenticated candidate",
  );
});
Deno.test("strict native guard rejects old source version and uncompiled Worker bindings", () => {
  for (const version of ["157.0", "159.0", "1580.1"]) {
    assertThrows(
      () => validateNativeCorpusBackground({ ...native(), version }),
      Error,
      "compiled Floorp 158",
    );
  }
  assertThrows(
    () =>
      validateNativeCorpusBackground({
        ...native(),
        workerWebIDLCompiled: false,
      }),
    Error,
    "compiled Floorp 158",
  );
});
Deno.test("strict native guard rejects fallback conversion even if a document works", () => {
  for (
    const background of [undefined, {
      nativeWorkerCompiled: false,
      selected: "document",
      serviceWorkerFallback: true,
    }, {
      nativeWorkerCompiled: true,
      selected: "document",
      serviceWorkerFallback: true,
    }] as const
  ) {
    assertThrows(
      () => validateNativeCorpusBackground({ ...native(), background }),
      Error,
      "fallback",
    );
  }
});
Deno.test("strict native guard requires same original service worker and actual native context", () => {
  assertThrows(
    () =>
      validateNativeCorpusBackground({
        ...native(),
        originalServiceWorker: null,
      }),
    Error,
    "original service worker",
  );
  for (const convertedServiceWorker of [null, "replacement.js"]) {
    assertThrows(
      () =>
        validateNativeCorpusBackground({ ...native(), convertedServiceWorker }),
      Error,
      "original service worker",
    );
  }
  for (const backgroundViewType of [null, "background", "tab"]) {
    assertThrows(
      () => validateNativeCorpusBackground({ ...native(), backgroundViewType }),
      Error,
      "actual background_worker",
    );
  }
});
Deno.test("strict native guard permits legitimate document backgrounds without global Worker opt-in", () => {
  assertEquals(
    validateNativeCorpusBackground({
      ...native(),
      originalServiceWorker: null,
      convertedServiceWorker: null,
      backgroundViewType: "background",
      background: {
        nativeWorkerCompiled: true,
        selected: "document",
        serviceWorkerFallback: false,
      },
    }),
    undefined,
  );
});
Deno.test("historical mode remains observable without qualifying as native", () => {
  assertEquals(
    validateNativeCorpusBackground({
      ...native(),
      strictNative: false,
      runId: null,
      version: "157.0",
      workerWebIDLCompiled: false,
      candidateIdentity: undefined,
      background: undefined,
      backgroundViewType: "background",
    }),
    undefined,
  );
});
