// SPDX-License-Identifier: MPL-2.0
import type {
  ViteBuildArgs,
  ViteBuildConfig,
  ViteBuildDependencies,
} from "./vite_build_types.ts";
/** Internal build-only CLI: reject watch/unknown/ambiguous args before loading Vite. */
export function parseViteBuildArgs(args: readonly string[]): ViteBuildArgs {
  if (args.length === 1 && args[0] === "--help") return { kind: "help" };
  const config: ViteBuildConfig = { build: { watch: null } };
  const seen = new Set<string>();
  for (let i = 0; i < args.length; i += 2) {
    const flag = args[i], value = args[i + 1];
    if (
      (flag !== "--base" && flag !== "--config") || seen.has(flag) ||
      typeof value !== "string" || value.length === 0 || value.startsWith("--")
    ) {
      throw new Error(
        "Expected unique --base/--config values; watch and other Vite commands are not supported",
      );
    }
    seen.add(flag);
    if (flag === "--base") config.base = value;
    else config.configFile = value;
  }
  return { kind: "build", config };
}
/** Completion means the actual compiler promise fulfilled, never merely a log match. */
export async function runViteBuildCommand(
  args: readonly string[],
  dependencies: ViteBuildDependencies,
): Promise<number> {
  try {
    const parsed = parseViteBuildArgs(args);
    if (parsed.kind === "help") {
      dependencies.info(
        "Build Vite once: [--base <base>] [--config <file>]. Watch is not supported.",
      );
      return 0;
    }
    const build = await dependencies.loadBuild();
    await build(parsed.config);
    return 0;
  } catch (error) {
    dependencies.error(error);
    return 1;
  }
}
