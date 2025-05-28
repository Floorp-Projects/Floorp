import { symlink } from "@std/fs/unstable-symlink";
import { emptyDir } from "@std/fs/empty-dir";
import { relative, resolve } from "npm:pathe@^2.0.2";
import { SymlinkOptions } from "@std/fs/unstable-types";
import { ensureDir } from "@std/fs/ensure-dir";
import { symlinkDirectory } from "./symlink-directory.ts";
import { isExists } from "../utils.ts";

export async function injectManifest(
  binPath: string,
  mode: "dev" | "run-prod" | "prod",
  dirName = "noraneko",
) {
  if (mode !== "prod") {
    const manifest_chrome = await Deno.readTextFile(
      `${binPath}/chrome.manifest`,
    );

    console.log(manifest_chrome);

    if (!manifest_chrome.includes(`manifest ${dirName}/noraneko.manifest`)) {
      await Deno.writeTextFile(
        `${binPath}/chrome.manifest`,
        `${manifest_chrome}\nmanifest ${dirName}/noraneko.manifest`,
      );
    }
  }

  console.log("aa11");
  if (await isExists(`${binPath}/${dirName}`)) {
    await Deno.remove(`${binPath}/${dirName}`, { recursive: true });
  }
  console.log("aa2");
  await ensureDir(`${binPath}/${dirName}`);
  console.log("aa2");
  await Deno.writeTextFile(
    `${binPath}/${dirName}/noraneko.manifest`,
    `content noraneko content/ contentaccessible=yes
content noraneko-startup startup/ contentaccessible=yes
skin noraneko classic/1.0 skin/
resource noraneko resource/ contentaccessible=yes
${
      mode !== "dev"
        ? "\ncontent noraneko-settings settings/ contentaccessible=yes"
        : ""
    }`,
  );
  console.log("aa");
  await symlinkDirectory(
    `${binPath}/${dirName}`,
    "apps/system/loader-features/_dist",
    "content",
  );

  await symlinkDirectory(
    `${binPath}/${dirName}`,
    "apps/system/startup/_dist",
    "startup",
  );
  await symlinkDirectory(
    `${binPath}/${dirName}`,
    "apps/designs/_dist",
    "skin",
  );
  await symlinkDirectory(
    `${binPath}/${dirName}`,
    "apps/loader-modules/_dist",
    "resource",
  );
  console.log("aa1");
  // if (mode !== "dev") {
  //   await symlink(
  //     r("../../src/apps/settings/_dist"),
  //     `${binPath}/${dirName}/settings`,
  //     option,
  //   );
  // }
}
