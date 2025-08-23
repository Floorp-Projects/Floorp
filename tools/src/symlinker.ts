import * as path from "@std/path";
import { PATHS } from "./defines.ts";
import { Logger, exists, safeRemove, createSymlink } from "./utils.ts";

const logger = new Logger("symlinker");

export function run(): void {
  const pairs: Array<[string, string]> = [
    [
      path.join(PATHS.loader_features, "link-features-chrome"),
      PATHS.features_chrome,
    ],
    [path.join(PATHS.loader_features, "link-i18n-chrome"), PATHS.i18n_chrome],
    [path.join(PATHS.loader_modules, "link-modules"), PATHS.modules],
  ];

  for (const [link, target] of pairs) {
    try {
      if (exists(link)) {
        try {
          safeRemove(link);
        } catch {
          // ignore
        }
      }
      createSymlink(link, target);
    } catch (e: any) {
      console.warn(
        `Failed to create symlink ${link} -> ${target}: ${e?.message ?? e}`,
      );
    }
  }

  logger.success("Symlinks created successfully.");
}
