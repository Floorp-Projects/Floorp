// SPDX-License-Identifier: MPL-2.0

export type TestLayer = "all" | "chrome" | "esm" | "pages";

export function escapeRegExp(s: string): string {
  return s.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

export function normalizeSlashes(value: string): string {
  return value.replaceAll("\\", "/");
}

export function normalizeBrowserResultPath(resultFile: string): string {
  const normalized = normalizeSlashes(resultFile);

  if (normalized.startsWith("/link-features-chrome/")) {
    return normalized.replace(
      "/link-features-chrome/",
      "browser-features/chrome/",
    );
  }

  if (normalized.startsWith("link-features-chrome/")) {
    return normalized.replace(
      "link-features-chrome/",
      "browser-features/chrome/",
    );
  }

  if (normalized.startsWith("#features-chrome/")) {
    return normalized.replace("#features-chrome/", "browser-features/chrome/");
  }

  if (normalized.startsWith("#features-modules/")) {
    return normalized.replace(
      "#features-modules/",
      "browser-features/modules/",
    );
  }

  if (normalized.startsWith("#features-pages/")) {
    return normalized.replace("#features-pages/", "browser-features/");
  }

  if (normalized.startsWith("#")) {
    return `[unknown-alias] ${normalized}`;
  }

  if (normalized.startsWith("/browser-features/")) {
    return normalized.slice(1);
  }

  const marker = "browser-features/";
  const idx = normalized.toLowerCase().indexOf(marker);
  if (idx >= 0) {
    return normalized.slice(idx);
  }

  return normalized.replace(/^\.?\//, "");
}

export function isResultMatchTarget(
  resultFile: string,
  targetRel: string,
): boolean {
  const resultNorm = normalizeBrowserResultPath(resultFile).toLowerCase();
  const targetNorm = normalizeSlashes(targetRel).toLowerCase();
  return (
    resultNorm === targetNorm ||
    resultNorm.endsWith(`/${targetNorm}`) ||
    resultNorm.endsWith(targetNorm)
  );
}

export function isTestFile(relPath: string): boolean {
  const normalized = relPath.replaceAll("\\", "/");
  if (
    normalized.includes("/_dist/") ||
    normalized.includes("/node_modules/") ||
    normalized.includes("/libs/@types/gecko/")
  ) {
    return false;
  }

  return /\.(test|spec)\.(?:ts|mts|tsx|js|mjs|jsx)$/.test(normalized);
}

export function detectLayer(relPath: string): Exclude<TestLayer, "all"> | null {
  const normalized = relPath.replaceAll("\\", "/");

  if (normalized.startsWith("browser-features/chrome/")) {
    return "chrome";
  }

  if (normalized.startsWith("browser-features/pages-")) {
    return "pages";
  }

  if (
    normalized.startsWith("browser-features/modules/") ||
    normalized.startsWith("bridge/loader-features/")
  ) {
    return "esm";
  }

  return null;
}

export function parseLayer(value: string | undefined): TestLayer {
  if (!value || value === "all") {
    return "all";
  }

  const normalized = value.toLowerCase();
  if (normalized === "chrome") {
    return "chrome";
  }

  if (normalized === "esm") {
    return "esm";
  }

  if (
    normalized === "pages" ||
    normalized === "page" ||
    normalized === "built-in-pages" ||
    normalized === "builtin-pages" ||
    normalized === "builtin"
  ) {
    return "pages";
  }

  throw new Error(
    `Invalid --layer value: ${value}. Allowed: all, chrome, esm, pages`,
  );
}
