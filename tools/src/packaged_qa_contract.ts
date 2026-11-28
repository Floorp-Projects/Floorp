// SPDX-License-Identifier: MPL-2.0

export function packagedQaEnabled(
  mode: string,
  flag: string | undefined,
  modules: string | undefined,
): boolean {
  if (!flag && !modules) return false;
  if (mode !== "production" || flag !== "1") {
    throw new Error(
      "Packaged QA requires production mode and FLOORP_PACKAGED_QA=1.",
    );
  }
  const parsed: unknown = JSON.parse(modules ?? "null");
  if (
    !Array.isArray(parsed) || parsed.length === 0 ||
    parsed.some((entry) => typeof entry !== "string" || !entry)
  ) {
    throw new Error("Packaged QA requires a nonempty test module array.");
  }
  return true;
}
