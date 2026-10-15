// SPDX-License-Identifier: MPL-2.0

import {
  assert,
  assertEquals,
  assertRejects,
  assertStrictEquals,
  assertThrows,
} from "@std/assert";
import {
  finishBrowserCleanup,
  MarionetteClient,
  parseMarionettePort,
  withBrowser,
} from "./browser_connector.ts";

const encoder = new TextEncoder();
const decoder = new TextDecoder();
const hello = { applicationType: "gecko", marionetteProtocol: 3 };

function frame(value: unknown): Uint8Array {
  const json = encoder.encode(JSON.stringify(value));
  const prefix = encoder.encode(String(json.length) + ":");
  const packet = new Uint8Array(prefix.length + json.length);
  packet.set(prefix);
  packet.set(json, prefix.length);
  return packet;
}

/** Host I/O fault fixture. It cannot launch or qualify a browser. */
class FixtureConnection {
  incoming: number[] = [];
  outgoing: number[] = [];
  commands: unknown[][] = [];
  closed = false;
  closeCalls = 0;
  dialCalls = 0;
  writeCalls = 0;
  activeReads = 0;
  activeWrites = 0;
  maxReadRequest = 0;
  readLimit = 4;
  writeLimit = 3;
  readProgress?: number;
  writeProgress?: number;
  stallWrites = false;
  eof = false;
  rawResponse?: Uint8Array;
  closeFailure?: Error;
  writeFailure?: Error;
  #wakeRead?: () => void;
  #rejectRead?: (error: Error) => void;
  #rejectWrite?: (error: Error) => void;
  timers = new Set<ReturnType<typeof setTimeout>>();
  response: (packet: unknown[]) => unknown = (packet) => [
    1,
    packet[1],
    null,
    packet[2] === "WebDriver:NewSession"
      ? { sessionId: "host-fixture-not-native-qualified" }
      : packet[2] === "WebDriver:DeleteSession"
      ? null
      : { value: "ok" },
  ];

  constructor(greeting: unknown = hello) {
    this.feed(frame(greeting));
  }
  asTcp(): Deno.TcpConn {
    return this as unknown as Deno.TcpConn;
  }
  feed(bytes: Uint8Array): void {
    this.incoming.push(...bytes);
    this.#wakeRead?.();
  }
  later(milliseconds: number, operation: () => void): void {
    const timer = setTimeout(() => {
      this.timers.delete(timer);
      operation();
    }, milliseconds);
    this.timers.add(timer);
  }
  async read(target: Uint8Array): Promise<number | null> {
    this.activeReads++;
    this.maxReadRequest = Math.max(this.maxReadRequest, target.length);
    try {
      if (this.closed) throw new Error("Fixture resource closed");
      if (this.readProgress !== undefined) return this.readProgress;
      const pull = () => {
        const count = Math.min(
          target.length,
          this.readLimit,
          this.incoming.length,
        );
        for (let i = 0; i < count; i++) target[i] = this.incoming.shift()!;
        return count;
      };
      if (this.incoming.length) return pull();
      if (this.eof) return null;
      return await new Promise<number | null>((resolve, reject) => {
        this.#rejectRead = reject;
        this.#wakeRead = () => {
          if (this.incoming.length) resolve(pull());
        };
      });
    } finally {
      this.activeReads--;
      this.#wakeRead = undefined;
      this.#rejectRead = undefined;
    }
  }
  async write(bytes: Uint8Array): Promise<number> {
    this.activeWrites++;
    this.writeCalls++;
    try {
      if (this.closed) throw new Error("Fixture resource closed");
      if (this.writeFailure) throw this.writeFailure;
      if (this.writeProgress !== undefined) return this.writeProgress;
      if (this.stallWrites) {
        return await new Promise<number>((_resolve, reject) => {
          this.#rejectWrite = reject;
        });
      }
      const count = Math.min(this.writeLimit, bytes.length);
      this.outgoing.push(...bytes.subarray(0, count));
      while (true) {
        const colon = this.outgoing.indexOf(58);
        if (colon < 0) break;
        const length = Number(
          decoder.decode(Uint8Array.from(this.outgoing.slice(0, colon))),
        );
        if (this.outgoing.length < colon + 1 + length) break;
        const packet: unknown = JSON.parse(
          decoder.decode(
            Uint8Array.from(
              this.outgoing.splice(0, colon + 1 + length).slice(colon + 1),
            ),
          ),
        );
        assert(Array.isArray(packet));
        this.commands.push(packet);
        if (this.rawResponse) {
          const raw = this.rawResponse;
          this.rawResponse = undefined;
          this.feed(raw);
        } else {
          const response = this.response(packet);
          if (response !== undefined) this.feed(frame(response));
        }
      }
      return count;
    } finally {
      this.activeWrites--;
      this.#rejectWrite = undefined;
    }
  }
  close(): void {
    this.closed = true;
    this.closeCalls++;
    for (const timer of this.timers) clearTimeout(timer);
    this.timers.clear();
    this.#rejectRead?.(new Error("Fixture resource closed"));
    this.#rejectWrite?.(new Error("Fixture resource closed"));
    if (this.closeFailure) throw this.closeFailure;
  }
}

