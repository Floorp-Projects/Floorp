import {
  applyPatches,
  initializeBinGit,
} from "./git-patches/git-patches-manager.ts";
import { applyMixin } from "./inject/mixin-loader.ts";
import { injectManifest } from "./inject/manifest.ts";
import { injectXHTML, injectXHTMLDev } from "./inject/xhtml.ts";
import { savePrefsForProfile } from "./launchDev/savePrefs.ts";
import { genVersion } from "./launchDev/writeVersion.ts";
import { writeBuildid2 } from "./update/buildid2.ts";
import {
  binDir,
  brandingBaseName,
  buildid2Path,
  devBrandingSuffix,
  devServerReadyString,
  featuresChromePath,
  getBinArchive,
  i18nFeaturesChromePath,
  loaderFeaturesPath,
  loaderModulesPath,
  modulesPath,
  mozbuildOutputDir,
  profileTestPath,
} from "./defines.ts";
import { symlinkDirectory } from "./inject/symlink-directory.ts";
import { decompressBin } from "./prepareDev/decompressBin.ts";
import { Readable } from "node:stream";
import { initBin } from "./prepareDev/initBin.ts";
import { isExists } from "./utils.ts";
import { log } from "./logger.ts";

const decoder = new TextDecoder();
const encoder = new TextEncoder();

/**
 * Initializes the binary and git repositories, then runs the application.
 * This function ensures a clean and prepared environment before starting the application.
 */
export async function runWithInitBinGit() {
  // Remove existing binary directory to ensure a clean slate for initialization.
  if (await isExists(binDir)) {
    await Deno.remove(binDir, { recursive: true });
  }

  // Initialize application binaries and set up the git environment for patches.
  await initBin();
  await initializeBinGit();
  // Proceed to run the application.
  await run();
}

let devViteProcess: Deno.ChildProcess | null = null;
let browserProcess: Deno.ChildProcess | null = null;
let devInit = false;

/**
 * Initializes the core application environment. This includes:
 * - Initializing application binaries.
 * - Applying necessary git patches.
 * - Generating version information.
 * - Setting up environment variables specific to the OS (e.g., for macOS sandboxing).
 * - Applying mixins (code injections) to the binary.
 * - Preparing the user profile directory for testing/development.
 * @returns The build ID string, which is crucial for subsequent build steps.
 */
async function initializeApplicationEnvironment(): Promise<string | null> {
  // Initialize application binaries and apply any necessary git patches.
  await initBin();
  await applyPatches();

  // Generate the current version for the development build.
  await genVersion();

  // Read the build ID from the specified path.
  let buildid2: string | null = null;
  if (await isExists(buildid2Path)) {
    buildid2 = await Deno.readTextFile(buildid2Path);
  }
  log.info(`buildid2: ${buildid2}`);

  // Set environment configuration for macOS to disable content sandboxing,
  // which is often necessary for development and debugging.
  if (Deno.build.os === "darwin") {
    Deno.env.set("MOZ_DISABLE_CONTENT_SANDBOX", "1");
  }

  // Perform parallel asynchronous operations:
  // 1. Apply mixins (code injections) to the binary directory.
  // 2. Ensure the _dist/profile directory is clean before setting up the test profile.
  await Promise.all([
    applyMixin(binDir),
    (async () => {
      if (await isExists("_dist/profile")) {
        await Deno.remove("_dist/profile", { recursive: true });
      }
    })(),
  ]);

  // Create the test profile directory and save default preferences.
  // This step is crucial for browser launch and consistent testing environments.
  // Reference: https://github.com/puppeteer/puppeteer/blob/c229fc8f9750a4c87d0ed3c7b541c31c8da5eaab/packages/puppeteer-core/src/node/FirefoxLauncher.ts#L123
  await Deno.mkdir(profileTestPath, { recursive: true });
  await savePrefsForProfile(profileTestPath);

  return buildid2;
}

/**
 * Runs the application in different modes (dev, test, release).
 * @param mode The mode to run the application in.
 */
export async function run(mode: "dev" | "test" | "release" = "dev") {
  const buildid2 = await initializeApplicationEnvironment();

  if (mode !== "release") {
    await runDevMode(mode, buildid2);
  } else {
    await runReleaseMode();
  }

  const command = new Deno.Command(Deno.execPath(), {
    args: ["-A", "./scripts/launchDev/child-browser.ts"],
    stdin: "piped",
    stdout: "piped",
  });

  const stream = new TextDecoderStream();

  (async () => {
    for await (const chunk of stream.readable) {
      log.info(chunk);
    }
    exit();
  })();
  browserProcess = command.spawn();
  browserProcess.stdout.pipeTo(stream.writable);
}

