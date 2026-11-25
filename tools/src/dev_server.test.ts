// SPDX-License-Identifier: MPL-2.0

import { assertEquals } from "@std/assert";
import { stop } from "./dev_server.ts";

Deno.test("Vite teardown awaits owned handles and never signals a saved PID, including after early exit", async () => {
  const kill = Deno.kill;
  let rawKills = 0;
  Deno.kill = () => {
    rawKills++;
  };
  try {
    for (const exited of [false, true]) {
      const status = Promise.withResolvers<Deno.CommandStatus>();
      let handleKills = 0;
      let stopped = false;
      const child = {
        pid: 81,
        status: status.promise,
        kill(signal: string) {
          assertEquals(signal, "SIGKILL");
          handleKills++;
          if (exited) throw new Deno.errors.BadResource("already exited");
        },
      } as unknown as Deno.ChildProcess;
      const stopping = stop([child]).then(() => {
        stopped = true;
      });
      await Promise.resolve();
      assertEquals(stopped, false);
      status.resolve({ code: 1, signal: null, success: false });
      await stopping;
      assertEquals(handleKills, 1);
    }
    assertEquals(rawKills, 0);
  } finally {
    Deno.kill = kill;
  }
});
