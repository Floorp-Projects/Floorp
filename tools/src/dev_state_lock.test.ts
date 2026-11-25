// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertRejects } from "@std/assert";
import { withDevStateLock } from "./dev_state_lock.ts";

function testEnvironment(token?: string) {
  const values = new Map<string, string>(
    token ? [["FLOORP_DEV_STATE_TOKEN", token]] : [],
  );
  return {
    get: (key: string) => values.get(key),
    set: (key: string, value: string) => {
      values.set(key, value);
    },
    delete: (key: string) => {
      values.delete(key);
    },
  };
}

async function exitOwner(path: string): Promise<void> {
  const output = await new Deno.Command(Deno.execPath(), {
    args: [
      "eval",
      `import { withDevStateLock } from ${
        JSON.stringify(new URL("./dev_state_lock.ts", import.meta.url).href)
      }; await withDevStateLock(() => { Deno.exit(0); }, "dead-parent", ${
        JSON.stringify(path)
      }, { get: () => undefined, set: () => {}, delete: () => {} });`,
    ],
    stdout: "null",
    stderr: "piped",
  }).output();
  assertEquals(output.success, true);
}

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
  async fn() {
    const root = await Deno.makeTempDir();
    const path = `${root}/lock`;
    try {
      await exitOwner(path);
      assertEquals(
        (await Deno.readTextFile(`${path}.owner`)).trim(),
        "dead-parent",
      );
      await assertRejects(
        () =>
          withDevStateLock(
            () => Promise.reject(new Error("must not enter")),
            undefined,
            path,
            testEnvironment("dead-parent"),
          ),
        Error,
        "delegation expired",
      );
      await withDevStateLock(
        () => Promise.resolve(),
        undefined,
        path,
        testEnvironment(),
      );
      await assertRejects(
        () => Deno.readTextFile(`${path}.owner`),
        Deno.errors.NotFound,
      );
    } finally {
      await Deno.remove(root, { recursive: true });
    }
  },
});

Deno.test("two former children cannot mistake each other's liveness probes for a live owner", async () => {
  for (const forceExclusive of [true, false]) {
    const root = await Deno.makeTempDir();
    const path = `${root}/lock`;
    const open = Deno.open;
    const tryLock = Deno.FsFile.prototype.tryLock;
    const checkoutFiles = new WeakSet<Deno.FsFile>();
    const probing = Promise.withResolvers<void>();
    const resume = Promise.withResolvers<void>();
    let first: Promise<unknown> | undefined;
    let calls = 0;
    try {
      await exitOwner(path);
      Deno.open = async (target, options) => {
        const file = await open(target, options);
        if (target === path) checkoutFiles.add(file);
        return file;
      };
      let intercepted = false;
      Deno.FsFile.prototype.tryLock = async function (exclusive) {
        const intercept = checkoutFiles.has(this) && !intercepted;
        if (intercept) intercepted = true;
        const acquired = await tryLock.call(
          this,
          intercept && forceExclusive ? true : exclusive,
        );
        if (intercept && acquired) {
          probing.resolve();
          await resume.promise;
        }
        return acquired;
      };
      const enter = () => {
        calls++;
        return Promise.resolve();
      };
      first = assertRejects(
        () =>
          withDevStateLock(
            enter,
            undefined,
            path,
            testEnvironment("dead-parent"),
          ),
        Error,
        "delegation expired",
      );
      await probing.promise;
      const second = withDevStateLock(
        enter,
        undefined,
        path,
        testEnvironment("dead-parent"),
      );
      if (forceExclusive) await second;
      else await assertRejects(() => second, Error, "delegation expired");
      assertEquals(calls, forceExclusive ? 1 : 0);
      resume.resolve();
      await first;
    } finally {
      resume.resolve();
      await first;
      Deno.open = open;
      Deno.FsFile.prototype.tryLock = tryLock;
      await Deno.remove(root, { recursive: true });
    }
  }
});

