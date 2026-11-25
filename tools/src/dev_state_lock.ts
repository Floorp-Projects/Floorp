// SPDX-License-Identifier: MPL-2.0

import { dirname, join } from "@std/path";
import { PROJECT_ROOT } from "./defines.ts";

export const DEV_STATE_TOKEN_ENV = "FLOORP_DEV_STATE_TOKEN";
const LOCK_PATH = join(PROJECT_ROOT, "_dist", "dev-state.lock");

async function lockBefore(
  file: Deno.FsFile,
  exclusive: boolean,
  deadline: number,
): Promise<void> {
  while (!await file.tryLock(exclusive)) {
    if (Date.now() >= deadline) {
      throw new Error(
        "Dev state is busy; retry after the current startup or operation finishes",
      );
    }
    await new Promise((resolve) => setTimeout(resolve, 25));
  }
}

export async function withDevStateLock<T>(
  operation: () => Promise<T>,
  ownerToken?: string,
  lockPath = LOCK_PATH,
  environment: Pick<typeof Deno.env, "get" | "set" | "delete"> = Deno.env,
  timeoutMs = 10000,
): Promise<T> {
  if (!Number.isFinite(timeoutMs) || timeoutMs <= 0) {
    throw new TypeError("Invalid dev state timeout");
  }
  const deadline = Date.now() + timeoutMs;
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
  let published = false;
  let lease: Deno.FsFile | undefined;
  try {
    lease = await Deno.open(`${lockPath}.lease`, {
      create: true,
      read: true,
      write: true,
      mode: 0o600,
    });
    if (!ownerToken && inherited) {
      await lockBefore(lease, false, deadline);
      locked = await file.tryLock(true);
      if (locked) {
        throw new Error(
          "Dev state delegation expired; retry without the inherited owner token",
        );
      }
      let owner: string | undefined;
      try {
        owner = (await Deno.readTextFile(ownerPath)).trim();
      } catch (error) {
        if (!(error instanceof Deno.errors.NotFound)) throw error;
      }
      if (owner === inherited) {
        return await operation();
      }
      throw new Error(
        "Dev state delegation expired; retry without the inherited owner token",
      );
    }
    while (!locked) {
      await lockBefore(lease, true, deadline);
      locked = await file.tryLock(true);
      if (!locked) {
        await lease.unlock();
        if (Date.now() >= deadline) {
          throw new Error(
            "Dev state is busy; retry after the current startup or operation finishes",
          );
        }
        await new Promise((resolve) => setTimeout(resolve, 25));
      }
    }
    try {
      await Deno.remove(ownerPath);
    } catch (error) {
      if (!(error instanceof Deno.errors.NotFound)) throw error;
    }
    await Deno.writeTextFile(ownerPath, ownerToken ?? crypto.randomUUID(), {
      mode: 0o600,
    });
    published = true;
    environment.set(
      DEV_STATE_TOKEN_ENV,
      (await Deno.readTextFile(ownerPath)).trim(),
    );
    await lease.unlock();
    await lockBefore(lease, false, deadline);
    return await operation();
  } finally {
    try {
      if (published) {
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
