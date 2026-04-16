#!/usr/bin/env -S deno run -A
// SPDX-License-Identifier: MPL-2.0

import { main as runVerify } from "./verify_os_server_full.ts";
import { withDefaultFlags } from "./test_wrapper_utils.ts";

const DEFAULT_FLAGS = [
  "--quick",
  "--skip-browser-info",
  "--skip-workspaces",
  "--skip-concurrency",
];

async function main(argv: string[] = Deno.args): Promise<number> {
  const nextArgs = withDefaultFlags(argv, DEFAULT_FLAGS);
  return await runVerify(nextArgs);
}

if (import.meta.main) {
  const code = await main(Deno.args);
  Deno.exit(code);
}
