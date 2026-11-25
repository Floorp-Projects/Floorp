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
  if (!ownerToken && inherited) {
    try {
      if ((await Deno.readTextFile(ownerPath)).trim() === inherited) {
        return await operation();
      }
    } catch (error) {
      if (!(error instanceof Deno.errors.NotFound)) throw error;
    }
  }
  await Deno.mkdir(dirname(lockPath), { recursive: true });
  const file = await Deno.open(lockPath, {
    create: true,
    read: true,
    write: true,
    mode: 0o600,
  });
  let locked = false;
  try {
    await file.lock(true);
    locked = true;
    await Deno.writeTextFile(ownerPath, ownerToken ?? crypto.randomUUID(), {
      mode: 0o600,
    });
    environment.set(
      DEV_STATE_TOKEN_ENV,
      (await Deno.readTextFile(ownerPath)).trim(),
    );
    return await operation();
  } finally {
    try {
      if (inherited === undefined) environment.delete(DEV_STATE_TOKEN_ENV);
      else environment.set(DEV_STATE_TOKEN_ENV, inherited);
      if (locked) await Deno.remove(ownerPath);
    } finally {
      file.close();
    }
  }
}
