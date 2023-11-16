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
    N0T3NOUGHR: { status: "not_enough_reviews", progress: 100.0 },
    PAG3N0TSUP: { status: "page_not_supported", progress: 100.0 },
    UNPR0C3SSA: { status: "unprocessable", progress: 100.0 },
  })
);

function handleRequest(request, response) {
  let body = getPostBody(request.bodyInputStream);
  let requestData = JSON.parse(body);
  let responseData = gResponses.get(requestData.product_id);
  if (!responseData) {
    // We want the status to be completed for most tests.
    responseData = {
      status: "completed",
      progress: 100.0,
    };
  }
  response.write(JSON.stringify(responseData));
}
