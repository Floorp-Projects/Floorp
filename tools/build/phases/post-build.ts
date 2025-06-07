/**
 * Post-build phase: Injection and finalization after Mozilla build
 */

import { log } from "../logger.ts";
import { applyMixin } from "../tasks/inject/mixin-loader.ts";
import { injectManifest } from "../tasks/inject/manifest.ts";
import { injectXHTML, injectXHTMLDev } from "../tasks/inject/xhtml.ts";
import { writeBuildid2 } from "../tasks/update/buildid2.ts";
import { genVersion } from "../../dev/launchDev/writeVersion.ts";
import { savePrefsForProfile } from "../../dev/launchDev/savePrefs.ts";
import { resolve } from "pathe";

export async function runPostBuildPhase(isDev: boolean = false): Promise<void> {
  log.info("🔨 Starting post-build phase...");

  try {
    // Get absolute bin path from project root
    const projectRoot = resolve(import.meta.dirname!, "../../../");
    const absoluteBinPath = resolve(projectRoot, "_dist/bin/noraneko");

    // Apply mixins
    log.info("🧩 Applying mixins...");
    await applyMixin(absoluteBinPath);

    // Inject manifest
    log.info("📋 Injecting manifest...");
    await injectManifest(absoluteBinPath, isDev ? "dev" : "prod");

    // Inject XHTML
    log.info("🌐 Injecting XHTML...");
    if (isDev) {
      await injectXHTMLDev(absoluteBinPath);
    } else {
      await injectXHTML(absoluteBinPath);
    }

    // Create symlinks
    // Note: Symlinks are now handled in manifest.ts
    log.info("🔗 Creating symlinks...");
    log.info("✅ Symlinks handled by manifest injection");

    // await symlinkDirectory(
    //   resolve(projectRoot, absoluteBinPath, "browser"),
    //   resolve(projectRoot, "src/core/glue/loader-features"),
    //   "features"
    // );
    // await symlinkDirectory(
    //   resolve(projectRoot, absoluteBinPath),
    //   resolve(projectRoot, "i18n/features-chrome"),
    //   "i18n"
    // );
    // await symlinkDirectory(
    //   resolve(projectRoot),
    //   resolve(projectRoot, "src/core/glue/loader-modules"),
    //   "modules"
    // );
    // await symlinkDirectory(
    //   resolve(projectRoot),
    //   resolve(projectRoot, "src/core/modules"),
    //   "modules"
    // );

    // Write build ID
    log.info("🆔 Writing build ID...");
    await writeBuildid2(absoluteBinPath, "nora-build-id");

    if (isDev) {
      // Development-specific tasks
      log.info("🚀 Setting up development environment...");
      await genVersion();
      await savePrefsForProfile(resolve(projectRoot, "_dist/profile/test"));
    }

    log.info("✅ Post-build phase completed successfully");
  } catch (error) {
    log.error(`❌ Post-build phase failed: ${error}`);
    throw error;
  }
}
