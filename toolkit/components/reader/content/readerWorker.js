/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

importScripts("JSDOMParser.js", "Readability.js");

self.onmessage = function (msg) {
  let uri = msg.data.uri;
  let doc = new JSDOMParser().parse(msg.data.doc);
  let article = new Readability(uri, doc).parse();
  postMessage(article);
};
