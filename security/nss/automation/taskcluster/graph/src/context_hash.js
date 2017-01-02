/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import fs from "fs";
import path from "path";
import crypto from "crypto";
import flatmap from "flatmap";

// Compute the SHA-256 digest.
function sha256(data) {
  let hash = crypto.createHash("sha256");
  hash.update(data);
  return hash.digest("hex");
}

// Recursively collect a list of all files of a given directory.
function collectFilesInDirectory(dir) {
  return flatmap(fs.readdirSync(dir), entry => {
    let entry_path = path.join(dir, entry);

    if (fs.lstatSync(entry_path).isDirectory()) {
      return collectFilesInDirectory(entry_path);
    }

    return [entry_path];
  });
}

// Compute a context hash for the given context path.
export default function (context_path) {
  let root = path.join(__dirname, "../../../..");
  let dir = path.join(root, context_path);
  let files = collectFilesInDirectory(dir).sort();
  let hashes = files.map(file => {
    return sha256(file + "|" + fs.readFileSync(file, "utf-8"));
  });

  return sha256(hashes.join(","));
}
