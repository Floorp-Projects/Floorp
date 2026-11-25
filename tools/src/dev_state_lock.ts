// SPDX-License-Identifier: MPL-2.0

import { dirname, join } from "@std/path";
import { PROJECT_ROOT } from "./defines.ts";

export const DEV_STATE_TOKEN_ENV = "FLOORP_DEV_STATE_TOKEN";
const LOCK_PATH = join(PROJECT_ROOT, "_dist", "dev-state.lock");

export async function withDevStateLock<T>(
  operation: () => Promise<T>,
  ownerToken?: string,
  lockPath = LOCK_PATH,
  environment: Pick<typeof Deno.env, "get" | "set" | "delete"> = Deno.env,
): Promise<T> {
  const inherited = environment.get(DEV_STATE_TOKEN_ENV);
  const ownerPath = `${lockPath}.owner`;
  await Deno.mkdir(dirname(lockPath), { recursive: true });
  const file = await Deno.open(lockPath, {
    create: true,
    read: true,
    write: true,
    mode: 0o600,
  });
  let locked = false;
  let lease: Deno.FsFile | undefined;
  try {
    lease = await Deno.open(`${lockPath}.lease`, {
      create: true,
      read: true,
      write: true,
      mode: 0o600,
    });
    if (!ownerToken && inherited) {
      locked = await file.tryLock(true);
      if (!locked) {
        let owner: string | undefined;
        try {
          owner = (await Deno.readTextFile(ownerPath)).trim();
        } catch (error) {
          if (!(error instanceof Deno.errors.NotFound)) throw error;
        }
        if (owner === inherited) {
          await lease.lock(false);
          let current: string | undefined;
          try {
            current = (await Deno.readTextFile(ownerPath)).trim();
          } catch (error) {
            if (!(error instanceof Deno.errors.NotFound)) throw error;
          }
          if (current === inherited) return await operation();
          await lease.unlock();
        }
      }
    }
    if (!locked) {
      await file.lock(true);
      locked = true;
    }
    await lease.lock(true);
    await Deno.writeTextFile(ownerPath, ownerToken ?? crypto.randomUUID(), {
      mode: 0o600,
    });
    environment.set(
      DEV_STATE_TOKEN_ENV,
      (await Deno.readTextFile(ownerPath)).trim(),
    );
    await lease.unlock();
    await lease.lock(false);
    return await operation();
  } finally {
    try {
      if (locked) {
        if (inherited === undefined) environment.delete(DEV_STATE_TOKEN_ENV);
        else environment.set(DEV_STATE_TOKEN_ENV, inherited);
        await Deno.remove(ownerPath);
      }
    } finally {
      lease?.close();
      file.close();
    }
  }
}