Deno.test("takeover excludes former delegation while the old marker still exists", async () => {
  const root = await Deno.makeTempDir();
  const path = `${root}/lock`;
  const remove = Deno.remove;
  const takingOver = Promise.withResolvers<void>();
  const resume = Promise.withResolvers<void>();
  let successor: Promise<void> | undefined;
  let admission: Deno.FsFile | undefined;
  let entered = false;
  try {
    await exitOwner(path);
    let intercepted = false;
    Deno.remove = async (target, options) => {
      if (target === `${path}.owner` && !intercepted) {
        intercepted = true;
        takingOver.resolve();
        await resume.promise;
      }
      return await remove(target, options);
    };
    successor = withDevStateLock(
      () => Promise.resolve(),
      "new",
      path,
      testEnvironment(),
    );
    await takingOver.promise;
    assertEquals(
      (await Deno.readTextFile(`${path}.owner`)).trim(),
      "dead-parent",
    );
    admission = await Deno.open(`${path}.lease`, { read: true, write: true });
    assertEquals(await admission.tryLock(false), false);
    const former = assertRejects(
      () =>
        withDevStateLock(
          () => {
            entered = true;
            return Promise.resolve();
          },
          undefined,
          path,
          testEnvironment("dead-parent"),
        ),
      Error,
      "delegation expired",
    );
    resume.resolve();
    await former;
    await successor;
    assertEquals(entered, false);
  } finally {
    resume.resolve();
    await successor;
    Deno.remove = remove;
    admission?.close();
    await remove(root, { recursive: true });
  }
});

Deno.test("successor closes expired delegation before draining leases; nested former children fail fast", async () => {
  const root = await Deno.makeTempDir();
  const path = `${root}/lock`;
  const entered = Promise.withResolvers<void>();
  const release = Promise.withResolvers<void>();
  let child: Promise<void> | undefined;
  let successor: Promise<void> | undefined;
  try {
    await withDevStateLock(
      async () => {
        child = withDevStateLock(
          async () => {
            entered.resolve();
            await release.promise;
          },
          undefined,
          path,
          testEnvironment("old"),
        );
        await entered.promise;
      },
      "old",
      path,
      testEnvironment(),
    );
    await assertRejects(
      () =>
        withDevStateLock(
          () => Promise.resolve(),
          undefined,
          path,
          testEnvironment("old"),
        ),
      Error,
      "delegation expired",
    );
    successor = withDevStateLock(
      () => Promise.resolve(),
      "new",
      path,
      testEnvironment(),
    );
    for (
      let attempt = 0;
      attempt < 100 &&
      await Deno.stat(`${path}.owner`).then(() => true, () => false);
      attempt++
    ) await new Promise((resolve) => setTimeout(resolve, 1));
    await assertRejects(() => Deno.stat(`${path}.owner`), Deno.errors.NotFound);
    await assertRejects(
      () =>
        withDevStateLock(
          () => Promise.resolve(),
          undefined,
          path,
          testEnvironment("old"),
        ),
      Error,
      "delegation expired",
    );
    release.resolve();
    await child;
    await successor;
  } finally {
    release.resolve();
    await child;
    await successor;
    await Deno.remove(root, { recursive: true });
  }
});

Deno.test("contending recovery fails with a bounded busy error instead of waiting indefinitely", async () => {
  const root = await Deno.makeTempDir();
  const path = `${root}/lock`;
  const entered = Promise.withResolvers<void>();
  const release = Promise.withResolvers<void>();
  const owner = withDevStateLock(
    async () => {
      entered.resolve();
      await release.promise;
    },
    "owner",
    path,
    testEnvironment(),
  );
  try {
    await entered.promise;
    await assertRejects(
      () =>
        withDevStateLock(
          () => Promise.resolve(),
          "recovery",
          path,
          testEnvironment(),
          20,
        ),
      Error,
      "Dev state is busy",
    );
  } finally {
    release.resolve();
    await owner;
    await Deno.remove(root, { recursive: true });
  }
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
