// SPDX-License-Identifier: MPL-2.0

import * as Initializer from "./src/initializer.ts";
import * as Patcher from "./src/patcher.ts";
import * as CustomAppIcons from "./src/custom_app_icons.ts";
import * as Pref from "../static/gecko/pref/pref.ts";
import * as Symlinker from "./src/symlinker.ts";
import * as Update from "./src/update.ts";
import * as Builder from "./src/builder.ts";
import * as DevServer from "./src/dev_server.ts";
import * as Injector from "./src/injector.ts";
import * as BrowserLauncher from "./src/browser_launcher.ts";
import * as DevEnvManager from "./src/dev_env_manager.ts";
import {
  describeLaunchPorts,
  parsePortOffsetSpec,
  resolveAutoDevPortOffset,
} from "./src/port_offset.ts";
import { devHttpOrigin } from "#libs/shared/dev-ports/defines.ts";
import { Logger } from "./src/utils.ts";
import { withDevStateLock } from "./src/dev_state_lock.ts";
import { reportDevStartupFailure, startDevBrowser } from "./src/dev_launch.ts";

const logger = new Logger("feles-build");

/**
 * Parse the optional `--port-offset <n|auto>` argument from feles-build argv
 * (FDH Gate 2 task 2-B). `auto` probes the smallest free admissible offset.
 * Without the flag the offset is 0 and every port/pref write stays
 * byte-identical to the pre-offset behavior.
 */
async function argvPortOffset(argv: string[]): Promise<number> {
  const idx = argv.indexOf("--port-offset");
  if (idx < 0) {
    return 0;
  }
  const raw = argv[idx + 1];
  if (raw === undefined || raw.startsWith("--")) {
    throw new Error("--port-offset requires a value: <n|auto>");
  }
  const spec = parsePortOffsetSpec(raw);
  const offset = spec === "auto" ? await resolveAutoDevPortOffset() : spec;
  logger.info(`Dev/test port offset: ${describeLaunchPorts(offset)}`);
  return offset;
}

async function runDev(portOffset = 0): Promise<void> {
  logger.info("Starting development environment...");
  if (portOffset !== 0) {
    logger.info(
      `Dev server ports shifted by +${portOffset} (see --port-offset).`,
    );
  }

  // Initial setup
  // Prefer the Debug FTP channel. Allow the locked Release runtime when the
  // latest Debug runtime is incompatible with this checkout's patches.
  await Initializer.run({
    distribution: Deno.env.get("FLOORP_DEV_RUNTIME_DISTRIBUTION") === "release"
      ? "release"
      : "debug",
  });
  CustomAppIcons.run();
  // A packaged Debug Runtime has no moz-src; `fdh task prepare` records
  // FLOORP_SKIP_PATCHER=1 so the local loop can start without aborting.
  if (Deno.env.get("FLOORP_SKIP_PATCHER") !== "1") Patcher.run("apply");
  Pref.run();
  Symlinker.run();

  const buildid2 = Update.generateUuidV7();
  await Builder.run("dev", buildid2);
  Injector.run("dev");
  await Injector.injectXhtmlFromTs({
    devPages: true,
    allowBrowserHttpLoader: true,
    portOffset,
  });
  // Writes the offset into the launch profile (only when > 0): the pref
  // `nora.dev.port_offset` for the browser-side resolver and `marionette.port`
  // so Marionette does not overlap between concurrent worktrees.
  DevEnvManager.setup({ allowBrowserHttpLoader: true, portOffset });

  // Graceful shutdown
  Deno.addSignalListener("SIGINT", () => {
    logger.info("Shutting down...");
    DevServer.shutdown();
    Deno.exit(130);
  });

  await startDevBrowser(
    (signal) => DevServer.run(() => {}, { portOffset, signal }),
    (onStarted, signal) =>
      BrowserLauncher.run({
        initialUrl: devHttpOrigin("settings", "/", portOffset),
        recordProcess: true,
        onStarted,
        signal,
      }),
    DevServer.stop,
    () => {
      logger.info("Browser closed — shutting down dev servers.");
      DevServer.shutdown();
      Deno.exit(0);
    },
    (error) => {
      logger.error(`Browser launcher failed: ${error}`);
      Deno.exit(1);
    },
    90000,
    reportDevStartupFailure,
  );
}

