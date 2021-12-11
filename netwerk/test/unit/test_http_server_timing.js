/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env node */
/* global serverPort */

"use strict";

class ServerCode {
  static async startServer(port) {
    global.http = require("http");
    global.server = global.http.createServer((req, res) => {
      res.setHeader("Content-Type", "text/plain");
      res.setHeader("Content-Length", "12");
      res.setHeader("Transfer-Encoding", "chunked");
      res.setHeader("Trailer", "Server-Timing");
      res.setHeader(
        "Server-Timing",
        "metric; dur=123.4; desc=description, metric2; dur=456.78; desc=description1"
      );
      res.write("data reached");
      res.addTrailers({
        "Server-Timing":
          "metric3; dur=789.11; desc=description2, metric4; dur=1112.13; desc=description3",
      });
      res.end();
    });

    let serverPort = await new Promise(resolve => {
      global.server.listen(0, "0.0.0.0", 2000, () => {
        resolve(global.server.address().port);
      });
    });

    if (process.env.MOZ_ANDROID_DATA_DIR) {
      // When creating a server on Android we must make sure that the port
      // is forwarded from the host machine to the emulator.
      let adb_path = "adb";
      if (process.env.MOZ_FETCHES_DIR) {
        adb_path = `${process.env.MOZ_FETCHES_DIR}/android-sdk-linux/platform-tools/adb`;
      }

      await new Promise(resolve => {
        const { exec } = require("child_process");
        exec(
          `${adb_path} reverse tcp:${serverPort} tcp:${serverPort}`,
          (error, stdout, stderr) => {
            if (error) {
              console.log(`error: ${error.message}`);
              return;
            }
            if (stderr) {
              console.log(`stderr: ${stderr}`);
            }
            // console.log(`stdout: ${stdout}`);
            resolve();
          }
        );
      });
    }

    return serverPort;
  }
}

const responseServerTiming = [
  { metric: "metric", duration: "123.4", description: "description" },
  { metric: "metric2", duration: "456.78", description: "description1" },
];
const trailerServerTiming = [
  { metric: "metric3", duration: "789.11", description: "description2" },
  { metric: "metric4", duration: "1112.13", description: "description3" },
];

let port;

add_task(async function setup() {
  let processId = await NodeServer.fork();
  registerCleanupFunction(async () => {
    await NodeServer.kill(processId);
  });
  await NodeServer.execute(processId, ServerCode);
  port = await NodeServer.execute(processId, `ServerCode.startServer(0)`);
  ok(port);
});

// Test that secure origins can use server-timing, even with plain http
add_task(async function test_localhost_origin() {
  let chan = NetUtil.newChannel({
    uri: `http://localhost:${port}/`,
    loadUsingSystemPrincipal: true,
  });
  await new Promise(resolve => {
    chan.asyncOpen(
      new ChannelListener((request, buffer) => {
        let channel = request.QueryInterface(Ci.nsITimedChannel);
        let headers = channel.serverTiming.QueryInterface(Ci.nsIArray);
        ok(headers.length);

        let expectedResult = responseServerTiming.concat(trailerServerTiming);
        Assert.equal(headers.length, expectedResult.length);

        for (let i = 0; i < expectedResult.length; i++) {
          let header = headers.queryElementAt(i, Ci.nsIServerTiming);
          Assert.equal(header.name, expectedResult[i].metric);
          Assert.equal(header.description, expectedResult[i].description);
          Assert.equal(header.duration, parseFloat(expectedResult[i].duration));
        }
        resolve();
      }, null)
    );
  });
});

// Test that insecure origins can't use server timing.
add_task(async function test_http_non_localhost() {
  Services.prefs.setBoolPref("network.dns.native-is-localhost", true);
  registerCleanupFunction(async () => {
    Services.prefs.clearUserPref("network.dns.native-is-localhost");
  });

  let chan = NetUtil.newChannel({
    uri: `http://example.org:${port}/`,
    loadUsingSystemPrincipal: true,
  });
  await new Promise(resolve => {
    chan.asyncOpen(
      new ChannelListener((request, buffer) => {
        let channel = request.QueryInterface(Ci.nsITimedChannel);
        let headers = channel.serverTiming.QueryInterface(Ci.nsIArray);
        Assert.equal(headers.length, 0);
        resolve();
      }, null)
    );
  });
});
