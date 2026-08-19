// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals } from "@std/assert";
import { resolveFromRoot } from "./utils.ts";

const TRANSLATION_TARGETS_PATH = resolveFromRoot(
  "i18n/translation-targets.json",
);
const NORANEKO_PAGE_PATH = resolveFromRoot(
  "browser-features/pages-settings/src/app/about/noraneko.tsx",
);
const SETTINGS_LOCALES_PATH =
  "browser-features/pages-settings/src/lib/i18n/locales";
const NORANEKO_KEYS = [
  "communityCredit",
  "description",
  "logoAlt",
  "repositoryLabel",
] as const;

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

async function readJson(path: string): Promise<unknown> {
  return JSON.parse(await Deno.readTextFile(path)) as unknown;
}

async function readNoranekoLocale(
  locale: "en-US" | "ja-JP",
): Promise<Record<string, unknown>> {
  const localePath = resolveFromRoot(
    `${SETTINGS_LOCALES_PATH}/${locale}.json`,
  );
  const root = await readJson(localePath);
  assert(isRecord(root), `${locale} locale must be a JSON object`);
  assert(isRecord(root.about), `${locale} locale must define about`);
  assert(
    isRecord(root.about.noraneko),
    `${locale} locale must define about.noraneko`,
  );
  return root.about.noraneko;
}

Deno.test("About Dialog is an exact f18n file target", async () => {
  const manifest = await readJson(TRANSLATION_TARGETS_PATH);
  assert(isRecord(manifest), "translation target manifest must be an object");
  assert(Array.isArray(manifest.targets), "manifest must define targets");

  const aboutDialogTarget = manifest.targets.find((target) =>
    isRecord(target) && target.name === "about-dialog"
  );
  assert(aboutDialogTarget, "about-dialog target must exist");
  assertEquals(aboutDialogTarget, {
    name: "about-dialog",
    type: "file",
    source_path: "browser-features/pages-aboutDialog/src/lib/i18n/locales",
    source_file: "en-US.json",
    f18n_path: "about-dialog/",
  });

  const sourceLocale = await readJson(
    resolveFromRoot(
      `${aboutDialogTarget.source_path}/${aboutDialogTarget.source_file}`,
    ),
  );
  assert(isRecord(sourceLocale), "About Dialog source locale must parse");
});

Deno.test("Noraneko locale keys stay in parity", async () => {
  const [enUS, jaJP] = await Promise.all([
    readNoranekoLocale("en-US"),
    readNoranekoLocale("ja-JP"),
  ]);

  assertEquals(Object.keys(enUS).sort(), Object.keys(jaJP).sort());
  for (const key of NORANEKO_KEYS) {
    assert(
      typeof enUS[key] === "string" && enUS[key].trim().length > 0,
      `en-US about.noraneko.${key} must be non-empty`,
    );
    assert(
      typeof jaJP[key] === "string" && jaJP[key].trim().length > 0,
      `ja-JP about.noraneko.${key} must be non-empty`,
    );
  }
});

Deno.test("Noraneko page uses locale keys instead of raw visible text", async () => {
  const source = await Deno.readTextFile(NORANEKO_PAGE_PATH);
  const legacyRawLiterals = [
    'alt="Browser Logo"',
    "Noraneko is a browser as testhead",
    "Made by Noraneko Community with ❤",
    "GitHub Repository: Floorp-Projects/Floorp",
  ];

  for (const literal of legacyRawLiterals) {
    assert(!source.includes(literal), `raw literal remains: ${literal}`);
  }
  for (const key of NORANEKO_KEYS) {
    assert(
      source.includes(`t("about.noraneko.${key}")`),
      `Noraneko page must use about.noraneko.${key}`,
    );
  }
});
