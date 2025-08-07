import {
  createSymlink,
  ensureDir,
  isExists,
  resolveFromRoot,
  safeRemove,
} from "../../utils/utils.ts";

export async function injectManifest(
  binPath: string,
  mode: "dev" | "run-prod" | "prod",
  dirName = "noraneko",
) {
  // Add entry to chrome.manifest (except for prod)
  if (mode !== "prod") {
    const manifest_chrome = await Deno.readTextFile(
      `${binPath}/chrome.manifest`,
    );
    if (!manifest_chrome.includes(`manifest ${dirName}/noraneko.manifest`)) {
      await Deno.writeTextFile(
        `${binPath}/chrome.manifest`,
        `${manifest_chrome}\nmanifest ${dirName}/noraneko.manifest`,
      );
    }
  }

  // Recreate noraneko.manifest directory
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

  // Create required symbolic links
  const symlinks = [
    {
      link: resolveFromRoot(`${binPath}/${dirName}/content`),
      target: resolveFromRoot("src/core/glue/loader-features/_dist"),
    },
    {
      link: resolveFromRoot(`${binPath}/${dirName}/startup`),
      target: resolveFromRoot("src/core/glue/startup/_dist"),
    },
    {
      link: resolveFromRoot(`${binPath}/${dirName}/skin`),
      target: resolveFromRoot("src/themes/_dist"),
    },
    {
      link: resolveFromRoot(`${binPath}/${dirName}/resource`),
      target: resolveFromRoot("src/core/glue/loader-modules/_dist"),
    },
  ];
  for (const { link, target } of symlinks) {
    await createSymlink(link, target);
  }
}
