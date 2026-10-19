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
    hasSidePanel: boolean;
  };
  conversion: {
    originalVersion: string;
    warnings: string[];
    signature?: { format: number; algorithm: string; publicKeySha256: string };
    background?: {
      nativeWorkerCompiled: boolean;
      selected: string;
      serviceWorkerFallback: boolean;
    };
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
    background = undefined as unknown,
    sidePanel = "none",
  } = {},
) {
  const chromeId = "c".repeat(32);
  const id = chromeId + "@chromium-extension";
  const manifest = {
    ...(sidePanel === "permission" ? { permissions: ["sidePanel"] } : {}),
    ...(sidePanel === "declaration"
      ? { side_panel: { default_path: "panel.html" } }
      : {}),
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
    ...(background === undefined ? {} : { background }),
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
  const navigation: unknown[] = [];
  let navigationOwner: unknown = {
    closed: false,
    openTrustedLinkIn: (url: string, where: string, options: unknown) => {
      navigation.push({ url, where, options });
    },
  };
  const scope = {
    getBrowserElement: () => ({
      ownerDocument: { defaultView: navigationOwner },
    }),
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
    source!.replace(/^import .*?;\n/gms, "").replaceAll(
      "export async function",
      "async function",
    ).replaceAll("export class", "class") +
      "\nglobalThis.collect=collectChromeExtensionDiagnostics;globalThis.fallback=FALLBACK_STRINGS;globalThis.navigate=FloorpChromeExtensionDiagnostics.prototype.openFloorpSettings;",
    scope,
  );
  const loaded = scope as unknown as {
    collect: (addon: unknown) => Promise<Diagnostics | null>;
    fallback: Record<string, string>;
    navigate: (action: string) => void;
  };
  const addon = {
    id,
    type: "extension",
    version: "2.4.1",
    isActive: false,
    getResourceURI: (path: string) => path,
  };
  return {
    ...loaded,
    addon,
    navigation,
    removeNavigationOwner: () => navigationOwner = null,
  };
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

Deno.test("diagnostics normalize optional background metadata without trusting running state", async () => {
  const background = {
    nativeWorkerCompiled: true,
    selected: "service_worker",
    serviceWorkerFallback: false,
    unknown: "drop",
  };
  const { collect, addon } = harness({ background });
  const data = await collect(addon);
  assertEquals(
    JSON.stringify(data?.conversion?.background),
    JSON.stringify({
      nativeWorkerCompiled: true,
      selected: "service_worker",
      serviceWorkerFallback: false,
    }),
  );
  assertEquals(data?.contexts.length, 0);
  for (
    const invalid of [{ ...background, nativeWorkerCompiled: 1 }, {
      ...background,
      selected: "worker",
    }, { ...background, serviceWorkerFallback: "false" }]
  ) {
    const { collect, addon } = harness({ background: invalid });
    const data = await collect(addon);
    assert(data?.conversion);
    assertEquals(data.conversion.background, undefined);
    assertEquals(data.conversionError, null);
  }
});

Deno.test("diagnostics offer sidebar pin settings only for converted side-panel declarations", async () => {
  for (const sidePanel of ["permission", "declaration"]) {
    const { collect, addon } = harness({ sidePanel });
    const data = await collect(addon);
    assert(
      data?.extension.hasSidePanel,
      "A declared Chrome panel has a settings path even while disabled",
    );
    const legacy = harness({ legacy: true, sidePanel });
    assertEquals(
      (await legacy.collect(legacy.addon))?.extension.hasSidePanel,
      false,
    );
  }
  const ordinary = harness();
  assertEquals(
    (await ordinary.collect(ordinary.addon))?.extension.hasSidePanel,
    false,
  );
});

Deno.test("diagnostics settings navigation stays in its owning browser and rejects unknown routes", () => {
  const { navigate, navigation, removeNavigationOwner } = harness();
  navigate("sidebar-settings");
  navigate("floorp-shortcuts");
  assertEquals(
    JSON.stringify(navigation),
    JSON.stringify([
      {
        url: "about:hub#/features/sidebar",
        where: "tab",
        options: { relatedToCurrent: true, allowInheritPrincipal: false },
      },
      {
        url: "about:hub#/features/shortcuts",
        where: "tab",
        options: { relatedToCurrent: true, allowInheritPrincipal: false },
      },
    ]),
  );
  for (
    const invalid of [
      "https://example.org/",
      "constructor",
      "toString",
      "extension-shortcuts",
    ]
  ) {
    let denied = false;
    try {
      navigate(invalid);
    } catch (error) {
      denied = String(error).includes(
        "owning browser cannot open Floorp settings",
      );
    }
    assert(denied, `Unknown settings action rejects: ${invalid}`);
  }
  removeNavigationOwner();
  let denied = false;
  try {
    navigate("sidebar-settings");
  } catch (error) {
    denied = String(error).includes(
      "owning browser cannot open Floorp settings",
    );
  }
  assert(
    denied,
    "A closed or missing owner never falls back to another browser window",
  );
  assertEquals(navigation.length, 2, "Denied navigation opens no tab");
});
