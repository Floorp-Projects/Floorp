import * as fs from "node:fs/promises";

export async function writeBuildid2(dir: string, buildid2: string) {
  const path_buildid2 = `${dir}/buildid2`;
  await fs.writeFile(path_buildid2, buildid2);
}
