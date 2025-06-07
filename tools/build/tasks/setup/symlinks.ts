/**
 * Symlink management utilities for Noraneko build system
 */

import { resolve } from "pathe";

/**
 * Create a symbolic link with error handling
 */
export async function createSymlink(
  linkPath: string,
  targetPath: string,
): Promise<void> {
  try {
    // Remove existing link if it exists
    try {
      await Deno.remove(linkPath);
      console.log(`Removed existing symlink: ${linkPath}`);
    } catch (error) {
      if (!(error instanceof Deno.errors.NotFound)) {
        console.warn(`Warning: Could not remove existing symlink: ${error}`);
      }
    }

    // Create the symbolic link
    await Deno.symlink(targetPath, linkPath);
    console.log(`Created symlink: ${linkPath} -> ${targetPath}`);
  } catch (error: unknown) {
    const errorMessage = error instanceof Error ? error.message : String(error);
    throw new Error(
      `Failed to create symlink from ${linkPath} to ${targetPath}: ${errorMessage}`,
    );
  }
}

/**
 * Setup all required symlinks for the build process
 */
export async function setupBuildSymlinks(): Promise<void> {
  const projectRoot = resolve(import.meta.dirname!, "../../../../");

  // Define symlinks to create
  const symlinks = [
    {
      link: resolve(
        projectRoot,
        "src/core/glue/loader-features/link-features-chrome",
      ),
      target: resolve(projectRoot, "src/features/chrome"),
    },
    {
      link: resolve(
        projectRoot,
        "src/core/glue/loader-features/link-i18n-features-chrome",
      ),
      target: resolve(projectRoot, "i18n/features-chrome"),
    },
  ];

  // Create all symlinks
  for (const { link, target } of symlinks) {
    await createSymlink(link, target);
  }
}
