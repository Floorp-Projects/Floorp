import { symlink } from "@std/fs/unstable-symlink";
import { resolve } from "npm:pathe@^2.0.2";
import { projectRoot, safeRemove } from "../../utils.ts";

export async function symlinkDirectory(
  linkParentDir: string, // Directory where the symlink will be created; e.g. loader path
  linkTarget: string, // Directory that the symlink points to
  linkName: string, // Name of the symlink
) {
  const symlinkPath = resolve(linkParentDir, linkName);
  try {
    await Deno.lstat(symlinkPath);
    await safeRemove(symlinkPath);
  } catch (err) {
    if (!(err instanceof Deno.errors.NotFound)) throw err;
  }

  await symlink(
    Deno.build.os === "windows" ? resolve(projectRoot, linkTarget) : linkTarget,
    resolve(linkParentDir, linkName),
    { "type": "dir" },
  );
}

if (import.meta.main) {
  await symlinkDirectory(Deno.args[0], Deno.args[1], Deno.args[2]);
}
