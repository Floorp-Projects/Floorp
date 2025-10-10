// SPDX-License-Identifier: MPL-2.0

import * as path from "@std/path";
import { PROJECT_ROOT, DEV_SERVER } from "./defines.ts";
import { Logger } from "./utils.ts";

const logger = new Logger("dev-server");
let viteProcesses: Array<{ pid?: number }> = [];

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
  ];

  // Ensure logs directory exists
  const logsDir = path.join(PROJECT_ROOT, "logs");
  try {
    Deno.mkdirSync(logsDir, { recursive: true });
  } catch (e: any) {
    logger.warn(`Failed to create logs dir ${logsDir}: ${e?.message ?? e}`);
  }

  for (const server of servers) {
    const port = getPortFor(server.name).toString();
    // Run Vite via Deno's npm compatibility (Deno-only)
    const cmd = "deno";
    const args = ["run", "-A", "npm:vite", "--port", port];

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

    viteProcesses.push(child);

    logger.info(
      `Started Vite dev server for ${server.name} with PID: ${child.pid}, logging to ${logFilePath}`,
    );
  }

  logger.info("All Vite dev servers started.");
  // Wait a short time for servers to start (parity with original sleep 5)
  await new Promise((res) => setTimeout(res, 5000));

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
  for (const child of viteProcesses) {
    try {
      if (child?.pid) {
        try {
          Deno.kill(child.pid);
        } catch {
          // ignore
        }
      }
    } catch (e: any) {
      logger.warn(`Failed to stop process ${child?.pid}: ${e?.message ?? e}`);
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
    default:
      return DEV_SERVER.default_port;
  }
}
