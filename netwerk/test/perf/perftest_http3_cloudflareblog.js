// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
/* eslint-env node */

/*
Ensure the `--firefox.preference=network.http.http3.enable:true` is
set for this test.
*/

async function test(context, commands) {
  let rootUrl = "https://blog.cloudflare.com/";
  let waitTime = 1000;

  if (
    (typeof context.options.browsertime !== "undefined") &
    (typeof context.options.browsertime.waitTime !== "undefined")
  ) {
    waitTime = context.options.browsertime.waitTime;
  }

  // Make firefox learn of HTTP/3 server
  // XXX: Need to build an HTTP/3-specific conditioned profile
  // to handle these pre-navigations.
  await commands.navigate(rootUrl);

  let cycles = 1;
  for (let cycle = 0; cycle < cycles; cycle++) {
    // Measure initial pageload
    await commands.measure.start("pageload");
    await commands.navigate(rootUrl);
    await commands.measure.stop();
    commands.measure.result[0].browserScripts.pageinfo.url =
      "Cloudflare Blog - Main";

    // Wait for X seconds
    await commands.wait.byTime(waitTime);

    // Measure navigation pageload
    await commands.measure.start("pageload");
    await commands.click.byJsAndWait(`
      document.querySelectorAll("article")[0].querySelector("a")
    `);
    await commands.measure.stop();
    commands.measure.result[1].browserScripts.pageinfo.url =
      "Cloudflare Blog - Article";
  }
}

module.exports = {
  test,
  owner: "Network Team",
  name: "cloudflare",
  component: "netwerk",
  description: "User-journey live site test for cloudflare blog.",
};
