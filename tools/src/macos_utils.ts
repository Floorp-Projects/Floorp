// SPDX-License-Identifier: MPL-2.0

import * as path from "@std/path";
import { Logger, exists } from "./utils.ts";

const logger = new Logger("macos_utils");

/**
 * Recursively find .app bundles under rootDir and patch their Info.plist
 * to include MozillaDeveloperRepoPath / MozillaDeveloperObjPath when missing.
 * If repoPath/objPath are not provided, fall back to environment variables
 * MOZ_DEVELOPER_REPO_DIR and MOZ_DEVELOPER_OBJ_DIR.
 */
export async function patchAppInfoPlists(
  rootDir: string,
  repoPath?: string,
  objPath?: string,
): Promise<void> {
  const rp = repoPath ?? Deno.env.get("MOZ_DEVELOPER_REPO_DIR");
  const op = objPath ?? Deno.env.get("MOZ_DEVELOPER_OBJ_DIR");

  if (!rp && !op) {
    logger.info(
      "No developer repo/obj environment variables set; skipping plist patch.",
    );
    return;
  }

  const entries: string[] = [];

  function walk(dir: string) {
    for (const e of Deno.readDirSync(dir)) {
      const p = path.join(dir, e.name);
      if (e.isDirectory) {
        if (e.name.endsWith(".app")) {
          entries.push(p);
        } else {
          try {
            walk(p);
          } catch {
            // ignore
          }
        }
      }
    }
  }

  try {
    walk(rootDir);
  } catch (e) {
    logger.error(`Failed to walk ${rootDir}: ${e.message ?? e}`);
    return;
  }

  for (const appPath of entries) {
    const infoPlist = path.join(appPath, "Contents", "Info.plist");
    try {
      if (!(await exists(infoPlist))) {
        logger.info(`No Info.plist at ${infoPlist}; skipping`);
        continue;
      }
      const content = Deno.readTextFileSync(infoPlist);

      const hasRepoKey = content.includes("MozillaDeveloperRepoPath");
      const hasObjKey = content.includes("MozillaDeveloperObjPath");
      if (hasRepoKey && hasObjKey) {
        logger.info(
          `Info.plist at ${infoPlist} already contains developer keys; skipping`,
        );
        continue;
      }

      // Insert keys before closing </dict>
      const insertIndex = content.lastIndexOf("</dict>");
      if (insertIndex === -1) {
        logger.warn(`Malformed Info.plist at ${infoPlist}; no </dict> found`);
        continue;
      }

      let inject = "\n\t<!-- Injected developer paths -->\n";
      if (!hasRepoKey && rp) {
        inject += `\t<key>MozillaDeveloperRepoPath</key>\n\t<string>${rp}</string>\n`;
      }
      if (!hasObjKey && op) {
        inject += `\t<key>MozillaDeveloperObjPath</key>\n\t<string>${op}</string>\n`;
      }

      const newContent =
        content.slice(0, insertIndex) + inject + content.slice(insertIndex);
      // Backup original
      try {
        Deno.writeTextFileSync(infoPlist + ".bak", content);
      } catch {
        // ignore backup errors
      }
      Deno.writeTextFileSync(infoPlist, newContent);
      logger.info(`Patched ${infoPlist} with developer paths`);
    } catch (e) {
      logger.error(`Error patching ${infoPlist}: ${e.message ?? e}`);
    }
  }
}

export default { patchAppInfoPlists };
