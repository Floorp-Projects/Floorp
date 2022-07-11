/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// A utility to check if given JSM is already ESM-ified.

/* global require, __dirname */

const fs = require("fs");
const _path = require("path");

/* global exports */

const uri_map = JSON.parse(
  fs.readFileSync(_path.resolve(__dirname, "./map.json"))
);

function esmify(path) {
  return path.replace(/\.(jsm|js|jsm\.js)$/, ".sys.mjs");
}

function isESMifiedSlow(resourceURI) {
  if (!(resourceURI in uri_map)) {
    console.log(`WARNING: Unknown module: ${resourceURI}`);
    return { result: false, jsms: [] };
  }

  let jsms = uri_map[resourceURI];
  if (typeof jsms === "string") {
    jsms = [jsms];
  }

  const prefix = "../../";
  for (const jsm of jsms) {
    if (fs.existsSync(prefix + jsm)) {
      return { result: false, jsms };
    }
    const esm = esmify(jsm);
    if (!fs.existsSync(prefix + esm)) {
      return { result: false, jsms };
    }
  }

  return { result: true, jsms };
}

const isESMified_memo = {};
function isESMified(resourceURI, files) {
  if (!(resourceURI in isESMified_memo)) {
    isESMified_memo[resourceURI] = isESMifiedSlow(resourceURI);
  }

  for (const jsm of isESMified_memo[resourceURI].jsms) {
    files.push(esmify(jsm));
  }

  return isESMified_memo[resourceURI].result;
}

exports.isESMified = isESMified;