async function connect(fixture: FixtureConnection): Promise<MarionetteClient> {
  return await MarionetteClient.connect(2828, {
    startupTimeoutMilliseconds: 1000,
    dial: (options) => {
      fixture.dialCalls++;
      assertEquals(options.hostname, "127.0.0.1");
      assertEquals(options.port, 2828);
      assert(options.signal);
      return Promise.resolve(fixture.asTcp());
    },
  });
}
function noPending(fixture: FixtureConnection): void {
  assertEquals(fixture.activeReads, 0);
  assertEquals(fixture.activeWrites, 0);
  assertEquals(fixture.timers.size, 0);
}

Deno.test("Marionette completes partial UTF-8 writes and fragmented ordered frames", async () => {
  const fixture = new FixtureConnection();
  const client = await connect(fixture);
  fixture.response = (
    packet,
  ) => [
    1,
    packet[1],
    null,
    packet[2] === "WebDriver:DeleteSession" ? null : { value: "日本語" },
  ];
  assertEquals(
    await client.executeScript<string>("return '日本語';", ["é"]),
    "日本語",
  );
  await client.close();
  await client.close();
  assertEquals(fixture.commands.map((packet) => packet[1]), [0, 1, 2]);
  assertEquals(fixture.commands[1][3], {
    script: "return '日本語';",
    args: ["é"],
  });
  assert(fixture.writeCalls > fixture.commands.length);
  assertEquals(fixture.outgoing, []);
  assertEquals(fixture.closeCalls, 1);
  noPending(fixture);
});

Deno.test("Marionette rejects wrong, boolean and string response IDs without retry", async () => {
  for (const id of [0, 2, true, "1"]) {
    const fixture = new FixtureConnection();
    const client = await connect(fixture);
    fixture.response = () => [1, id, null, { value: "wrong evaluation" }];
    await assertRejects(() => client.getTitle(), Error, "identity mismatch");
    const count = fixture.writeCalls;
    await assertRejects(() => client.getTitle(), Error, "closed");
    await client.close();
    assertEquals(fixture.writeCalls, count);
    assertEquals(fixture.dialCalls, 1);
    noPending(fixture);
  }
});

Deno.test("Marionette rejects malformed response envelopes and errors", async () => {
  for (
    const value of [[1, 1, null, {}, "extra"], [0, 1, null, {}], [
      1,
      1,
      false,
      {},
    ], {}]
  ) {
    const fixture = new FixtureConnection();
    const client = await connect(fixture);
    fixture.response = () => value;
    await assertRejects(() => client.send("Fixture:Observe"), Error);
    assert(fixture.closed);
    await client.close();
    noPending(fixture);
  }
});

Deno.test("Marionette rejects noncanonical and oversized frame prefixes before body allocation", async () => {
  for (
    const prefix of [
      "0:",
      "01:",
      ":",
      "1x:",
      "-1:",
      "1e3:",
      "9999999999:",
      "16777217:",
    ]
  ) {
    const fixture = new FixtureConnection();
    const client = await connect(fixture);
    fixture.maxReadRequest = 0;
    fixture.rawResponse = encoder.encode(prefix);
    await assertRejects(() => client.send("Fixture:Observe"), Error);
    assertEquals(fixture.maxReadRequest, 1);
    await client.close();
    noPending(fixture);
  }
});

