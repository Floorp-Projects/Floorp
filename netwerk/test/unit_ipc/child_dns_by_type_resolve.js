/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../unit/head_trr.js */

const dns = Cc["@mozilla.org/network/dns-service;1"].getService(
  Ci.nsIDNSService
);

let test_answer = "bXkgdm9pY2UgaXMgbXkgcGFzc3dvcmQ=";
let test_answer_addr = "127.0.0.1";

add_task(async function testTXTResolve() {
  // use the h2 server as DOH provider
  let [, inRecord, inStatus] = await new TRRDNSListener("_esni.example.com", {
    type: dns.RESOLVE_TYPE_TXT,
  });
  Assert.equal(inStatus, Cr.NS_OK, "status OK");
  let answer = inRecord
    .QueryInterface(Ci.nsIDNSTXTRecord)
    .getRecordsAsOneString();
  Assert.equal(answer, test_answer, "got correct answer");
});
