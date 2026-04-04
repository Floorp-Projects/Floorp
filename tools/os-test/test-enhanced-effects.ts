#!/usr/bin/env -S deno run -A
// SPDX-License-Identifier: MPL-2.0

import { main as runVerify } from "./verify_os_server_full.ts";

function hasFlag(args: string[], flag: string): boolean {
  return args.includes(flag) || args.some((arg) => arg.startsWith(`${flag}=`));
}

async function main(argv: string[] = Deno.args): Promise<number> {
  const nextArgs = [...argv];
  if (!hasFlag(nextArgs, "--quick")) {
    nextArgs.push("--quick");
  }
  if (!hasFlag(nextArgs, "--skip-browser-info")) {
    nextArgs.push("--skip-browser-info");
  }
  if (!hasFlag(nextArgs, "--skip-workspaces")) {
    nextArgs.push("--skip-workspaces");
  }

  // Keep tabs + scraper coverage enabled because visual effects are triggered there.
  return await runVerify(nextArgs);
}

if (import.meta.main) {
  const code = await main(Deno.args);
  Deno.exit(code);
}
