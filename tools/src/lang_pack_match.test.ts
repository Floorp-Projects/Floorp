// SPDX-License-Identifier: MPL-2.0

import { assertEquals } from "@std/assert";
import { findRequestedLangPack } from "../../browser-features/modules/modules/i18n/lang-pack-match.ts";

const packs = [
  { target_locale: "ja" },
  { target_locale: "pt-BR" },
  { target_locale: "pt-PT" },
];

Deno.test("matches a Japanese language pack after upgrading an existing profile", () => {
  assertEquals(findRequestedLangPack(packs, "ja"), packs[0]);
  assertEquals(findRequestedLangPack(packs, "ja-JP"), packs[0]);
  assertEquals(findRequestedLangPack(packs, "JA_jp,en-US"), packs[0]);
});

Deno.test("does not substitute an unrelated regional language pack", () => {
  assertEquals(findRequestedLangPack(packs, "pt-BR"), packs[1]);
  assertEquals(findRequestedLangPack(packs, "pt-PT"), packs[2]);
  assertEquals(findRequestedLangPack(packs, "pt-AO"), null);
  assertEquals(findRequestedLangPack(null, "ja"), null);
});