async function runStage(
  options: { marionette?: boolean; portOffset?: number } = {},
): Promise<void> {
  const marionette = options.marionette ?? false;
  const portOffset = options.portOffset ?? 0;
  logger.info(
    "Starting staged production build (production assets) with browser in dev mode...",
  );
  if (!marionette) {
    logger.info(
      "Marionette is disabled by default (use --marionette to enable).",
    );
  }

  // Initial setup
  await Initializer.run();
  CustomAppIcons.run();
  // A packaged Debug Runtime has no moz-src; `fdh task prepare` records
  // FLOORP_SKIP_PATCHER=1 so the local loop can start without aborting.
  if (Deno.env.get("FLOORP_SKIP_PATCHER") !== "1") Patcher.run("apply");
  Pref.run();
  Symlinker.run();

  // Build packaged assets with a stage startup marker for profile isolation.
  const buildid2 = Update.generateUuidV7();
  await Builder.run("stage", buildid2);

  // Inject manifests but keep dev-style directory so dev servers and browser use the built assets
  Injector.run("stage");
  await Injector.injectXhtmlFromTs({
    devPages: true,
    allowBrowserHttpLoader: false,
    portOffset,
  });
  DevEnvManager.setup({ allowBrowserHttpLoader: false, portOffset });

  // Graceful shutdown
  Deno.addSignalListener("SIGINT", () => {
    logger.info("Shutting down...");
    DevServer.shutdown();
    Deno.exit(130);
  });

  await startDevBrowser(
    (signal) => DevServer.run(() => {}, { portOffset, signal }),
    (onStarted, signal) =>
      BrowserLauncher.run({
        marionette,
        recordProcess: true,
        onStarted,
        signal,
      }),
    DevServer.stop,
    () => {
      logger.info("Browser closed — shutting down dev servers.");
      DevServer.shutdown();
      Deno.exit(0);
    },
    (error) => {
      logger.error(`Browser launcher failed: ${error}`);
      Deno.exit(1);
    },
    90000,
    reportDevStartupFailure,
  );
}

async function runTest(
  ownNativeSession = false,
  portOffset = 0,
): Promise<void> {
  logger.info("Starting test environment...");

  // Initial setup
  await Initializer.run();
  CustomAppIcons.run();
  // A packaged Debug Runtime has no moz-src; `fdh task prepare` records
  // FLOORP_SKIP_PATCHER=1 so the local loop can start without aborting.
  if (Deno.env.get("FLOORP_SKIP_PATCHER") !== "1") Patcher.run("apply");
  Pref.run();
  Symlinker.run();

  const buildid2 = Update.generateUuidV7();
  await Builder.run("test", buildid2);
  Injector.run("dev");
  await Injector.injectXhtmlFromTs({
    devPages: true,
    allowBrowserHttpLoader: true,
    portOffset,
  });
  DevEnvManager.setup({ allowBrowserHttpLoader: true, portOffset });

  // Graceful shutdown
  Deno.addSignalListener("SIGINT", () => {
    logger.info("Shutting down...");
    DevServer.shutdown();
    Deno.exit(130);
  });

  await startDevBrowser(
    (signal) => DevServer.run(() => {}, { portOffset, signal }),
    (onStarted, signal) =>
      BrowserLauncher.run({
        marionette: true,
        ownLinuxSession: ownNativeSession,
        recordProcess: true,
        onStarted,
        signal,
      }),
    DevServer.stop,
    () => {},
    (error) => logger.error(`Browser launcher failed: ${error}`),
    90000,
    reportDevStartupFailure,
  );
}

