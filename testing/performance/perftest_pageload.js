/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

async function setUp(context) {
  context.log.info("setUp example!");
}

async function test(context, commands) {
  let url = context.options.browsertime.url;
  await commands.navigate("https://www.mozilla.org/en-US/");
  await commands.wait.byTime(100);
  await commands.navigate("about:blank");
  await commands.wait.byTime(50);
  return commands.measure.start(url);
}

async function tearDown(context) {
  context.log.info("tearDown example!");
}

module.exports = {
  setUp,
  tearDown,
  test,
  owner: "Performance Team",
  name: "pageload",
  description: "Measures time to load mozilla page",
};
