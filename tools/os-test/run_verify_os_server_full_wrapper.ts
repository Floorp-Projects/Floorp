#!/usr/bin/env -S deno run -A
// SPDX-License-Identifier: MPL-2.0

/**
 * Wrapper for tools/os-test/verify_os_server_full.ts (KAN-135).
 *
 * Start modes and the default Floorp OS-Server base URL (explicit
 * --base-url / FLOORP_OS_BASE_URL always win, see
 * tools/src/os_test_options.ts):
 * - default (starts 'deno task feles-build dev'): the dev launch binds the
 *   dev-only OS-Server port 5175 + --port-offset, so the derived default
 *   follows it;
 * - --skip-start (no offset given): verifying an already-running target,
 *   typically an installed Floorp, keeps the packaged production default
 *   58261;
 * - --skip-start with --port-offset: targets a running dev launch with that
 *   offset and derives 5175 + offset.
 *
 * The pre-verify wait is a *reachability* check only: the OS-Server answers
 * /health with 401 when a bearer token is configured, so both 200 and 401
 * count as "server is up". Authenticated checks stay with
 * verify_os_server_full.ts, which is authoritative. This wrapper never
 * reads or changes the OS-Server token. Pass --token-file <path> (or
 * FLOORP_OS_TOKEN_FILE); the verifier alone reads the explicit file. No automatic
 * Home/profile discovery. Python wrappers do not yet support authentication.
 */

import { parseArgs } from "@std/cli/parse-args";

import { sleep } from "../src/async_utils.ts";
import {
  reconcileOsTestClientArgs,
  resolveTokenFile,
  validateAuthenticatedBaseUrl,
} from "../src/os_test_client.ts";
import {
  parseWrapperPortOffsetArg,
  resolveOsVerifyOptions,
} from "../src/os_test_options.ts";
import { resolveAutoDevPortOffset } from "../src/port_offset.ts";

const DEFAULT_START_CMD = ["deno", "task", "feles-build", "dev"];

/**
 * Wait until the OS-Server answers on the given base URL.
 *
 * Reachability semantics (KAN-135): the OS-Server may answer /health with
 * 401 Unauthorized when a bearer token is configured - that still proves a
 * live server on this port. Authenticated readiness is the verifier's job.
 */
async function waitForOsServerReachability(
  baseUrl: string,
  timeoutMs: number,
  intervalMs: number,
): Promise<boolean> {
  const deadline = Date.now() + timeoutMs;
  const url = baseUrl.replace(/\/$/, "") + "/health";

  while (Date.now() < deadline) {
    try {
      const response = await fetch(url, {
        signal: AbortSignal.timeout(Math.max(intervalMs, 2_000)),
        redirect: "error",
      });
      if (response.status === 200 || response.status === 401) {
        if (response.status === 401) {
          console.log(
            "OS-Server reachable (401 bearer-protected /health); " +
              "authenticated checks run in verify_os_server_full.ts.",
          );
        }
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
    })
      .output()
      .catch(() => undefined);
    return;
  }

  try {
    process.kill("SIGTERM");
  } catch {
    // ignore
  }

  await Promise.race([process.status, sleep(10_000)]);

  try {
    process.kill("SIGKILL");
  } catch {
    // ignore
  }
}

function splitVerifierArgs(args: string[]): {
  wrapperArgs: string[];
  verifierArgs: string[];
} {
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
  const split = splitVerifierArgs(argv);
  const { wrapperArgs, verifierArgs, baseUrlFlag, tokenFileFlag } =
    reconcileOsTestClientArgs(split.wrapperArgs, split.verifierArgs);
  const tokenFile = resolveTokenFile(
    tokenFileFlag,
    Deno.env.get("FLOORP_OS_TOKEN_FILE"),
  );

  const parsed = parseArgs(wrapperArgs, {
    string: ["base-url", "timeout", "interval", "cmd", "port-offset"],
    boolean: ["skip-start"],
    default: {
      timeout: "120",
      interval: "2",
      "skip-start": false,
    },
  });

  // --skip-start is needed during offset parsing: 'auto' + --skip-start is
  // rejected before any probing (a free rung has no running OS-Server).
  const skipStart = parsed["skip-start"] === true;
  const spec = parseWrapperPortOffsetArg(parsed["port-offset"], skipStart);
  const portOffset = spec === "auto" ? await resolveAutoDevPortOffset() : spec;

  const resolved = resolveOsVerifyOptions({
    baseUrlFlag,
    envBaseUrl: Deno.env.get("FLOORP_OS_BASE_URL"),
    portOffset,
    skipStart,
  });

  const baseUrl = resolved.baseUrl;
  if (tokenFile !== undefined) validateAuthenticatedBaseUrl(baseUrl);
  const timeoutMs = Math.max(1, Number(parsed.timeout)) * 1000;
  const intervalMs = Math.max(1, Number(parsed.interval)) * 1000;

  Deno.env.set("FLOORP_OS_BASE_URL", baseUrl);

  const startCommand =
    (typeof parsed.cmd === "string" && parsed.cmd.trim().length > 0
      ? parsed.cmd.trim().split(/\s+/)
      : DEFAULT_START_CMD).concat(resolved.startCommandArgs);

  let floorpProcess: Deno.ChildProcess | null = null;

  try {
    if (skipStart) {
      console.log(
        "Skipping Floorp start; verifying already-running OS-Server at " +
          baseUrl + ".",
      );
    } else {
      console.log("Starting Floorp: " + startCommand.join(" "));
      floorpProcess = new Deno.Command(startCommand[0], {
        args: startCommand.slice(1),
        cwd: Deno.cwd(),
        stdin: "null",
        stdout: "inherit",
        stderr: "inherit",
      }).spawn();

      console.log(
        "Waiting for OS-Server reachability (200 or 401 on /health)...",
      );
      if (
        !(await waitForOsServerReachability(baseUrl, timeoutMs, intervalMs))
      ) {
        console.error(
          "OS-Server did not become reachable within timeout.",
        );
        await stopProcess(floorpProcess);
        return 1;
      }

      console.log("OS-Server reachable; waiting 20s for full startup...");
      await sleep(20_000);
    }

    const verifyArgs = [
      "run",
      "-A",
      "tools/os-test/verify_os_server_full.ts",
      "--base-url",
      baseUrl,
      ...(tokenFile === undefined ? [] : ["--token-file", tokenFile]),
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