Deno.test("Marionette rejects malformed UTF-8 and JSON frames", async () => {
  for (
    const raw of [new Uint8Array([50, 58, 255, 255]), encoder.encode("1:{")]
  ) {
    const fixture = new FixtureConnection();
    const client = await connect(fixture);
    fixture.rawResponse = raw;
    await assertRejects(() => client.send("Fixture:Observe"), Error);
    await client.close();
    noPending(fixture);
  }
});

Deno.test("Marionette rejects EOF during a prefix or JSON body", async () => {
  for (const raw of ["12", "10:{"]) {
    const fixture = new FixtureConnection();
    const client = await connect(fixture);
    fixture.rawResponse = encoder.encode(raw);
    fixture.eof = true;
    await assertRejects(() => client.send("Fixture:Observe"), Error, "ended");
    await client.close();
    noPending(fixture);
  }
});

Deno.test("Marionette deadline closes and reaps a stalled original read", async () => {
  const fixture = new FixtureConnection();
  const client = await connect(fixture);
  fixture.response = () => undefined;
  await assertRejects(
    () => client.sendWithTimeout("Fixture:Observe", {}, 15),
    Error,
    "timed out",
  );
  assert(fixture.closed);
  await client.close();
  noPending(fixture);
  assertEquals(fixture.dialCalls, 1);
});

Deno.test("Marionette deadline includes a stalled write and reaps it", async () => {
  const fixture = new FixtureConnection();
  const client = await connect(fixture);
  fixture.stallWrites = true;
  await assertRejects(
    () => client.sendWithTimeout("Fixture:Observe", {}, 15),
    Error,
    "timed out",
  );
  await client.close();
  noPending(fixture);
});

Deno.test("Marionette rejects invalid read and write progress", async () => {
  for (const mode of ["read", "write"]) {
    for (const count of [0, -1, 0.5, 100_000_000]) {
      const fixture = new FixtureConnection();
      const client = await connect(fixture);
      if (mode === "read") fixture.readProgress = count;
      else fixture.writeProgress = count;
      await assertRejects(
        () => client.send("Fixture:Observe"),
        Error,
        "progress",
      );
      await client.close();
      noPending(fixture);
    }
  }
});

Deno.test("Marionette rejects a foreign greeting and cleans its first connection", async () => {
  for (
    const greeting of [{ applicationType: "gecko", marionetteProtocol: 2 }, {
      applicationType: "other",
      marionetteProtocol: 3,
    }, null]
  ) {
    const fixture = new FixtureConnection(greeting);
    await assertRejects(() => connect(fixture), Error, "greeting");
    assertEquals(fixture.dialCalls, 1);
    assertEquals(fixture.commands.length, 0);
    assertEquals(fixture.closeCalls, 1);
    noPending(fixture);
  }
});

Deno.test("Marionette preserves the original NewSession rejection", async () => {
  const fixture = new FixtureConnection();
  fixture.response = (
    packet,
  ) => [1, packet[1], {
    error: "session not created",
    message: "Original session failure",
  }, null];
  await assertRejects(
    () => connect(fixture),
    Error,
    "Original session failure",
  );
  assertEquals(fixture.dialCalls, 1);
  assertEquals(fixture.closeCalls, 1);
  noPending(fixture);
});

Deno.test("Marionette requires a real string session identity", async () => {
  for (const session of [{}, { sessionId: "" }, { sessionId: true }]) {
    const fixture = new FixtureConnection();
    fixture.response = (packet) => [1, packet[1], null, session];
    await assertRejects(() => connect(fixture), Error, "session identity");
    assertEquals(fixture.closeCalls, 1);
    noPending(fixture);
  }
});

Deno.test("Marionette does not retry the original TCP failure", async () => {
  const original = new Error("Original connection denied");
  let attempts = 0;
  let observed: unknown;
  try {
    await MarionetteClient.connect(2828, {
      dial: () => {
        attempts++;
        return Promise.reject(original);
      },
    });
  } catch (error) {
    observed = error;
  }
  assertStrictEquals(observed, original);
  assertEquals(attempts, 1);
});

Deno.test("Marionette startup uses one shared deadline through greeting and session", async () => {
  const fixture = new FixtureConnection();
  fixture.response = () => undefined;
  const start = performance.now();
  await assertRejects(
    () =>
      MarionetteClient.connect(2828, {
        startupTimeoutMilliseconds: 30,
        dial: async () => {
          await new Promise((resolve) => setTimeout(resolve, 15));
          return fixture.asTcp();
        },
      }),
    Error,
    "timed out",
  );
  assert(performance.now() - start < 150);
  assertEquals(fixture.closeCalls, 1);
  noPending(fixture);
});

