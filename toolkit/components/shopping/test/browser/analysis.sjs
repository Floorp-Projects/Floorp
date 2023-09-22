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

let gResponses = new Map(
  Object.entries({
    ABCDEFG123: { needs_analysis: false, grade: "B", adjusted_rating: 4.1 },
    HIJKLMN456: { needs_analysis: false, grade: "F", adjusted_rating: 1.0 },
    OPQRSTU789: { needs_analysis: true },
  })
);

function handleRequest(request, response) {
  var body = getPostBody(request.bodyInputStream);
  let requestData = JSON.parse(body);
  let productDetails = gResponses.get(requestData.product_id);
  if (!productDetails) {
    response.setStatusLine(request.httpVersion, 400, "Bad Request");
    productDetails = {
      status: 400,
      error: "Bad Request",
    };
  }
  response.write(JSON.stringify(productDetails));
}
