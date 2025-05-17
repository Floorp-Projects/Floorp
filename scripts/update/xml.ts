import { json2xml } from "xml-js";
import fs from "node:fs/promises";
import path from "node:path";
import process from "node:process";

const meta = JSON.parse(
  await fs.readFile(process.argv[2], { encoding: "utf-8" }),
) as {
  version_display: string;
  version: string;
  noraneko_version: string;
  mar_size: string;
  mar_shasum: string;
  buildid: string;
  noraneko_buildid: string;
};

// Determine platform from output XML filename
function getPlatformFromPath(xmlPath: string): string {
  const filename = path.basename(xmlPath);
  if (filename.includes("WINNT")) return "win";
  if (filename.includes("Linux")) return "linux";
  if (filename.includes("Darwin")) return "mac";
  throw new Error(`Unknown platform in filename: ${filename}`);
}

function getMarUrl(platform: string): string {
  const base =
    "http://github.com/Floorp-Projects/Floorp-12/releases/download/beta/floorp-";
  switch (platform) {
    case "win":
      return `${base}win-amd64-full.mar`;
    case "linux":
      return `${base}linux-amd64-full.mar`;
    case "mac":
      return `${base}mac-universal-full.mar`;
    default:
      throw new Error(`Unknown platform: ${platform}`);
  }
}

const platform = getPlatformFromPath(process.argv[3]);

const update = {
  _declaration: {
    _attributes: {
      version: "1.0",
      encoding: "UTF-8",
    },
  },
  updates: {
    update: {
      _attributes: {
        type: "minor",
        displayVersion: meta.version_display,
        appVersion: meta.version,
        platformVersion: meta.version,
        buildID: meta.buildid,
        appVersion2: meta.noraneko_version,
        buildID2: meta.noraneko_buildid,
        detailsURL:
          "https://blog.ablaze.one/category/ablaze/ablaze-project/floorp/",
      },
      patch: [
        {
          _attributes: {
            type: "complete",
            URL: getMarUrl(platform),
            size: meta.mar_size,
          },
        },
      ],
    },
  },
};

await fs.writeFile(
  process.argv[3],
  json2xml(JSON.stringify(update), { "compact": true, spaces: 4 }),
);