/**
 * Configures and runs the application in development mode.
 * This involves setting up symlinks for development assets, running child build processes,
 * and starting development servers.
 * @param mode The development mode (e.g., "dev", "test").
 * @param buildid2 The build ID string.
 */
export async function runDevMode(mode: string, buildid2: string | null) {
  // Ensure development environment initialization happens only once.
  if (!devInit) {
    // Create symbolic links for various application components to enable development features.
    // These links point to shared loader directories within 'apps/system'.
    await symlinkDirectory(
      loaderFeaturesPath, // Source: apps/system/loader-features
      featuresChromePath, // Destination: apps/features-chrome
      "link-features-chrome",
    );
    await symlinkDirectory(
      loaderFeaturesPath, // Source: apps/system/loader-features
      i18nFeaturesChromePath, // Destination: i18n/features-chrome
      "link-i18n-features-chrome",
    );
    await symlinkDirectory(
      loaderModulesPath, // Source: apps/system/loader-modules
      modulesPath, // Destination: apps/modules
      "link-modules",
    );

    // Run a child build process for development, which might compile or prepare assets.
    const childBuildCommand = new Deno.Command(Deno.execPath(), {
      args: [
        "run",
        "-A",
        "./scripts/launchDev/child-build.ts",
        mode,
        buildid2 ?? "",
      ],
    });
    // Wait for the child build process to complete.
    await childBuildCommand.output();
    log.info("run dev servers");

    // Start the development server (e.g., Vite) in a child process.
    // This server provides Hot Module Replacement (HMR) for rapid development.
    const command = new Deno.Command(Deno.execPath(), {
      args: ["-A", "./scripts/launchDev/child-dev.ts", mode, buildid2 ?? ""],
      stdin: "piped",
      stdout: "piped",
    });

    // Use Promise.withResolvers to wait for a specific log message from the dev server,
    // indicating that it's ready to accept connections.
    // Reference: https://github.com/denoland/std/issues/3757
    const p = Promise.withResolvers();
    const stream = new TextDecoderStream();

    (async () => {
      for await (const chunk of stream.readable) {
        // Resolve the promise once the specific "ready" string is found in the output.
        if (chunk.includes(devServerReadyString)) {
          p.resolve(void 0);
        }
        log.info(chunk); // Log all output from the dev server.
      }
      exit(); // Exit the main process if the dev server process terminates.
    })();
    devViteProcess = command.spawn(); // Spawn the dev server process.
    devViteProcess.stdout.pipeTo(stream.writable); // Pipe its stdout to our stream.
    await p.promise; // Wait for the dev server to signal readiness.

    devInit = true; // Mark development environment as initialized.
  }
  // Inject manifest and XHTML content specific to the development build into the binary directory.
  await injectManifest(binDir, "dev", devBrandingSuffix);
  await injectXHTMLDev(binDir);
}

/**
 * Configures and prepares the application for a production release.
 * This involves running a production build, injecting release-specific manifests,
 * and setting up final symlinks for the distributable package.
 */
export async function runReleaseMode() {
  // Perform initial build steps required before the mozbuild system takes over.
  await buildForProduction("before-mozbuild");
  // Inject the manifest for the production run.
  await injectManifest(binDir, "run-prod", devBrandingSuffix);

  // Clean up any existing development symlinks for the branding suffix.
  if (await isExists(`${binDir}/${devBrandingSuffix}`)) {
    await Deno.remove(`${binDir}/${devBrandingSuffix}`, {
      recursive: true,
    });
  }
  // Create a final symlink for the production-ready noraneko binary.
  await symlinkDirectory(
    binDir, // Source directory for the symlink.
    "_dist/noraneko", // Destination path for the symlink.
    devBrandingSuffix, // Name of the symlink.
  );
}

let runningExit = false;

/**
 * Exits the application gracefully by shutting down all child processes.
 * Ensures that resources are released and prevents orphaned processes.
 */