Deno.test("Marionette late TCP completion is closed and never creates a new session", async () => {
  const fixture = new FixtureConnection();
  const late = new Promise<Deno.TcpConn>((resolve) =>
    setTimeout(() => resolve(fixture.asTcp()), 30)
  );
  await assertRejects(
    () =>
      MarionetteClient.connect(2828, {
        startupTimeoutMilliseconds: 5,
        dial: () => late,
      }),
    Error,
    "timed out",
  );
  await late;
  await Promise.resolve();
  assertEquals(fixture.closeCalls, 1);
  assertEquals(fixture.commands.length, 0);
  noPending(fixture);
});

Deno.test("Marionette rejects an oversized command before its first byte", async () => {
  const fixture = new FixtureConnection();
  const client = await connect(fixture);
  const count = fixture.writeCalls;
  await assertRejects(
    () => client.send("Fixture:Observe", { value: "x".repeat(1024 * 1024) }),
    Error,
    "byte bound",
  );
  assertEquals(fixture.writeCalls, count);
  await client.close();
  noPending(fixture);
});

Deno.test("Marionette rejects a completion past its deadline before a delayed timer fires", async () => {
  const fixture = new FixtureConnection();
  let now = 0;
  const client = await MarionetteClient.connect(2828, {
    now: () => now,
    dial: () => Promise.resolve(fixture.asTcp()),
  });
  fixture.response = (packet) => {
    now += 31_000;
    return [1, packet[1], null, { value: "Late success must be rejected" }];
  };
  await assertRejects(() => client.getTitle(), Error, "after its deadline");
  await assertRejects(() => client.getTitle(), Error, "closed");
  await client.close();
  assertEquals(fixture.closeCalls, 1);
  noPending(fixture);
});

Deno.test("Marionette rejects concurrent commands without consuming the next ID", async () => {
  const fixture = new FixtureConnection();
  const client = await connect(fixture);
  fixture.response = (packet) =>
    packet[2] === "WebDriver:DeleteSession"
      ? [1, packet[1], null, null]
      : undefined;
  const pending = client.sendWithTimeout("Fixture:Observe", {}, 1000);
  await assertRejects(
    () => client.send("Fixture:Other"),
    Error,
    "already pending",
  );
  fixture.feed(frame([1, 1, null, { value: "original" }]));
  assertEquals(await pending, { value: "original" });
  await client.close();
  assertEquals(fixture.commands.map((packet) => packet[1]), [0, 1, 2]);
  noPending(fixture);
});

Deno.test("Marionette close aborts and reaps an active command", async () => {
  const fixture = new FixtureConnection();
  const client = await connect(fixture);
  fixture.response = () => undefined;
  const observed = client.send("Fixture:Observe").then(
    () => "unexpected",
    () => "failed",
  );
  await new Promise((resolve) => setTimeout(resolve, 0));
  await assertRejects(() => client.close(), Error, "command pending");
  assertEquals(await observed, "failed");
  noPending(fixture);
});

Deno.test("Marionette exposes DeleteSession or transport cleanup failures", async () => {
  for (const closeFailure of [false, true]) {
    const fixture = new FixtureConnection();
    const client = await connect(fixture);
    if (closeFailure) {
      fixture.closeFailure = new Error("Original close failure");
    } else {fixture.response = (
        packet,
      ) => [1, packet[1], {
        error: "unknown error",
        message: "Original DeleteSession failure",
      }, null];}
    await assertRejects(
      () => client.close(),
      Error,
      closeFailure
        ? "Original close failure"
        : "Original DeleteSession failure",
    );
    assertEquals(fixture.closeCalls, 1);
    noPending(fixture);
  }
});

Deno.test("Marionette preserves first I/O failure when resource close also fails", async () => {
  const fixture = new FixtureConnection();
  const client = await connect(fixture);
  const original = new Error("Original write failure");
  fixture.writeFailure = original;
  fixture.closeFailure = new Error("Secondary close failure");
  let observed: unknown;
  try {
    await client.send("Fixture:Observe");
  } catch (error) {
    observed = error;
  }
  assertStrictEquals(observed, original);
  await assertRejects(() => client.close(), Error, "Secondary close failure");
  noPending(fixture);
});

