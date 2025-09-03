import * as Initializer from "./src/initializer.ts";
import * as Patcher from "./src/patcher.ts";
import * as Symlinker from "./src/symlinker.ts";
import * as Update from "./src/update.ts";
import * as Builder from "./src/builder.ts";
import * as DevServer from "./src/dev_server.ts";
import * as Injector from "./src/injector.ts";
import * as BrowserLauncher from "./src/browser_launcher.ts";
import * as DevEnvManager from "./src/dev_env_manager.ts";
import { Logger } from "./src/utils.ts";

const logger = new Logger("feles-build");

async function runDev(): Promise<void> {
  logger.info("Starting development environment...");

  // Initial setup
  await Initializer.run();
  Patcher.run("apply");
  Symlinker.run();

  const buildid2 = Update.generateUuidV7();
  await Builder.run("dev", buildid2);
  Injector.run("dev");
  await Injector.injectXhtmlFromTs(true);
  DevEnvManager.setup();

  // Graceful shutdown
  Deno.addSignalListener("SIGINT", () => {
    logger.info("Shutting down...");
    DevServer.shutdown();
    Deno.exit(130);
  });

  // Simple writable-like object to capture ready signal from DevServer.run
  class ReadyPipe {
    private listeners: Array<(chunk: string) => void> = [];
    on(event: "data", cb: (chunk: string) => void) {
      if (event === "data") this.listeners.push(cb);
    }
    write(chunk: Uint8Array | string) {
      const s =
        typeof chunk === "string" ? chunk : new TextDecoder().decode(chunk);
      for (const cb of this.listeners) cb(s);
    }
    end() {
      // no-op
    }
  }
  const pipe = new ReadyPipe();
  let readyReceived = false;

  pipe.on("data", (chunk: string) => {
    const s = chunk.toString().trim();
    if (
      s === (DevServer as any).DEV_SERVER?.ready_string ||
      s.includes("nora-")
    ) {
      readyReceived = true;
      logger.success("Dev servers are ready.");
      // Launch browser
      BrowserLauncher.run().catch((e: any) => {
        logger.error(`Browser launcher failed: ${e?.message ?? e}`);
      });
    }
  });

  // Start dev server (it will write to the writer and end it)
  // DevServer.run expects a writable-like object.
  DevServer.run(pipe as any).catch((e: any) => {
    logger.error(`Dev server failed: ${e?.message ?? e}`);
    Deno.exit(1);
  });

  // Wait until browser finishes; the BrowserLauncher.run call above is async but we don't await here
  // After browser closed, shut down servers
  // Simple polling to detect when ready was received and browser process done is not trivial here.
  // Keep process alive until SIGINT or process termination from BrowserLauncher path
}

async function runStage(): Promise<void> {
  logger.info(
    "Starting staged production build (production assets) with browser in dev mode...",
  );

  // Initial setup
  await Initializer.run();
  Patcher.run("apply");
  Symlinker.run();

  // Build production assets
  const buildid2 = Update.generateUuidV7();
  await Builder.run("production", buildid2);

  // Inject manifests but keep dev-style directory so dev servers and browser use the built assets
  Injector.run("dev");
  await Injector.injectXhtmlFromTs(true);
  DevEnvManager.setup();

  // Graceful shutdown
  Deno.addSignalListener("SIGINT", () => {
    logger.info("Shutting down...");
    DevServer.shutdown();
    Deno.exit(130);
  });

  // Simple writable-like object to capture ready signal from DevServer.run
  class ReadyPipe {
    private listeners: Array<(chunk: string) => void> = [];
    on(event: "data", cb: (chunk: string) => void) {
      if (event === "data") this.listeners.push(cb);
    }
    write(chunk: Uint8Array | string) {
      const s =
        typeof chunk === "string" ? chunk : new TextDecoder().decode(chunk);
      for (const cb of this.listeners) cb(s);
    }
    end() {
      // no-op
    }
  }
  const pipe = new ReadyPipe();
  let readyReceived = false;

  pipe.on("data", (chunk: string) => {
    const s = chunk.toString().trim();
    if (
      s === (DevServer as any).DEV_SERVER?.ready_string ||
      s.includes("nora-")
    ) {
      readyReceived = true;
      logger.success("Dev servers are ready.");
      // Launch browser
      BrowserLauncher.run().catch((e: any) => {
        logger.error(`Browser launcher failed: ${e?.message ?? e}`);
      });
    }
  });

  // Start dev server (it will write to the writer and end it)
  // DevServer.run expects a writable-like object.
  DevServer.run(pipe as any).catch((e: any) => {
    logger.error(`Dev server failed: ${e?.message ?? e}`);
    Deno.exit(1);
  });

  // Keep process alive until SIGINT or browser termination like runDev
}

async function runBuild(phase?: string): Promise<void> {
  const optionsPhase = phase ?? null;
  if (!optionsPhase) {
    console.error("Error: --phase is required for the build command.");
    process.exit(1);
  }

  if (optionsPhase === "before-mach") {
    Symlinker.run();
    // Build production assets
    const buildid2 = Update.generateUuidV7();
    await Builder.run("production", buildid2);
  } else if (optionsPhase === "after-mach") {
    // Injector.run("production");
    await Injector.injectXhtmlFromTs(false, true);
  } else {
    console.error(`Unknown phase: ${optionsPhase}`);
    process.exit(1);
  }
}

async function runPatch(action = "apply"): Promise<void> {
  Patcher.run(action);
}

function printHelp(): void {
  console.log("Usage: deno task feles-build <command> [options]\n");
  console.log("Commands:");
  console.log("  dev        Run the development workflow");
  console.log(
    "  stage      Build production assets and run browser in dev mode",
  );
  console.log("  build      Run the production build workflow (--phase)");
  console.log("  misc       Misc commands (e.g. 'misc patch')");
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
        console.log("Usage: feles-build dev");
        return;
      }
      await runDev();
      break;
    }
    case "stage": {
      if (argv.includes("--help") || argv.includes("-h")) {
        console.log("Usage: feles-build stage");
        return;
      }
      await runStage();
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

main().catch((e: any) => {
  logger.error(`Unhandled error: ${e?.message ?? e}`);
  Deno.exit(1);
});
