/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

async function test(context, commands) {
  let rootUrl = "https://www.bbc.com/";

  await commands.navigate(rootUrl);

  // Wait for browser to settle
  await commands.wait.byTime(10000);

  // Start the measurement
  await commands.measure.start("pageload");

  // Click on the link and wait for page complete check to finish.
  await commands.click.byClassNameAndWait("block-link__overlay-link");

  // Stop and collect the measurement
  await commands.measure.stop();
}

module.exports = {
  test,
  owner: "Performance Team",
  name: "BBC Link",
  component: "pageload",
  description: "Measures time to load BBC homepage",
};
