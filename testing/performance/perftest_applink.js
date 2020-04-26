/* eslint-env node */

module.exports = async function(context, commands) {
  "use strict";

  // This violates all sorts of abstraction boundaries, but I don't see supported APIs for "just
  // waiting" nor for allowing navigation scripts to produce measurements.
  await commands.measure.start();
  await commands.measure.browser.wait(commands.measure.pageCompleteCheck);
  await commands.measure.stop();

  const browserScripts = commands.measure.result[0].browserScripts;

  const processLaunchToNavStart =
    browserScripts.pageinfo.navigationStartTime -
    browserScripts.browser.processStartTime;

  browserScripts.pageinfo.processLaunchToNavStart = processLaunchToNavStart;
  console.log("processLaunchToNavStart: " + processLaunchToNavStart);

  return true;
};
