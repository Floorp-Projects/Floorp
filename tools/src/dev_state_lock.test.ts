// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertRejects } from "@std/assert";
import { withDevStateLock } from "./dev_state_lock.ts";

Deno.test("delegated NotFound propagates once without reacquiring the parent's lock", async () => {
  const root = await Deno.makeTempDir();
  const path = `${root}/lock`;
  let calls = 0;
  const values = new Map<string, string>();
  const environment = {
    get: (key: string) => values.get(key),
    set: (key: string, value: string) => {
      values.set(key, value);
    },
    delete: (key: string) => {
      values.delete(key);
    },
  };
  let timer: ReturnType<typeof setTimeout> | undefined;
  try {
    await withDevStateLock(
      async () => {
        await assertRejects(
          () =>
            Promise.race([
              withDevStateLock(
                () => {
                  calls++;
                  return Promise.reject(
                    new Deno.errors.NotFound("operation missing"),
                  );
                },
                undefined,
                path,
                environment,
              ),
              new Promise<never>((_, reject) => {
                timer = setTimeout(
                  () => reject(new Error("nested deadlock")),
                  1000,
                );
              }),
            ]),
          Deno.errors.NotFound,
          "operation missing",
        );
      },
      "parent",
      path,
      environment,
    );
    assertEquals(calls, 1);
    await withDevStateLock(() => Promise.resolve(), "next", path, environment);
  } finally {
    clearTimeout(timer);
    await Deno.remove(root, { recursive: true });
  }
});

Deno.test({
  name:
    "an exited lock owner's marker never delegates entry without a live kernel lock",
  ignore: Deno.permissions.querySync({ name: "run", command: Deno.execPath() })
    .state !== "granted",
  async fn() {
    const root = await Deno.makeTempDir();
    const path = `${root}/lock`;
    const token = Deno.env.get("FLOORP_DEV_STATE_TOKEN");
    try {
      const output = await new Deno.Command(Deno.execPath(), {
        args: [
          "eval",
          `import { withDevStateLock } from ${
            JSON.stringify(new URL("./dev_state_lock.ts", import.meta.url).href)
          }; await withDevStateLock(() => { Deno.exit(0); }, "dead-parent", ${
            JSON.stringify(path)
          });`,
        ],
        stdout: "null",
        stderr: "piped",
      }).output();
      assertEquals(output.success, true);
      assertEquals(
        (await Deno.readTextFile(`${path}.owner`)).trim(),
        "dead-parent",
      );
      Deno.env.set("FLOORP_DEV_STATE_TOKEN", "dead-parent");
      await withDevStateLock(
        async () => {
          const contender = await Deno.open(path, { read: true, write: true });
          try {
            assertEquals(await contender.tryLock(true), false);
          } finally {
            contender.close();
          }
        },
        undefined,
        path,
      );
      await assertRejects(
        () => Deno.readTextFile(`${path}.owner`),
        Deno.errors.NotFound,
      );
    } finally {
      if (token === undefined) Deno.env.delete("FLOORP_DEV_STATE_TOKEN");
      else Deno.env.set("FLOORP_DEV_STATE_TOKEN", token);
      await Deno.remove(root, { recursive: true });
    }
  },
});

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

Deno.test("delegated work retains a kernel lease after its parent releases the checkout lock", async () => {
  const root = await Deno.makeTempDir();
  const path = `${root}/lock`;
  const values = new Map<string, string>();
  const environment = {
    get: (key: string) => values.get(key),
    set: (key: string, value: string) => {
      values.set(key, value);
    },
    delete: (key: string) => {
      values.delete(key);
    },
  };
  const ready = Promise.withResolvers<void>();
  const release = Promise.withResolvers<void>();
  let child: Promise<void> = Promise.resolve();
  let peer: Promise<void> = Promise.resolve();
  let peerEntered = false;
  try {
    await withDevStateLock(
      async () => {
        child = withDevStateLock(
          async () => {
            ready.resolve();
            await release.promise;
          },
          undefined,
          path,
          environment,
        );
        await ready.promise;
      },
      "parent",
      path,
      environment,
    );
    peer = withDevStateLock(
      () => {
        peerEntered = true;
        return Promise.resolve();
      },
      "peer",
      path,
      environment,
    );
    await new Promise((resolve) => setTimeout(resolve, 20));
    assertEquals(peerEntered, false);
    release.resolve();
    await Promise.all([child, peer]);
    assertEquals(peerEntered, true);
  } finally {
    release.resolve();
    await Promise.allSettled([child, peer]);
    await Deno.remove(root, { recursive: true });
  }
});
