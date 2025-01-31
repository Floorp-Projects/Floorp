import fg from "fast-glob";
import path from "node:path";
import fs from "node:fs/promises";
import { transform } from "./wasm/nora-inject.js";
import {transformFileSync} from "@swc/core"

const fileList = await fg("*", {
  cwd: import.meta.dirname + "/mixins",
  onlyFiles: true,
});

export async function applyMixin(binPath: string) {
  const result = transformFileSync(
    path.resolve(import.meta.dirname, `./shared/define.ts`),
    {
      "jsc":{
        "parser":{
          syntax:"typescript",
        }
      }
    }
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
    const result = transformFileSync(
      path.resolve(import.meta.dirname, `./mixins/${file}`),
      {
        "jsc":{
          "parser":{
            syntax:"typescript",
            decorators:true
          },
          transform: {
            decoratorMetadata: true,
            decoratorVersion: "2022-03",
          }
        }
      }
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
        await fs.readFile(`${binPath}/${meta.meta.path}`)
      ).toString();
      const ret = transform(source, JSON.stringify(meta));

      await fs.writeFile(`${binPath}/${meta.meta.path}`, ret);
    }
  }
}
