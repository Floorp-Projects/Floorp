/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals require, __dirname, global, Buffer, process */

const fs = require("fs");
const options = {
  key: fs.readFileSync(__dirname + "/http2-cert.key.pem"),
  cert: fs.readFileSync(__dirname + "/http2-cert.pem"),
};
const http2 = require("http2");
const http = require("http");
const url = require("url");

const path = require("path");

let dnsPacket;
let libPath = path.join(__dirname, "../../xpcshell/dns-packet");
if (fs.existsSync(libPath)) {
  // This is the path of dns-packet when running mochitest locally.
  dnsPacket = require(libPath);
} else {
  // This path is for running mochitest on try.
  dnsPacket = require(path.join(__dirname, "./dns_packet"));
}

let serverPort = parseInt(process.argv[2].split("=")[1]);
let listeningPort = parseInt(process.argv[3].split("=")[1]);
let alpn = process.argv[4].split("=")[1];

let server = http2.createSecureServer(
  options,
  function handleRequest(req, res) {
    let u = "";
    if (req.url != undefined) {
      u = url.parse(req.url, true);
    }

    if (u.pathname === "/dns-query") {
      let payload = Buffer.from("");
      req.on("data", function receiveData(chunk) {
        payload = Buffer.concat([payload, chunk]);
      });
      req.on("end", function finishedData() {
        let packet = dnsPacket.decode(payload);
        let answers = [];
        // Return the HTTPS RR to let Firefox connect to the HTTP/3 server
        if (packet.questions[0].type === "HTTPS") {
          answers.push({
            name: packet.questions[0].name,
            type: "HTTPS",
            ttl: 55,
            class: "IN",
            flush: false,
            data: {
              priority: 1,
              name: packet.questions[0].name,
              values: [
                { key: "alpn", value: [alpn] },
                { key: "port", value: serverPort },
              ],
            },
          });
        } else if (packet.questions[0].type === "A") {
          answers.push({
            name: packet.questions[0].name,
            type: "A",
            ttl: 55,
            flush: false,
            data: "127.0.0.1",
          });
        }

        let buf = dnsPacket.encode({
          type: "response",
          id: packet.id,
          flags: dnsPacket.RECURSION_DESIRED,
          questions: packet.questions,
          answers,
        });

        res.setHeader("Content-Type", "application/dns-message");
        res.setHeader("Content-Length", buf.length);
        res.writeHead(200);
        res.write(buf);
        res.end("");
      });
    }
  }
);

server.listen(listeningPort);

console.log(`DoH server listening on ports ${server.address().port}`);
