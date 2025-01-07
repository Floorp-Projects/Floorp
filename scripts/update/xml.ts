import {json2xml} from "xml-js"

const meta = JSON.parse(await fs.readFile(process.argv[2],{encoding: "utf-8"})) as {version_display:string, version:string, noraneko_version:string, mar_size: string, mar_shasum: string, buildid: string, noraneko_buildid: string};

const update = {
  _declaration: {
    _attributes: {
      version: "1.0",
      encoding: "utf-8",
    }
  },
  updates: {
    update: {
      _attributes: {
        type: "minor",
        displayVersion:meta.version_display,
        appVersion: meta.version,
        platformVersion:meta.version,
        buildID:meta.buildid,
        appVersion2:meta.noraneko_version,
        buildID2: meta.noraneko_buildid
      },
      patch: [
        {
          _attributes: {
            type: "complete",
            URL: "http://github.com/nyanrus/noraneko/releases/download/alpha/noraneko-win-amd64-full.mar",
            hashFunctions: "sha512",
            hashValue: meta.mar_shasum,
            size: meta.mar_size
          }
        }
      ]
    }
  }
}
import fs from "node:fs/promises"
await fs.writeFile(process.argv[3],json2xml(JSON.stringify(update),{"compact":true,spaces:4}));
