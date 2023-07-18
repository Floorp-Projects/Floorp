/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";
/* exported createHttpServer, loadJSONfromFile, readFile */

Cu.importGlobalProperties(["fetch"]);

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);

const { NetUtil } = ChromeUtils.importESModule(
  "resource://gre/modules/NetUtil.sys.mjs"
);

const createHttpServer = (...args) => {
  AddonTestUtils.maybeInit(this);
  return AddonTestUtils.createHttpServer(...args);
};

async function loadJSONfromFile(path) {
  let file = do_get_file(path);
  let uri = Services.io.newFileURI(file);
  return fetch(uri.spec).then(resp => {
    if (!resp.ok) {
      return undefined;
    }
    return resp.json();
  });
}

function readFile(path) {
  let file = do_get_file(path);
  let fstream = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(
    Ci.nsIFileInputStream
  );
  fstream.init(file, -1, 0, 0);
  let data = NetUtil.readInputStreamToString(fstream, fstream.available());
  fstream.close();
  return data;
}
