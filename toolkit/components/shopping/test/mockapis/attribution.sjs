/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function loadHelperScript(path) {
  let scriptFile = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  scriptFile.initWithPath(getState("__LOCATION__"));
  scriptFile = scriptFile.parent;
  scriptFile.append(path);
  let scriptSpec = Services.io.newFileURI(scriptFile).spec;
  Services.scriptloader.loadSubScript(scriptSpec, this);
}
/* import-globals-from ./server_helper.js */
loadHelperScript("server_helper.js");

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

function handleRequest(request, response) {
  var body = getPostBody(request.bodyInputStream);
  let requestData = JSON.parse(body);

  let key = requestData.aid || (requestData.aidvs && requestData.aidvs[0]);
  let responseObj = {
    [key]: null,
  };

  response.write(JSON.stringify(responseObj));
}
