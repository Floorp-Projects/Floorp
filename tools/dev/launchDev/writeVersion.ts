import { v7 as uuidv7 } from "uuid";
import { writeVersion } from "../../build/tasks/update/version.ts";
import { writeBuildid2 } from "../../build/tasks/update/buildid2.ts";
import { resolve } from "pathe";
import fs from "node:fs/promises";

const r = (value: string): string => {
  return resolve(import.meta.dirname, "../..", value);
};

export async function genVersion() {
  await writeVersion(r("./static/gecko"));
  try {
    await fs.access("_dist");
  } catch {
    await fs.mkdir("_dist");
  }
  await writeBuildid2(r("./_dist"), uuidv7());
}
