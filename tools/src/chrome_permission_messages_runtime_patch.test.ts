// SPDX-License-Identifier: MPL-2.0
import { assert, assertEquals } from "@std/assert";
import vm from "node:vm";

const sourcePath =
  ".github/patches/floorp-runtime/common/chrome-extension-permission-messages.patch";
const artifactPath = "tools/patches/chrome-extension-permission-messages.patch";
const patch = Deno.readTextFileSync(sourcePath);
const moduleSection = patch.split(/^diff --git /m).find((section) =>
  section.startsWith(
    "a/toolkit/components/extensions/ExtensionPermissionMessages.sys.mjs ",
  )
);
assert(moduleSection);
const additions = moduleSection.split("\n").filter((line) =>
  line.startsWith("+") && !line.startsWith("+++")
).map((line) => line.slice(1)).join("\n");
const helpers = additions.slice(
  additions.indexOf("export function floorpChromePermissionInfo"),
).replaceAll("export function", "function");
const messages = new Map(
  [...patch.matchAll(
    /^\+(floorp-webext-perms-description-tab-capture(?:-ja)?) = (.+)$/gm,
  )].map((match) => [match[1], match[2]]),
);
interface PermissionInfo {
  floorpChromeOriginalId?: unknown;
  permissions?: string[];
}
interface DescriptionResult {
  messages: string[];
  optionalPermissions: Record<string, string>;
}
interface Helpers {
  floorpChromePermissionInfo(manifest: unknown, id: unknown): PermissionInfo;
  floorpChromePermissionMessages(
    addon: { id?: string; userPermissions?: PermissionInfo },
    permissions?: PermissionInfo,
    optional?: PermissionInfo,
  ): DescriptionResult;
}
function fixture(locale = "en-US"): Helpers {
  return vm.runInNewContext(
    helpers + "\n({floorpChromePermissionInfo,floorpChromePermissionMessages})",
    {
      Services: { locale: { appLocaleAsBCP47: locale } },
      PERMISSION_L10N: {
        formatValueSync(id: string) {
          const value = messages.get(id);
          assert(value, `Missing actual Fluent resource ${id}`);
          return value;
        },
      },
    },
  ) as Helpers;
}
const id = "a".repeat(32), addonId = `${id}@chromium-extension`;
const marker = { schema_version: 1, original_id: id };
const manifest = {
  browser_specific_settings: { gecko: { floorp_chrome_extension: marker } },
};
Deno.test("capture consent metadata requires exact normalized Chrome marker and installed identity", () => {
  const api = fixture();
  assertEquals(
    api.floorpChromePermissionInfo(manifest, addonId).floorpChromeOriginalId,
    id,
  );
  for (
    const [candidate, installed] of [
      [{}, addonId],
      [manifest, id],
      [manifest, `b${addonId}`],
      [{
        browser_specific_settings: {
          gecko: { floorp_chrome_extension: { ...marker, schema_version: 2 } },
        },
      }, addonId],
      [{
        browser_specific_settings: {
          gecko: {
            floorp_chrome_extension: { ...marker, original_id: "z".repeat(32) },
          },
        },
      }, addonId],
    ] as const
  ) {
    assertEquals(
      Object.keys(api.floorpChromePermissionInfo(candidate, installed)).length,
      0,
    );
  }
  assertEquals(
    api.floorpChromePermissionInfo({
      applications: manifest.browser_specific_settings,
    }, addonId).floorpChromeOriginalId,
    id,
  );
});
Deno.test("the genuine power+tabCapture optional pair has one capture description", () => {
  const api = fixture();
  const request = {
    ...api.floorpChromePermissionInfo(manifest, addonId),
    permissions: ["power", "tabCapture"],
  };
  const result = api.floorpChromePermissionMessages({ id: addonId }, request);
  assertEquals(result.messages.length, 1);
  assertEquals(result.messages[0], "Capture video and audio from browser tabs");
  assertEquals(Object.keys(result.optionalPermissions).length, 0);
});
Deno.test("power alone remains quiet for both required and optional lists", () => {
  const api = fixture(),
    permissions = { floorpChromeOriginalId: id, permissions: ["power"] };
  const result = api.floorpChromePermissionMessages(
    { id: addonId },
    permissions,
    permissions,
  );
  assertEquals(result.messages.length, 0);
  assertEquals(Object.keys(result.optionalPermissions).length, 0);
});
Deno.test("install/update addon metadata and about:addons optional descriptions preserve capture warning", () => {
  const api = fixture();
  const addon = {
    id: addonId,
    userPermissions: api.floorpChromePermissionInfo(manifest, addonId),
  };
  assertEquals(
    api.floorpChromePermissionMessages(addon, { permissions: ["tabCapture"] })
      .messages.length,
    1,
  );
  const optional = api.floorpChromePermissionMessages(addon, undefined, {
    permissions: ["tabCapture", "power"],
  });
  assertEquals(
    optional.optionalPermissions.tabCapture,
    "Capture video and audio from browser tabs",
  );
  assertEquals(Object.keys(optional.optionalPermissions).length, 1);
});
Deno.test("Japanese capture warning ships in the actual base Fluent resource with English fallback", () => {
  for (const locale of ["ja", "ja-JP", "ja-JP-mac"]) {
    const result = fixture(locale).floorpChromePermissionMessages({
      id: addonId,
    }, { floorpChromeOriginalId: id, permissions: ["tabCapture"] });
    assertEquals(
      result.messages[0],
      "ブラウザーのタブの映像と音声をキャプチャする",
    );
  }
  assertEquals(
    fixture("fr-FR").floorpChromePermissionMessages({ id: addonId }, {
      floorpChromeOriginalId: id,
      permissions: ["tabCapture"],
    }).messages[0],
    messages.get("floorp-webext-perms-description-tab-capture"),
  );
});
Deno.test("ordinary Gecko, absent marker and mismatched metadata never gain a Chrome capture description", () => {
  const api = fixture();
  for (
    const [addon, permissions] of [
      [{ id: "ordinary@tests" }, {
        floorpChromeOriginalId: id,
        permissions: ["tabCapture"],
      }],
      [{ id: addonId }, { permissions: ["tabCapture"] }],
      [{ id: addonId }, {
        floorpChromeOriginalId: "b".repeat(32),
        permissions: ["tabCapture"],
      }],
      [{ id: addonId }, {
        floorpChromeOriginalId: 1,
        permissions: ["tabCapture"],
      }],
    ] as const
  ) {
    const result = api.floorpChromePermissionMessages(addon, {
      ...permissions,
      permissions: [...permissions.permissions],
    });
    assertEquals(result.messages.length, 0);
    assertEquals(Object.keys(result.optionalPermissions).length, 0);
  }
});
Deno.test("native formatter and real optional observer carry consent context through existing UI", () => {
  assert(
    patch.includes(
      "...lazy.floorpChromePermissionInfo(this.manifest, this.id)",
    ),
  );
  assert(
    patch.includes(
      "...floorpChromePermissionInfo(context.extension.manifest, context.extension.id)",
    ),
  );
  assert(patch.includes("+        addon: { name, id },"));
  assert(patch.includes("result.msgs.push(...chromeMessages.messages)"));
  assert(
    patch.includes(
      "Object.assign(result.optionalPermissions, chromeMessages.optionalPermissions)",
    ),
  );
  const artifact = Deno.readTextFileSync(artifactPath);
  assert(artifact.includes("b/browser/modules/ExtensionsUI.sys.mjs"));
  assert(
    artifact.includes(
      "b/localization/en-US/toolkit/global/extensionPermissions.ftl",
    ),
  );
  assert(!patch.includes('+  "tabCapture",')); // The global Gecko permission-message set remains unchanged.
});
