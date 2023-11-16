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

function handleRequest(_request, response) {
  // We always want the status to be pending for the current tests.
  let status = {
    status: "pending",
  };
  response.write(JSON.stringify(status));
}
