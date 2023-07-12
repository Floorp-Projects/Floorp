// This test is checking the `network.socket.forcePort` preference has an effect.
// We remap an ilusional port `8765` to go to the port the server actually binds to.

"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

function make_channel(url, callback, ctx) {
  return NetUtil.newChannel({ uri: url, loadUsingSystemPrincipal: true });
}

const REMAPPED_PORT = 8765;

add_task(async function check_protocols() {
  function contentHandler(metadata, response) {
    let responseBody = "The server should never return this!";
    response.setHeader("Content-Type", "text/plain");
    response.bodyOutputStream.write(responseBody, responseBody.length);
  }

  const httpserv = new HttpServer();
  httpserv.registerPathHandler("/content", contentHandler);
  httpserv.start(-1);

  do_get_profile();
  Services.prefs.setCharPref(
    "network.socket.forcePort",
    `${REMAPPED_PORT}=${httpserv.identity.primaryPort}`
  );

  function get_response() {
    return new Promise(resolve => {
      const URL = `http://localhost:${REMAPPED_PORT}/content`;
      const channel = make_channel(URL);
      channel.asyncOpen(
        new ChannelListener((request, data) => {
          resolve(data);
        })
      );
    });
  }

  // We expect "Bad request" from the test server because the server doesn't
  // have identity for the remapped port. We don't want to add it too, because
  // that would not prove we actualy remap the port number.
  Assert.equal(await get_response(), "Bad request\n");
  await new Promise(resolve => httpserv.stop(resolve));
});
