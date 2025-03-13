import { symlink } from "@std/fs/unstable-symlink";
import { emptyDir } from "@std/fs/empty-dir";
import { relative, resolve } from "npm:pathe@^2.0.2";
import { SymlinkOptions } from "@std/fs/unstable-types";
import { ensureDir } from "@std/fs/ensure-dir";

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

  await emptyDir(`${binPath}/${dirName}`);

  const option: SymlinkOptions = { type: "dir" };

  await ensureDir(`${binPath}/${dirName}`);

  await Deno.writeTextFile(
    `${binPath}/${dirName}/noraneko.manifest`,
    `content noraneko content/ contentaccessible=yes
content noraneko-startup startup/ contentaccessible=yes
skin noraneko classic/1.0 skin/
resource noraneko resource/ contentaccessible=yes
${
      mode !== "dev"
        ? `\ncontent noraneko-settings settings/ contentaccessible=yes
content noraneko-modal-child modal-child/ contentaccessible=yes`
        : ""
    }`,
  );

  const r = (v: string) => {
    if (Deno.build.os === "windows") {
      return resolve(import.meta.dirname, v);
    } else {
      return relative(`${binPath}/${dirName}`, `./${v.replace("../../", "")}`);
    }
  };

  await symlink(
    r("../../src/apps/main/_dist"),
    `${binPath}/${dirName}/content`,
    option,
  );

  await symlink(
    r("../../src/apps/startup/_dist"),
    `${binPath}/${dirName}/startup`,
    option,
  );
  await symlink(
    r("../../src/apps/designs/_dist"),
    `${binPath}/${dirName}/skin`,
    option,
  );
  await symlink(
    r("../../src/apps/modules/_dist"),
    `${binPath}/${dirName}/resource`,
    option,
  );

  if (mode !== "dev") {
    await symlink(
      r("../../src/apps/settings/_dist"),
      `${binPath}/${dirName}/settings`,
      option,
    );
    await symlink(
      r("../../src/apps/main/core/utils/modal-child/_dist"),
      `${binPath}/${dirName}/modal-child`,
      option,
    );
  }
}
