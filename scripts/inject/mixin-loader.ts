import fg from "fast-glob";
import path from "node:path";
import babel from "@babel/core";
import fs from "node:fs/promises";
import { transform } from "./wasm/nora-inject";

const fileList = await fg("*", {
  cwd: import.meta.dirname + "/mixins",
  onlyFiles: true,
});

export async function applyMixin() {
  const result = babel.transformFileSync(
    path.resolve(import.meta.dirname, `./shared/define.ts`),
    {
      presets: ["@babel/preset-typescript"],
    },
  );
  await fs.mkdir(
    `${path.relative(process.cwd(), import.meta.dirname)}/../../_dist/temp/mixins`,
    {
      recursive: true,
    },
  );
  await fs.mkdir(
    `${path.relative(process.cwd(), import.meta.dirname)}/../../_dist/temp/shared`,
    {
      recursive: true,
    },
  );
  await fs.writeFile(
    `${path.relative(process.cwd(), import.meta.dirname)}/../../_dist/temp/shared/define.js`,
    result.code,
  );

  for (const file of fileList) {
    const result = babel.transformFileSync(
      path.resolve(import.meta.dirname, `./mixins/${file}`),
      {
        plugins: [
          [
            "@babel/plugin-proposal-decorators",
            { version: "2023-11", options: { decoratorsBeforeExport: true } },
          ],
        ],
      },
    );

    await fs.writeFile(
      `${path.relative(process.cwd(), import.meta.dirname)}/../../_dist/temp/mixins/${file}`,

      result.code,
    );
    //result.code;
    const module = await import(`../../_dist/temp/mixins/${file}`);
    for (const i in module) {
      const meta = new module[i]().__meta__;
      console.log(JSON.stringify(meta));
      const source = (
        await fs.readFile("_dist/bin/" + meta.meta.path)
      ).toString();
      const ret = transform(source, JSON.stringify(meta));

      await fs.writeFile("_dist/bin/" + meta.meta.path, ret);
    }
  }
}
