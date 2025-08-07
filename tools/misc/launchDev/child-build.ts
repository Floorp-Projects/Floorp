import { resolve } from "pathe";
import { build } from "vite";
import packageJson from "../../../package.json" with { type: "json" };

const r = (value: string): string => {
  return resolve(import.meta.dirname!, "../../..", value);
};

async function launchBuild(mode: string, buildid2: string) {
  if (mode.startsWith("dev")) {
    await Promise.all([
      new Deno.Command(Deno.execPath(), {
        args: ["task", "build", `--env.MODE=${mode}`],
        cwd: r("./src/core/glue/startup"),
      }).output(),
      new Deno.Command(Deno.execPath(), {
        args: [
          "task",
          "build",
          `--env.__BUILDID2__=${buildid2}`,
          `--env.__VERSION2__=${packageJson.version}`,
        ],
        cwd: r("./src/core/glue/loader-modules"),
      }).output(),
    ]);
    await Promise.all([
      new Deno.Command(Deno.execPath(), {
        args: ["task", "build", `--env.MODE=${mode}`],
        cwd: r("./src/core/glue/startup"),
      }).output(),
      build({
        root: r("./src/core/glue/loader-features"),
        configFile: r("./src/core/glue/loader-features/vite.config.ts"),
        define: {
          "import.meta.env.__BUILDID2__": `"${buildid2 ?? ""}"`,
          "import.meta.env.__VERSION2__": `"${packageJson.version}"`,
        },
        base: "chrome://noraneko/content",
      }),
      new Deno.Command(Deno.execPath(), {
        args: [
          "task",
          "build",
          `--env.__BUILDID2__=${buildid2}`,
          `--env.__VERSION2__=${packageJson.version}`,
        ],
        cwd: r("./src/core/glue/loader-modules"),
      }).output(),
    ]);
  }
}

{
  //* main
  await launchBuild(Deno.args[0], Deno.args[1]);
}
