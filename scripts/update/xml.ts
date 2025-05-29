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

function getPlatformFromPath(xmlPath: string): string {
  const filename = path.basename(xmlPath);
  if (filename.includes("WINNT")) return "win";
  if (filename.includes("Linux")) return "linux";
  if (filename.includes("Darwin")) return "mac";
  throw new Error(`Unknown platform in filename: ${filename}`);
}

function getMarUrl(platform: string, baseType: string): string {
  let base: string;
  switch (baseType) {
    case "release":
      base =
        `https://github.com/Floorp-Projects/Floorp/releases/download/v${meta.noraneko_version}/floorp-`;
      break;
    case "rc":
      base =
        "https://github.com/Floorp-Projects/Floorp/releases/download/v12.0.0-rc/floorp-";
      break;
    case "beta":
      base =
        "https://github.com/Floorp-Projects/Floorp/releases/download/beta/floorp-";
      break;
    default:
      throw new Error(`Unknown baseType: ${baseType}`);
  }
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

const baseType = process.argv[4] || "release";
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
        detailsURL: process.argv[4] === "rc"
          ? "https://blog.floorp.app/ja/release/12.0.0-RC1.html"
          : `https://blog.floorp.app/release/${meta.noraneko_version}/`,
      },
      patch: [
        {
          _attributes: {
            type: "complete",
            URL: getMarUrl(platform, baseType),
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
