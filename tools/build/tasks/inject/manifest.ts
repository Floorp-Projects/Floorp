import {
  createSymlink,
  ensureDir,
  isExists,
  resolveFromRoot,
  safeRemove,
} from "../../utils.ts";

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
    /**
     * Setup all required symlinks for the build process
     */

    // Define symlinks to create
    const symlinks = [
      {
        link: resolveFromRoot(
          `${binPath}/${dirName}/content`,
        ),
        target: resolveFromRoot("src/core/glue/loader-features/_dist"),
      },
      {
        link: resolveFromRoot(
          `${binPath}/${dirName}/startup`,
        ),
        target: resolveFromRoot("src/core/glue/startup/_dist"),
      },
      {
        link: resolveFromRoot(
          `${binPath}/${dirName}/skin`,
        ),
        target: resolveFromRoot("src/themes/_dist"),
      },
      {
        link: resolveFromRoot(
          `${binPath}/${dirName}/resource`,
        ),
        target: resolveFromRoot("src/core/glue/loader-modules/_dist"),
      },
    ];

    // Create all symlinks
    for (const { link, target } of symlinks) {
      await createSymlink(link, target);
    }
  }
  // if (mode !== "dev") {  //   await symlink(
  //     r("../../src/ui/settings/_dist"),
  //     `${binPath}/${dirName}/settings`,
  //     option,
  //   );
  // }
}
