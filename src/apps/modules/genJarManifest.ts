import { generateJarManifest } from "../common/scripts/gen_jarmanifest.js";
import fg from "fast-glob";
import fs from "node:fs/promises";

const file_list = await fg("./_dist/**/*");
await fs.writeFile(
  "./_dist/jar.mn",
  await generateJarManifest(
    file_list.map((v) => {
      return { fileName: v.replace("./_dist/", "") };
    }),
    { namespace: "noraneko", register_type: "resource", prefix: "modules" },
  ),
);
await fs.writeFile("./_dist/moz.build", 'JAR_MANIFESTS += ["jar.mn"]');
