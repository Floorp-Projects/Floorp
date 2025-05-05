import { symlink } from "@std/fs/unstable-symlink";
import { resolve } from "npm:pathe@^2.0.2";

export async function symlinkDirectory(
  targetDirpath: string,
  srcDirpath: string,
  symlinkName: string,
) {
  const symlinkPath = resolve(targetDirpath, symlinkName);
  try {
    const _stats = await Deno.lstat(symlinkPath);
    await Deno.remove(symlinkPath, { recursive: true });
  } catch (err) {
    if (!(err instanceof Deno.errors.NotFound)) throw err;
  }

  await symlink(
    Deno.build.os === "windows"
      ? resolve(import.meta.dirname, "../..", srcDirpath)
      : srcDirpath,
    resolve(targetDirpath, symlinkName),
    { "type": "dir" },
  );
}

if (import.meta.main) {
  await symlinkDirectory(Deno.args[0], Deno.args[1], Deno.args[2]);
}
