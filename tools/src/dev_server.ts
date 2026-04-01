// SPDX-License-Identifier: MPL-2.0

import * as path from "@std/path";
import { PROJECT_ROOT, DEV_SERVER } from "./defines.ts";
import { Logger } from "./utils.ts";
import { sleep } from "./async_utils.ts";

const logger = new Logger("dev-server");

type ManagedViteProcess = {
  name: string;
  port: number;
  child: Deno.ChildProcess;
};

let viteProcesses: ManagedViteProcess[] = [];

async function detectEarlyExit(
  proc: ManagedViteProcess,
  graceMs: number,
): Promise<Deno.CommandStatus | null> {
  return await Promise.race([
    proc.child.status,
    sleep(graceMs).then(() => null),
  ]);
}

export async function run(writer: any): Promise<void> {
  logger.info("Starting Vite dev servers...");

  const servers = [
    { name: "designs", path: path.join(PROJECT_ROOT, "browser-features/skin") },
    { name: "main", path: path.join(PROJECT_ROOT, "bridge/loader-features") },
    {
      name: "modal-child",
      path: path.join(PROJECT_ROOT, "browser-features/pages-modal-child"),
    },
    {
      name: "newtab",
      path: path.join(PROJECT_ROOT, "browser-features/pages-newtab"),
    },
    {
      name: "notes",
      path: path.join(PROJECT_ROOT, "browser-features/pages-notes"),
    },
    {
      name: "settings",
      path: path.join(PROJECT_ROOT, "browser-features/pages-settings"),
    },
    {
      name: "welcome",
      path: path.join(PROJECT_ROOT, "browser-features/pages-welcome"),
    },
    {
      name: "profile-manager",
      path: path.join(PROJECT_ROOT, "browser-features/pages-profile-manager"),
    },
    {
      name: "workflow-progress",
      path: path.join(PROJECT_ROOT, "browser-features/pages-workflow-progress"),
    },
  ];

  // Ensure logs directory exists
  const logsDir = path.join(PROJECT_ROOT, "logs");
  try {
    Deno.mkdirSync(logsDir, { recursive: true });
  } catch (e: any) {
    logger.warn(`Failed to create logs dir ${logsDir}: ${e?.message ?? e}`);
  }

  for (const server of servers) {
    const port = getPortFor(server.name);
    // Run Vite via Deno's npm compatibility (Deno-only)
    const cmd = "deno";
    const args = ["run", "-A", "npm:vite", "--port", String(port)];

    const child = new Deno.Command(cmd, {
      args,
      cwd: server.path,
      stdin: "null",
      stdout: "piped",
      stderr: "piped",
    }).spawn();

    const logFilePath = path.join(logsDir, `vite-${server.name}.log`);
    let fh: Deno.FsFile | null = null;
    try {
      fh = await Deno.open(logFilePath, {
        write: true,
        create: true,
        truncate: true,
      });
    } catch (e: any) {
      logger.warn(`Failed to open log file ${logFilePath}: ${e?.message ?? e}`);
    }

    const pipeToFile = async (
      stream: ReadableStream<Uint8Array> | null,
      file: Deno.FsFile | null,
    ) => {
      if (!stream || !file) return;
      const reader = stream.getReader();
      try {
        while (true) {
          const { value, done } = await reader.read();
          if (done) break;
          if (value) {
            await file.write(value);
          }
        }
      } catch (e: any) {
        logger.warn(`Error piping to file ${logFilePath}: ${e?.message ?? e}`);
      }
    };

    // Start piping without awaiting so servers run concurrently
    pipeToFile(child.stdout, fh);
    pipeToFile(child.stderr, fh);

    viteProcesses.push({ name: server.name, port, child });

    logger.info(
      `Started Vite dev server for ${server.name} with PID: ${child.pid}, logging to ${logFilePath}`,
    );
  }

  logger.info("All Vite dev servers started. Verifying startup health...");

  const startupFailures: string[] = [];
  for (const proc of viteProcesses) {
    const status = await detectEarlyExit(proc, 2_000);
    if (status && status.code !== 0) {
      startupFailures.push(
        `${proc.name} (port ${proc.port}) exited early with code ${status.code}`,
      );
    }
  }

  if (startupFailures.length > 0) {
    shutdown();
    throw new Error(
      `One or more Vite dev servers failed to start:\n${startupFailures.join("\n")}`,
    );
  }

  // Keep a small additional wait for initial compile/warm-up.
  await sleep(3_000);

  try {
    if (typeof writer?.write === "function") {
      // prefer direct write
      writer.write(`${DEV_SERVER.ready_string}\n`);
    } else if (typeof writer === "function") {
      // some callers pass a callback
      writer(`${DEV_SERVER.ready_string}\n`);
    } else {
      // fallback to console
      console.log(DEV_SERVER.ready_string);
    }

    if (typeof writer?.end === "function") {
      writer.end();
    }
  } catch (e: any) {
    logger.warn(`Failed to write ready string to writer: ${e?.message ?? e}`);
  }
}

export function shutdown(): void {
  logger.info("Shutting down Vite dev servers...");
  for (const proc of viteProcesses) {
    try {
      if (proc.child?.pid) {
        try {
          Deno.kill(proc.child.pid);
        } catch {
          // ignore
        }
      }
    } catch (e: any) {
      logger.warn(
        `Failed to stop process ${proc.child?.pid}: ${e?.message ?? e}`,
      );
    }
  }
  viteProcesses = [];
  logger.success("Vite dev servers shut down.");
}

export function getPortFor(serverName: string): number {
  switch (serverName) {
    case "main":
      return 5181;
    case "designs":
      return 5174;
    case "modal-child":
      return 5185;
    case "newtab":
      return 5186;
    case "notes":
      return 5188;
    case "settings":
      return 5183;
    case "welcome":
      return 5187;
    case "profile-manager":
      return 5179;
    case "workflow-progress":
      return 5192;
    default:
      return DEV_SERVER.default_port;
  }
}
