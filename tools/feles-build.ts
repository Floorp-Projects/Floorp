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
import { DEV_SERVER } from "./src/defines.ts";
import {
  describeLaunchPorts,
  parsePortOffsetSpec,
  resolveAutoDevPortOffset,
} from "./src/port_offset.ts";
import {
  type FelesBuildGlobalArgs,
  parseFelesBuildGlobalArgs,
} from "./src/feles_build_args.ts";
import { devHttpOrigin } from "#libs/shared/dev-ports/defines.ts";
import { Logger } from "./src/utils.ts";

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

type ReadyPipe = {
  on(event: "data", cb: (chunk: string) => void): void;
  write(chunk: Uint8Array | string): void;
  end(): void;
};

function createReadyPipe(): ReadyPipe {
  class ReadyPipeImpl implements ReadyPipe {
    private listeners: Array<(chunk: string) => void> = [];

    on(event: "data", cb: (chunk: string) => void) {
      if (event === "data") this.listeners.push(cb);
    }

    write(chunk: Uint8Array | string) {
      const s = typeof chunk === "string"
        ? chunk
        : new TextDecoder().decode(chunk);
      for (const cb of this.listeners) cb(s);
    }

    end() {
      // no-op
    }
  }

  return new ReadyPipeImpl();
}

function isDevServerReady(chunk: string): boolean {
  const s = chunk.toString().trim();
  return s === DEV_SERVER.ready_string || s.includes("nora-");
}

async function runDev(
  portOffset = 0,
  localArtifactPath?: string,
): Promise<void> {
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
    localArtifactPath,
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

  const pipe = createReadyPipe();

  pipe.on("data", (chunk: string) => {
    if (isDevServerReady(chunk)) {
      logger.success("Dev servers are ready.");
      // Launch browser; when it exits (or fails to launch), take the dev
      // servers down with it — otherwise they keep listening on their
      // ports and the next run dies with "port already in use".
      BrowserLauncher.run({
        initialUrl: devHttpOrigin("settings", "/", portOffset),
        recordProcess: true,
      }).then(() => {
        logger.info("Browser closed — shutting down dev servers.");
        DevServer.shutdown();
        Deno.exit(0);
        // deno-lint-ignore no-explicit-any
      }).catch((e: any) => {
        logger.error(`Browser launcher failed: ${e?.message ?? e}`);
        DevServer.shutdown();
        Deno.exit(1);
      });
    }
  });

  // Start dev server (it will write to the writer and end it)
  // DevServer.run expects a writable-like object.
  // deno-lint-ignore no-explicit-any
  DevServer.run(pipe as any, { portOffset }).catch((e: any) => {
    logger.error(`Dev server failed: ${e?.message ?? e}`);
    Deno.exit(1);
  });
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

  // Build production assets
  const buildid2 = Update.generateUuidV7();
  await Builder.run("production", buildid2);

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

  const pipe = createReadyPipe();

  pipe.on("data", (chunk: string) => {
    if (isDevServerReady(chunk)) {
      logger.success("Dev servers are ready.");
      // Launch browser; when it exits (or fails to launch), take the dev
      // servers down with it — otherwise ~9 vite servers keep listening
      // on 5170-5190 after "Browser Closed" and the next stage run fails
      // with "Port 5181 is already in use".
      BrowserLauncher.run({ marionette }).then(() => {
        logger.info("Browser closed — shutting down dev servers.");
        DevServer.shutdown();
        Deno.exit(0);
        // deno-lint-ignore no-explicit-any
      }).catch((e: any) => {
        logger.error(`Browser launcher failed: ${e?.message ?? e}`);
        DevServer.shutdown();
        Deno.exit(1);
      });
    }
  });

  // Start dev server (it will write to the writer and end it)
  // DevServer.run expects a writable-like object.
  // deno-lint-ignore no-explicit-any
  DevServer.run(pipe as any, { portOffset }).catch((e: any) => {
    logger.error(`Dev server failed: ${e?.message ?? e}`);
    Deno.exit(1);
  });
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

  const pipe = createReadyPipe();

  pipe.on("data", (chunk: string) => {
    if (isDevServerReady(chunk)) {
      logger.success(
        "Dev servers are ready. Launching browser in test mode...",
      );
      BrowserLauncher.run({
        marionette: true,
        ownLinuxSession: ownNativeSession,
      }).catch((e: unknown) => {
        logger.error(
          `Browser launcher failed: ${e instanceof Error ? e.message : e}`,
        );
      });
    }
  });

  // deno-lint-ignore no-explicit-any
  DevServer.run(pipe as any, { portOffset }).catch((e: any) => {
    logger.error(`Dev server failed: ${e?.message ?? e}`);
    Deno.exit(1);
  });
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
  console.log(
    "Usage: deno task feles-build [--artifact <path>] <command> [options]\n",
  );
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
  console.log("Global options:");
  console.log(
    "  --artifact <path>  dev only: install a local Runtime artifact",
  );
  console.log(
    "                     (.zip/.tar.xz/.dmg) instead of downloading it",
  );
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

function parseGlobalArgsOrExit(): FelesBuildGlobalArgs {
  try {
    return parseFelesBuildGlobalArgs(Deno.args);
  } catch (error) {
    if (Deno.args.includes("--help") || Deno.args.includes("-h")) {
      printHelp();
      Deno.exit(0);
    }
    logger.error(error instanceof Error ? error.message : String(error));
    logger.error("Use 'feles-build --artifact <path> dev'.");
    Deno.exit(1);
  }
}

async function main(): Promise<void> {
  const parsed = parseGlobalArgsOrExit();
  const argv = parsed.argv.slice();
  const command = argv.shift();

  if (
    parsed.artifactPath !== undefined &&
    command !== "dev" &&
    command !== "--help" &&
    command !== "-h" &&
    command !== undefined
  ) {
    logger.error(
      `--artifact is only supported by the dev command (received: ${command}).`,
    );
    logger.error("Use 'feles-build --artifact <path> dev'.");
    Deno.exit(1);
  }

  if (parsed.artifactPath !== undefined && command === undefined) {
    logger.error("--artifact requires a command (e.g. dev).");
    logger.error("Use 'feles-build --artifact <path> dev'.");
    Deno.exit(1);
  }

  switch (command) {
    case "dev": {
      // simple help support
      if (argv.includes("--help") || argv.includes("-h")) {
        console.log(
          "Usage: feles-build dev [--port-offset <n|auto>]",
        );
        console.log(
          "       feles-build --artifact <path> dev [--port-offset <n|auto>]",
        );
        console.log(
          "  --artifact installs a local Runtime artifact instead of downloading it.",
        );
        return;
      }
      await runDev(await argvPortOffset(argv), parsed.artifactPath);
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
      await runTest(
        argv.includes("--own-native-session"),
        await argvPortOffset(argv),
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
      await runStage({
        marionette: argv.includes("--marionette"),
        portOffset: await argvPortOffset(argv),
      });
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
