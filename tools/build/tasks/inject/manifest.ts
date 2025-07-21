import { symlinkDirectory } from "./symlink-directory.ts";
import { ensureDir, isExists, safeRemove } from "../../utils.ts";

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

  if (await isExists(`${binPath}/${dirName}`)) {
    await safeRemove(`${binPath}/${dirName}`);
  }
  await ensureDir(`${binPath}/${dirName}`);
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
  {
    await symlinkDirectory(
      `${binPath}/${dirName}`,
      "src/core/glue/loader-features/_dist",
      "content",
    );

    await symlinkDirectory(
      `${binPath}/${dirName}`,
      "src/core/glue/startup/_dist",
      "startup",
    );
    await symlinkDirectory(`${binPath}/${dirName}`, "src/themes/_dist", "skin");
    await symlinkDirectory(
      `${binPath}/${dirName}`,
      "src/core/glue/loader-modules/_dist",
      "resource",
    );
  }
  // if (mode !== "dev") {  //   await symlink(
  //     r("../../src/ui/settings/_dist"),
  //     `${binPath}/${dirName}/settings`,
  //     option,
  //   );
  // }
}
