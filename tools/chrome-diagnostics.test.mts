import { assert, assertEquals } from "@std/assert";
import { runInNewContext } from "node:vm";

const patch = await Deno.readTextFile(
  new URL("./patches/chrome-extension-diagnostics.patch", import.meta.url),
);
const source = patch.split(
  "+++ b/chrome/toolkit/content/mozapps/extensions/components/floorp-chrome-extension-diagnostics.mjs\n",
)[1]?.split("\ndiff --git ")[0].split("\n").filter((line) =>
  line.startsWith("+") && !line.startsWith("+++")
).map((line) => line.slice(1)).join("\n");
if (!source) {
  throw new Error("Diagnostics component is missing from its artifact patch");
}

interface Diagnostics {
  extension: {
    chromeId: string;
    provenance: string;
    runtimeSchemaVersion: number | null;
  };
  conversion: {
    originalVersion: string;
    warnings: string[];
    signature?: { format: number; algorithm: string; publicKeySha256: string };
  } | null;
  conversionError: string | null;
  contexts: unknown[];
}

function harness(
  {
    legacy = false,
    mismatchedMarker = false,
    mismatchedReport = false,
    signature = undefined as unknown,
  } = {},
) {
  const chromeId = "c".repeat(32);
  const id = chromeId + "@chromium-extension";
  const manifest = {
    browser_specific_settings: {
      gecko: {
        id,
        ...(!legacy
          ? {
            floorp_chrome_extension: {
              schema_version: 1,
              original_id: mismatchedMarker ? "d".repeat(32) : chromeId,
            },
          }
          : {}),
      },
    },
  };
  const report = {
    schemaVersion: 1,
    chromeId: mismatchedReport ? "e".repeat(32) : chromeId,
    addonId: id,
    originalVersion: "2.4.1",
    warnings: ["Warning"],
    removedPermissions: ["tabCapture"],
    ...(signature === undefined ? {} : { signature }),
  };
  const resources: Record<string, string> = {
    "manifest.json": JSON.stringify(manifest),
  };
  if (!legacy) {
    resources["__floorp_chrome__/conversion-report.json"] = JSON.stringify(
      report,
    );
  }
  const NetUtil = {
    asyncFetch: (
      { uri }: { uri: string },
      callback: (
        input: { available: () => number; data: string },
        status: number,
      ) => void,
    ) => {
      const data = resources[uri];
      callback(
        { available: () => data?.length ?? 0, data },
        data == null ? 1 : 0,
      );
    },
    readInputStreamToString: (input: { data: string }) => input.data,
  };
  const scope = {
    AboutAddonsHTMLElement: class {},
    ChromeUtils: {
      importESModule: (uri: string) =>
        uri.includes("NetUtil") ? { NetUtil } : { AddonManager: {} },
    },
    Components: { isSuccessCode: (status: number) => status === 0 },
    Services: { appinfo: { version: "157.0", appBuildID: "fixture" } },
    WebExtensionPolicy: { getByID: () => null },
    customElements: { define: () => {} },
  };
  runInNewContext(
    source!.replace(/^import .*?;\n/ms, "").replaceAll(
      "export async function",
      "async function",
    ).replaceAll("export class", "class") +
      "\nglobalThis.collect=collectChromeExtensionDiagnostics;globalThis.fallback=FALLBACK_STRINGS;",
    scope,
  );
  const loaded = scope as unknown as {
    collect: (addon: unknown) => Promise<Diagnostics | null>;
    fallback: Record<string, string>;
  };
  const addon = {
    id,
    type: "extension",
    version: "2.4.1",
    isActive: false,
    getResourceURI: (path: string) => path,
  };
  return { ...loaded, addon };
}

Deno.test("diagnostics preserve conversion data for a disabled extension", async () => {
  const { collect, addon } = harness();
  const data = await collect(addon);
  assert(data);
  assertEquals(data.extension.provenance, "converted");
  assertEquals(data.conversion?.originalVersion, "2.4.1");
  assertEquals(
    JSON.stringify(data.conversion?.warnings),
    JSON.stringify(["Warning"]),
  );
  assertEquals(data.contexts.length, 0);
});

Deno.test("diagnostics do not downgrade mismatched metadata into legacy provenance", async () => {
  const { collect, addon } = harness({ mismatchedMarker: true });
  assertEquals(await collect(addon), null);
});

Deno.test("diagnostics identify legacy conversion metadata as unverified", async () => {
  const { collect, addon } = harness({ legacy: true });
  const data = await collect(addon);
  assert(data);
  assertEquals(data.extension.provenance, "legacy-unverified");
  assertEquals(data.extension.runtimeSchemaVersion, null);
  assertEquals(data.conversion, null);
  assert(data.conversionError);
});

Deno.test("diagnostics reject conversion reports belonging to another Chrome extension", async () => {
  const { collect, addon } = harness({ mismatchedReport: true });
  const data = await collect(addon);
  assert(data);
  assertEquals(data.conversion, null);
  assert(
    data.conversionError?.includes(
      "Invalid Chrome extension conversion report",
    ),
  );
});

Deno.test("diagnostics fallback labels and English/Japanese catalogs stay complete", async () => {
  const { fallback } = harness();
  const en = JSON.parse(
    await Deno.readTextFile(
      new URL("../i18n/en-US/browser-chrome.json", import.meta.url),
    ),
  ).addons.chromeDiagnostics as Record<string, string>;
  const ja = JSON.parse(
    await Deno.readTextFile(
      new URL("../i18n/ja-JP/browser-chrome.json", import.meta.url),
    ),
  ).addons.chromeDiagnostics as Record<string, string>;
  assertEquals(Object.keys(fallback).sort(), Object.keys(en).sort());
  assertEquals(Object.keys(en).sort(), Object.keys(ja).sort());
  for (const key of Object.keys(en)) {
    assertEquals(fallback[key], en[key]);
    assert(ja[key].length > 0);
  }
});

Deno.test("diagnostics export only normalized recognized signature metadata", async () => {
  const signature = {
    format: 3,
    algorithm: "rsa-sha256",
    publicKeySha256: "f".repeat(64),
    unknown: "do not export",
  };
  const { collect, addon } = harness({ signature });
  const data = await collect(addon);
  assert(data?.conversion?.signature);
  assertEquals(
    JSON.stringify(data.conversion.signature),
    JSON.stringify({
      format: 3,
      algorithm: "rsa-sha256",
      publicKeySha256: "f".repeat(64),
    }),
  );
  for (
    const invalid of [
      { ...signature, format: 1 },
      { ...signature, algorithm: "unknown" },
      { ...signature, publicKeySha256: "F".repeat(64) },
      { ...signature, publicKeySha256: "f".repeat(63) },
    ]
  ) {
    const { collect, addon } = harness({ signature: invalid });
    const result = await collect(addon);
    assertEquals(result?.conversion?.signature, undefined);
  }
});
