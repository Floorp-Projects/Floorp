/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const BinaryInputStream = Components.Constructor(
  "@mozilla.org/binaryinputstream;1",
  "nsIBinaryInputStream",
  "setInputStream"
);

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

let gResponses = new Map(
  Object.entries({
    ABCDEFG123: { message: "report created" },
    HIJKLMN456: { message: "already reported" },
    OPQRSTU789: { message: "not deleted" },
  })
);

function handleRequest(request, response) {
  var body = getPostBody(request.bodyInputStream);
  let requestData = JSON.parse(body);
  let report = gResponses.get(requestData.product_id);

  response.write(JSON.stringify(report));
}
