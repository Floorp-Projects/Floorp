import fg from "fast-glob";
import { join, resolve } from "pathe";
import { transform } from "./wasm/nora-inject.js";
import { transformFileSync } from "@swc/core";

const mixinsDir = join(import.meta.dirname!, "mixins");
const fileList = await fg("*", {
  cwd: mixinsDir,
  onlyFiles: true,
});

const tempDistDir = join(
  import.meta.dirname!,
  "../../../../_dist/temp",
);
const tempMixinsDir = join(tempDistDir, "mixins");
const tempSharedDir = join(tempDistDir, "shared");

export async function applyMixin(binPath: string) {
  try {
    // Ensure temporary directories exist
    await Deno.mkdir(tempMixinsDir, { recursive: true });
    await Deno.mkdir(tempSharedDir, { recursive: true });

    // Transform and write shared define file
    const defineFilePath = resolve(
      import.meta.dirname!,
      "./shared/define.ts",
    );
    const defineResult = transformFileSync(defineFilePath, {
      jsc: {
        parser: {
          syntax: "typescript",
        },
      },
    });
    const tempDefinePath = join(tempSharedDir, "define.js");
    await Deno.writeTextFile(tempDefinePath, defineResult.code);

    // Process mixin files
    for (const file of fileList) {
      const mixinFilePath = join(mixinsDir, file);
      const mixinResult = transformFileSync(mixinFilePath, {
        jsc: {
          parser: {
            syntax: "typescript",
            decorators: true,
          },
          transform: {
            decoratorMetadata: true,
            decoratorVersion: "2022-03",
          },
        },
      });

      const tempMixinPath = join(tempMixinsDir, file);
      await Deno.writeTextFile(tempMixinPath, mixinResult.code);

      // Dynamically import and apply mixin after transformation.
      // This is necessary to execute the transformed code.
      const module = await import(
        `../../../../_dist/temp/mixins/${file}`
      );
      for (const i in module) {
        try {
          const instance = new module[i]();
          if (instance && instance.__meta__) {
            const meta = instance.__meta__;
            console.log(JSON.stringify(meta));

            const sourceFilePath = join(binPath, meta.meta.path);
            const source = await Deno.readTextFile(sourceFilePath);
            const ret = transform(source, JSON.stringify(meta));

            await Deno.writeTextFile(sourceFilePath, ret);
          } else {
            console.warn(
              `Mixin module ${file} property ${i} does not have a __meta__ property.`,
            );
          }
        } catch (error) {
          console.error(
            `Error processing mixin ${file}, property ${i}:`,
            error,
          );
        }
      }
    }
  } catch (error) {
    console.error("Error during mixin application:", error);
    throw error; // Re-throw to indicate failure
  }
}