async function runBuild(phase?: string): Promise<void> {
  const optionsPhase = phase ?? null;
  if (!optionsPhase) {
    console.error("Error: --phase is required for the build command.");
    Deno.exit(1);
  }

  if (optionsPhase === "before-mach") {
    Symlinker.run();
    // Build production assets
    const buildid2 = Update.generateUuidV7();
    await Builder.run("production", buildid2);
  } else if (optionsPhase === "after-mach") {
    // await Injector.createManifest("production", "_dist/noraneko");
    await Injector.injectXhtmlFromTs({
      // Local after-mach injection must use the local BIN_DIR; only real CI
      // (GitHub Actions sets CI=true) uses the obj-artifact-build-output path.
      isCI: Deno.env.get("CI") === "true",
      allowBrowserHttpLoader: false,
    });
  } else {
    console.error(`Unknown phase: ${optionsPhase}`);
    Deno.exit(1);
  }
}

function runPatch(action = "apply"): void {
  Patcher.run(action);
}

function printHelp(): void {
  console.log("Usage: deno task feles-build <command> [options]\n");
  console.log("Commands:");
  console.log("  dev        Run the development workflow");
  console.log(
    "  test       Build with MODE=test and launch browser for browser tests",
  );
  console.log(
    "  stage      Build production assets and run browser in dev mode",
  );
  console.log("  build      Run the production build workflow (--phase)");
  console.log("  misc       Misc commands (e.g. 'misc patch')");
  console.log("");
  console.log(
    "dev/test/stage accept --port-offset <n|auto> to shift every dev/test",
  );
  console.log(
    'server port (and Marionette) by n; "auto" picks a free offset.',
  );
  console.log("");
  console.log("Run 'feles-build <command> --help' for command-specific help.");
}

async function main(): Promise<void> {
  const argv = Deno.args.slice();
  const command = argv.shift();

  switch (command) {
    case "dev": {
      // simple help support
      if (argv.includes("--help") || argv.includes("-h")) {
        console.log(
          "Usage: feles-build dev [--port-offset <n|auto>]",
        );
        return;
      }
      await withDevStateLock(async () => runDev(await argvPortOffset(argv)));
      break;
    }
    case "test": {
      if (argv.includes("--help") || argv.includes("-h")) {
        console.log(
          "Usage: feles-build test [--own-native-session] [--port-offset <n|auto>]",
        );
        console.log(
          "  Build with MODE=test, start dev servers, and launch browser.",
        );
        return;
      }
      await withDevStateLock(async () =>
        runTest(
          argv.includes("--own-native-session"),
          await argvPortOffset(argv),
        )
      );
      break;
    }
    case "stage": {
      if (argv.includes("--help") || argv.includes("-h")) {
        console.log(
          "Usage: feles-build stage [--marionette] [--port-offset <n|auto>]",
        );
        return;
      }
      await withDevStateLock(async () =>
        runStage({
          marionette: argv.includes("--marionette"),
          portOffset: await argvPortOffset(argv),
        })
      );
      break;
    }
    case "build": {
      if (argv.includes("--help") || argv.includes("-h")) {
        console.log(
          "Usage: feles-build build --phase <before-mach|after-mach>",
        );
        return;
      }
      const idx = argv.indexOf("--phase");
      const phase = idx >= 0 ? argv[idx + 1] : undefined;
      await runBuild(phase);
      break;
    }
    case "misc": {
      if (argv.includes("--help") || argv.includes("-h")) {
        console.log(
          "Usage: feles-build misc patch --action <apply|create|init>",
        );
        return;
      }
      const sub = argv[0];
      if (sub === "patch") {
        const idx = argv.indexOf("--action");
        const action = idx >= 0 ? argv[idx + 1] : "apply";
        runPatch(action);
      } else if (sub === "writeVersion") {
        Update.writeVersion("static/gecko");
        logger.success("Version written to static/gecko/config/");
      } else {
        logger.error(`Unknown misc command: ${sub}`);
        printHelp();
        Deno.exit(1);
      }
      break;
    }
    case "--help":
    case "-h":
    case undefined:
      printHelp();
      break;
    default:
      logger.error(`Unknown command: ${command}`);
      printHelp();
      Deno.exit(1);
  }
}

// deno-lint-ignore no-explicit-any
main().catch((e: any) => {
  logger.error(`Unhandled error: ${e?.message ?? e}`);
  Deno.exit(1);
});
