/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env node */

module.exports = async function (context, commands) {
  context.log.info("Starting a first-install test");
  let page_cycles = context.options.browsertime.page_cycles;

  for (let count = 0; count < page_cycles; count++) {
    context.log.info("Navigating to about:blank");

    // See bug 1717754
    context.log.info(await commands.js.run(`return document.documentURI;`));

    await commands.navigate("about:blank");

    await commands.measure.start();
    await commands.wait.byTime(1000);
    await commands.navigate("about:welcome");
    await commands.wait.byTime(2000);
    await commands.measure.stop();

    await commands.wait.byTime(2000);
  }

  context.log.info("First-install test ended.");
  return true;
};
