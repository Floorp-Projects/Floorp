
import { v7 as uuidv7 } from "uuid";
import { writeVersion } from "../update/version";
import { writeBuildid2 } from "../update/buildid2";
import {resolve} from 'pathe'
import fs from "fs/promises"

const r = (value:string) : string => {
  return resolve(import.meta.dirname,"../..",value)
}

export async function genVersion() {
  await writeVersion(r("./gecko"));
  try {
    await fs.access("_dist");
  } catch {
    await fs.mkdir("_dist");
  }
  await writeBuildid2(r("./_dist"), uuidv7());
}
