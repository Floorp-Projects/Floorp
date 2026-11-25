// SPDX-License-Identifier: MPL-2.0

import { settleWithin } from "./async_utils.ts";

export async function commandOutput(
  cmd: string,
  args: string[],
  input?: string,
  signal?: AbortSignal,
  environment: Record<string, string> = {},
): Promise<string> {
  signal?.throwIfAborted();
  const command = new Deno.Command(cmd, {
    args,
    stdin: input === undefined ? "null" : "piped",
    stdout: "piped",
    stderr: "piped",
    env: { LC_ALL: "C", ...environment },
  });
  let output: Deno.CommandOutput;
  if (signal) {
    const child = command.spawn();
    const status = child.status;
    const cancelled = Promise.withResolvers<never>();
    let killed = false;
    const abort = () => {
      if (!killed) {
        killed = true;
        try {
          child.kill("SIGKILL");
        } catch { /* Already exited. */ }
      }
      cancelled.reject(signal.reason);
    };
    signal.addEventListener("abort", abort, { once: true });
    if (signal.aborted) abort();
    const pending = Promise.resolve().then(() => child.output());
    try {
      output = await Promise.race([pending, cancelled.promise]);
      signal.throwIfAborted();
    } catch (error) {
      abort();
      await settleWithin(Promise.allSettled([pending, status]), 1000);
      throw error;
    } finally {
      signal.removeEventListener("abort", abort);
    }
  } else if (input === undefined) {
    output = await command.output();
  } else {
    const child = command.spawn();
    const writer = child.stdin.getWriter();
    await writer.write(new TextEncoder().encode(input));
    await writer.close();
    output = await child.output();
  }
  signal?.throwIfAborted();
  if (!output.success) {
    throw new Error(
      `${cmd}: ${new TextDecoder().decode(output.stderr).trim()}`,
    );
  }
  return new TextDecoder().decode(output.stdout);
}
