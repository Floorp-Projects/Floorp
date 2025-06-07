/**
 * Post-build phase: Injection and finalization after Mozilla build
 */

import { log } from "../logger.ts";
import { applyMixin } from "../tasks/inject/mixin-loader.ts";
import { injectManifest } from "../tasks/inject/manifest.ts";
import { injectXHTML, injectXHTMLDev } from "../tasks/inject/xhtml.ts";
import { symlinkDirectory } from "../tasks/inject/symlink-directory.ts";
import { writeBuildid2 } from "../tasks/update/buildid2.ts";
import { genVersion } from "../../dev/launchDev/writeVersion.ts";
import { savePrefsForProfile } from "../../dev/launchDev/savePrefs.ts";
import {
  loaderFeaturesPath,
  loaderModulesPath,
  actualModulesPath,
  featuresChromePath,
  i18nFeaturesChromePath,
  profileTestPath,
  devBrandingSuffix,
} from "../defines.ts";

export async function runPostBuildPhase(isDev: boolean = false): Promise<void> {
  log.info("🔨 Starting post-build phase...");
  
  try {
    // Apply mixins
    log.info("🧩 Applying mixins...");
    await applyMixin();
    
    // Inject manifest
    log.info("📋 Injecting manifest...");
    await injectManifest();
    
    // Inject XHTML
    log.info("🌐 Injecting XHTML...");
    if (isDev) {
      await injectXHTMLDev();
    } else {
      await injectXHTML();
    }
    
    // Create symlinks
    log.info("🔗 Creating symlinks...");
    await symlinkDirectory(loaderFeaturesPath, featuresChromePath);
    await symlinkDirectory("../../i18n/features-chrome", i18nFeaturesChromePath);
    await symlinkDirectory(loaderModulesPath, "../../modules");
    await symlinkDirectory(actualModulesPath, "../../modules");
    
    // Write build ID
    log.info("🆔 Writing build ID...");
    await writeBuildid2();
    
    if (isDev) {
      // Development-specific tasks
      log.info("🚀 Setting up development environment...");
      await genVersion(devBrandingSuffix);
      await savePrefsForProfile(profileTestPath);
    }
    
    log.info("✅ Post-build phase completed successfully");
  } catch (error) {
    log.error(`❌ Post-build phase failed: ${error}`);
    throw error;
  }
}
