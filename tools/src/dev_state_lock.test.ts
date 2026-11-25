// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertRejects } from "@std/assert";
import { withDevStateLock } from "./dev_state_lock.ts";

Deno.test("shared dev state serializes independent launches, permits owned children and releases after failure", async () => {
  const root = await Deno.makeTempDir();
  const file = `${root}/lock`;
  const environment = () => {
    const values = new Map<string, string>();
    return {
      get: (key: string) => values.get(key),
      set: (key: string, value: string) => {
        values.set(key, value);
      },
      delete: (key: string) => {
        values.delete(key);
      },
    };
  };
  const firstEnv = environment();
  const secondEnv = environment();
  let release!: () => void;
  const barrier = new Promise<void>((resolve) => {
    release = resolve;
  });
  let entered!: () => void;
  const ready = new Promise<void>((resolve) => {
    entered = resolve;
  });
  const order: string[] = [];
  try {
    const first = withDevStateLock(
      async () => {
        await withDevStateLock(
          () => {
            order.push("child");
            return Promise.resolve();
          },
          undefined,
          file,
          firstEnv,
        );
        entered();
        await barrier;
        order.push("first");
      },
      "first",
      file,
      firstEnv,
    );
    await ready;
    const second = withDevStateLock(
      () => {
        order.push("second");
        return Promise.resolve();
      },
      "second",
      file,
      secondEnv,
    );
    await Promise.resolve();
    assertEquals(order, ["child"]);
    release();
    await Promise.all([first, second]);
    assertEquals(order, ["child", "first", "second"]);
    await assertRejects(() =>
      withDevStateLock(
        () => Promise.reject(new Error("failed")),
        "third",
        file,
        firstEnv,
      )
    );
    await withDevStateLock(
      () => {
        order.push("released");
        return Promise.resolve();
      },
      "fourth",
      file,
      firstEnv,
    );
    assertEquals(order.at(-1), "released");
  } finally {
    release();
    await Deno.remove(root, { recursive: true });
  }
});
