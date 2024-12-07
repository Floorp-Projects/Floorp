import { NoranekoConstants } from "../../src/apps/modules/src/modules/NoranekoConstants.sys.mjs";
import fs from "node:fs/promises";

export async function writeVersion(geckoDir: string) {
  await fs.writeFile(
    `${geckoDir}/config/version.txt`,
    NoranekoConstants.version2,
  );
  await fs.writeFile(
    `${geckoDir}/config/version_display.txt`,
    NoranekoConstants.version2,
  );
}