Deno.test("withBrowser preserves callback failure while retaining cleanup diagnostics", async () => {
  const fixture = new FixtureConnection();
  fixture.response = (packet) =>
    packet[2] === "WebDriver:NewSession"
      ? [1, packet[1], null, { sessionId: "host-fixture" }]
      : [1, packet[1], {
        error: "unknown error",
        message: "Secondary delete failure",
      }, null];
  const original = new Error("Original callback failure");
  const notices: unknown[][] = [];
  const saved = console.error;
  console.error = (...args: unknown[]) => {
    notices.push(args);
  };
  let observed: unknown;
  try {
    await withBrowser(() => Promise.reject(original), {
      port: 2828,
      dial: () => Promise.resolve(fixture.asTcp()),
    });
  } catch (error) {
    observed = error;
  } finally {
    console.error = saved;
  }
  assertStrictEquals(observed, original);
  assertEquals(notices.length, 1);
  assertEquals(fixture.closeCalls, 1);
  noPending(fixture);
});

Deno.test("gesture cleanup preserves even primitive first failures when diagnostics throw", async () => {
  const saved = console.error;
  console.error = () => {
    throw new Error("Broken diagnostic sink");
  };
  try {
    for (
      const original of [null, undefined, new Error("First gesture failure")]
    ) {
      const fixture = new FixtureConnection();
      const client = await connect(fixture);
      fixture.closeFailure = new Error("Secondary resource close failure");
      const errors: unknown[] = [new Error("Earlier restoration failure")];
      let rejected = false;
      let observed: unknown;
      try {
        await finishBrowserCleanup(
          client,
          { failed: true, error: original },
          errors,
        );
      } catch (error) {
        rejected = true;
        observed = error;
      }
      assert(rejected);
      assertStrictEquals(observed, original);
      assertEquals(errors.length, 2);
      assertStrictEquals(errors[1], fixture.closeFailure);
      noPending(fixture);
    }
  } finally {
    console.error = saved;
  }
});

Deno.test("gesture cleanup fails an otherwise successful run and retains every cleanup error", async () => {
  const fixture = new FixtureConnection();
  const client = await connect(fixture);
  const restoreError = new Error("State restoration failed");
  fixture.closeFailure = new Error("Resource close failed");
  const error = await assertRejects(
    () => finishBrowserCleanup(client, { failed: false }, [restoreError]),
    AggregateError,
    "cleanup failed",
  );
  assertStrictEquals(error.errors[0], restoreError);
  assertStrictEquals(error.errors[1], fixture.closeFailure);
  noPending(fixture);
});

Deno.test("gesture cleanup succeeds only after session and resource closure", async () => {
  const fixture = new FixtureConnection();
  const client = await connect(fixture);
  await finishBrowserCleanup(client, { failed: false });
  assertEquals(fixture.commands.map((packet) => packet[2]), [
    "WebDriver:NewSession",
    "WebDriver:DeleteSession",
  ]);
  assertEquals(fixture.closeCalls, 1);
  noPending(fixture);
});

Deno.test("Marionette validates ports and deadlines before any connection or command", async () => {
  for (
    const value of ["2828junk", "02828", "0", "", "65536", -1, 1.5, true, null]
  ) {
    assertThrows(() => parseMarionettePort(value), Error, "Invalid");
  }
  assertEquals(parseMarionettePort("2828"), 2828);
  for (const milliseconds of [0, -1, 0.5, Infinity, 15001]) {
    let calls = 0;
    await assertRejects(
      () =>
        MarionetteClient.connect(2828, {
          startupTimeoutMilliseconds: milliseconds,
          dial: () => {
            calls++;
            throw new Error("Must not connect");
          },
        }),
      Error,
      "fixed bound",
    );
    assertEquals(calls, 0);
  }
  const fixture = new FixtureConnection();
  const client = await connect(fixture);
  const count = fixture.writeCalls;
  for (const milliseconds of [0, -1, 0.5, Infinity, 30001]) {
    await assertRejects(
      () => client.sendWithTimeout("Fixture:Observe", {}, milliseconds),
      Error,
      "fixed bound",
    );
  }
  assertEquals(fixture.writeCalls, count);
  await client.close();
  noPending(fixture);
});
