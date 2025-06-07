import { v7 as uuidv7 } from "uuid";
import { writeVersion } from "../../build/tasks/update/version.ts";
import { writeBuildid2 } from "../../build/tasks/update/buildid2.ts";
import { resolve } from "pathe";

export async function genVersion() {
  const projectRoot = resolve(import.meta.dirname!, "../../../");
  await writeVersion(resolve(projectRoot, "static/gecko"));
  try {
    await Deno.stat("_dist");
  } catch {
    await Deno.mkdir("_dist", { recursive: true });
  }
  await writeBuildid2(resolve(projectRoot, "_dist"), uuidv7());
}
