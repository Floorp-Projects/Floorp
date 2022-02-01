// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
/* eslint-env node */

/*
Ensure the `--firefox.preference=network.http.http3.enable:true` is
set for this test.
*/

async function test(context, commands) {
  let url = context.options.browsertime.url;

  // Make firefox learn of HTTP/3 server
  // XXX: Need to build an HTTP/3-specific conditioned profile
  // to handle these pre-navigations.
  await commands.navigate(url);

  // Measure initial pageload
  await commands.measure.start("pageload");
  await commands.navigate(url);
  await commands.measure.stop();
  commands.measure.result[0].browserScripts.pageinfo.url = url;
}

module.exports = {
  test,
  owner: "Network Team",
  name: "controlled",
  description: "User-journey live site test for controlled server",
  tags: ["throttlable"],
};
