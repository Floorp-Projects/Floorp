// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals, assertRejects } from "@std/assert";
import { MarionetteClient } from "./browser_connector.ts";

Deno.test("Marionette deadlines cover connect, handshake, write and read, close sockets and clear timers", async () => {
  const original = Deno.connect;
  const encoder = new TextEncoder();
  const packet = (value: unknown) => {
    const bytes = encoder.encode(JSON.stringify(value));
    const prefix = encoder.encode(`${bytes.length}:`);
    const result = new Uint8Array(prefix.length + bytes.length);
    result.set(prefix);
    result.set(bytes, prefix.length);
    return result;
  };
  try {
    for (const timeout of [0, -1, NaN, Infinity]) {
      await assertRejects(
        () => MarionetteClient.connect(2828, timeout),
        Error,
        "Invalid Marionette timeout",
      );
    }
    for (
      const scenario of [
        "normal",
        "connect",
        "handshake",
        "write",
        "read",
        "oversize",
        "invalid-prefix",
        "malformed",
        "remote",
        "wrong-id",
        "empty",
      ]
    ) {
      let closed = false;
      let reads = 0;
      let writes = 0;
      let resolveConnect: ((connection: Deno.TcpConn) => void) | undefined;
      let pendingRead: ((value: number | null) => void) | undefined;
      const connection = {
        read(buffer: Uint8Array): Promise<number | null> {
          reads++;
          if (scenario === "handshake" || (scenario === "read" && reads > 2)) {
            return new Promise((resolve) => {
              pendingRead = resolve;
            });
          }
          if (scenario === "empty" && reads > 2) return Promise.resolve(null);
          const bytes = reads === 1
            ? packet({ marionetteProtocol: 3 })
            : reads === 2
            ? packet([1, 0, null, { sessionId: "session-secret-sentinel" }])
            : scenario === "oversize"
            ? encoder.encode("16777217:")
            : scenario === "invalid-prefix"
            ? encoder.encode("123456789")
            : scenario === "malformed"
            ? encoder.encode("1:{")
            : scenario === "remote"
            ? packet([1, 1, {
              error: "invalid argument",
              message: "remote message",
            }, null])
            : packet([1, scenario === "wrong-id" ? 7 : 1, null, {
              value: "héllo",
            }]);
          buffer.set(bytes);
          return Promise.resolve(bytes.length);
        },
        write(bytes: Uint8Array): Promise<number> {
          writes++;
          if (scenario === "write" && writes > 1) return new Promise(() => {});
          return Promise.resolve(bytes.length);
        },
        close() {
          closed = true;
          pendingRead?.(null);
        },
      } as unknown as Deno.TcpConn;
      Deno.connect = (() =>
        scenario === "connect"
          ? new Promise<Deno.TcpConn>((resolve) => {
            resolveConnect = resolve;
          })
          : Promise.resolve(connection)) as unknown as typeof Deno.connect;
      if (scenario === "connect" || scenario === "handshake") {
        await assertRejects(() =>
          MarionetteClient.connect(2828, 20)
        );
        if (resolveConnect) {
          resolveConnect(connection);
          await Promise.resolve();
          await Promise.resolve();
        }
        assert(closed, scenario);
      } else {
        const client = await MarionetteClient.connect(2828, 100);
        if (scenario === "normal") {
          assertEquals(await client.sendWithTimeout("test", {}, 20), {
            value: "héllo",
          });
          assertEquals(closed, false);
          client.abort();
        } else {
          await assertRejects(() => client.sendWithTimeout("test", {}, 20));
          assert(closed, scenario);
          if (
            scenario === "oversize" || scenario === "invalid-prefix"
          ) assertEquals(reads, 3);
        }
      }
    }
  } finally {
    Deno.connect = original;
  }
});
