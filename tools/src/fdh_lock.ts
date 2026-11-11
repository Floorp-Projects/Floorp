// SPDX-License-Identifier: MPL-2.0

/**
 * Exclusive file lock for fdh read-modify-write updates (KAN-129).
 *
 * The quota ledger and the GUI desktop lease are JSON files that fdh reads,
 * changes and writes back atomically (tmp file + rename). The rename keeps
 * readers consistent, but two processes that read before either writes can
 * both succeed, e.g. two agents claiming the same lease (Gate 4 limitation).
 * `withFileLock` serialises those updates with an OS-level lock on a sidecar
 * `<file>.lock` (Deno `FsFile.lock`: flock on POSIX, LockFileEx on Windows).
 * The OS drops the lock when the holder exits or crashes, so there is no
 * stale-lock cleanup. Critical sections are a JSON read and write; there is
 * no timeout.
 */

import * as path from "@std/path";

/** Sidecar lock file next to `file`. */
export function lockPathFor(file: string): string {
  return `${file}.lock`;
}

/**
 * Run `fn` while holding an exclusive lock on `lockPath`. Waiters block until
 * the holder releases it. The lock file itself is left in place (empty).
 */
export async function withFileLock<T>(
  lockPath: string,
  fn: () => T | Promise<T>,
): Promise<T> {
  await Deno.mkdir(path.dirname(lockPath), { recursive: true });
  const file = await Deno.open(lockPath, { create: true, write: true });
  try {
    await file.lock(true);
    try {
      return await fn();
    } finally {
      await file.unlock();
    }
  } finally {
    file.close();
  }
}
