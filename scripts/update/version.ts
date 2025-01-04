import packageJson from "../../package.json" assert { type: "json" }
import fs from "node:fs/promises";

export async function writeVersion(geckoDir: string) {
  await fs.writeFile(
    `${geckoDir}/config/version.txt`,
    packageJson.version,
  );
  await fs.writeFile(
    `${geckoDir}/config/version_display.txt`,
    packageJson.version,
  );
}
