import packageJson from "../../package.json" with { type: "json" }
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
