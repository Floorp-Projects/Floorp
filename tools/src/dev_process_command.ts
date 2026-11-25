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
    const stdout = child.stdout.getReader();
    const stderr = child.stderr.getReader();
    const collect = async (reader: ReadableStreamDefaultReader<Uint8Array>) => {
      const chunks: Uint8Array[] = [];
      let size = 0;
      try {
        for (;;) {
          const { done, value } = await reader.read();
          if (done) break;
          chunks.push(value);
          size += value.length;
        }
        const bytes = new Uint8Array(size);
        let offset = 0;
        for (const chunk of chunks) {
          bytes.set(chunk, offset);
          offset += chunk.length;
        }
        return bytes;
      } finally {
        reader.releaseLock();
      }
    };
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
    const pending = Promise.all([status, collect(stdout), collect(stderr)])
      .then(([status, stdout, stderr]) => ({ ...status, stdout, stderr }));
    try {
      output = await Promise.race([pending, cancelled.promise]);
      signal.throwIfAborted();
    } catch (error) {
      abort();
      await settleWithin(Promise.allSettled([pending, status]), 1000);
      await settleWithin(
        Promise.allSettled([stdout.cancel(), stderr.cancel()]),
        1000,
      );
      await settleWithin(pending, 1000);
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
