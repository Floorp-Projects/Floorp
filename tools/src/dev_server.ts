// SPDX-License-Identifier: MPL-2.0

import * as path from "@std/path";
import {
  DEV_PORTS,
  devPort,
  type DevPortName,
  requireDevPortOffset,
} from "#libs/shared/dev-ports/defines.ts";
import { DEV_SERVER, PROJECT_ROOT } from "./defines.ts";
import { Logger } from "./utils.ts";
import { recordDevProcess, reportDevPortConflict } from "./dev_processes.ts";
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

export async function run(
  // deno-lint-ignore no-explicit-any
  writer: any,
  options: { portOffset?: number } = {},
): Promise<void> {
  const portOffset = requireDevPortOffset(options.portOffset ?? 0);
  logger.info("Starting Vite dev servers...");
  if (portOffset > 0) {
    logger.info(
      `Dev server ports shifted by --port-offset +${portOffset}.`,
    );
  }
  await recordDevProcess(Deno.pid);

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
    // deno-lint-ignore no-explicit-any
  } catch (e: any) {
    logger.warn(`Failed to create logs dir ${logsDir}: ${e?.message ?? e}`);
  }

  for (const server of servers) {
    const port = getPortFor(server.name, portOffset);
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
      // deno-lint-ignore no-explicit-any
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
        // deno-lint-ignore no-explicit-any
      } catch (e: any) {
        logger.warn(`Error piping to file ${logFilePath}: ${e?.message ?? e}`);
      }
    };

    // Start piping without awaiting so servers run concurrently
    pipeToFile(child.stdout, fh);
    pipeToFile(child.stderr, fh);

    viteProcesses.push({ name: server.name, port, child });
    await recordDevProcess(child.pid);

    logger.info(
      `Started Vite dev server for ${server.name} with PID: ${child.pid}, logging to ${logFilePath}`,
    );
  }

  logger.info("All Vite dev servers started. Verifying startup health...");

  const startupFailures: string[] = [];
  for (const proc of viteProcesses) {
    const status = await detectEarlyExit(proc, 2_000);
    if (status && status.code !== 0) {
      const log = await Deno.readTextFile(
        path.join(logsDir, `vite-${proc.name}.log`),
      ).catch(() => "");
      await reportDevPortConflict(log);
      startupFailures.push(
        `${proc.name} (port ${proc.port}) exited early with code ${status.code}`,
      );
    }
  }

  if (startupFailures.length > 0) {
    shutdown();
    throw new Error(
      `One or more Vite dev servers failed to start:\n${
        startupFailures.join("\n")
      }`,
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
    // deno-lint-ignore no-explicit-any
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
      // deno-lint-ignore no-explicit-any
    } catch (e: any) {
      logger.warn(
        `Failed to stop process ${proc.child?.pid}: ${e?.message ?? e}`,
      );
    }
  }
  viteProcesses = [];
  logger.success("Vite dev servers shut down.");
}

/** Dev-server names that map directly onto the shared port table. */
const SERVER_NAME_TO_PORT_NAME: Record<string, DevPortName> = {
  "main": "loader",
  "designs": "designs",
  "modal-child": "modalChild",
  "newtab": "newtab",
  "notes": "notes",
  "settings": "settings",
  "welcome": "welcome",
  "profile-manager": "profileManager",
  "workflow-progress": "workflowProgress",
};

export function getPortFor(serverName: string, portOffset = 0): number {
  const offset = requireDevPortOffset(portOffset);
  const portName = SERVER_NAME_TO_PORT_NAME[serverName];
  if (portName) return devPort(portName, offset);
  return DEV_SERVER.default_port;
}

/** Exposed for tests/tools that need the whole name-keyed port table. */
export { DEV_PORTS };
