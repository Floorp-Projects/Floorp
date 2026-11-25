// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals } from "@std/assert";
import i18next from "i18next";

async function readLocale(locale: string) {
  const resource = JSON.parse(
    await Deno.readTextFile(
      new URL(`../../i18n/${locale}/browser-chrome.json`, import.meta.url),
    ),
  );
  const containers: Record<string, unknown> = resource.workspaces?.containers;
  assert(containers && typeof containers === "object");
  return { resource, containers };
}

function placeholders(value: string): string[] {
  return [...value.matchAll(/\{\{\s*([^{}]+?)\s*\}\}/g)]
    .map((match) => match[1]).sort();
}

// Translation publication waits for test:host. Keep missing source translations
// from silently deleting this Japanese UI again (Floorp PR #3000).
Deno.test("Japanese workspace container keys and interpolation stay in parity", async () => {
  const [english, japanese] = await Promise.all([
    readLocale("en-US"),
    readLocale("ja-JP"),
  ]);
  assert(Object.keys(english.containers).length > 0);
  assertEquals(
    Object.keys(japanese.containers).sort(),
    Object.keys(english.containers).sort(),
  );

  const instance = i18next.createInstance();
  await instance.init({
    showSupportNotice: false,
    lng: "ja-JP",
    fallbackLng: "en-US",
    resources: {
      "en-US": { translation: english.resource },
      "ja-JP": { translation: japanese.resource },
    },
  });
  for (const [key, source] of Object.entries(english.containers)) {
    const translation = japanese.containers[key];
    assert(typeof source === "string" && source.trim().length > 0, key);
    assert(
      typeof translation === "string" && translation.trim().length > 0,
      key,
    );
    assertEquals(placeholders(translation), placeholders(source), key);
    const resolved = instance.t(`workspaces.containers.${key}`, {
      name: "検証用コンテナ",
      error: "検証用エラー",
      returnDetails: true,
    });
    assertEquals(resolved.usedLng, "ja-JP", key);
    assertEquals(
      resolved.res,
      translation.replaceAll("{{name}}", "検証用コンテナ")
        .replaceAll("{{error}}", "検証用エラー"),
      key,
    );
  }
});
