/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const FILE_ENTRY = "201: ";

async function readJarDirectory(jarPath, installToDirPath) {
  let extractedPaths = [];
  // Construct a jar URI from the file URI so we can use the JAR URI scheme
  // handling to navigate inside the zip file.
  let jarResponse = await fetch(jarPath);
  let dirListing = await jarResponse.text();
  let lines = dirListing.split("\n");
  let reader = new FileReader();
  for (let line of lines) {
    if (!line.startsWith(FILE_ENTRY)) {
      // Not a file entry, skip.
      continue;
    }
    // code: entry size modified-time type
    let lineSplits = line.split(" ");
    let jarEntry = lineSplits[1];
    let jarType = lineSplits[4];
    // Descend into and flatten any subfolders.
    if (jarType === "DIRECTORY") {
      extractedPaths.push(
        ...(await readJarDirectory(jarPath + jarEntry, installToDirPath))
      );
      continue;
    }
    let fileName = jarEntry.split("/").pop();
    // Only keep the binaries and metadata files.
    if (
      !fileName.endsWith(".info") &&
      !fileName.endsWith(".dll") &&
      !fileName.endsWith(".dylib") &&
      !fileName.endsWith(".sig") &&
      !fileName.endsWith(".so") &&
      !fileName.endsWith(".txt") &&
      fileName !== "LICENSE" &&
      fileName !== "manifest.json"
    ) {
      continue;
    }
    let filePath = jarPath + jarEntry;
    let filePathResponse = await fetch(filePath);
    let fileContents = await filePathResponse.blob();
    let fileData = await new Promise(resolve => {
      reader.onloadend = function () {
        resolve(reader.result);
      };
      reader.readAsArrayBuffer(fileContents);
    });
    await IOUtils.makeDirectory(installToDirPath);
    // Do not extract into directories. Extract all files to the same
    // directory.
    let destPath = PathUtils.join(installToDirPath, fileName);
    await IOUtils.write(destPath, new Uint8Array(fileData), {
      tmpPath: destPath + ".tmp",
    });
    // Ensure files are writable and executable. Otherwise, we may be
    // unable to execute or uninstall them.
    await IOUtils.setPermissions(destPath, 0o700);
    if (IOUtils.delMacXAttr) {
      // If we're on MacOS Firefox will add the quarantine xattr to files it
      // downloads. In this case we want to clear that xattr so we can load
      // the CDM.
      try {
        await IOUtils.delMacXAttr(destPath, "com.apple.quarantine");
      } catch (e) {
        // Failed to remove the attribute. This could be because the profile
        // exists on a file system without xattr support.
        //
        // Don't fail the extraction here, as in this case it's likely we
        // didn't set quarantine on these files in the first place.
      }
    }
    extractedPaths.push(destPath);
  }
  return extractedPaths;
}

onmessage = async function (msg) {
  try {
    // Construct a jar URI from the file URI so we can use the JAR URI scheme
    // handling to navigate inside the zip file.
    let jarPath = "jar:" + msg.data.zipURI + "!/";
    let installToDirPath = PathUtils.join(
      await PathUtils.getProfileDir(),
      ...msg.data.relativeInstallPath
    );
    let extractedPaths = await readJarDirectory(jarPath, installToDirPath);
    postMessage({
      result: "success",
      extractedPaths,
    });
  } catch (e) {
    postMessage({
      result: "fail",
      exception: e.message,
    });
  }
};
