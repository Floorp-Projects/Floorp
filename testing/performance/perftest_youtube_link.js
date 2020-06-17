module.exports = async function(context, commands) {
  let rootUrl = "https://www.politico.com/";

  await commands.navigate(rootUrl);

  // Wait for browser to settle
  await commands.wait.byTime(10000);

  // Start the measurement
  await commands.measure.start("pageload");

  // Click on the link and wait for page complete check to finish.
  await commands.click.byClassNameAndWait("js-tealium-tracking");

  // Stop and collect the measurement
  await commands.measure.stop();
};
