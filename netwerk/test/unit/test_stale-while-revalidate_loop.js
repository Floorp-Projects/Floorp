/*

Tests the Cache-control: stale-while-revalidate response directive.

Loads a HTTPS resource with the stale-while-revalidate and tries to load it
twice.

*/

"use strict";

function make_channel(url) {
  return NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
}

async function get_response(channel) {
  return new Promise(resolve => {
    channel.asyncOpen(
      new ChannelListener((request, buffer) => {
        resolve(buffer);
      })
    );
  });
}

add_task(async function () {
  do_get_profile();
  const PORT = Services.env.get("MOZHTTP2_PORT");
  const URI = `https://localhost:${PORT}/stale-while-revalidate-loop-test`;

  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");

  let response = await get_response(make_channel(URI), false);
  Assert.equal(response, "1", "got response ver 1");
  response = await get_response(make_channel(URI), false);
  Assert.equal(response, "1", "got response ver 1");
});
