// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals, assertRejects } from "@std/assert";
import { MarionetteClient } from "./browser_connector.ts";
import type { MarionetteStage } from "./browser_connector.types.ts";

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
        "normal-observer-failure",
        "connect",
        "connect-cancel",
        "handshake",
        "handshake-cancel",
        "session-cancel",
        "retry-cancel",
        "write",
        "read",
        "oversize",
        "invalid-prefix",
        "malformed",
        "malformed-error",
        "truncated",
        "remote",
        "wrong-id",
        "empty",
      ]
    ) {
      let closed = false;
      const controller = new AbortController();
      const cancelling = scenario.endsWith("-cancel");
      const stages: MarionetteStage[] = [];
      const onStage = (stage: MarionetteStage) => {
        stages.push(stage);
        if (
          cancelling &&
          stage ===
            (scenario === "session-cancel"
              ? "command"
              : scenario === "handshake-cancel"
              ? "handshake"
              : "connect")
        ) {
          setTimeout(() => controller.abort(new Error("startup-cancelled")), 1);
        }
        if (scenario === "normal-observer-failure") {
          throw new Error("observer sentinel");
        }
      };
      let reads = 0;
      let writes = 0;
      let resolveConnect: ((connection: Deno.TcpConn) => void) | undefined;
      let pendingRead: ((value: number | null) => void) | undefined;
      const connection = {
        read(buffer: Uint8Array): Promise<number | null> {
          reads++;
          if (
            scenario.startsWith("handshake") ||
            (scenario === "session-cancel" && reads === 2) ||
            (scenario === "read" && reads > 2)
          ) {
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
            : scenario === "malformed-error"
            ? packet([1, 1, { error: "invalid argument", message: 42 }, null])
            : scenario === "truncated"
            ? packet([1, 1])
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
        scenario.startsWith("connect")
          ? new Promise<Deno.TcpConn>((resolve) => {
            resolveConnect = resolve;
          })
          : scenario === "retry-cancel"
          ? Promise.reject(new Error("refused"))
          : Promise.resolve(connection)) as unknown as typeof Deno.connect;
      if (scenario === "connect" || scenario === "handshake" || cancelling) {
        await assertRejects(() =>
          MarionetteClient.connect(
            2828,
            cancelling ? 10000 : 20,
            onStage,
            controller.signal,
          )
        );
        if (resolveConnect) {
          resolveConnect(connection);
          await Promise.resolve();
          await Promise.resolve();
        }
        assertEquals(closed, scenario !== "retry-cancel", scenario);
        assertEquals(stages[0], "connect");
        assertEquals(
          stages.includes("handshake"),
          scenario.startsWith("handshake") || scenario === "session-cancel",
        );
        assertEquals(stages.includes("timeout"), !cancelling);
      } else {
        const client = await MarionetteClient.connect(2828, 100, onStage);
        assertEquals(stages, ["connect", "handshake", "command"]);
        if (scenario.startsWith("normal")) {
          assertEquals(await client.sendWithTimeout("test", {}, 20), {
            value: "héllo",
          });
          assertEquals(closed, false);
          assertEquals(stages, ["connect", "handshake", "command", "command"]);
          client.abort();
        } else {
          await assertRejects(() =>
            client.sendWithTimeout("test", {}, 20)
          );
          if (scenario === "remote") {
            assertEquals(closed, false);
            client.abort();
          } else assert(closed, scenario);
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

Deno.test("valid command errors preserve the connection for ReleaseActions and preference cleanup", async () => {
  const original = Deno.connect;
  let reads = 0;
  let closed = false;
  const commands: string[] = [];
  const connection = {
    read(buffer: Uint8Array) {
      const response = reads === 0
        ? { marionetteProtocol: 3 }
        : reads === 2
        ? [1, 1, { error: "invalid argument", message: "bad action" }, null]
        : [1, reads - 1, null, {}];
      reads++;
      const text = JSON.stringify(response);
      const bytes = new TextEncoder().encode(
        `${new TextEncoder().encode(text).length}:${text}`,
      );
      buffer.set(bytes);
      return Promise.resolve(bytes.length);
    },
    write(bytes: Uint8Array) {
      const text = new TextDecoder().decode(bytes);
      commands.push(JSON.parse(text.slice(text.indexOf(":") + 1))[2]);
      return Promise.resolve(bytes.length);
    },
    close() {
      closed = true;
    },
  } as unknown as Deno.TcpConn;
  Deno.connect =
    (() => Promise.resolve(connection)) as unknown as typeof Deno.connect;
  try {
    const client = await MarionetteClient.connect(2828, 100);
    await assertRejects(
      () => client.send("WebDriver:PerformActions"),
      Error,
      "invalid argument",
    );
    assertEquals(closed, false);
    await client.send("WebDriver:ReleaseActions");
    await client.executeScript("Services.prefs.clearUserPref('test.pref');");
    await client.close();
    assertEquals(commands, [
      "WebDriver:NewSession",
      "WebDriver:PerformActions",
      "WebDriver:ReleaseActions",
      "WebDriver:ExecuteScript",
      "WebDriver:DeleteSession",
    ]);
    assertEquals(closed, true);
  } finally {
    Deno.connect = original;
  }
});
