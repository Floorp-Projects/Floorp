import { log } from "../logger.ts";
import { applyMixin } from "../tasks/inject/mixin-loader.ts";
import { injectManifest } from "../tasks/inject/manifest.ts";
import { injectXHTML, injectXHTMLDev } from "../tasks/inject/xhtml.ts";
import { writeBuildid2 } from "../tasks/update/buildid2.ts";
import { genVersion } from "../../dev/launchDev/writeVersion.ts";
import { savePrefsForProfile } from "../../dev/launchDev/savePrefs.ts";
import { resolve } from "@std/path";
import { resolveFromRoot } from "../utils.ts";

export async function runPostBuildPhase(isDev = false): Promise<void> {
  log.info("🔨 Starting post-build phase...");

  const binPath = resolveFromRoot("_dist/bin/noraneko");

  log.info("🧩 Applying mixins...");
  await applyMixin(binPath);

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
