export async function writeBuildid2(dir: string, buildid2: string) {
  const path_buildid2 = `${dir}/buildid2`;
  await Deno.writeTextFile(path_buildid2, buildid2);
}
