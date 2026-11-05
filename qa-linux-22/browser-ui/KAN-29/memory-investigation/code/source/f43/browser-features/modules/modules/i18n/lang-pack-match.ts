// SPDX-License-Identifier: MPL-2.0

export function findRequestedLangPack<T extends { target_locale: string }>(
  available: T[] | null,
  requested: string,
): T | null {
  const locale = requested.split(",", 1)[0]?.trim().replaceAll("_", "-");
  if (!locale) return null;

  // Language pack identifiers can be less specific than requested app locales:
  // Firefox publishes Japanese as "ja", while an existing preference may be "ja-JP".
  const exact = available?.find((pack) =>
    pack.target_locale.toLowerCase() === locale.toLowerCase()
  );
  if (exact) return exact;

  const subtags = locale.split("-");
  const language = subtags[0];
  // Script-qualified requests such as zh-Hant-TW use the regional pack zh-TW.
  const region = subtags.slice(1).find((subtag) =>
    /^[A-Za-z]{2}$/.test(subtag) || /^\d{3}$/.test(subtag)
  );
  if (region) {
    const regional = available?.find((pack) =>
      pack.target_locale.toLowerCase() ===
        `${language}-${region}`.toLowerCase()
    );
    if (regional) return regional;
  }
  if (language === locale) return null;
  return available?.find((pack) =>
    pack.target_locale.toLowerCase() === language.toLowerCase()
  ) ?? null;
}