export async function exit() {
  // Prevent multiple calls to exit.
  if (runningExit) return;
  runningExit = true;

  // Attempt to gracefully shut down the browser process.
  if (browserProcess) {
    log.info("Start Shutdown browserProcess");
    // Send 'q' to stdin to signal the browser process to quit.
    browserProcess.stdin.getWriter().write(encoder.encode("q"));
    try {
      // Wait for the browser process to terminate.
      await browserProcess.status; // Use .status to wait for process completion
    } catch (error) {
      log.error("Error shutting down browserProcess:", error);
    }
    log.info("End Shutdown browserProcess");
  }

  // Attempt to gracefully shut down the development Vite server process.
  if (devViteProcess) {
    log.info("Start Shutdown devViteProcess");
    // Send 'q' to stdin to signal the Vite process to quit.
    devViteProcess.stdin.getWriter().write(encoder.encode("q"));
    try {
      // Wait for the Vite process to terminate.
      await devViteProcess.status; // Use .status to wait for process completion
    } catch (error) {
      log.error("Error shutting down devViteProcess:", error);
    }
    log.info("End Shutdown devViteProcess");
  }

  log.info("Cleanup Complete!");
  // Exit the Deno process with a success code.
  Deno.exit(0);
}

/**
 * Determines the binary path for macOS application bundles.
 * This function attempts to find a .app bundle within the mozbuild output directory
 * and constructs the correct path to its resources.
 * @param outputDir The output directory from mozbuild.
 * @returns The determined binary path within the macOS app bundle or a fallback bin directory.
 */
async function getMacOsAppBundleBinPath(outputDir: string): Promise<string> {
  try {
    // Determine the correct binary path based on the OS (e.g., .app bundle for macOS).
    const files = Deno.readDir(outputDir);
    const appFiles = [];
    for await (const file of files) {
      if (file.name.endsWith(".app")) {
        appFiles.push(file.name);
      }
    }
    if (appFiles.length > 0) {
      const appFile = appFiles[0];
      const appPath = `${outputDir}/${appFile}`;
      const binPath = `${appPath}/Contents/Resources`;
      log.info(`Using app bundle directory: ${appPath}/Contents/Resources`);
      return binPath;
    } else {
      const binPath = `${outputDir}/bin`;
      log.info(`Using bin directory: ${outputDir}/bin`);
      return binPath;
    }
  } catch (error) {
    log.warn("Error reading output directory for macOS app bundle:", error);
    const binPath = `${outputDir}/bin`; // Fallback to default bin path on error.
    return binPath;
  }
}

/**
 * Orchestrates the production build process, integrating with an external `mozbuild` system.
 * This function handles steps both before and after `mozbuild` performs its packaging.
 * @param mode Specifies whether the function is called "before-mozbuild" or "after-mozbuild".
 */
export async function buildForProduction(
  mode: "before-mozbuild" | "after-mozbuild",
) {
  // Read the build ID, which might be used by mozbuild or for debugging.
  let buildid2: string | null = null;
  if (await isExists(buildid2Path)) {
    buildid2 = await Deno.readTextFile(buildid2Path);
  }
  log.info(`buildid2: ${buildid2}`);

  if (mode === "before-mozbuild") {
    // This phase prepares files for mozbuild.
    // It's crucial because mozbuild needs noraneko files (e.g., compiled assets)
    // for packing into its `jar.mn` manifest.
    const childBuildCommand = new Deno.Command(Deno.execPath(), {
      args: [
        "run",
        "-A",
        "./scripts/launchDev/child-build.ts",
        "production",
        buildid2 ?? "",
      ],
    });
    await childBuildCommand.output(); // Wait for the production child build to complete.
    await injectManifest("./_dist", "prod"); // Inject the production manifest into the _dist directory.
  } else if (mode === "after-mozbuild") {
    // This phase runs after mozbuild has completed its packaging.
    // It locates the final binary output from mozbuild and performs final injections.
    let binPath: string;
    if (Deno.build.os === "darwin") {
      binPath = await getMacOsAppBundleBinPath(mozbuildOutputDir);
    } else {
      binPath = `${mozbuildOutputDir}/bin`;
      log.info(`Using bin directory: ${mozbuildOutputDir}/bin`);
    }
    injectXHTML(binPath); // Inject final XHTML content into the located binary path.

    // Write buildid2 into the browser directory within the final binary.
    // This is typically for debugging or version tracking within the browser itself,
    // and might not be used in the final shipped browser.
    let buildid2: string | null = null;
    if (await isExists(buildid2Path)) {
      buildid2 = await Deno.readTextFile(buildid2Path);
    }
    await writeBuildid2(`${binPath}/browser`, buildid2 ?? "");
  }
}
