/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env node */

module.exports = async function (context, commands) {
  let urlstr = context.options.browsertime.url;
  const parsedUrls = urlstr.split(",");

  let startTime = await commands.js.run(
    `return performance.timeOrigin + performance.now();`
  );
  for (let count = 0; count < parsedUrls.length; count++) {
    context.log.info("Navigating to url:" + parsedUrls[count]);
    context.log.info("Cycle %d, starting the measure", count);
    await commands.measure.start(parsedUrls[count]);
  }

  let endTime = await commands.js.run(
    `return performance.timeOrigin + performance.now();`
  );

  context.log.info("Browsertime pageload benchmark ended.");
  await commands.measure.add("pageload-benchmark", {
    totalTime: endTime - startTime,
  });
  return true;
};
