// Helpers for handling certs.
// These are taken from
// https://searchfox.org/mozilla-central/rev/36aa22c7ea92bd3cf7910774004fff7e63341cf5/security/manager/ssl/tests/unit/head_psm.js
// but we don't want to drag that file in here because
// - it conflicts with `head_addons.js`.
// - it has a lot of extra code we don't need.
// So dupe relevant code here.

// This file will be included along with head_addons.js, use its globals.
/* import-globals-from head_addons.js */

"use strict";

function readFile(file) {
  let fstream = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(
    Ci.nsIFileInputStream
  );
  fstream.init(file, -1, 0, 0);
  let available = fstream.available();
  let data =
    available > 0 ? NetUtil.readInputStreamToString(fstream, available) : "";
  fstream.close();
  return data;
}

function loadCertChain(prefix, names) {
  let chain = [];
  for (let name of names) {
    let filename = `${prefix}_${name}.pem`;
    chain.push(readFile(do_get_file(filename)));
  }
  return chain;
}
