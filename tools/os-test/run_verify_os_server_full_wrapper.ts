#!/usr/bin/env -S deno run -A
// SPDX-License-Identifier: MPL-2.0

import { parseArgs } from "@std/cli/parse-args";

const DEFAULT_BASE_URL = Deno.env.get("FLOORP_OS_BASE_URL") ?? "http://127.0.0.1:58261";
const DEFAULT_START_CMD = ["deno", "task", "feles-build", "dev"];

function sleep(ms: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

async function waitForHealth(baseUrl: string, timeoutMs: number, intervalMs: number): Promise<boolean> {
  const deadline = Date.now() + timeoutMs;
  const url = `${baseUrl.replace(/\/$/, "")}/health`;

  while (Date.now() < deadline) {
    try {
      const response = await fetch(url, { signal: AbortSignal.timeout(Math.max(intervalMs, 2_000)) });
      if (response.status === 200) {
        return true;
      }
    } catch {
      // retry
    }
    await sleep(intervalMs);
  }

  return false;
}

async function stopProcess(process: Deno.ChildProcess): Promise<void> {
  if (process.pid === undefined) {
    return;
  }

  if (Deno.build.os === "windows") {
    await new Deno.Command("taskkill", {
      args: ["/PID", String(process.pid), "/T", "/F"],
      stdout: "null",
      stderr: "null",
    }).output().catch(() => undefined);
    return;
  }

  try {
    process.kill("SIGTERM");
  } catch {
    // ignore
  }

  await Promise.race([
    process.status,
    sleep(10_000),
  ]);

  try {
    process.kill("SIGKILL");
  } catch {
    // ignore
  }
}

function splitVerifierArgs(args: string[]): { wrapperArgs: string[]; verifierArgs: string[] } {
  const separatorIndex = args.indexOf("--");
  if (separatorIndex < 0) {
    return { wrapperArgs: args, verifierArgs: [] };
  }
  return {
    wrapperArgs: args.slice(0, separatorIndex),
    verifierArgs: args.slice(separatorIndex + 1),
  };
}

export async function main(argv: string[] = Deno.args): Promise<number> {
  const { wrapperArgs, verifierArgs } = splitVerifierArgs(argv);

  const parsed = parseArgs(wrapperArgs, {
    string: ["base-url", "timeout", "interval", "cmd"],
    boolean: ["skip-start"],
    default: {
      "base-url": DEFAULT_BASE_URL,
      timeout: "120",
      interval: "2",
      "skip-start": false,
    },
  });

  const baseUrl = String(parsed["base-url"]);
  const timeoutMs = Math.max(1, Number(parsed.timeout)) * 1000;
  const intervalMs = Math.max(1, Number(parsed.interval)) * 1000;
  const skipStart = parsed["skip-start"] === true;

  Deno.env.set("FLOORP_OS_BASE_URL", baseUrl);

  const startCommand = typeof parsed.cmd === "string" && parsed.cmd.trim().length > 0
    ? parsed.cmd.trim().split(/\s+/)
    : DEFAULT_START_CMD;

  let floorpProcess: Deno.ChildProcess | null = null;

  try {
    if (skipStart) {
      console.log("Skipping Floorp start; expecting server already running.");
    } else {
      console.log(`Starting Floorp: ${startCommand.join(" ")}`);
      floorpProcess = new Deno.Command(startCommand[0], {
        args: startCommand.slice(1),
        cwd: Deno.cwd(),
        stdin: "null",
        stdout: "inherit",
        stderr: "inherit",
      }).spawn();

      console.log("Waiting for OS API health endpoint...");
      if (!(await waitForHealth(baseUrl, timeoutMs, intervalMs))) {
        console.error("OS API server did not become healthy within timeout.");
        await stopProcess(floorpProcess);
        return 1;
      }

      console.log("OS API health is ready; waiting 20s for full startup...");
      await sleep(20_000);
    }

    const verifyArgs = [
      "run",
      "-A",
      "tools/os-test/verify_os_server_full.ts",
      "--base-url",
      baseUrl,
      ...verifierArgs,
    ];

    const verify = new Deno.Command(Deno.execPath(), {
      args: verifyArgs,
      cwd: Deno.cwd(),
      stdout: "inherit",
      stderr: "inherit",
    }).spawn();

    const status = await verify.status;
    return status.code;
  } finally {
    if (floorpProcess) {
      console.log("Stopping Floorp...");
      await stopProcess(floorpProcess);
    }
  }
}

if (import.meta.main) {
  const code = await main(Deno.args);
  Deno.exit(code);
}
