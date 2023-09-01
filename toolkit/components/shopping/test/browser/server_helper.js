/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";
/* exported getPostBody */

const BinaryInputStream = Components.Constructor(
  "@mozilla.org/binaryinputstream;1",
  "nsIBinaryInputStream",
  "setInputStream"
);

// eslint-disable-next-line  mozilla/reject-importGlobalProperties
Cu.importGlobalProperties(["TextDecoder"]);

function getPostBody(stream) {
  let binaryStream = new BinaryInputStream(stream);
  let count = binaryStream.available();
  let arrayBuffer = new ArrayBuffer(count);
  while (count > 0) {
    let actuallyRead = binaryStream.readArrayBuffer(count, arrayBuffer);
    if (!actuallyRead) {
      throw new Error("Nothing read from input stream!");
    }
    count -= actuallyRead;
  }
  return new TextDecoder().decode(arrayBuffer);
}
