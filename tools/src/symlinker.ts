// SPDX-License-Identifier: MPL-2.0

import * as path from "@std/path";
import { PROJECT_ROOT } from "./defines.ts";
import { SOURCE_LINKS } from "./source_link_plan.ts";
import { createSymlink, exists, Logger, safeRemove } from "./utils.ts";

const logger = new Logger("symlinker");

export function run(): void {
  const pairs: Array<[string, string]> = SOURCE_LINKS.map(
    ({ link, target }) => [
      path.join(PROJECT_ROOT, ...link),
      path.join(PROJECT_ROOT, ...target),
    ],
  );

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
      // deno-lint-ignore no-explicit-any
    } catch (e: any) {
      console.warn(
        `Failed to create symlink ${link} -> ${target}: ${e?.message ?? e}`,
      );
    }
  }

  logger.success("Symlinks created successfully.");
}
