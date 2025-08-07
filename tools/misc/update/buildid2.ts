import { ensureDir, safeRemove } from "../../utils/utils.ts";

export async function writeBuildid2(dir: string, buildid2: string) {
  const pathBuildid2 = `${dir}/buildid2`;
  try {
    // Create directory if it does not exist
    await ensureDir(dir);
    // Remove existing file if present
    await safeRemove(pathBuildid2);
    console.log(`[buildid2] Removed existing file: ${pathBuildid2}`);
    // Write buildid2
    await Deno.writeTextFile(pathBuildid2, buildid2);
    console.log(`[buildid2] Write complete: ${pathBuildid2}`);
  } catch (error) {
    const errorMessage = error instanceof Error ? error.message : String(error);
    console.error(`[buildid2] Write failed: ${pathBuildid2}:`, errorMessage);
    throw error;
  }
}

export async function readBuildid2(dir: string): Promise<string | null> {
  const pathBuildid2 = `${dir}/buildid2`;
  try {
    const buildid2 = await Deno.readTextFile(pathBuildid2);
    return buildid2.trim();
  } catch (error) {
    if (error instanceof Deno.errors.NotFound) {
      return null;
    }
    const errorMessage = error instanceof Error ? error.message : String(error);
    console.warn(`[buildid2] Read warning: ${pathBuildid2}: ${errorMessage}`);
    return null;
  }
}
