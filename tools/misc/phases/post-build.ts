import { log } from "../../utils/logger.ts";
import { injectManifest } from "../inject-script-loaders/manifest.ts";
import { injectXHTML, injectXHTMLDev } from "../inject-script-loaders/xhtml.ts";
import { writeBuildid2 } from "../update/buildid2.ts";
import { genVersion } from "../launchDev/writeVersion.ts";
import { savePrefsForProfile } from "../launchDev/savePrefs.ts";
import { resolve } from "@std/path";
import { resolveFromRoot } from "../../utils/utils.ts";

export async function runPostBuildPhase(isDev = false): Promise<void> {
  log.info("🔨 Starting post-build phase...");

  const binPath = resolveFromRoot("_dist/bin/noraneko");

  log.info("📋 Injecting manifest...");
  await injectManifest(binPath, isDev ? "dev" : "prod");

  log.info("🌐 Injecting XHTML...");
  await (isDev ? injectXHTMLDev(binPath) : injectXHTML(binPath));

  log.info("🆔 Writing build ID...");
  await writeBuildid2(binPath, "nora-build-id");

  if (isDev) {
    log.info("🚀 Setting up development environment...");
    await genVersion();
    await savePrefsForProfile(resolve(resolveFromRoot("_dist/profile/test")));
  }

  log.success("Post-build phase completed");
}
