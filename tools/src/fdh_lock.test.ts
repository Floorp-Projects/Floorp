// SPDX-License-Identifier: MPL-2.0

/** Host tests for the fdh file lock (KAN-129). */

import { assertEquals, assertRejects } from "@std/assert";
import * as path from "@std/path";
import { lockPathFor, withFileLock } from "./fdh_lock.ts";
import {
  claimGuiLease,
  createGuiLease,
  readGuiLease,
  writeGuiLease,
} from "./fdh_gui.ts";

async function withTempDir(fn: (dir: string) => Promise<void>): Promise<void> {
  const dir = await Deno.makeTempDir({ prefix: "fdh-lock-test-" });
  try {
    await fn(dir);
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
}

const tick = (ms: number) => new Promise((r) => setTimeout(r, ms));

/** A read-modify-write that yields between the read and the write. */
async function increment(file: string): Promise<void> {
  const value = Number(Deno.readTextFileSync(file));
  await tick(30);
  Deno.writeTextFileSync(file, String(value + 1));
}

Deno.test("without the lock, two concurrent read-modify-writes lose an update", async () => {
  await withTempDir(async (dir) => {
    const file = path.join(dir, "counter");
    Deno.writeTextFileSync(file, "0");
    await Promise.all([increment(file), increment(file)]);
    // This is the race the lock exists to prevent.
    assertEquals(Deno.readTextFileSync(file), "1");
  });
});

Deno.test("withFileLock serialises concurrent read-modify-writes", async () => {
  await withTempDir(async (dir) => {
    const file = path.join(dir, "counter");
    Deno.writeTextFileSync(file, "0");
    const lock = lockPathFor(file);
    await Promise.all(
      Array.from(
        { length: 5 },
        () => withFileLock(lock, () => increment(file)),
      ),
    );
    assertEquals(Deno.readTextFileSync(file), "5");
  });
});

Deno.test("withFileLock releases the lock when the callback throws", async () => {
  await withTempDir(async (dir) => {
    const lock = path.join(dir, "nested", "ledger.json.lock");
    await assertRejects(
      () =>
        withFileLock(lock, () => {
          throw new Error("claim rejected");
        }),
      Error,
      "claim rejected",
    );
    // The parent directory was created, and the lock can be taken again.
    assertEquals(await withFileLock(lock, () => "again"), "again");
  });
});

Deno.test("two locked GUI claims: exactly one agent gets the lease", async () => {
  await withTempDir(async (dir) => {
    const file = path.join(dir, "gui-desktop-lease.json");
    writeGuiLease(file, createGuiLease());
    const claim = (agent: string) =>
      withFileLock(lockPathFor(file), async () => {
        const lease = readGuiLease(file);
        await tick(30);
        claimGuiLease(lease, agent, `task-${agent}`, dir);
        writeGuiLease(file, lease);
      });
    const results = await Promise.allSettled([
      claim("agent-a"),
      claim("agent-b"),
    ]);
    assertEquals(results.filter((r) => r.status === "fulfilled").length, 1);
    assertEquals(results.filter((r) => r.status === "rejected").length, 1);
  });
});
