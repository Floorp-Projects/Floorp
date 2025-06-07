import packageJson from "../../../../package.json" with { type: "json" };
import fs from "node:fs/promises";

export async function writeVersion(geckoDir: string) {
  await Promise.all([
    Deno.writeTextFile(
      `${geckoDir}/config/version.txt`,
      packageJson.version,
    ),
    Deno.writeTextFile(
      `${geckoDir}/config/version_display.txt`,
      packageJson.version,
    ),
  ]);
}
