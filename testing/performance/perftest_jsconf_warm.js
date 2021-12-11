/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

async function test(context, commands) {
  let rootUrl = "https://2019.jsconf.eu/";

  await commands.navigate(rootUrl);

  // Wait for browser to settle
  await commands.wait.byTime(10000);

  // Click on the link and wait a few seconds
  await commands.click.byLinkTextAndWait("Artists");
  await commands.wait.byTime(3000);

  // Back to about
  await commands.click.byLinkTextAndWait("About");
  await commands.wait.byTime(3000);

  // Start the measurement
  await commands.measure.start("pageload");
  await commands.click.byLinkTextAndWait("Artists");

  // Stop and collect the measurement
  await commands.measure.stop();
}

module.exports = {
  test,
  owner: "Performance Team",
  name: "JSConf (warm)",
  description: "Measures time to load JSConf page (warm)",
};
