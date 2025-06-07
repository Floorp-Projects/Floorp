/**
 * Symlink management utilities for Noraneko build system
 */

import { createSymlink, resolveFromRoot } from "../../utils.ts";

/**
 * Setup all required symlinks for the build process
 */
export async function setupBuildSymlinks(): Promise<void> {
  // Define symlinks to create
  const symlinks = [
    {
      link: resolveFromRoot(
        "src/core/glue/loader-features/link-features-chrome",
      ),
      target: resolveFromRoot("src/features/chrome"),
    },
    {
      link: resolveFromRoot(
        "src/core/glue/loader-features/link-i18n-features-chrome",
      ),
      target: resolveFromRoot("i18n/features-chrome"),
    },
  ];

  // Create all symlinks
  for (const { link, target } of symlinks) {
    await createSymlink(link, target);
  }
}
