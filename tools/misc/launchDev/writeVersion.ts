import { v7 as uuidv7 } from "uuid";
import { writeVersion } from "../update/version.ts";
import { writeBuildid2 } from "../update/buildid2.ts";
import { resolve } from "pathe";
import { ensureDir } from "../../utils/utils.ts";

// Generate version and build ID
export async function genVersion() {
  const projectRoot = resolve(import.meta.dirname!, "../../../");
  await writeVersion(resolve(projectRoot, "static/gecko"));
  try {
    await Deno.stat("_dist");
  } catch {
    await ensureDir("_dist");
  }
  await writeBuildid2(resolve(projectRoot, "_dist"), uuidv7());
}
