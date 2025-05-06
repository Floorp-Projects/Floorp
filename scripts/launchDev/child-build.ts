import { resolve } from "pathe";
import { build } from "vite";
import packageJson from "../../package.json" with { type: "json" };
import { $, usePwsh } from "zx";

switch (Deno.build.os) {
  case "windows":
    usePwsh();
}

$.verbose = true;

const r = (value: string): string => {
  return resolve(import.meta.dirname, "../..", value);
};

async function launchBuild(mode: string, buildid2: string) {
  if (mode.startsWith("dev")) {
    await Promise.all([
      $({
        cwd: r("./apps/system/startup"),
      })`deno task build --env.MODE=${mode}`,
      $({
        cwd: r("./apps/system/loader-modules"),
      })`deno task build --env.__BUILDID2__=${buildid2} --env.__VERSION2__=${packageJson.version}`,
    ]);

    // await Promise.all([
    //   // build({
    //   //   configFile: r("./src/apps/designs/vite.config.ts"),
    //   //   root: r("./src/apps/designs"),
    //   // }),
    // ]);
  } else {
    await Promise.all([
      $({
        cwd: r("./apps/system/startup"),
      })`deno task build --env.MODE ${mode}`,
      build({
        root: r("./apps/system/loader-features"),
        configFile: r("./apps/system/loader-features/vite.config.ts"),
        define: {
          "import.meta.env.__BUILDID2__": `"${buildid2 ?? ""}"`,
          "import.meta.env.__VERSION2__": `"${packageJson.version}"`,
        },
        base: "chrome://noraneko/content",
      }),
      $({
        cwd: r("./apps/system/loader-modules"),
      })`deno task build --env.__BUILDID2__=${buildid2} --env.__VERSION2__=${packageJson.version}`,
      // build({
      //   configFile: r("./src/apps/designs/vite.config.ts"),
      //   root: r("./src/apps/designs"),
      // }),
      // build({
      //   configFile: r("./src/apps/settings/vite.config.ts"),
      //   root: r("./src/apps/settings"),
      //   base: "chrome://noraneko-settings/content",
      // }),
    ]);
  }
}

{ //* main
  await launchBuild(Deno.args[0], Deno.args[1]);
}
