// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
/* eslint-env node */

/*
Ensure the `--firefox.preference=network.http.http3.enable:true` is
set for this test.
*/

async function test(context, commands) {
  let rootUrl = "https://www.google.com/";
  let waitTime = 1000;
  const driver = context.selenium.driver;
  const webdriver = context.selenium.webdriver;

  if (
    (typeof context.options.browsertime !== "undefined") &
    (typeof context.options.browsertime.waitTime !== "undefined")
  ) {
    waitTime = context.options.browsertime.waitTime;
  }

  // Make firefox learn of HTTP/3 server
  await commands.navigate(rootUrl);

  let cycles = 1;
  for (let cycle = 0; cycle < cycles; cycle++) {
    await commands.navigate(rootUrl);
    await commands.wait.byTime(1000);

    // Set up the search
    context.log.info("Setting up search");
    const searchfield = driver.findElement(webdriver.By.name("q"));
    searchfield.sendKeys("Python\n");
    await commands.wait.byTime(5000);

    // Measure the search time
    context.log.info("Start search");
    await commands.measure.start("pageload");
    await commands.click.byJs(`document.querySelector("input[name='btnK']")`);
    await commands.wait.byTime(5000);
    await commands.measure.stop();
    context.log.info("Done");

    commands.measure.result[0].browserScripts.pageinfo.url =
      "Google Search (Python)";

    // Wait for X seconds
    context.log.info(`Waiting for ${waitTime} milliseconds`);
    await commands.wait.byTime(waitTime);

    // Go to the next search page and measure
    context.log.info("Going to second page of search results");
    await commands.measure.start("pageload");
    await commands.click.byIdAndWait("pnnext");

    // XXX: Increase wait time when we add latencies
    await commands.wait.byTime(3000);
    await commands.measure.stop();

    commands.measure.result[1].browserScripts.pageinfo.url =
      "Google Search (Python) - Next Page";
  }
}

module.exports = {
  test,
  owner: "Network Team",
  component: "netwerk",
  name: "g-search",
  description: "User-journey live site test for google search",
};
